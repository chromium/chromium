// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/chromeos/bluetooth_utils.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "device/base/features.h"

namespace device {

namespace {

// https://www.bluetooth.com/specifications/gatt/services.
const char kHIDServiceUUID[] = "1812";

// https://www.bluetooth.com/specifications/assigned-numbers/16-bit-uuids-for-sdos.
const char kSecurityKeyServiceUUID[] = "FFFD";

const size_t kLongTermKeyHexStringLength = 32;

constexpr base::TimeDelta kMaxDeviceSelectionDuration =
    base::TimeDelta::FromSeconds(30);

// Get limited number of devices from |devices| and
// prioritize paired/connecting devices over other devices.
BluetoothAdapter::DeviceList GetLimitedNumDevices(
    size_t max_device_num,
    const BluetoothAdapter::DeviceList& devices) {
  // If |max_device_num| is 0, it means there's no limit.
  if (max_device_num == 0)
    return devices;

  BluetoothAdapter::DeviceList result;
  for (BluetoothDevice* device : devices) {
    if (result.size() == max_device_num)
      break;

    if (device->IsPaired() || device->IsConnecting())
      result.push_back(device);
  }

  for (BluetoothDevice* device : devices) {
    if (result.size() == max_device_num)
      break;

    if (!device->IsPaired() && !device->IsConnecting())
      result.push_back(device);
  }

  return result;
}

// Filter out unknown devices from the list.
BluetoothAdapter::DeviceList FilterUnknownDevices(
    const BluetoothAdapter::DeviceList& devices) {
  if (chromeos::switches::IsUnfilteredBluetoothDevicesEnabled())
    return devices;

  BluetoothAdapter::DeviceList result;
  for (BluetoothDevice* device : devices) {
    // Always allow paired devices to appear in the UI.
    if (device->IsPaired()) {
      result.push_back(device);
      continue;
    }

    // Always filter out laptops, etc. There is no intended use case or
    // Bluetooth profile in this context.
    if (base::FeatureList::IsEnabled(
            chromeos::features::kBluetoothAggressiveAppearanceFilter) &&
        device->GetDeviceType() == BluetoothDeviceType::COMPUTER) {
      continue;
    }

    // Always filter out phones. There is no intended use case or Bluetooth
    // profile in this context.
    if (base::FeatureList::IsEnabled(
            chromeos::features::kBluetoothPhoneFilter) &&
        device->GetDeviceType() == BluetoothDeviceType::PHONE) {
      continue;
    }

    switch (device->GetType()) {
      // Device with invalid bluetooth transport is filtered out.
      case BLUETOOTH_TRANSPORT_INVALID:
        break;
      // For LE devices, check the service UUID to determine if it supports HID
      // or second factor authenticator (security key).
      case BLUETOOTH_TRANSPORT_LE:
        if (base::Contains(device->GetUUIDs(),
                           device::BluetoothUUID(kHIDServiceUUID)) ||
            base::Contains(device->GetUUIDs(),
                           device::BluetoothUUID(kSecurityKeyServiceUUID))) {
          result.push_back(device);
        }
        break;
      // For classic mode devices, only filter out if the name is empty because
      // the device could have an unknown or even known type and still also
      // provide audio/HID functionality.
      case BLUETOOTH_TRANSPORT_CLASSIC:
        if (device->GetName())
          result.push_back(device);
        break;
      // For dual mode devices, a device::BluetoothDevice object without a name
      // and type/appearance most likely signals that it is truly only a LE
      // advertisement for a peripheral which is active, but not pairable. Many
      // popular headphones behave in this exact way. Filter them out until they
      // provide a type/appearance; this means they've become pairable. See
      // https://crbug.com/1656971 for more.
      case BLUETOOTH_TRANSPORT_DUAL:
        if (device->GetName()) {
          if (base::FeatureList::IsEnabled(
                  chromeos::features::kBluetoothAggressiveAppearanceFilter) &&
              device->GetDeviceType() == BluetoothDeviceType::UNKNOWN) {
            continue;
          }

          result.push_back(device);
        }
        break;
    }
  }
  return result;
}

void RecordDeviceSelectionDuration(const std::string& histogram_name,
                                   base::TimeDelta duration) {
  base::UmaHistogramCustomTimes(
      histogram_name, duration, base::TimeDelta::FromMilliseconds(1) /* min */,
      kMaxDeviceSelectionDuration /* max */, 50 /* buckets */);
}

}  // namespace

device::BluetoothAdapter::DeviceList FilterBluetoothDeviceList(
    const BluetoothAdapter::DeviceList& devices,
    BluetoothFilterType filter_type,
    int max_devices) {
  BluetoothAdapter::DeviceList filtered_devices =
      filter_type == BluetoothFilterType::KNOWN ? FilterUnknownDevices(devices)
                                                : devices;
  return GetLimitedNumDevices(max_devices, filtered_devices);
}

std::vector<std::vector<uint8_t>> GetBlockedLongTermKeys() {
  std::string blocklist = base::GetFieldTrialParamValueByFeature(
      chromeos::features::kBlueZLongTermKeyBlocklist,
      chromeos::features::kBlueZLongTermKeyBlocklistParamName);
  std::vector<std::vector<uint8_t>> long_term_keys;
  if (blocklist.empty())
    return long_term_keys;

  std::vector<std::string> hex_keys = base::SplitString(
      blocklist, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& hex_key : hex_keys) {
    // Must be |kLongTermKeyHexStringLength| nibbles in length.
    if (hex_key.length() != kLongTermKeyHexStringLength) {
      LOG(WARNING) << "Incorrect Long Term Key length";
      continue;
    }

    std::vector<uint8_t> bytes_key;
    if (base::HexStringToBytes(hex_key, &bytes_key))
      long_term_keys.push_back(std::move(bytes_key));
  }

  return long_term_keys;
}

void RecordDeviceSelectionDuration(base::TimeDelta duration,
                                   BluetoothUiSurface surface,
                                   bool was_paired,
                                   BluetoothTransport transport) {
  // Throw out longtail results of the user taking longer than
  // |kMaxDeviceSelectionDuration|. Assume that these thrown out results reflect
  // the user not being actively engaged with device connection: leaving the
  // page open for a long time, walking away from computer, etc.
  if (duration > kMaxDeviceSelectionDuration)
    return;

  std::string base_histogram_name =
      "Bluetooth.ChromeOS.DeviceSelectionDuration";
  RecordDeviceSelectionDuration(base_histogram_name, duration);

  std::string surface_name =
      (surface == BluetoothUiSurface::kSettings ? "Settings" : "SystemTray");
  std::string surface_histogram_name = base_histogram_name + "." + surface_name;
  RecordDeviceSelectionDuration(surface_histogram_name, duration);

  std::string paired_name = (was_paired ? "Paired" : "NotPaired");
  std::string paired_histogram_name =
      surface_histogram_name + "." + paired_name;
  RecordDeviceSelectionDuration(paired_histogram_name, duration);

  if (!was_paired) {
    std::string transport_name;
    switch (transport) {
      case BLUETOOTH_TRANSPORT_CLASSIC:
        transport_name = "Classic";
        break;
      case BLUETOOTH_TRANSPORT_LE:
        transport_name = "BLE";
        break;
      case BLUETOOTH_TRANSPORT_DUAL:
        transport_name = "Dual";
        break;
      default:
        return;
    }

    std::string transport_histogram_name =
        paired_histogram_name + "." + transport_name;
    RecordDeviceSelectionDuration(transport_histogram_name, duration);
  }
}

}  // namespace device
