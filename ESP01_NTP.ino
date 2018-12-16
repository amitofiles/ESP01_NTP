
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define LEAP_YEAR(Y)((Y>0) && !(Y%4) && ((Y%100) || !(Y%400)))

char ssid[] = "<your-ssid>";
char pass[] = "<your-network-password>";

//local port to listen for UDP packets
const unsigned int localPort = 2390;

//NTP timestamp is in the first 48 bytes of the message
const int NTP_PACKET_SIZE = 48;

const char* ntpServers[] = {
  "0.asia.pool.ntp.org",
  "1.asia.pool.ntp.org",
  "2.asia.pool.ntp.org",
  "3.asia.pool.ntp.org"
};

//Indian GMT +5:30 which is 19800 seconds
const unsigned long GMT = 19800UL;

IPAddress timeServerIP;
WiFiUDP udp;

//buffer to hold incoming and outgoing packets
byte packetBuffer[NTP_PACKET_SIZE];

uint8_t h = 0, m = 0, s = 0, d = 0, mo = 0;
uint16_t y = 0;
uint8_t try_server = 0;
uint8_t monthLength;

void setup()
{
  Serial.begin(115200);

  //connect wifi
  Serial.println();
  Serial.print("Connecting WIFI...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("Done.");
  Serial.print("Wifi connected on IP ");
  Serial.println(WiFi.localIP());

  //open udp port
  Serial.print("Opening UDP...");
  udp.begin(localPort);
  Serial.println("Done.");
  Serial.print("Local UDP port started at ");
  Serial.println(udp.localPort());

  uint8_t count = (sizeof(ntpServers) / sizeof(ntpServers[0]));
  uint8_t cb = 0;
  uint8_t ntp = 0;

  while (!cb) {

    //delay before next NTP request
    delay(2000);

    //change server after trying 10 times
    if (try_server > 10) {
      Serial.println("Changing NTP server.");
      ntp++;
      try_server = 0;
    }

    //server list ended
    if (ntp > count) {
      ntp = 0;
    }

    //fetch server IP and send request
    Serial.printf("Sending NTP request on %s ...", ntpServers[ntp]);
    WiFi.hostByName(ntpServers[ntp], timeServerIP);
    sendNTPpacket(timeServerIP);
    Serial.println("Done.");
    delay(1000);

    //check received data
    cb = udp.parsePacket();
    if (!cb) {
      Serial.println("No packet received.");
      try_server++;
    }
    else {
      Serial.print("Packet received, length = ");
      Serial.println(cb);
      udp.read(packetBuffer, NTP_PACKET_SIZE); //read the packet into the buffer
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      Serial.print("Seconds since Jan 1 1900 = " );
      Serial.println(secsSince1900);
      const unsigned long seventyYears = 2208988800UL;
      unsigned long epoch = secsSince1900 - seventyYears;
      Serial.print("Unix time = ");
      Serial.println(epoch);

      //update received seconds for Indian time
      epoch = epoch + GMT;
      Serial.print("Indian Unix time = ");
      Serial.println(epoch);

      //now we will extract year, month, day, hour, minute and second
      unsigned long rawTime = epoch / 86400UL;
      unsigned long days = 0, year = 1970;
      uint8_t month;
      static const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

      while ((days += (LEAP_YEAR(year) ? 366 : 365)) <= rawTime) {
        year++;
      }

      rawTime -= days - (LEAP_YEAR(year) ? 366 : 365); // now it is days in this year, starting at 0
      days = 0;
      for (month = 0; month < 12; month++) {
        if (month == 1) { // february
          monthLength = LEAP_YEAR(year) ? 29 : 28;
        } else {
          monthLength = monthDays[month];
        }
        if (rawTime < monthLength) break;
        rawTime -= monthLength;
      }
      String monthStr = ++month < 10 ? "0" + String(month) : String(month); // jan is month 1
      mo = month;
      String dayStr = ++rawTime < 10 ? "0" + String(rawTime) : String(rawTime); // day of month
      d = rawTime;
      y = year;
      h = ((epoch  % 86400L) / 3600);
      m = ((epoch  % 3600) / 60);
      s = epoch % 60;
    }
  }
  delay(1000);

  //all done. Lets print
  Serial.print("Date time is ");
  Serial.printf("%d:%d:%d %d-%d-%d\n", h, m, s, d, mo, y);
}

void loop()
{}

//send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);

  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0; // Stratum, or type of clock
  packetBuffer[2] = 6; // Polling Interval
  packetBuffer[3] = 0xEC; // Peer Clock Precision
  //8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  //send a packet on port 123 of NTP
  udp.beginPacket(address, 123);

  //write recieived data on our local port
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}
