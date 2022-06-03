// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_local_gatt_descriptor_bluez.h"

#include <string>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"

namespace device {

// static
base::WeakPtr<BluetoothLocalGattDescriptor>
BluetoothLocalGattDescriptor::Create(
    const BluetoothUUID& uuid,
    BluetoothGattCharacteristic::Permissions permissions,
    BluetoothLocalGattCharacteristic* characteristic) {
  DCHECK(characteristic);
  bluez::BluetoothLocalGattCharacteristicBlueZ* characteristic_bluez =
      static_cast<bluez::BluetoothLocalGattCharacteristicBlueZ*>(
          characteristic);
  bluez::BluetoothLocalGattDescriptorBlueZ* descriptor =
      new bluez::BluetoothLocalGattDescriptorBlueZ(uuid, permissions,
                                                   characteristic_bluez);
  return descriptor->weak_ptr_factory_.GetWeakPtr();
}

}  // namespace device

namespace bluez {

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
  characteristic->AddDescriptor(base::WrapUnique(this));
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
