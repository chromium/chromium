// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_local_gatt_characteristic_floss.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/floss/bluetooth_adapter_floss.h"
#include "device/bluetooth/floss/bluetooth_gatt_service_floss.h"
#include "device/bluetooth/floss/bluetooth_local_gatt_service_floss.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"

namespace floss {

// static
base::WeakPtr<BluetoothLocalGattCharacteristicFloss>
BluetoothLocalGattCharacteristicFloss::Create(
    const device::BluetoothUUID& uuid,
    Properties properties,
    Permissions permissions,
    BluetoothLocalGattServiceFloss* service) {
  auto* characteristic = new BluetoothLocalGattCharacteristicFloss(
      uuid, properties, permissions, service);
  auto weak_ptr = characteristic->weak_ptr_factory_.GetWeakPtr();
  weak_ptr->index_ =
      service->AddCharacteristic(base::WrapUnique(characteristic));
  return weak_ptr;
}

BluetoothLocalGattCharacteristicFloss::BluetoothLocalGattCharacteristicFloss(
    const device::BluetoothUUID& uuid,
    Properties properties,
    Permissions permissions,
    BluetoothLocalGattServiceFloss* service)
    : uuid_(uuid),
      properties_(properties),
      permissions_(permissions),
      service_(raw_ref<BluetoothLocalGattServiceFloss>::from_ptr(service)),
      client_instance_id_(service_->NewInstanceId()) {}

BluetoothLocalGattCharacteristicFloss::
    ~BluetoothLocalGattCharacteristicFloss() {
  service_->RemoveServerObserverForHandle(floss_instance_id_);
}

std::string BluetoothLocalGattCharacteristicFloss::GetIdentifier() const {
  return base::StringPrintf("%s-%s/%04x",
                            service_->GetAdapter()->GetAddress().c_str(),
                            GetUUID().value().c_str(), client_instance_id_);
}

device::BluetoothUUID BluetoothLocalGattCharacteristicFloss::GetUUID() const {
  return uuid_;
}

device::BluetoothGattCharacteristic::Properties
BluetoothLocalGattCharacteristicFloss::GetProperties() const {
  return properties_;
}

device::BluetoothGattCharacteristic::Permissions
BluetoothLocalGattCharacteristicFloss::GetPermissions() const {
  return permissions_;
}

device::BluetoothLocalGattCharacteristic::NotificationStatus
BluetoothLocalGattCharacteristicFloss::NotifyValueChanged(
    const device::BluetoothDevice* device,
    const std::vector<uint8_t>& new_value,
    bool indicate) {
  if (indicate && !(properties_ & PROPERTY_INDICATE)) {
    return INDICATE_PROPERTY_NOT_SET;
  }
  if (!indicate && !(properties_ & PROPERTY_NOTIFY)) {
    return NOTIFY_PROPERTY_NOT_SET;
  }
  return service_->GetAdapter()->SendValueChanged(this, new_value)
             ? NOTIFICATION_SUCCESS
             : SERVICE_NOT_REGISTERED;
}

device::BluetoothLocalGattService*
BluetoothLocalGattCharacteristicFloss::GetService() const {
  return &*service_;
}

GattCharacteristic
BluetoothLocalGattCharacteristicFloss::ToGattCharacteristic() {
  GattCharacteristic characteristic;
  characteristic.uuid = uuid_;
  characteristic.properties = properties_;
  characteristic.permissions = permissions_;
  for (auto& descriptor : descriptors_) {
    characteristic.descriptors.push_back(descriptor->ToGattDescriptor());
  }
  return characteristic;
}

void BluetoothLocalGattCharacteristicFloss::ResolveInstanceId(
    const GattService& service) {
  DCHECK(service.characteristics[index_].uuid == GetUUID());
  floss_instance_id_ = service.characteristics[index_].instance_id;
  service_->AddServerObserverForHandle(floss_instance_id_, this);
}

void BluetoothLocalGattCharacteristicFloss::GattServerCharacteristicReadRequest(
    std::string address,
    int32_t request_id,
    int32_t offset,
    bool is_long,
    int32_t handle) {
  NOTIMPLEMENTED();
}

void BluetoothLocalGattCharacteristicFloss::
    GattServerCharacteristicWriteRequest(std::string address,
                                         int32_t request_id,
                                         int32_t offset,
                                         int32_t length,
                                         bool is_prepared_write,
                                         bool needs_response,
                                         int32_t handle,
                                         std::vector<uint8_t> value) {
  NOTIMPLEMENTED();
}

int32_t BluetoothLocalGattCharacteristicFloss::AddDescriptor(
    std::unique_ptr<BluetoothLocalGattDescriptorFloss> descriptor) {
  descriptors_.push_back(std::move(descriptor));
  return descriptors_.size() - 1;
}

const std::vector<std::unique_ptr<BluetoothLocalGattDescriptorFloss>>&
BluetoothLocalGattCharacteristicFloss::GetDescriptors() const {
  return descriptors_;
}

}  // namespace floss
