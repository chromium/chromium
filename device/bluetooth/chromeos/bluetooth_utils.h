// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_UTILS_H_
#define DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_UTILS_H_

#include <vector>

#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"

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

// Return filtered devices based on the filter type and max number of devices.
device::BluetoothAdapter::DeviceList DEVICE_BLUETOOTH_EXPORT
FilterBluetoothDeviceList(const BluetoothAdapter::DeviceList& devices,
                          BluetoothFilterType filter_type,
                          int max_devices);

std::vector<std::vector<uint8_t>> DEVICE_BLUETOOTH_EXPORT
GetBlockedLongTermKeys();

// Record how long it took for a user to find and select the device they wished
// to connect to.
void DEVICE_BLUETOOTH_EXPORT
RecordDeviceSelectionDuration(base::TimeDelta duration,
                              BluetoothUiSurface surface,
                              bool was_paired,
                              BluetoothTransport transport);

}  // namespace device

#endif  // DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_UTILS_H_
