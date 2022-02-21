#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "secrets.h"

//Define Publication and Subscribe Topic
#define AWS_IOT_PUBLISH_TOPIC   "esp8266/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "esp8266/sub"



WiFiClientSecure net;

BearSSL::X509List cert(cacert);
BearSSL::X509List client_crt(client_cert);
BearSSL::PrivateKey key(privkey);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Variable to save current epoch time
unsigned long epochTime; 

// Function that gets current epoch time
unsigned long getTime() {
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  return now;
}

PubSubClient client(net);

unsigned long lastMillis = 0;
time_t now;
time_t nowish = 1510592825;

void NTPConnect(void)
{
  Serial.print("Setting time using SNTP");
  configTime(TIME_ZONE * 3600, 0 * 3600, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}


//Function to receive messages from broker (AWS IoT Core)
void messageReceived(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

//Function to connect with AWS
void connectAWS()
{

  WiFi.mode(WIFI_STA);
  
  //Get Wi-Fi Information
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print(String("Attempting to connect to SSID: ") + String(WIFI_SSID));

  //Await connection
  while (WiFi.status() != WL_CONNECTED)
  { 
    Serial.print(".");
    Serial.print("223");
    delay(1000);
  }
  
  NTPConnect();

  //Get private key and certification 
  net.setTrustAnchors(&cert);
  net.setClientRSACert(&client_crt, &key);

  //AWS Endpoint and port for comunication
  client.setServer(MQTT_HOST, 8883);
  client.setCallback(messageReceived);


  Serial.print("Connecting to AWS IOT");

  //Await Client connect
  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(1000);
  }


  if(!client.connected()){
    Serial.println("AWS IoT Timeout!");
    return;
  }
  

  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

  Serial.println("AWS IoT Connected!");
  
}



unsigned long previousMillis = 0;
const long interval = 5000;

//Publish Message
void publishMessage()
{
  // Set time em Unix Timestamp
  epochTime = getTime();
  StaticJsonDocument<200> doc;
  doc["timestamp"] = epochTime;
  doc["sensor_gate"] = 1;
  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
}

void setup()
{
  Serial.begin(9600);
  // call the function to connect to AWS
  connectAWS();
}

void loop()
{
  now = time(nullptr);
  if (!client.connected())
  {
    connectAWS();
  }
  else
  {
    client.loop();
    if (millis() - lastMillis > 5000)
    {
      lastMillis = millis();
      publishMessage();
    }
  }
}
