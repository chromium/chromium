// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_DEVICE_MAC_H_
#define DEVICE_GAMEPAD_GAMEPAD_DEVICE_MAC_H_

#include <stddef.h>

#include <CoreFoundation/CoreFoundation.h>
#include <ForceFeedback/ForceFeedback.h>
#include <IOKit/hid/IOHIDManager.h>

#include "base/memory/weak_ptr.h"
#include "device/gamepad/abstract_haptic_gamepad.h"
#include "device/gamepad/gamepad_standard_mappings.h"
#include "device/gamepad/public/cpp/gamepad.h"

namespace device {

class Dualshock4Controller;
class HidHapticGamepad;
class XboxHidController;

// GamepadDeviceMac represents a single gamepad device. Gamepad enumeration
// and state polling is handled through the raw HID interface, while haptics
// commands are issued through the ForceFeedback framework.
//
// Dualshock4 haptics are not supported through ForceFeedback and are instead
// sent through the raw HID interface.
class GamepadDeviceMac final : public AbstractHapticGamepad {
 public:
  GamepadDeviceMac(int location_id,
                   IOHIDDeviceRef device_ref,
                   int vendor_id,
                   int product_id);
  ~GamepadDeviceMac() override;

  // Initialize |gamepad| with the number of buttons and axes described in the
  // device's elements array.
  bool AddButtonsAndAxes(Gamepad* gamepad);

  // Update the button and axis state in |gamepad| with the new data in |value|.
  // If the updated element is an axis, the axis value will first be normalized.
  void UpdateGamepadForValue(IOHIDValueRef value, Gamepad* gamepad);

  // Return the OS-assigned ID for this device.
  int GetLocationId() { return location_id_; }

  // Return true if |device| refers to this device.
  bool IsSameDevice(IOHIDDeviceRef device) { return device == device_ref_; }

  // Return true if this device supports force feedback through the
  // ForceFeedback framework.
  bool SupportsVibration();

  // AbstractHapticGamepad public implementation.
  void SetVibration(double strong_magnitude, double weak_magnitude) override;
  void SetZeroVibration() override;
  base::WeakPtr<AbstractHapticGamepad> GetWeakPtr() override;

 private:
  // AbstractHapticGamepad private implementation.
  void DoShutdown() override;

  // Initialize button capabilities for |gamepad|.
  bool AddButtons(Gamepad* gamepad);

  // Initialize axis capabilities for |gamepad|.
  bool AddAxes(Gamepad* gamepad);

  // Return true if this element has a parent collection with a usage page that
  // suggests it could be a gamepad.
  static bool CheckCollection(IOHIDElementRef element);

  // Create a force feedback device node for controlling haptics on
  // |device_ref|. Ownership of the returned reference is retained by the
  // caller.
  static FFDeviceObjectReference CreateForceFeedbackDevice(
      IOHIDDeviceRef device_ref);

  // Create a force feedback effect on |ff_device_ref| and store the description
  // to |ff_effect|. Ownership of the returned reference is retained by the
  // caller.
  static FFEffectObjectReference CreateForceFeedbackEffect(
      FFDeviceObjectReference ff_device_ref,
      FFEFFECT* ff_effect,
      FFCUSTOMFORCE* ff_custom_force,
      LONG* force_data,
      DWORD* axes_data,
      LONG* direction_data);

  int location_id_;
  IOHIDDeviceRef device_ref_;
  GamepadBusType bus_type_;

  IOHIDElementRef button_elements_[Gamepad::kButtonsLengthCap];
  IOHIDElementRef axis_elements_[Gamepad::kAxesLengthCap];
  CFIndex axis_minimums_[Gamepad::kAxesLengthCap];
  CFIndex axis_maximums_[Gamepad::kAxesLengthCap];
  CFIndex axis_report_sizes_[Gamepad::kAxesLengthCap];

  // Force feedback
  FFDeviceObjectReference ff_device_ref_;
  FFEffectObjectReference ff_effect_ref_;
  FFEFFECT ff_effect_;
  FFCUSTOMFORCE ff_custom_force_;
  LONG force_data_[2];
  DWORD axes_data_[2];
  LONG direction_data_[2];

  // Dualshock4 functionality, if available.
  std::unique_ptr<Dualshock4Controller> dualshock4_;

  // Xbox Wireless Controller behaves like a HID gamepad when connected over
  // Bluetooth. In this mode, haptics functionality is provided by |xbox_hid_|.
  // When connected over USB, Xbox Wireless Controller is supported through
  // XboxDataFetcher.
  std::unique_ptr<XboxHidController> xbox_hid_;

  // A controller that uses a HID output report for vibration effects.
  std::unique_ptr<HidHapticGamepad> hid_haptics_;

  base::WeakPtrFactory<GamepadDeviceMac> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_DEVICE_MAC_H_
