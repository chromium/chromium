// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_local_gatt_characteristic.h"

#include "device/bluetooth/bluetooth_local_gatt_service.h"

namespace bluetooth {

FakeLocalGattCharacteristic::FakeLocalGattCharacteristic(
    const std::string& characteristic_id,
    const device::BluetoothUUID& characteristic_uuid,
    device::BluetoothLocalGattService* service,
    Properties properties,
    Permissions permissions)
    : properties_(properties),
      permissions_(permissions),
      characteristic_id_(characteristic_id),
      characteristic_uuid_(characteristic_uuid),
      service_(service) {}

FakeLocalGattCharacteristic::~FakeLocalGattCharacteristic() = default;

std::string FakeLocalGattCharacteristic::GetIdentifier() const {
  return characteristic_id_;
}

device::BluetoothUUID FakeLocalGattCharacteristic::GetUUID() const {
  return characteristic_uuid_;
}

FakeLocalGattCharacteristic::Properties
FakeLocalGattCharacteristic::GetProperties() const {
  return properties_;
}

FakeLocalGattCharacteristic::Permissions
FakeLocalGattCharacteristic::GetPermissions() const {
  return permissions_;
}

FakeLocalGattCharacteristic::NotificationStatus
FakeLocalGattCharacteristic::NotifyValueChanged(
    const device::BluetoothDevice* device,
    const std::vector<uint8_t>& new_value,
    bool indicate) {
  NOTIMPLEMENTED();
  return NotificationStatus::SERVICE_NOT_REGISTERED;
}

device::BluetoothLocalGattService* FakeLocalGattCharacteristic::GetService()
    const {
  return service_.get();
}

std::vector<device::BluetoothLocalGattDescriptor*>
FakeLocalGattCharacteristic::GetDescriptors() const {
  return {};
}

}  // namespace bluetooth
