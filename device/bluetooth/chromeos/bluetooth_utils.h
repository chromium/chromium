// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_UTILS_H_
#define DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_UTILS_H_

#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
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

enum class DeviceSelectionUISurfaces {
  kSettings,
  kSystemTray,
};

enum class PoweredStateOperation {
  kEnable,
  kDisable,
};

enum class UserInitiatedReconnectionUISurfaces {
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
  kAuthCanceled = 8,
  kAuthRejected = 9,
  kInprogress = 10,
  kMaxValue = kInprogress
};

// This enum is tied directly to a UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other).
enum class BluetoothUiSurface {
  kSettingsDeviceListSubpage = 0,
  kSettingsDeviceDetailSubpage = 1,
  kSettingsPairingDialog = 2,
  kBluetoothQuickSettings = 3,
  kStandalonePairingDialog = 4,
  // [Deprecated] kPairedNotification = 5,
  kConnectionToast = 6,
  kDisconnectedToast = 7,
  kOobeHidDetection = 8,
  kPairedToast = 9,
  kMaxValue = kPairedToast
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ForgetResult {
  kFailure = 0,
  kSuccess = 1,
  kMaxValue = kSuccess,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DisconnectResult {
  kFailure = 0,
  kSuccess = 1,
  kMaxValue = kSuccess
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SetNicknameResult {
  kInvalidNicknameFormat = 0,
  kDeviceNotFound = 1,
  kPrefsUnavailable = 2,
  kSuccess = 3,
  kMaxValue = kSuccess,
};

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

// Return filtered devices based on the filter type and max number of devices.
DEVICE_BLUETOOTH_EXPORT device::BluetoothAdapter::DeviceList
FilterBluetoothDeviceList(const BluetoothAdapter::DeviceList& devices,
                          BluetoothFilterType filter_type,
                          int max_devices);

// Returns |true| if the device is unsupported and should not be known by the
// UI.
DEVICE_BLUETOOTH_EXPORT bool IsUnsupportedDevice(
    const device::BluetoothDevice* device);

// Record outcome of user attempting to pair to a device.
DEVICE_BLUETOOTH_EXPORT void RecordPairingResult(
    absl::optional<ConnectionFailureReason> failure_reason,
    BluetoothTransport transport,
    base::TimeDelta duration);

// Record outcome of user attempting to reconnect to a previously paired device.
DEVICE_BLUETOOTH_EXPORT void RecordUserInitiatedReconnectionAttemptResult(
    absl::optional<ConnectionFailureReason> failure_reason,
    UserInitiatedReconnectionUISurfaces surface);

// Record how long it took for a user to find and select the device they wished
// to connect to.
DEVICE_BLUETOOTH_EXPORT void RecordDeviceSelectionDuration(
    base::TimeDelta duration,
    DeviceSelectionUISurfaces surface,
    bool was_paired,
    BluetoothTransport transport);

// Record the result of device's Bluetooth being powered on or off.
DEVICE_BLUETOOTH_EXPORT void RecordPoweredStateOperationResult(
    PoweredStateOperation operation,
    bool success);

// Record each time the local device's Bluetooth is powered on or off.
DEVICE_BLUETOOTH_EXPORT void RecordPoweredState(bool is_powered);

// Record each time a device forget attempt completes.
DEVICE_BLUETOOTH_EXPORT void RecordForgetResult(ForgetResult forget_result);

// Records each bluetooth device disconnect.
DEVICE_BLUETOOTH_EXPORT void RecordDeviceDisconnect(
    BluetoothDeviceType device_type);

// Record the result of each user initiated bluetooth device disconnect attempt.
DEVICE_BLUETOOTH_EXPORT void RecordUserInitiatedDisconnectResult(
    DisconnectResult disconnect_result,
    BluetoothTransport transport);

// Record each time a bluetooth UI surface is displayed.
DEVICE_BLUETOOTH_EXPORT void RecordUiSurfaceDisplayed(
    BluetoothUiSurface ui_surface);

// Record how long it took for an attempted user initiated bluetooth device
// reconnection to occur.
DEVICE_BLUETOOTH_EXPORT void RecordUserInitiatedReconnectionAttemptDuration(
    absl::optional<ConnectionFailureReason> failure_reason,
    BluetoothTransport transport,
    base::TimeDelta duration);

// Record each time a Bluetooth device nickname change is attempted.
DEVICE_BLUETOOTH_EXPORT void RecordSetDeviceNickName(SetNicknameResult success);

}  // namespace device

#endif  // DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_UTILS_H_
