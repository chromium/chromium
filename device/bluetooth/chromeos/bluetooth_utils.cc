// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/chromeos/bluetooth_utils.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"

#include "device/base/features.h"

namespace device {

namespace {

// https://www.bluetooth.com/specifications/gatt/services.
const char kHIDServiceUUID[] = "1812";

// https://www.bluetooth.com/specifications/assigned-numbers/16-bit-uuids-for-sdos.
const char kSecurityKeyServiceUUID[] = "FFFD";

constexpr base::TimeDelta kMaxDeviceSelectionDuration =
    base::TimeDelta::FromSeconds(30);

// This enum is tied directly to a UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other).
enum class BluetoothTransportType {
  kUnknown = 0,
  kClassic = 1,
  kLE = 2,
  kDual = 3,
  kInvalid = 4,
  kMaxValue = kInvalid
};

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

void RecordPairingDuration(const std::string& histogram_name,
                           base::TimeDelta pairing_duration) {
  base::UmaHistogramCustomTimes(histogram_name, pairing_duration,
                                base::TimeDelta::FromMilliseconds(1) /* min */,
                                base::TimeDelta::FromSeconds(30) /* max */,
                                50 /* buckets */);
}

void RecordPairingTransport(BluetoothTransport transport) {
  BluetoothTransportType type;
  switch (transport) {
    case BLUETOOTH_TRANSPORT_CLASSIC:
      type = BluetoothTransportType::kClassic;
      break;
    case BLUETOOTH_TRANSPORT_LE:
      type = BluetoothTransportType::kLE;
      break;
    case BLUETOOTH_TRANSPORT_DUAL:
      type = BluetoothTransportType::kDual;
      break;
    case BLUETOOTH_TRANSPORT_INVALID:
      type = BluetoothTransportType::kInvalid;
      break;
    default:
      type = BluetoothTransportType::kUnknown;
      break;
  }

  base::UmaHistogramEnumeration("Bluetooth.ChromeOS.Pairing.TransportType",
                                type);
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

void RecordPairingResult(base::Optional<ConnectionFailureReason> failure_reason,
                         BluetoothTransport transport,
                         base::TimeDelta duration) {
  RecordPairingTransport(transport);

  std::string transport_histogram_name;
  switch (transport) {
    case BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC:
      transport_histogram_name = "Classic";
      break;
    case BluetoothTransport::BLUETOOTH_TRANSPORT_LE:
      transport_histogram_name = "BLE";
      break;
    case BluetoothTransport::BLUETOOTH_TRANSPORT_DUAL:
      transport_histogram_name = "Dual";
      break;
    default:
      // A transport type of INVALID or other is unexpected, and no success
      // metric for it exists.
      return;
  }

  bool success = !failure_reason.has_value();
  std::string result_histogram_name_prefix =
      "Bluetooth.ChromeOS.Pairing.Result";

  base::UmaHistogramBoolean(result_histogram_name_prefix, success);
  base::UmaHistogramBoolean(
      result_histogram_name_prefix + "." + transport_histogram_name, success);

  std::string duration_histogram_name_prefix =
      "Bluetooth.ChromeOS.Pairing.Duration";
  std::string success_histogram_name = success ? "Success" : "Failure";

  std::string base_histogram_name =
      duration_histogram_name_prefix + "." + success_histogram_name;
  RecordPairingDuration(base_histogram_name, duration);
  RecordPairingDuration(base_histogram_name + "." + transport_histogram_name,
                        duration);

  if (!success) {
    base::UmaHistogramEnumeration(
        result_histogram_name_prefix + ".FailureReason", *failure_reason);
    base::UmaHistogramEnumeration(result_histogram_name_prefix +
                                      ".FailureReason." +
                                      transport_histogram_name,
                                  *failure_reason);
  }
}

void RecordUserInitiatedReconnectionAttemptResult(
    base::Optional<ConnectionFailureReason> failure_reason,
    BluetoothUiSurface surface) {
  bool success = !failure_reason.has_value();
  std::string base_histogram_name =
      "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Result";

  base::UmaHistogramBoolean(base_histogram_name, success);

  std::string surface_name =
      (surface == BluetoothUiSurface::kSettings ? "Settings" : "SystemTray");
  base::UmaHistogramBoolean(base_histogram_name + "." + surface_name, success);

  if (!success) {
    base::UmaHistogramEnumeration(base_histogram_name + ".FailureReason",
                                  *failure_reason);
    base::UmaHistogramEnumeration(
        base_histogram_name + ".FailureReason." + surface_name,
        *failure_reason);
  }
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
