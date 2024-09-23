// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_local_gatt_characteristic_bluez.h"

#include <string>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_gatt_service_bluez.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"

namespace bluez {

// static
base::WeakPtr<BluetoothLocalGattCharacteristicBlueZ>
BluetoothLocalGattCharacteristicBlueZ::Create(
    const device::BluetoothUUID& uuid,
    Properties properties,
    Permissions permissions,
    BluetoothLocalGattServiceBlueZ* service) {
  auto* characteristic = new BluetoothLocalGattCharacteristicBlueZ(
      uuid, properties, permissions, service);
  auto weak_ptr = characteristic->weak_ptr_factory_.GetWeakPtr();
  service->AddCharacteristic(base::WrapUnique(characteristic));
  return weak_ptr;
}

BluetoothLocalGattCharacteristicBlueZ::BluetoothLocalGattCharacteristicBlueZ(
    const device::BluetoothUUID& uuid,
    Properties properties,
    Permissions permissions,
    BluetoothLocalGattServiceBlueZ* service)
    : BluetoothGattCharacteristicBlueZ(
          BluetoothLocalGattServiceBlueZ::AddGuidToObjectPath(
              service->object_path().value() + "/characteristic")),
      uuid_(uuid),
      properties_(properties),
      permissions_(permissions),
      service_(service) {
  DVLOG(1) << "Creating local GATT characteristic with identifier: "
           << GetIdentifier();
}

BluetoothLocalGattCharacteristicBlueZ::
    ~BluetoothLocalGattCharacteristicBlueZ() = default;

device::BluetoothUUID BluetoothLocalGattCharacteristicBlueZ::GetUUID() const {
  return uuid_;
}

device::BluetoothGattCharacteristic::Properties
BluetoothLocalGattCharacteristicBlueZ::GetProperties() const {
  return properties_;
}

device::BluetoothGattCharacteristic::Permissions
BluetoothLocalGattCharacteristicBlueZ::GetPermissions() const {
  return permissions_;
}

device::BluetoothLocalGattCharacteristic::NotificationStatus
BluetoothLocalGattCharacteristicBlueZ::NotifyValueChanged(
    const device::BluetoothDevice* device,
    const std::vector<uint8_t>& new_value,
    bool indicate) {
  if (indicate && !(properties_ & PROPERTY_INDICATE)) {
    return INDICATE_PROPERTY_NOT_SET;
  }
  if (!indicate && !(properties_ & PROPERTY_NOTIFY)) {
    return NOTIFY_PROPERTY_NOT_SET;
  }
  DCHECK(service_);
  return service_->GetAdapter()->SendValueChanged(this, new_value)
             ? NOTIFICATION_SUCCESS
             : SERVICE_NOT_REGISTERED;
}

device::BluetoothLocalGattService*
BluetoothLocalGattCharacteristicBlueZ::GetService() const {
  return service_;
}

void BluetoothLocalGattCharacteristicBlueZ::AddDescriptor(
    std::unique_ptr<BluetoothLocalGattDescriptorBlueZ> descriptor) {
  descriptors_.push_back(std::move(descriptor));
}

std::vector<device::BluetoothLocalGattDescriptor*>
BluetoothLocalGattCharacteristicBlueZ::GetDescriptors() const {
  std::vector<device::BluetoothLocalGattDescriptor*> descriptors;
  descriptors.reserve(descriptors_.size());
  for (const auto& d : descriptors_) {
    descriptors.push_back(d.get());
  }
  return descriptors;
}

}  // namespace bluez
