// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_UTILS_H_
#define DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_UTILS_H_

#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class TimeDelta;
}  // namespace base

// This file contains common utilities, including filtering bluetooth devices
// based on the filter criteria.
namespace device {

enum class BluetoothFilterType {
  // No filtering, all bluetooth devices will be returned.
  ALL = 0,
  // Return bluetooth devices that are known to the UI.
  // I.e. bluetooth device type != UNKNOWN
  KNOWN,
};

enum class BluetoothUiSurface {
  kSettings,
  kSystemTray,
};

// This enum is tied directly to a UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other).
enum class ConnectionFailureReason {
  kUnknownError = 0,
  kSystemError = 1,
  kAuthFailed = 2,
  kAuthTimeout = 3,
  kFailed = 4,
  kUnknownConnectionError = 5,
  kUnsupportedDevice = 6,
  kNotConnectable = 7,
  kMaxValue = kNotConnectable
};

// Return filtered devices based on the filter type and max number of devices.
DEVICE_BLUETOOTH_EXPORT device::BluetoothAdapter::DeviceList
FilterBluetoothDeviceList(const BluetoothAdapter::DeviceList& devices,
                          BluetoothFilterType filter_type,
                          int max_devices);

// Record outcome of user attempting to pair to a device.
DEVICE_BLUETOOTH_EXPORT void RecordPairingResult(
    absl::optional<ConnectionFailureReason> failure_reason,
    BluetoothTransport transport,
    base::TimeDelta duration);

// Record outcome of user attempting to reconnect to a previously paired device.
DEVICE_BLUETOOTH_EXPORT void RecordUserInitiatedReconnectionAttemptResult(
    absl::optional<ConnectionFailureReason> failure_reason,
    BluetoothUiSurface surface);

// Record how long it took for a user to find and select the device they wished
// to connect to.
DEVICE_BLUETOOTH_EXPORT void RecordDeviceSelectionDuration(
    base::TimeDelta duration,
    BluetoothUiSurface surface,
    bool was_paired,
    BluetoothTransport transport);

}  // namespace device

#endif  // DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_UTILS_H_
