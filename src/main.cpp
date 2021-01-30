#include <SimpleTimer.h>  //https://github.com/jfturcot/SimpleTimer
#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h> //https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA

/*
 * Windows start mosquitto in terminal (no powershell)
 * to start -> .../mosquitto> mosquitto -c mosquitto.conf -v
 * to stop ->  .../mosquitto> net stop mosquitto
 * 
 * subscribe to all -> mosquitto_sub -h 192.168.1.141 -t "#" -v
 */

//USER CONFIGURED SECTION START//
const char *ssid = "BAZZINGA";
const char *password = "12345";
const char *mqtt_server = "192.168.1.141";
const int mqtt_port = 1883;
const char *mqtt_user = "YOUR_MQTT_USERNAME";
const char *mqtt_pass = "YOUR_MQTT_PASSWORD";
const char *mqtt_client_name = "Doorbell"; // Client connections can't have the same connection name
//USER CONFIGURED SECTION END//

WiFiClient espClient;
PubSubClient client(espClient);
SimpleTimer timer;

// Variables
bool alreadyTriggered = false;
const int doorBellPin = 16; //marked as D0 on the board
const int frontPin = 0;     //marked as D3 on the board
const int silencePin = 2;   //marked as D4 on the board
int frontOldStatus = 1;
bool boot = true;

//Functions

void setup_wifi()
{
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{
  // Loop until we're reconnected
  int retries = 0;
  while (!client.connected())
  {
    if (retries < 15)
    {
      Serial.print("Attempting MQTT connection...");
      // Attempt to connect
      if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass))
      {
        Serial.println("connected");
        // Once connected, publish an announcement...
        if (boot == true)
        {
          client.publish("checkIn/DoorbellMCU", "Rebooted");
          boot = false;
        }
        if (boot == false)
        {
          client.publish("checkIn/DoorbellMCU", "Reconnected");
        }
        // ... and resubscribe
        client.subscribe("doorbell/set");
      }
      else
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    if (retries > 14)
    {
      ESP.restart();
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  String newTopic = topic;
  Serial.print(topic);
  Serial.print("] ");
  payload[length] = '\0';
  String newPayload = String((char *)payload);
  Serial.println(newPayload);
  Serial.println();
  if (newTopic == "doorbell/set")
  {
    if (newPayload == "Silent")
    {
      digitalWrite(silencePin, LOW);
      client.publish("state/doorbell", "Silent Doorbell", true);
    }
    if (newPayload == "Audio")
    {
      digitalWrite(silencePin, HIGH);
      client.publish("state/doorbell", "Audio Doorbell", true);
    }
  }
}

void resetTrigger()
{
  alreadyTriggered = false;
}

void getDoorBell()
{
  if (digitalRead(doorBellPin) == 1 && alreadyTriggered == false)
  {
    Serial.println("ring");
    client.publish("doorbell", "Ding");
    alreadyTriggered = true;
    timer.setTimeout(6000, resetTrigger);
  }
}

void getFrontState()
{
  int newStatus = digitalRead(frontPin);
  if (newStatus != frontOldStatus && newStatus == 0)
  {
    client.publish("doors/Front", "Closed", true);
    frontOldStatus = newStatus;
  }
  if (newStatus != frontOldStatus && newStatus == 1)
  {
    client.publish("doors/Front", "Open", true);
    frontOldStatus = newStatus;
  }
}

void checkIn()
{
  client.publish("checkIn/doorbellMCU", "OK");
}

//Run once setup
void setup()
{
  Serial.begin(9600);

  // GPIO Pin Setup
  pinMode(frontPin, INPUT_PULLUP);
  pinMode(doorBellPin, INPUT_PULLDOWN_16);
  pinMode(silencePin, OUTPUT);

  WiFi.mode(WIFI_STA);
  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  ArduinoOTA.setHostname("doorbellMCU");
  ArduinoOTA.begin();

  timer.setInterval(120000, checkIn);
  timer.setInterval(500, getFrontState);
  timer.setInterval(200, getDoorBell);
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();
  timer.run();
}
