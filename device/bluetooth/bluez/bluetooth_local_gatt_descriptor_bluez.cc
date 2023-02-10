// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_local_gatt_descriptor_bluez.h"

#include <string>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"

namespace bluez {

// static
base::WeakPtr<BluetoothLocalGattDescriptorBlueZ>
BluetoothLocalGattDescriptorBlueZ::Create(
    const device::BluetoothUUID& uuid,
    device::BluetoothGattCharacteristic::Permissions permissions,
    BluetoothLocalGattCharacteristicBlueZ* characteristic) {
  auto* descriptor =
      new BluetoothLocalGattDescriptorBlueZ(uuid, permissions, characteristic);
  auto weak_ptr = descriptor->weak_ptr_factory_.GetWeakPtr();
  characteristic->AddDescriptor(base::WrapUnique(descriptor));
  return weak_ptr;
}

BluetoothLocalGattDescriptorBlueZ::BluetoothLocalGattDescriptorBlueZ(
    const device::BluetoothUUID& uuid,
    device::BluetoothGattCharacteristic::Permissions permissions,
    BluetoothLocalGattCharacteristicBlueZ* characteristic)
    : BluetoothGattDescriptorBlueZ(
          BluetoothLocalGattServiceBlueZ::AddGuidToObjectPath(
              characteristic->object_path().value() + "/descriptor")),
      uuid_(uuid),
      permissions_(permissions),
      characteristic_(characteristic) {
  DCHECK(characteristic->GetService());
  DVLOG(1) << "Creating local GATT descriptor with identifier: "
           << GetIdentifier();
}

BluetoothLocalGattDescriptorBlueZ::~BluetoothLocalGattDescriptorBlueZ() =
    default;

device::BluetoothUUID BluetoothLocalGattDescriptorBlueZ::GetUUID() const {
  return uuid_;
}

device::BluetoothGattCharacteristic::Permissions
BluetoothLocalGattDescriptorBlueZ::GetPermissions() const {
  return permissions_;
}

device::BluetoothLocalGattCharacteristic*
BluetoothLocalGattDescriptorBlueZ::GetCharacteristic() const {
  return characteristic_;
}

}  // namespace bluez
