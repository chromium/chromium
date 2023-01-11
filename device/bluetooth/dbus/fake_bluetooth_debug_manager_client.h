// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_DEBUG_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_DEBUG_MANAGER_CLIENT_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_debug_manager_client.h"

namespace bluez {

// FakeBluetoothDebugManagerClient simulates the behavior of the Bluetooth
// Daemon's debug manager object and is used both in test cases in place of a
// mock and on the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothDebugManagerClient
    : public BluetoothDebugManagerClient {
 public:
  FakeBluetoothDebugManagerClient();
  ~FakeBluetoothDebugManagerClient() override;

  // BluetoothDebugManagerClient overrides
  void Init(dbus::Bus* bus, const std::string& bluetooth_service_name) override;
  void SetLogLevels(const uint8_t bluez_level,
                    const uint8_t kernel_level,
                    base::OnceClosure callback,
                    ErrorCallback error_callback) override;

  void SetDevCoredump(const bool enable,
                      base::OnceClosure callback,
                      ErrorCallback error_callback) override {}

  void SetLLPrivacy(const bool enable,
                    base::OnceClosure callback,
                    ErrorCallback error_callback) override {}
  void SetBluetoothQualityReport(const bool enable,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) override {}

  // Make the next call to SetLogLevels() to fail only once.
  void MakeNextSetLogLevelsFail();

  int set_log_levels_fail_count() const { return set_log_levels_fail_count_; }
  int bluez_level() const { return bluez_level_; }

 private:
  // When set, next call to SetLogLevels() will fail.
  bool should_next_set_log_levels_fail_ = false;

  // Counter to track how many times SetLogLevels() fails.
  int set_log_levels_fail_count_ = 0;

  // The latest bluez_level assigned.
  int bluez_level_ = 0;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_DEBUG_MANAGER_CLIENT_H_
