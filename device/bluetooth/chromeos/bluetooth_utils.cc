// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/chromeos/bluetooth_utils.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "device/base/features.h"

namespace device {

namespace {

// https://www.bluetooth.com/specifications/gatt/services.
const char kHIDServiceUUID[] = "1812";

// https://www.bluetooth.com/specifications/assigned-numbers/16-bit-uuids-for-sdos.
const char kSecurityKeyServiceUUID[] = "FFFD";

constexpr base::TimeDelta kMaxDeviceSelectionDuration = base::Seconds(30);

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
    // Always filter out laptops, etc. There is no intended use case or
    // Bluetooth profile in this context.
    if (device->GetDeviceType() == BluetoothDeviceType::COMPUTER) {
      continue;
    }

    // Always filter out phones. There is no intended use case or Bluetooth
    // profile in this context.
    if (base::FeatureList::IsEnabled(
            chromeos::features::kBluetoothPhoneFilter) &&
        device->GetDeviceType() == BluetoothDeviceType::PHONE) {
      continue;
    }

    // Allow paired devices which are not filtered above to appear in the UI.
    if (device->IsPaired()) {
      result.push_back(device);
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
          if (device->GetDeviceType() == BluetoothDeviceType::UNKNOWN) {
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
                                base::Milliseconds(1) /* min */,
                                base::Seconds(30) /* max */, 50 /* buckets */);
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
      histogram_name, duration, base::Milliseconds(1) /* min */,
      kMaxDeviceSelectionDuration /* max */, 50 /* buckets */);
}

std::string GetTransportName(BluetoothTransport transport) {
  switch (transport) {
    case BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC:
      return "Classic";
    case BluetoothTransport::BLUETOOTH_TRANSPORT_LE:
      return "BLE";
    case BluetoothTransport::BLUETOOTH_TRANSPORT_DUAL:
      return "Dual";
    default:
      // A transport type of INVALID or other is unexpected, and no success
      // metric for it exists.
      return "";
  }
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

void RecordPairingResult(absl::optional<ConnectionFailureReason> failure_reason,
                         BluetoothTransport transport,
                         base::TimeDelta duration) {
  RecordPairingTransport(transport);

  std::string transport_name = GetTransportName(transport);
  if (transport_name.empty()) {
    return;
  }

  bool success = !failure_reason.has_value();
  std::string result_histogram_name_prefix =
      "Bluetooth.ChromeOS.Pairing.Result";

  base::UmaHistogramBoolean(result_histogram_name_prefix, success);
  base::UmaHistogramBoolean(result_histogram_name_prefix + "." + transport_name,
                            success);

  std::string duration_histogram_name_prefix =
      "Bluetooth.ChromeOS.Pairing.Duration";
  std::string success_histogram_name = success ? "Success" : "Failure";

  std::string base_histogram_name =
      duration_histogram_name_prefix + "." + success_histogram_name;
  RecordPairingDuration(base_histogram_name, duration);
  RecordPairingDuration(base_histogram_name + "." + transport_name, duration);

  if (!success) {
    base::UmaHistogramEnumeration(
        result_histogram_name_prefix + ".FailureReason", *failure_reason);
    base::UmaHistogramEnumeration(
        result_histogram_name_prefix + ".FailureReason." + transport_name,
        *failure_reason);
  }
}

void RecordUserInitiatedReconnectionAttemptResult(
    absl::optional<ConnectionFailureReason> failure_reason,
    UserInitiatedReconnectionUISurfaces surface) {
  bool success = !failure_reason.has_value();
  std::string base_histogram_name =
      "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Result";

  base::UmaHistogramBoolean(base_histogram_name, success);

  std::string surface_name =
      (surface == UserInitiatedReconnectionUISurfaces::kSettings
           ? "Settings"
           : "SystemTray");
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
                                   DeviceSelectionUISurfaces surface,
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
      (surface == DeviceSelectionUISurfaces::kSettings ? "Settings"
                                                       : "SystemTray");
  std::string surface_histogram_name = base_histogram_name + "." + surface_name;
  RecordDeviceSelectionDuration(surface_histogram_name, duration);

  std::string paired_name = (was_paired ? "Paired" : "NotPaired");
  std::string paired_histogram_name =
      surface_histogram_name + "." + paired_name;
  RecordDeviceSelectionDuration(paired_histogram_name, duration);

  if (!was_paired) {
    std::string transport_name = GetTransportName(transport);
    if (transport_name.empty()) {
      return;
    }
    std::string transport_histogram_name =
        paired_histogram_name + "." + transport_name;
    RecordDeviceSelectionDuration(transport_histogram_name, duration);
  }
}

void RecordPoweredStateOperationResult(PoweredStateOperation operation,
                                       bool success) {
  std::string operation_name =
      operation == PoweredStateOperation::kEnable ? "Enable" : "Disable";

  base::UmaHistogramBoolean(base::StrCat({"Bluetooth.ChromeOS.PoweredState.",
                                          operation_name, ".Result"}),
                            success);
}

void RecordPoweredState(bool is_powered) {
  base::UmaHistogramBoolean("Bluetooth.ChromeOS.PoweredState", is_powered);
}

void RecordForgetResult(ForgetResult forget_result) {
  base::UmaHistogramEnumeration("Bluetooth.ChromeOS.Forget.Result",
                                forget_result);
}

void RecordDisconnectResult(DisconnectResult disconnect_result,
                            BluetoothTransport transport) {
  std::string transport_name = GetTransportName(transport);

  if (transport_name.empty()) {
    return;
  }

  base::UmaHistogramEnumeration("Bluetooth.ChromeOS.Disconnect.Result",
                                disconnect_result);
  base::UmaHistogramEnumeration(
      base::StrCat({"Bluetooth.ChromeOS.Disconnect.Result.", transport_name}),
      disconnect_result);
}

void RecordUiSurfaceDisplayed(BluetoothUiSurface ui_surface) {
  base::UmaHistogramEnumeration("Bluetooth.ChromeOS.UiSurfaceDisplayed",
                                ui_surface);
}

void RecordUserInitiatedReconnectionAttemptDuration(
    absl::optional<ConnectionFailureReason> failure_reason,
    BluetoothTransport transport,
    base::TimeDelta duration) {
  bool success = !failure_reason.has_value();
  std::string transport_name = GetTransportName(transport);

  if (transport_name.empty()) {
    return;
  }
  std::string success_histogram_name = success ? "Success" : "Failure";

  std::string base_histogram_name = base::StrCat(
      {"Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Duration.",
       success_histogram_name});
  base::UmaHistogramTimes(base_histogram_name, duration);
  base::UmaHistogramTimes(
      base::StrCat({base_histogram_name, ".", transport_name}), duration);
}

}  // namespace device
