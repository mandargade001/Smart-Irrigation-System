#include "FS.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "DHT.h"

// Update these with values suitable for your network.

const char *ssid="Kiwi"; 
const char *password="xxxxxxxxxx";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

const char* AWS_endpoint = "aco7yryk91twk-ats.iot.us-east-2.amazonaws.com"; //MQTT broker ip


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}

WiFiClientSecure espClient;
PubSubClient client(AWS_endpoint, 8883, callback, espClient); //set  MQTT port number to 8883 as per //standard
#define BUFFER_LEN 256
long lastMsg = 0;
char msg[BUFFER_LEN];
int Entry_no = 0;

#define DHTTYPE DHT11   // DHT 11 
#define dht_dpin D1     // Signal pin of DHT 11 is connected to the Digital pin 6

DHT dht(dht_dpin, DHTTYPE); 

const int moisturePin = A0;             // moisteure sensor pin
const int motorPin = D0;
float moisturePercentage;              //moisture reading
float h;                  // humidity reading
float t;                  //temperature reading
int motorState=0;

void setup_wifi() {
  
  delay(100);
  // We start by connecting to a WiFi network
  
  espClient.setBufferSizes(512, 512);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  
  while(!timeClient.update()){
    timeClient.forceUpdate();
  }

  espClient.setX509Time(timeClient.getEpochTime());

}


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("TestThing")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("outTopic","hello");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");

      char buf[256];
      espClient.getLastSSLError(buf,256);
      Serial.print("WiFiClientSecure SSL error: ");
      Serial.println(buf);

      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup() {
  
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  pinMode(motorPin, OUTPUT);
  digitalWrite(motorPin, LOW); // keep motor off initally
  
  dht.begin();
  
  setup_wifi();
  delay(1000);
  
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());

  // Load certificate file
  File cert = SPIFFS.open("/cert.der", "r"); //replace cert.crt with your uploaded file name
  if (!cert) {
    Serial.println("Failed to open cert file");
  }
  else
    Serial.println("Success to open cert file");

  delay(1000);

  if (espClient.loadCertificate(cert))
    Serial.println("cert loaded");
  else
    Serial.println("cert not loaded");
    

  // Load private key file
  File private_key = SPIFFS.open("/private.der", "r"); //replace private with your uploaded file name
  if (!private_key) {
    Serial.println("Failed to open private cert file");
  }
  else
    Serial.println("Success to open private cert file");

  delay(1000);

  if (espClient.loadPrivateKey(private_key))
    Serial.println("private key loaded");
  else
    Serial.println("private key not loaded");
    

   // Load CA file
  File ca = SPIFFS.open("/ca.der", "r"); //replace ca eith your uploaded file name
  if (!ca) {
    Serial.println("Failed to open ca ");
  }
  else
    Serial.println("Success to open ca");

    delay(1000);

    if(espClient.loadCACert(ca))
      Serial.println("ca loaded");
    else
      Serial.println("ca failed");
      Serial.print("Heap: "); Serial.println(ESP.getFreeHeap());
  }



void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    
    h = dht.readHumidity();      // Get humidity reading
    t = dht.readTemperature();   // Get temperature reading

    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }
    moisturePercentage = ( 100.00 - ( (analogRead(moisturePin) / 1023.00) * 100.00 ) );
    if(moisturePercentage<0)
    {
      moisturePercentage=0;
    } 
    if (moisturePercentage <= 10 ) 
    {
      digitalWrite(motorPin, HIGH);        // turn on motor
      motorState=1;
    }
  //if (moisturePercentage > 30 && moisturePercentage < 35) {
  //  digitalWrite(motorPin, HIGH);        //turn on motor pump
  //}
    if (moisturePercentage >= 11) 
    {
      digitalWrite(motorPin, LOW);          // turn off motor
      motorState=0;
    }

    ++Entry_no;
//    snprintf (msg, BUFFER_LEN, "{\"Entry_no\" : %d, \"Soil_Moisture\" : %d %, \"Temperature\" : %d deg C, \"Humidity\" : %d %, \"Motor_State\" : \"%s\"}", Entry_no, moisturePercentage, t, h, motorState);
    String temp = "{\"Entry_no\":" + String(Entry_no) + ", \"Soil_Moisture\":" + String(moisturePercentage) + ", \"Temperature\":" + String(t) + ", \"Humidity\":" + String(h) + ", \"Motor_State\":" + String(motorState) + "}";
    int len = temp.length();
    char msg[len+1];
    temp.toCharArray(msg, len+1);
 

    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("outTopic", msg);
    Serial.print("Heap: "); Serial.println(ESP.getFreeHeap()); //Low heap can cause problems
  }
}
