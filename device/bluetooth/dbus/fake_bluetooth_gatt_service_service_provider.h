// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_SERVICE_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_SERVICE_SERVICE_PROVIDER_H_

#include <string>
#include <vector>

#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_service_provider.h"

namespace bluez {

// FakeBluetoothGattServiceServiceProvider simulates behavior of a local GATT
// service object and is used both in test cases in place of a mock and on the
// Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothGattServiceServiceProvider
    : public BluetoothGattServiceServiceProvider {
 public:
  FakeBluetoothGattServiceServiceProvider(
      const dbus::ObjectPath& object_path,
      const std::string& uuid,
      const std::vector<dbus::ObjectPath>& includes);

  FakeBluetoothGattServiceServiceProvider(
      const FakeBluetoothGattServiceServiceProvider&) = delete;
  FakeBluetoothGattServiceServiceProvider& operator=(
      const FakeBluetoothGattServiceServiceProvider&) = delete;

  ~FakeBluetoothGattServiceServiceProvider() override;

  const dbus::ObjectPath& object_path() const override;
  const std::string& uuid() const { return uuid_; }

 private:
  // D-Bus object path of the fake GATT service.
  dbus::ObjectPath object_path_;

  // 128-bit GATT service UUID.
  std::string uuid_;

  // List of included GATT services.
  std::vector<dbus::ObjectPath> includes_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_SERVICE_SERVICE_PROVIDER_H_
