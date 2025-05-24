// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_STANDARD_MAPPINGS_H_
#define DEVICE_GAMEPAD_GAMEPAD_STANDARD_MAPPINGS_H_

#include <string_view>

#include "device/gamepad/public/cpp/gamepad.h"

namespace device {

// For a connected gamepad, specify the type of bus through which it is
// connected. This allows for specialized mappings depending on how the device
// is connected. For instance, a gamepad may require different mappers for USB
// and Bluetooth.
enum GamepadBusType {
  GAMEPAD_BUS_UNKNOWN,
  GAMEPAD_BUS_USB,
  GAMEPAD_BUS_BLUETOOTH
};

typedef void (*GamepadStandardMappingFunction)(const Gamepad& original,
                                               Gamepad* mapped);

// Returns the most suitable mapping function for a particular gamepad.
// |vendor_id| and |product_id| are the USB or Bluetooth vendor and product IDs
// reported by the device. |hid_specification_version| is the binary-coded
// decimal representation of the version of the HID specification that the
// device is compliant with (bcdHID). |version_number| is the firmware version
// number reported by the device (bcdDevice). |bus_type| is the transport
// used to connect to this device, or GAMEPAD_BUS_UNKNOWN if unknown.
GamepadStandardMappingFunction GetGamepadStandardMappingFunction(
    std::string_view product_name,
    const uint16_t vendor_id,
    const uint16_t product_id,
    const uint16_t hid_specification_version,
    const uint16_t version_number,
    GamepadBusType bus_type);

// This defines our canonical mapping order for gamepad-like devices. If these
// items cannot all be satisfied, it is a case-by-case judgement as to whether
// it is better to leave the device unmapped, or to partially map it. In
// general, err towards leaving it *unmapped* so that content can handle
// appropriately.

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.device.gamepad
// GENERATED_JAVA_PREFIX_TO_STRIP: BUTTON_INDEX_
enum CanonicalButtonIndex {
  BUTTON_INDEX_PRIMARY,
  BUTTON_INDEX_SECONDARY,
  BUTTON_INDEX_TERTIARY,
  BUTTON_INDEX_QUATERNARY,
  BUTTON_INDEX_LEFT_SHOULDER,
  BUTTON_INDEX_RIGHT_SHOULDER,
  BUTTON_INDEX_LEFT_TRIGGER,
  BUTTON_INDEX_RIGHT_TRIGGER,
  BUTTON_INDEX_BACK_SELECT,
  BUTTON_INDEX_START,
  BUTTON_INDEX_LEFT_THUMBSTICK,
  BUTTON_INDEX_RIGHT_THUMBSTICK,
  BUTTON_INDEX_DPAD_UP,
  BUTTON_INDEX_DPAD_DOWN,
  BUTTON_INDEX_DPAD_LEFT,
  BUTTON_INDEX_DPAD_RIGHT,
  BUTTON_INDEX_META,
  BUTTON_INDEX_COUNT
};

// Xbox Series X has an extra share button.
enum XboxSeriesXButtons {
  XBOX_SERIES_X_BUTTON_SHARE = CanonicalButtonIndex::BUTTON_INDEX_COUNT,
  XBOX_SERIES_X_BUTTON_COUNT
};

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.device.gamepad
// GENERATED_JAVA_PREFIX_TO_STRIP: AXIS_INDEX_
enum CanonicalAxisIndex {
  AXIS_INDEX_LEFT_STICK_X,
  AXIS_INDEX_LEFT_STICK_Y,
  AXIS_INDEX_RIGHT_STICK_X,
  AXIS_INDEX_RIGHT_STICK_Y,
  AXIS_INDEX_COUNT
};

// The Switch Pro controller has a Capture button that has no equivalent in the
// Standard Gamepad.
enum SwitchProButtons {
  SWITCH_PRO_BUTTON_CAPTURE = BUTTON_INDEX_COUNT,
  SWITCH_PRO_BUTTON_COUNT
};

// Common mapping functions
GamepadButton AxisToButton(float input);
GamepadButton AxisNegativeAsButton(float input);
GamepadButton AxisPositiveAsButton(float input);
GamepadButton ButtonFromButtonAndAxis(GamepadButton button, float axis);
GamepadButton NullButton();
void DpadFromAxis(Gamepad* mapped, float dir);
float RenormalizeAndClampAxis(float value, float min, float max);

// Gamepad common mapping functions
void MapperSwitchPro(const Gamepad& input, Gamepad* mapped);
void MapperSwitchJoyCon(const Gamepad& input, Gamepad* mapped);
void MapperSwitchComposite(const Gamepad& input, Gamepad* mapped);

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_STANDARD_MAPPINGS_H_
