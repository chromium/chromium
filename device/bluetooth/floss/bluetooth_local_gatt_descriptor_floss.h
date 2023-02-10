// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_FLOSS_H_
#define DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_FLOSS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_characteristic_floss.h"
#include "device/bluetooth/floss/bluetooth_remote_gatt_descriptor_floss.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace floss {

class BluetoothLocalGattCharacteristicFloss;

// The BluetoothLocalGattDescriptorFloss class implements
// BluetoothRemoteGattDescriptor for remote and local GATT characteristic
// descriptors for platforms that use Floss.
class BluetoothLocalGattDescriptorFloss
    : public device::BluetoothLocalGattDescriptor {
 public:
  static base::WeakPtr<BluetoothLocalGattDescriptorFloss> Create(
      const device::BluetoothUUID& uuid,
      device::BluetoothGattCharacteristic::Permissions permissions,
      BluetoothLocalGattCharacteristicFloss* characteristic);

  BluetoothLocalGattDescriptorFloss(const BluetoothLocalGattDescriptorFloss&) =
      delete;
  BluetoothLocalGattDescriptorFloss& operator=(
      const BluetoothLocalGattDescriptorFloss&) = delete;

  ~BluetoothLocalGattDescriptorFloss() override;

  // device::BluetoothGattDescriptor overrides.
  std::string GetIdentifier() const override;

  // device::BluetoothLocalGattDescriptor overrides.
  device::BluetoothUUID GetUUID() const override;
  device::BluetoothGattCharacteristic::Permissions GetPermissions()
      const override;
  device::BluetoothLocalGattCharacteristic* GetCharacteristic() const override;

 private:
  BluetoothLocalGattDescriptorFloss(
      const device::BluetoothUUID& uuid,
      device::BluetoothGattCharacteristic::Permissions permissions,
      BluetoothLocalGattCharacteristicFloss* characteristic);

  // Descriptor represented by this class.
  GattDescriptor descriptor_;

  // Characteristic that contains this descriptor.
  raw_ref<BluetoothLocalGattCharacteristicFloss> characteristic_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothLocalGattDescriptorFloss> weak_ptr_factory_{
      this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_BLUETOOTH_LOCAL_GATT_DESCRIPTOR_FLOSS_H_
