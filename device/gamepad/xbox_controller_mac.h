// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_XBOX_CONTROLLER_MAC_H_
#define DEVICE_GAMEPAD_XBOX_CONTROLLER_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "base/mac/scoped_ioplugininterface.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "device/gamepad/abstract_haptic_gamepad.h"
#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/public/mojom/gamepad.mojom-forward.h"

struct IOUSBDeviceStruct320;
struct IOUSBInterfaceStruct300;

namespace device {

class XboxControllerMac final : public AbstractHapticGamepad {
 public:
  enum LEDPattern {
    LED_OFF = 0,

    // 2 quick flashes, then a series of slow flashes (about 1 per second).
    LED_FLASH = 1,

    // Flash three times then hold the LED on. This is the standard way to tell
    // the player which player number they are.
    LED_FLASH_TOP_LEFT = 2,
    LED_FLASH_TOP_RIGHT = 3,
    LED_FLASH_BOTTOM_LEFT = 4,
    LED_FLASH_BOTTOM_RIGHT = 5,

    // Simply turn on the specified LED and turn all other LEDs off.
    LED_HOLD_TOP_LEFT = 6,
    LED_HOLD_TOP_RIGHT = 7,
    LED_HOLD_BOTTOM_LEFT = 8,
    LED_HOLD_BOTTOM_RIGHT = 9,

    LED_ROTATE = 10,

    LED_FLASH_FAST = 11,
    LED_FLASH_SLOW = 12,  // Flash about once per 3 seconds

    // Flash alternating LEDs for a few seconds, then flash all LEDs about once
    // per second
    LED_ALTERNATE_PATTERN = 13,

    // 14 is just another boring flashing speed.

    // Flash all LEDs once then go black.
    LED_FLASH_ONCE = 15,

    LED_NUM_PATTERNS
  };

  enum OpenDeviceResult {
    OPEN_SUCCEEDED = 0,
    OPEN_FAILED,
    OPEN_FAILED_EXCLUSIVE_ACCESS
  };

  struct Data {
    bool buttons[15];
    float triggers[2];
    float axes[4];
  };

  class Delegate {
   public:
    virtual void XboxControllerGotData(XboxControllerMac* controller,
                                       const Data& data) = 0;
    virtual void XboxControllerGotGuideData(XboxControllerMac* controller,
                                            bool guide) = 0;
    virtual void XboxControllerError(XboxControllerMac* controller) = 0;
  };

  explicit XboxControllerMac(Delegate* delegate);
  XboxControllerMac(const XboxControllerMac& entry) = delete;
  XboxControllerMac& operator=(const XboxControllerMac& entry) = delete;
  ~XboxControllerMac() override;

  // Open the Xbox controller represented by |service| and perform any necessary
  // initialization. Returns OPEN_SUCCEEDED if the device was opened
  // successfully or OPEN_FAILED on failure. Returns
  // OPEN_FAILED_EXCLUSIVE_ACCESS if the device is already opened by another
  // process.
  OpenDeviceResult OpenDevice(io_service_t service);

  // Send a command to an Xbox 360 controller to set the player indicator LED
  // |pattern|.
  void SetLEDPattern(LEDPattern pattern);

  // AbstractHapticGamepad implementation.
  void DoShutdown() override;
  double GetMaxEffectDurationMillis() override;
  void SetVibration(mojom::GamepadEffectParametersPtr params) override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

  uint32_t location_id() const { return location_id_; }
  GamepadId gamepad_id() const { return gamepad_id_; }
  XInputType xinput_type() const { return xinput_type_; }
  uint16_t vendor_id() const { return vendor_id_; }
  uint16_t product_id() const { return product_id_; }
  std::string product_name() const { return product_name_; }
  bool SupportsVibration() const;

 private:
  // Callback to be called when outgoing packets are sent to the device.
  // |context| is a pointer to the XboxControllerMac and |result| is the error
  // code for the write operation. |arg0| is unused.
  static void WriteComplete(void* context, IOReturn result, void* arg0);

