// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_APPLICATION_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_APPLICATION_SERVICE_PROVIDER_H_

#include <map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_application_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_service_provider.h"

namespace bluez {

class BluetoothLocalGattServiceBlueZ;

// FakeBluetoothGattApplicationServiceProvider simulates behavior of a local
// GATT service object and is used both in test cases in place of a mock and on
// the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothGattApplicationServiceProvider
    : public BluetoothGattApplicationServiceProvider {
 public:
  FakeBluetoothGattApplicationServiceProvider(
      const dbus::ObjectPath& object_path,
      const std::map<dbus::ObjectPath,
                     raw_ptr<BluetoothLocalGattServiceBlueZ, CtnExperimental>>&
          services);

  FakeBluetoothGattApplicationServiceProvider(
      const FakeBluetoothGattApplicationServiceProvider&) = delete;
  FakeBluetoothGattApplicationServiceProvider& operator=(
      const FakeBluetoothGattApplicationServiceProvider&) = delete;

  ~FakeBluetoothGattApplicationServiceProvider() override;

  const dbus::ObjectPath& object_path() const { return object_path_; }

 private:
  // D-Bus object path of the fake GATT service.
  dbus::ObjectPath object_path_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_APPLICATION_SERVICE_PROVIDER_H_
