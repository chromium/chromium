// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_CONNECTION_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_CONNECTION_BLUEZ_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"

namespace device {

class BluetoothAdapter;

}  // namespace device

namespace bluez {

// BluetoothGattConnectionBlueZ implements BluetoothGattConnection for
// platforms that use BlueZ.
class BluetoothGattConnectionBlueZ
    : public device::BluetoothGattConnection,
      public bluez::BluetoothDeviceClient::Observer {
 public:
  explicit BluetoothGattConnectionBlueZ(
      scoped_refptr<device::BluetoothAdapter> adapter,
      const std::string& device_address,
      const dbus::ObjectPath& object_path);

  BluetoothGattConnectionBlueZ(const BluetoothGattConnectionBlueZ&) = delete;
  BluetoothGattConnectionBlueZ& operator=(const BluetoothGattConnectionBlueZ&) =
      delete;

  ~BluetoothGattConnectionBlueZ() override;

  // BluetoothGattConnection overrides.
  bool IsConnected() override;
  void Disconnect() override;

 private:
  // bluez::Bluetooth$1Client::Observer overrides.
  void DeviceRemoved(const dbus::ObjectPath& object_path) override;
  void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                             const std::string& property_name) override;

  // True, if the connection is currently active.
  bool connected_;

  // D-Bus object path of the underlying device. This is used to filter observer
  // events.
  dbus::ObjectPath object_path_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_CONNECTION_BLUEZ_H_
