// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_CONNECTION_LOGGER_H_
#define DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_CONNECTION_LOGGER_H_

#include <optional>

#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

// Records metrics which track successful Bluetooth connections
class DEVICE_BLUETOOTH_EXPORT BluetoothConnectionLogger {
 public:
  // Records a successful connection by a device with ID |device_identifier|
  // and type |device_type|.
  static void RecordDeviceConnected(const std::string& device_identifier,
                                    BluetoothDeviceType device_type);

  // Resets logging for this session.
  static void Shutdown();

 private:
  BluetoothConnectionLogger();
  ~BluetoothConnectionLogger();

  // Helper function to persist in logs each succesful bluetooth device
  // connection, also records a new unique connection per device per session.
  void RecordDeviceConnectedMetric(const std::string& device_identifier,
                                   BluetoothDeviceType device_type);

  base::flat_set<std::string> device_ids_logged_this_session_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_CHROMEOS_BLUETOOTH_CONNECTION_LOGGER_H_