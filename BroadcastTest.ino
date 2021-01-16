// Code to test Broadcasting messages to other ESP8266s

#include <WiFiManager.h>        // For managing the Wifi Connection
#include <ESP8266mDNS.h>        // For running OTA and Web Server
#include <WiFiUdp.h>            // For running OTA
#include <ArduinoOTA.h>         // For running OTA
#include <TelnetSerial.h>       // For debugging via Telnet


// Device Info
const char* devicename = "broadcast2";
const char* devicepassword = "broadcast";

//for using LED as a startup status indicator
#include <Ticker.h>
Ticker ticker;
boolean ledState = LOW;   // Used for blinking LEDs when WifiManager in Connecting and Configuring

// On board LED used to show status
#ifndef LED_BUILTIN
#define LED_BUILTIN 13 // ESP32 DOES NOT DEFINE LED_BUILTIN
#endif
const int ledPin =  LED_BUILTIN;  // the number of the LED pin

// Telnet Serial variables
TelnetSerial telnetSerial;  // Manage Telnet connection to receive Serial data
Stream *usbSerial;          // Pointer to USB/Hardware Serial for fallback debugging

// Broadcast variables
WiFiUDP broadcastUdp;
unsigned int broadcastPort = 6789;
IPAddress broadcastIp(192,168,48,255);
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,


// Serial Input variables
String readLine;      // Holds input line from terminal

/*************************************************
 * Setup
 *************************************************/
void setup() {
  Serial.begin(115200);
  Serial.println(ESP.getFullVersion());

  // set the digital pin as output:
  pinMode(ledPin, OUTPUT);

  // start ticker to slow blink LED strip during Setup
  ticker.attach(0.6, tick);


  //
  // Set up the Wifi Connection
  //
  WiFi.hostname(devicename);
  WiFi.mode(WIFI_STA);      // explicitly set mode, esp defaults to STA+AP
  WiFiManager wm;
  // wm.resetSettings();    // reset settings - for testing
  wm.setAPCallback(configModeCallback); //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  //if it does not connect it starts an access point with the specified name here  "AutoConnectAP"
  if (!wm.autoConnect(devicename,devicepassword)) {
    //Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(1000);
  }
  //Serial.println("connected");


  //
  // Set up the Multicast DNS
  //
  MDNS.begin(devicename);


  //
  // Set up OTA
  //
  // ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(devicename);
  ArduinoOTA.setPassword(devicepassword);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    //Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    //Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      //Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      //Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      //Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      //Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      //Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  // Setup Telnet Serial
  telnetSerial.begin(115200);
  usbSerial = telnetSerial.getOriginalSerial();

  // Let USB/Hardware Serial know where to connect.
  usbSerial->println("Ready!");
  usbSerial->print("use 'telnet ");
  usbSerial->print(WiFi.localIP());
  usbSerial->printf(" %d' to connect\n", TELNETSERIAL_DEFAULT_PORT);
  usbSerial->printf(" or '%s.local %d' to connect\n", devicename, TELNETSERIAL_DEFAULT_PORT);

  usbSerial->println("Input to broadcast will come from USB/hardware like SerialMonitor.");
  usbSerial->println("Broadcasts received will published to telnet clients.");

  broadcastUdp.begin(broadcastPort);

  //
  // Done with Setup
  //
  ticker.detach();          // Stop blinking the LED
}


/*************************************************
 * Loop
 *************************************************/
void loop() {
  // Handle any requests
  ArduinoOTA.handle();
  MDNS.update();
  telnetSerial.handle();

  // Listen for input from Terminal
  //while (Serial.available()) {
    if (usbSerial->available() > 0) {
    readLine = usbSerial->readStringUntil('\r');
/*
      char c = Serial.read();
      if ( c == '\r') {
        break;          // Reading until a CR
      }
      else {
        readLine += c;  // Build up string entered
      }
    //}
*/
  }

  // Did we read any input to broadcast?
  if (readLine.length() > 0) {
    usbSerial->print("Typed: ");
    usbSerial->println(readLine);

    //broadcastUdp.send(readLine, broadcastIp, broadcastPort);
    broadcastUdp.beginPacket(broadcastIp, broadcastPort);
    broadcastUdp.print(readLine);
    broadcastUdp.endPacket();
    Serial.print("Broadcast: ");
    Serial.println(readLine);
    
    readLine = "";
  }

  // Handle incoming broadcast packets
  int packetSize = broadcastUdp.parsePacket();
  if (packetSize) {
    Serial.print("Received packet of size ");
    Serial.println(packetSize);
    Serial.print("From ");
    IPAddress remote = broadcastUdp.remoteIP();
    for (int i = 0; i < 4; i++) {
      Serial.print(remote[i], DEC);
      if (i < 3) {
        Serial.print(".");
      }
    }
    Serial.print(", port ");
    Serial.println(broadcastUdp.remotePort());

    // read the packet into packetBufffer
    broadcastUdp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    Serial.println("Contents:");
    Serial.println(packetBuffer);

    // send a reply, to the IP address and port that sent us the packet we received
    //Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    //Udp.write(ReplyBuffer);
    //Udp.endPacket();
  }

}


/*************************************************
 * Callback Utilities during setup
 *************************************************/
 
/*
 * Blink the LED Strip.
 * If on  then turn off
 * If off then turn on
 */
void tick()
{
  //toggle state
  digitalWrite(ledPin, !digitalRead(ledPin));     // set pin to the opposite state
}

/*
 * gets called when WiFiManager enters configuration mode
 */
void configModeCallback (WiFiManager *myWiFiManager) {
  //Serial.println("Entered config mode");
  //Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  //Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}
