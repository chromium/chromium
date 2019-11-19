// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_DESCRIPTOR_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_DESCRIPTOR_BLUEZ_H_

#include <string>

#include "base/macros.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_descriptor.h"

namespace bluez {

// The BluetoothGattDescriptorBlueZ class implements BluetoothGattDescriptor for
// GATT characteristic descriptors for platforms that use BlueZ.
class BluetoothGattDescriptorBlueZ
    : public virtual device::BluetoothGattDescriptor {
 public:
  // device::BluetoothGattDescriptor overrides.
  std::string GetIdentifier() const override;

  // Object path of the underlying D-Bus characteristic.
  const dbus::ObjectPath& object_path() const { return object_path_; }

 protected:
  explicit BluetoothGattDescriptorBlueZ(const dbus::ObjectPath& object_path);
  ~BluetoothGattDescriptorBlueZ() override;

 private:
  // Called by dbus:: on unsuccessful completion of a request to read or write
  // the descriptor value.
  void OnError(ErrorCallback error_callback,
               const std::string& error_name,
               const std::string& error_message);

  // Object path of the D-Bus descriptor object.
  dbus::ObjectPath object_path_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattDescriptorBlueZ);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_GATT_DESCRIPTOR_BLUEZ_H_
