// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_local_gatt_descriptor_floss.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"

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
  weak_ptr->index_ =
      characteristic->AddDescriptor(base::WrapUnique(descriptor));
  return weak_ptr;
}

BluetoothLocalGattDescriptorFloss::BluetoothLocalGattDescriptorFloss(
    const device::BluetoothUUID& uuid,
    device::BluetoothGattCharacteristic::Permissions permissions,
    BluetoothLocalGattCharacteristicFloss* characteristic)
    : uuid_(uuid),
      permissions_(permissions),
      characteristic_(raw_ref<BluetoothLocalGattCharacteristicFloss>::from_ptr(
          characteristic)),
      client_instance_id_(characteristic_->service_->NewInstanceId()) {}

BluetoothLocalGattDescriptorFloss::~BluetoothLocalGattDescriptorFloss() {
  characteristic_->service_->RemoveServerObserverForHandle(floss_instance_id_);
}

std::string BluetoothLocalGattDescriptorFloss::GetIdentifier() const {
  return base::StringPrintf(
      "%s-%s/%04x",
      characteristic_->service_->GetAdapter()->GetAddress().c_str(),
      GetUUID().value().c_str(), client_instance_id_);
}

device::BluetoothUUID BluetoothLocalGattDescriptorFloss::GetUUID() const {
  return uuid_;
}

device::BluetoothGattCharacteristic::Permissions
BluetoothLocalGattDescriptorFloss::GetPermissions() const {
  return permissions_;
}

device::BluetoothLocalGattCharacteristic*
BluetoothLocalGattDescriptorFloss::GetCharacteristic() const {
  return &*characteristic_;
}

GattDescriptor BluetoothLocalGattDescriptorFloss::ToGattDescriptor() {
  GattDescriptor descriptor;
  descriptor.uuid = uuid_;
  descriptor.instance_id = floss_instance_id_;
  descriptor.permissions = permissions_;
  return descriptor;
}

void BluetoothLocalGattDescriptorFloss::ResolveInstanceId(
    const GattCharacteristic& characteristic) {
  DCHECK(characteristic.descriptors[index_].uuid == GetUUID());
  floss_instance_id_ = characteristic.descriptors[index_].instance_id;
  characteristic_->service_->AddServerObserverForHandle(floss_instance_id_,
                                                        this);
}

void BluetoothLocalGattDescriptorFloss::GattServerDescriptorReadRequest(
    std::string address,
    int32_t request_id,
    int32_t offset,
    bool is_long,
    int32_t handle) {
  NOTIMPLEMENTED();
}

void BluetoothLocalGattDescriptorFloss::GattServerDescriptorWriteRequest(
    std::string address,
    int32_t request_id,
    int32_t offset,
    int32_t length,
    bool is_prepared_write,
    bool needs_response,
    int32_t handle,
    std::vector<uint8_t> value) {
  NOTIMPLEMENTED();
}

}  // namespace floss
