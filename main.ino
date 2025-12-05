#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <config.h>


/* --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
// Variable Global definition

// Setup for get timestamp with timezone
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// Setup MQTT Client Object
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Setup DS18B20 Sensor Object
const int oneWireBus = 4;
OneWire oneWire(oneWireBus);
DallasTemperature DS18B20(&oneWire);

// Setup Turbidity Sensor
int turbidity_port = 36;

// Variable for publish interval 
const unsigned long PUBLISH_INTERVAL = 5000;
unsigned long lastPublish = 0;


/* --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
// Wifi Setup
void wifi_setup(){
  WiFi.begin(ssid, password, 6);
  Serial.print("\nconnecting to ");
  Serial.println(ssid);

  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(100);
  }

  Serial.println(" Connected");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
}


/* --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
// Setup mqtt client before publish
String makeClientMac(){
  uint8_t chipid = ESP.getEfuseMac();
  char cid[32];
  snprintf(cid, sizeof(cid), "ESP32-%06X", chipid);
  return String(cid);
}

void connectMQTT(){
  mqttClient.setServer(mqtt_server, mqtt_port);

  while (!mqttClient.connected()){
    String clientId = makeClientMac();
    Serial.print("Connecting to MQTT...");
    bool ok = mqttClient.connect(clientId.c_str(), NULL, NULL, MQTT_TOPIC_LWT, 0, true, "offline");

    if (ok){
      Serial.println(" connected!");
      mqttClient.publish(MQTT_TOPIC_LWT, "online", true);
    }else{
      Serial.print(" failed rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void ensureConnection(){
  if(WiFi.status() != WL_CONNECTED){
    wifi_setup();
  }

  if(!mqttClient.connected()){
    connectMQTT();
  }
}


/* --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
// Sensor
// DS18B20 Get Temperature
float dallasTemperature(){
  DS18B20.requestTemperatures();
  float temperatureC = DS18B20.getTempCByIndex(0);
  return temperatureC;
}

// Turbidity Sensor
float turbidityConvert(int sensorValue){
  float turbidity = map(sensorValue, 0, 4095, 0, 100);
  return turbidity;
}

/* --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
// Publish
void publishMQTT(){
  float temperatureC = dallasTemperature();

  int turbidity_value = analogRead(turbidity_port);
  float turbidity = turbidityConvert(turbidity_value);

  time_t now;
  time(&now);

  if (now < 100000) {
      Serial.println("Time not synced yet, data may be from 1970");
  }

  char payload[128];
  snprintf(payload, sizeof(payload), "{\"timestamp\": %ld, \"temperatureC\": %1.f, \"turbidity\": %1.f}", (long)now, temperatureC, turbidity);

  bool ok = mqttClient.publish(MQTT_TOPIC_SUB, payload);
  if(ok){
    Serial.print("Published: ");
    Serial.println(payload);
  }else{
    Serial.println("Publish failed");
  }
}


/* --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
// Main Function
void setup(){
  Serial.begin(115200);
  DS18B20.begin();
  wifi_setup();
  connectMQTT();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop(){
  ensureConnection();
  mqttClient.loop();

  unsigned long now = millis();

  if(now - lastPublish >= PUBLISH_INTERVAL){
    publishMQTT();
    lastPublish = now;
  }
}
