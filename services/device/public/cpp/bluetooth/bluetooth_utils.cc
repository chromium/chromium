// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/string_util_icu.h"
#include "device/bluetooth/strings/grit/bluetooth_strings.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"

namespace device {

using DeviceType = mojom::BluetoothDeviceInfo::DeviceType;

std::u16string GetBluetoothAddressForDisplay(
    const std::array<uint8_t, 6>& address) {
  static constexpr char kAddressFormat[] =
      "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX";

  return base::UTF8ToUTF16(
      base::StringPrintf(kAddressFormat, address[0], address[1], address[2],
                         address[3], address[4], address[5]));
}

std::u16string GetBluetoothDeviceNameForDisplay(
    const mojom::BluetoothDeviceInfoPtr& device_info) {
  if (device_info->name) {
    const std::string& device_name = device_info->name.value();
    if (HasGraphicCharacter(device_name))
      return base::UTF8ToUTF16(device_name);
  }

  auto address_utf16 = GetBluetoothAddressForDisplay(device_info->address);
  auto device_type = device_info->device_type;
  switch (device_type) {
    case DeviceType::kComputer:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_COMPUTER,
                                        address_utf16);
    case DeviceType::kPhone:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_PHONE,
                                        address_utf16);
    case DeviceType::kModem:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MODEM,
                                        address_utf16);
    case DeviceType::kAudio:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_AUDIO,
                                        address_utf16);
    case DeviceType::kCarAudio:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_CAR_AUDIO,
                                        address_utf16);
    case DeviceType::kVideo:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_VIDEO,
                                        address_utf16);
    case DeviceType::kPeripheral:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_PERIPHERAL,
                                        address_utf16);
    case DeviceType::kJoystick:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_JOYSTICK,
                                        address_utf16);
    case DeviceType::kGamepad:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_GAMEPAD,
                                        address_utf16);
    case DeviceType::kKeyboard:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_KEYBOARD,
                                        address_utf16);
    case DeviceType::kMouse:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_MOUSE,
                                        address_utf16);
    case DeviceType::kTablet:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_TABLET,
                                        address_utf16);
    case DeviceType::kKeyboardMouseCombo:
      return l10n_util::GetStringFUTF16(
          IDS_BLUETOOTH_DEVICE_KEYBOARD_MOUSE_COMBO, address_utf16);
    case DeviceType::kUnknown:
      return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_UNKNOWN,
                                        address_utf16);
  }
  NOTREACHED();
}

namespace {

int GetBluetoothDeviceTypeAccessibilityLabelId(DeviceType device_type) {
  switch (device_type) {
    case DeviceType::kComputer:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_COMPUTER;
    case DeviceType::kPhone:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_PHONE;
    case DeviceType::kModem:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_MODEM;
    case DeviceType::kAudio:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_AUDIO;
    case DeviceType::kCarAudio:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_CAR_AUDIO;
    case DeviceType::kVideo:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_VIDEO;
    case DeviceType::kPeripheral:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_PERIPHERAL;
    case DeviceType::kJoystick:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_JOYSTICK;
    case DeviceType::kGamepad:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_GAMEPAD;
    case DeviceType::kKeyboard:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_KEYBOARD;
    case DeviceType::kMouse:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_MOUSE;
    case DeviceType::kTablet:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_TABLET;
    case DeviceType::kKeyboardMouseCombo:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_KEYBOARD_MOUSE_COMBO;
    case DeviceType::kUnknown:
      return IDS_BLUETOOTH_ACCESSIBILITY_DEVICE_TYPE_UNKNOWN;
  }
  NOTREACHED();
}

}  // namespace

// Returns a a11y accessibility label of the device
std::u16string GetBluetoothDeviceLabelForAccessibility(
    const mojom::BluetoothDeviceInfoPtr& device_info) {
  std::u16string name_utf16 =
      device::GetBluetoothAddressForDisplay(device_info->address);
  if (device_info->name) {
    const std::string& device_name = device_info->name.value();
    if (device::HasGraphicCharacter(device_name))
      name_utf16 = base::UTF8ToUTF16(device_name);
  }

  return l10n_util::GetStringFUTF16(
      GetBluetoothDeviceTypeAccessibilityLabelId(device_info->device_type),
      name_utf16);
}

const BluetoothUUID& GetSerialPortProfileUUID() {
  // The Serial Port Profile (SPP) UUID is 1101.
  // https://chromium-review.googlesource.com/c/chromium/src/+/2334682/17..19
  static const BluetoothUUID kValue("1101");
  return kValue;
}

}  // namespace device
