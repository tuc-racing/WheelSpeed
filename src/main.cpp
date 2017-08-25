#include "mbed.h"

Ticker sender;                  // sender is sending CAN messages every 20 milliseconds
Timer t;                        // timer to measure time between two sensor signals
//DigitalIn pin(USER_BUTTON);   // USER_BUTTON = PC_13 on F767ZI - not usable on L432KC
DigitalOut led_green(LED3);     // for LED - it should blink when device is working
Serial device(USBTX, USBRX);    // for debug output on terminal
InterruptIn sensor(PA_9);       // for sensor signal (top left pin = PC_10 on L053R8 and PA_9 on L432KC)
CAN can(PA_11, PA_12);          // for CAN communication: sending frequency to MainECU (PD_0, PD_1 on F767ZI)

uint16_t frequency = 0;
uint32_t delta_us = 0;
uint32_t counter = 0;
int stop = 1;

void signalOccurred() {
    delta_us = t.read_us();     // time difference between two signals
    t.reset();                  // reset
    //counter++;
    //device.printf("Signal arrived! (delta to previous signal = %dµs)\r\n", delta_us);
}

void sendMessage() {
    char temp[2];

    if (stop) return;

    if (delta_us == 0) {
        frequency = 0;
    } else {
        frequency = 10000000 / delta_us;        // 10'000'000 / delta_us = frequency * 10
    }

    if (frequency < 8) frequency = 0;           // if car is pushed, it should still show no wheel speed
    if (t.read_us() > 1000000) frequency = 0;   // to avoid, that wheel speed is shown, even if car stopped for one second (in case last frequency was higher than 8 - line above)

    memcpy(&temp, &frequency, 2);               // make frequency to character array (string)
    can.write(CANMessage(1337, temp, 2));       // send CAN message

    //device.printf("Frequency: %i\tDelta: %i\r\n", frequency, delta_us);
    //delta_us = 0;
    //counter = 0;
}

int main() {
    CANMessage msg;                 // create empty CAN message

    can.frequency(500000);
    device.baud(115200);
    device.printf("========== Device started up ==========\r\n");
    t.start();
    sensor.rise(&signalOccurred);  // attach the address of the signalOccurred function to the rising edge
    sender.attach(&sendMessage, 0.020); // the address of the function to be attached (sendMessage) and the interval (20 milliseconds)

    // spin in a main loop. sender will interrupt it to call sendMessage; interrupts will interrupt this too!
    while(1) {
        if (can.read(msg)) {          // if message is available, read into msg
            device.printf("Message received: %d\r\n", msg.data[0]);   // display message data

            switch (msg.data[0]) {
                case 0x01: stop = 0; break;
                case 0x80: stop = 1; break;
                default: break;
            }
        }

        //if (can.rderror() >= 255) { device.printf("CAN read error: %s\r\n", can.rderror()); can.reset(); }
        if (can.tderror() >= 255) { device.printf("CAN write error: %s\r\n", can.tderror()); can.reset(); }

        device.printf("Frequency: %f Hz\r\n", frequency / 10.0);

        led_green = !led_green;
        wait(0.5);
    }
}
