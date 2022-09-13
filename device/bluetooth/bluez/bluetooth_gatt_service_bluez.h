// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_SERVICE_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_SERVICE_BLUEZ_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_service.h"

namespace bluez {

class BluetoothAdapterBlueZ;
class BluetoothDeviceBlueZ;

// The BluetoothGattServiceBlueZ class implements BluetootGattService
// for GATT services on platforms that use BlueZ.
class BluetoothGattServiceBlueZ : public virtual device::BluetoothGattService {
 public:
  BluetoothGattServiceBlueZ(const BluetoothGattServiceBlueZ&) = delete;
  BluetoothGattServiceBlueZ& operator=(const BluetoothGattServiceBlueZ&) =
      delete;

  // device::BluetoothGattService overrides.
  std::string GetIdentifier() const override;

  // Object path of the underlying service.
  const dbus::ObjectPath& object_path() const { return object_path_; }

  // Parses a named D-Bus error into a service error code.
  static device::BluetoothGattService::GattErrorCode DBusErrorToServiceError(
      const std::string error_name);

  // Returns the adapter associated with this service.
  BluetoothAdapterBlueZ* GetAdapter() const;

 protected:
  BluetoothGattServiceBlueZ(BluetoothAdapterBlueZ* adapter,
                            dbus::ObjectPath object_path);
  ~BluetoothGattServiceBlueZ() override;

 private:
  friend class BluetoothDeviceBlueZ;

  // The adapter associated with this service. It's ok to store a raw pointer
  // here since |adapter_| indirectly owns this instance.
  raw_ptr<BluetoothAdapterBlueZ> adapter_;

  // Object path of the GATT service.
  dbus::ObjectPath object_path_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_SERVICE_BLUEZ_H_
