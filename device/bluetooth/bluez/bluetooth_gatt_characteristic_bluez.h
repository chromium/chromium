// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_CHARACTERISTIC_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_CHARACTERISTIC_BLUEZ_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"

namespace bluez {

// The BluetoothGattCharacteristicBlueZ class implements
// BluetoothRemoteGattCharacteristic for GATT characteristics for platforms
// that use BlueZ.
class BluetoothGattCharacteristicBlueZ
    : public virtual device::BluetoothGattCharacteristic {
 public:
  // device::BluetoothGattCharacteristic overrides.
  std::string GetIdentifier() const override;

  // Object path of the underlying D-Bus characteristic.
  const dbus::ObjectPath& object_path() const { return object_path_; }

 protected:
  explicit BluetoothGattCharacteristicBlueZ(dbus::ObjectPath object_path);
  ~BluetoothGattCharacteristicBlueZ() override;

 private:
  // Object path of the D-Bus characteristic object.
  dbus::ObjectPath object_path_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothGattCharacteristicBlueZ> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattCharacteristicBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_CHARACTERISTIC_BLUEZ_H_
