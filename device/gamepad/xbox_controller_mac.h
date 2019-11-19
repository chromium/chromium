// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_XBOX_CONTROLLER_MAC_H_
#define DEVICE_GAMEPAD_XBOX_CONTROLLER_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioplugininterface.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "device/gamepad/abstract_haptic_gamepad.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"

struct IOUSBDeviceStruct320;
struct IOUSBInterfaceStruct300;

namespace device {

class XboxControllerMac final : public AbstractHapticGamepad {
 public:
  static const uint16_t kVendorMicrosoft = 0x045e;
  static const uint16_t kProductXbox360Controller = 0x028e;
  static const uint16_t kProductXboxOneController2013 = 0x02d1;
  static const uint16_t kProductXboxOneController2015 = 0x02dd;
  static const uint16_t kProductXboxOneEliteController = 0x02e3;
  static const uint16_t kProductXboxOneSController = 0x02ea;
  static const uint16_t kProductXboxAdaptiveController = 0x0b0a;

  enum ControllerType {
    UNKNOWN_CONTROLLER,
    XBOX_360_CONTROLLER,
    XBOX_ONE_CONTROLLER_2013,
    XBOX_ONE_CONTROLLER_2015,
    XBOX_ONE_ELITE_CONTROLLER,
    XBOX_ONE_S_CONTROLLER,
    XBOX_ADAPTIVE_CONTROLLER,
  };

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
  ~XboxControllerMac() override;

  OpenDeviceResult OpenDevice(io_service_t service);

  void SetLEDPattern(LEDPattern pattern);

  // AbstractHapticGamepad implementation.
  void DoShutdown() override;
  double GetMaxEffectDurationMillis() override;
  void SetVibration(double strong_magnitude, double weak_magnitude) override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

  UInt32 location_id() { return location_id_; }
  uint16_t GetVendorId() const;
  uint16_t GetProductId() const;
  ControllerType GetControllerType() const;
  std::string GetControllerTypeString() const;
  std::string GetIdString() const;
  bool SupportsVibration() const;

 private:
  static void WriteComplete(void* context, IOReturn result, void* arg0);
  static void GotData(void* context, IOReturn result, void* arg0);

  void ProcessXbox360Packet(size_t length);
  void ProcessXboxOnePacket(size_t length);
  void QueueRead();

  void IOError();

  void WriteXbox360Rumble(uint8_t strong_magnitude, uint8_t weak_magnitude);
  void WriteXboxOneInit();
  void WriteXboxOneRumble(uint8_t strong_magnitude, uint8_t weak_magnitude);
  void WriteXboxOneAckGuide(uint8_t sequence_number);

  // Handle for the USB device. IOUSBDeviceStruct320 is the latest version of
  // the device API that is supported on Mac OS 10.6.
  base::mac::ScopedIOPluginInterface<IOUSBDeviceStruct320> device_;

  // Handle for the interface on the device which sends button and analog data.
  // The other interfaces (for the ChatPad and headset) are ignored.
  base::mac::ScopedIOPluginInterface<IOUSBInterfaceStruct300> interface_;

  bool device_is_open_ = false;
  bool interface_is_open_ = false;

  base::ScopedCFTypeRef<CFRunLoopSourceRef> source_;

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

  UInt32 location_id_ = 0;

  Delegate* delegate_ = nullptr;

  ControllerType controller_type_ = UNKNOWN_CONTROLLER;
  int read_endpoint_ = 0;
  int control_endpoint_ = 0;

  uint8_t counter_ = 0;

  base::WeakPtrFactory<XboxControllerMac> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(XboxControllerMac);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_XBOX_CONTROLLER_MAC_H_
