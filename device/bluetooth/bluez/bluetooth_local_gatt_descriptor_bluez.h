// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_BLUEZ_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/bluez/bluetooth_gatt_descriptor_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace bluez {

class BluetoothLocalGattCharacteristicBlueZ;

// The BluetoothLocalGattDescriptorBlueZ class implements
// BluetoothRemoteGattDescriptor for remote and local GATT characteristic
// descriptors for platforms that use BlueZ.
class BluetoothLocalGattDescriptorBlueZ
    : public BluetoothGattDescriptorBlueZ,
      public device::BluetoothLocalGattDescriptor {
 public:
  static base::WeakPtr<BluetoothLocalGattDescriptorBlueZ> Create(
      const device::BluetoothUUID& uuid,
      device::BluetoothGattCharacteristic::Permissions permissions,
      BluetoothLocalGattCharacteristicBlueZ* characteristic);

  BluetoothLocalGattDescriptorBlueZ(const BluetoothLocalGattDescriptorBlueZ&) =
      delete;
  BluetoothLocalGattDescriptorBlueZ& operator=(
      const BluetoothLocalGattDescriptorBlueZ&) = delete;

  ~BluetoothLocalGattDescriptorBlueZ() override;

  // device::BluetoothLocalGattDescriptor overrides.
  device::BluetoothUUID GetUUID() const override;
  device::BluetoothGattCharacteristic::Permissions GetPermissions()
      const override;
  device::BluetoothLocalGattCharacteristic* GetCharacteristic() const override;

 private:
  BluetoothLocalGattDescriptorBlueZ(
      const device::BluetoothUUID& uuid,
      device::BluetoothGattCharacteristic::Permissions permissions,
      BluetoothLocalGattCharacteristicBlueZ* characteristic);

  // UUID of this descriptor.
  device::BluetoothUUID uuid_;

  // Permissions of this descriptor.
  device::BluetoothGattCharacteristic::Permissions permissions_;

  // Characteristic that contains this descriptor.
  raw_ptr<BluetoothLocalGattCharacteristicBlueZ> characteristic_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattDescriptorBlueZ> weak_ptr_factory_{
      this};
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_BLUEZ_H_
