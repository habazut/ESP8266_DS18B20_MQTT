/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp8266-nodemcu-mqtt-publish-ds18b20-arduino/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  Adoptions and improvements Copyright Harald Barth 2020
  Attribution 4.0 International (CC BY 4.0) 

*/

#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>

/* credentials.h contains defines of WIFI_SSID and WIFI_PASSWORD */
#include "credentials.h"

// Raspberri Pi Mosquitto MQTT Broker
#define MQTT_HOST IPAddress(192, 168, 0, 17)
// For a cloud MQTT broker, type the domain name
//#define MQTT_HOST "example.com"
#define MQTT_PORT 1883

// Temperature MQTT Topics
#define MQTT_PUB_TEMP "tingfast45/fridgeESP/temperature/"

// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;          
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);
int numberOfDevices = 0;

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);
// Temperature value
float temp;

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

// function to print a device address
void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++){
    if (deviceAddress[i] < 16) Serial.print("0");
      Serial.print(deviceAddress[i], HEX);
  }
}

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

/*void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}*/

void onMqttPublish(uint16_t packetId) {
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

DeviceAddress allDevices[10]; 
byte tries = 0;

void setup() {
  DeviceAddress tempDeviceAddress; 
  sensors.begin();

  Serial.begin(115200);
  Serial.println();

  // Init Sensors
  numberOfDevices = sensors.getDeviceCount();
  Serial.println(numberOfDevices);
  int found=0;
  for(int i=0;i<numberOfDevices && found < 10; i++){
    // Search the wire for address
      if(sensors.getAddress(allDevices[found], i)){
	  Serial.print("Found device ");
	  Serial.print(i, DEC);
	  Serial.print(" with address: ");
	  printAddress(allDevices[found]);
	  found++;
	  Serial.println();
      } else {
	  Serial.print("Found ghost device at ");
	  Serial.print(i, DEC);
	  Serial.print(" but could not detect address. Check power and cabling");
      }
  }
  numberOfDevices = found;

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  //mqttClient.onSubscribe(onMqttSubscribe);
  //mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  // If your broker requires authentication (username and password), set them below
  //mqttClient.setCredentials("REPlACE_WITH_YOUR_USER", "REPLACE_WITH_YOUR_PASSWORD");
  
  connectToWifi();
}

void loop() {
  uint16_t packetIdPub1 = 0;
  sensors.requestTemperatures(); 
  for(int i=0; i<numberOfDevices; i++){
      // Temperature in Celsius degrees
      temp = sensors.getTempC(allDevices[i]);
      String str = String(MQTT_PUB_TEMP) + String(i);
      packetIdPub1 = mqttClient.publish(str.c_str(), 1, true, String(temp).c_str());
      Serial.printf("Publishing on topic %s at QoS 1, packetId: %i ", str.c_str(), packetIdPub1);
      Serial.printf("Message: %.2f \n", temp);
  }
  tries++;
  delay(10000);
  if (packetIdPub1 >= numberOfDevices*2  || tries > 20)
      ESP.deepSleep(5 * 60 * 1000000L); // 5 min
}
