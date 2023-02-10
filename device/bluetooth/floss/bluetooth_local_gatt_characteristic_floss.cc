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
  service->AddCharacteristic(base::WrapUnique(characteristic));
  return weak_ptr;
}

BluetoothLocalGattCharacteristicFloss::BluetoothLocalGattCharacteristicFloss(
    const device::BluetoothUUID& uuid,
    Properties properties,
    Permissions permissions,
    BluetoothLocalGattServiceFloss* service)
    : service_(raw_ref<BluetoothLocalGattServiceFloss>::from_ptr(service)) {
  characteristic_.uuid = uuid;
  characteristic_.properties = properties;
  characteristic_.permissions = permissions;
  // TODO: Redesign after the GATT server registration wiring is finished.
  // Temporarily use a random number to prefill the instance_id, as the
  // application may want to access the object before GATT service registration
  // when an instance_id is provided by the daemon through DBUS callback.
  characteristic_.instance_id = static_cast<int32_t>(base::RandUint64());
}

BluetoothLocalGattCharacteristicFloss::
    ~BluetoothLocalGattCharacteristicFloss() = default;

std::string BluetoothLocalGattCharacteristicFloss::GetIdentifier() const {
  return base::StringPrintf("%s/%d", service_->GetIdentifier().c_str(),
                            characteristic_.instance_id);
}

device::BluetoothUUID BluetoothLocalGattCharacteristicFloss::GetUUID() const {
  return characteristic_.uuid;
}

device::BluetoothGattCharacteristic::Properties
BluetoothLocalGattCharacteristicFloss::GetProperties() const {
  return characteristic_.properties;
}

device::BluetoothGattCharacteristic::Permissions
BluetoothLocalGattCharacteristicFloss::GetPermissions() const {
  return characteristic_.permissions;
}

device::BluetoothLocalGattCharacteristic::NotificationStatus
BluetoothLocalGattCharacteristicFloss::NotifyValueChanged(
    const device::BluetoothDevice* device,
    const std::vector<uint8_t>& new_value,
    bool indicate) {
  if (indicate && !(characteristic_.properties & PROPERTY_INDICATE)) {
    return INDICATE_PROPERTY_NOT_SET;
  }
  if (!indicate && !(characteristic_.properties & PROPERTY_NOTIFY)) {
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

void BluetoothLocalGattCharacteristicFloss::AddDescriptor(
    std::unique_ptr<BluetoothLocalGattDescriptorFloss> descriptor) {
  descriptors_.push_back(std::move(descriptor));
}

const std::vector<std::unique_ptr<BluetoothLocalGattDescriptorFloss>>&
BluetoothLocalGattCharacteristicFloss::GetDescriptors() const {
  return descriptors_;
}

}  // namespace floss