  // Callback to be called when incoming packets are received from the device.
  // |context| is a pointer to the XboxControllerMac, |result| is the error
  // code for the read operation, and |*arg0| contains the number of bytes
  // received.
  //
  // GotData calls IOError if |result| indicates the current read operation
  // failed, or if scheduling the next read operation fails.
  static void GotData(void* context, IOReturn result, void* arg0);

  // Process the incoming packet in |read_buffer_| as an Xbox 360 packet.
  // |length| is the size of the packet in bytes.
  void ProcessXbox360Packet(size_t length);

  // Process the incoming packet in |read_buffer_| as an Xbox One packet.
  // |length| is the size of the packet in bytes.
  void ProcessXboxOnePacket(size_t length);

  // Queue a read from the device. Returns true if the read was queued, or false
  // on I/O error.
  bool QueueRead();

  // Notify the delegate that a fatal I/O error occurred.
  void IOError();

  // Send an Xbox 360 rumble packet to the device, where |strong_magnitude| and
  // |weak_magnitude| are values in the range [0,255] that represent the
  // vibration intensity for the strong and weak rumble motors.
  void WriteXbox360Rumble(uint8_t strong_magnitude, uint8_t weak_magnitude);

  // Send an Xbox One S initialization packet to the device. Returns true if the
  // packet was sent successfully, or false on I/O error.
  bool WriteXboxOneInit();

  // Send an Xbox One rumble packet to the device, where |strong_magnitude| and
  // |weak_magnitude| are values in the range [0,255] that represent the
  // vibration intensity for the strong and weak rumble motors.
  void WriteXboxOneRumble(uint8_t strong_magnitude,
                          uint8_t weak_magnitude,
                          uint8_t left_trigger,
                          uint8_t right_trigger);

  // Send an Xbox One packet to the device acknowledging that the Xbox button
  // was pressed or released. |sequence_number| must match the value in the
  // incoming report containing the new button state.
  void WriteXboxOneAckGuide(uint8_t sequence_number);

  // Handle for the USB device. IOUSBDeviceStruct320 is the latest version of
  // the device API that is supported on Mac OS 10.6.
  base::mac::ScopedIOPluginInterface<IOUSBDeviceStruct320> device_;

  // Handle for the interface on the device which sends button and analog data.
  // The other interfaces (for the ChatPad and headset) are ignored.
  base::mac::ScopedIOPluginInterface<IOUSBInterfaceStruct300> interface_;

  bool device_is_open_ = false;
  bool interface_is_open_ = false;

  base::apple::ScopedCFTypeRef<CFRunLoopSourceRef> source_;

  // This will be set to the max packet size reported by the interface, which
  // is 32 bytes. I would have expected USB to do message framing itself, but
  // somehow we still sometimes (rarely!) get packets off the interface which
  // aren't correctly framed. The 360 controller frames its packets with a 2
  // byte header (type, total length) so we can reframe the packet data
  // ourselves.
  uint16_t read_buffer_size_ = 0;
  std::unique_ptr<uint8_t[]> read_buffer_;

  // The pattern that the LEDs on the device are currently displaying, or
  // LED_NUM_PATTERNS if unknown.
  LEDPattern led_pattern_ = LED_NUM_PATTERNS;

  uint32_t location_id_ = 0;

  raw_ptr<Delegate> delegate_ = nullptr;

  XInputType xinput_type_ = kXInputTypeNone;
  GamepadId gamepad_id_ = GamepadId::kUnknownGamepad;
  uint16_t vendor_id_ = 0;
  uint16_t product_id_ = 0;
  std::string product_name_;

  int read_endpoint_ = 0;
  int control_endpoint_ = 0;

  uint8_t counter_ = 0;

  base::WeakPtrFactory<XboxControllerMac> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_XBOX_CONTROLLER_MAC_H_
