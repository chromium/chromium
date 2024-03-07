// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_local_gatt_service.h"
#include "base/containers/contains.h"

namespace bluetooth {

FakeLocalGattService::FakeLocalGattService(
    const std::string& service_id,
    const device::BluetoothUUID& service_uuid,
    bool is_primary)
    : service_id_(service_id),
      service_uuid_(service_uuid),
      is_primary_(is_primary) {}

FakeLocalGattService::~FakeLocalGattService() = default;

void FakeLocalGattService::AddFakeCharacteristic(
    const std::string& characteristic_id,
    const device::BluetoothUUID& characteristic_uuid) {
  CHECK(!base::Contains(uuid_to_fake_characteristic_map_, characteristic_id));
  auto fake_characteristic = std::make_unique<FakeLocalGattCharacteristic>(
      /*characteristic_id=*/characteristic_id,
      /*characteristic_uuid=*/characteristic_uuid, /*service=*/this);
  uuid_to_fake_characteristic_map_.insert_or_assign(
      characteristic_id, std::move(fake_characteristic));
}

std::string FakeLocalGattService::GetIdentifier() const {
  return service_id_;
}

device::BluetoothUUID FakeLocalGattService::GetUUID() const {
  return service_uuid_;
}

bool FakeLocalGattService::IsPrimary() const {
  return is_primary_;
}

void FakeLocalGattService::Register(base::OnceClosure callback,
                                    ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

void FakeLocalGattService::Unregister(base::OnceClosure callback,
                                      ErrorCallback error_callback) {
  NOTIMPLEMENTED();
}

bool FakeLocalGattService::IsRegistered() {
  NOTIMPLEMENTED();
  return false;
}

void FakeLocalGattService::Delete() {
  NOTIMPLEMENTED();
}

device::BluetoothLocalGattCharacteristic*
FakeLocalGattService::GetCharacteristic(const std::string& identifier) {
  auto it = uuid_to_fake_characteristic_map_.find(identifier);
  if (it == uuid_to_fake_characteristic_map_.end()) {
    return nullptr;
  }
  return it->second.get();
}

base::WeakPtr<device::BluetoothLocalGattCharacteristic>
FakeLocalGattService::CreateCharacteristic(
    const device::BluetoothUUID& uuid,
    device::BluetoothGattCharacteristic::Properties properties,
    device::BluetoothGattCharacteristic::Permissions permissions) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace bluetooth
