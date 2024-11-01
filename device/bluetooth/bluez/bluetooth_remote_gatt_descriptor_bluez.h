// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_BLUEZ_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluez/bluetooth_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluez/bluetooth_remote_gatt_characteristic_bluez.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace bluez {

// The BluetoothGattDescriptorBlueZ class implements
// BluetoothRemoteGattDescriptor for remote GATT characteristic descriptors for
// platforms that use BlueZ.
class BluetoothRemoteGattDescriptorBlueZ
    : public BluetoothGattDescriptorBlueZ,
      public device::BluetoothRemoteGattDescriptor {
 public:
  BluetoothRemoteGattDescriptorBlueZ(
      const BluetoothRemoteGattDescriptorBlueZ&) = delete;
  BluetoothRemoteGattDescriptorBlueZ& operator=(
      const BluetoothRemoteGattDescriptorBlueZ&) = delete;

  // device::BluetoothRemoteGattDescriptor overrides.
  ~BluetoothRemoteGattDescriptorBlueZ() override;
  device::BluetoothUUID GetUUID() const override;
  const std::vector<uint8_t>& GetValue() const override;
  device::BluetoothRemoteGattCharacteristic* GetCharacteristic() const override;
  device::BluetoothRemoteGattCharacteristic::Permissions GetPermissions()
      const override;
  void ReadRemoteDescriptor(ValueCallback callback) override;
  void WriteRemoteDescriptor(const std::vector<uint8_t>& new_value,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override;

 private:
  friend class BluetoothRemoteGattCharacteristicBlueZ;

  BluetoothRemoteGattDescriptorBlueZ(
      BluetoothRemoteGattCharacteristicBlueZ* characteristic,
      const dbus::ObjectPath& object_path);

  // Called by dbus:: on unsuccessful completion of a request to read
  // the descriptor value.
  void OnReadError(ValueCallback callback,
                   const std::string& error_name,
                   const std::string& error_message);

  // Called by dbus:: on unsuccessful completion of a request to write
  // the descriptor value.
  void OnError(ErrorCallback error_callback,
               const std::string& error_name,
               const std::string& error_message);

  // The GATT characteristic this descriptor belongs to.
  raw_ptr<BluetoothRemoteGattCharacteristicBlueZ> characteristic_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothRemoteGattDescriptorBlueZ> weak_ptr_factory_{
      this};
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_REMOTE_GATT_DESCRIPTOR_BLUEZ_H_
