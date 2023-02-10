// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_local_gatt_descriptor_floss.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"

namespace floss {

// static
base::WeakPtr<BluetoothLocalGattDescriptorFloss>
BluetoothLocalGattDescriptorFloss::Create(
    const device::BluetoothUUID& uuid,
    device::BluetoothGattCharacteristic::Permissions permissions,
    BluetoothLocalGattCharacteristicFloss* characteristic) {
  auto* descriptor =
      new BluetoothLocalGattDescriptorFloss(uuid, permissions, characteristic);
  auto weak_ptr = descriptor->weak_ptr_factory_.GetWeakPtr();
  characteristic->AddDescriptor(base::WrapUnique(descriptor));
  return weak_ptr;
}

BluetoothLocalGattDescriptorFloss::BluetoothLocalGattDescriptorFloss(
    const device::BluetoothUUID& uuid,
    device::BluetoothGattCharacteristic::Permissions permissions,
    BluetoothLocalGattCharacteristicFloss* characteristic)
    : characteristic_(raw_ref<BluetoothLocalGattCharacteristicFloss>::from_ptr(
          characteristic)) {
  DCHECK(characteristic);

  descriptor_.uuid = uuid;
  descriptor_.permissions = permissions;
  // TODO: Redesign after the GATT server registration wiring is finished.
  // Temporarily use a random number to prefill the instance_id, as the
  // application may want to access the object before GATT service registration
  // when an instance_id is provided by the daemon through DBUS callback.
  descriptor_.instance_id = static_cast<int32_t>(base::RandUint64());
}

BluetoothLocalGattDescriptorFloss::~BluetoothLocalGattDescriptorFloss() =
    default;

std::string BluetoothLocalGattDescriptorFloss::GetIdentifier() const {
  return base::StringPrintf("%s/%d", characteristic_->GetIdentifier().c_str(),
                            descriptor_.instance_id);
}

device::BluetoothUUID BluetoothLocalGattDescriptorFloss::GetUUID() const {
  return descriptor_.uuid;
}

device::BluetoothGattCharacteristic::Permissions
BluetoothLocalGattDescriptorFloss::GetPermissions() const {
  return descriptor_.permissions;
}

device::BluetoothLocalGattCharacteristic*
BluetoothLocalGattDescriptorFloss::GetCharacteristic() const {
  return &*characteristic_;
}

}  // namespace floss
