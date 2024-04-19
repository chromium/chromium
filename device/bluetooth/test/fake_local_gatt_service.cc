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

FakeLocalGattCharacteristic* FakeLocalGattService::AddFakeCharacteristic(
    const std::string& characteristic_id,
    const device::BluetoothUUID& characteristic_uuid,
    device::BluetoothGattCharacteristic::Properties properties,
    device::BluetoothGattCharacteristic::Permissions permissions) {
  CHECK(!base::Contains(uuid_to_fake_characteristic_map_, characteristic_uuid));
  auto fake_characteristic = std::make_unique<FakeLocalGattCharacteristic>(
      /*characteristic_id=*/characteristic_id,
      /*characteristic_uuid=*/characteristic_uuid, /*service=*/this, properties,
      permissions);
  auto* fake_characteristic_ptr = fake_characteristic.get();
  uuid_to_fake_characteristic_map_.insert_or_assign(
      characteristic_uuid, std::move(fake_characteristic));
  return fake_characteristic_ptr;
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
  if (set_should_registration_succeed_) {
    std::move(callback).Run();
    return;
  }

  std::move(error_callback).Run(BluetoothGattService::GattErrorCode::kFailed);
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
  deleted_ = true;

  if (on_deleted_callback_) {
    std::move(on_deleted_callback_).Run();
  }
}

device::BluetoothLocalGattCharacteristic*
FakeLocalGattService::GetCharacteristic(const std::string& identifier) {
  auto it =
      uuid_to_fake_characteristic_map_.find(device::BluetoothUUID(identifier));
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
  if (!should_create_local_gatt_characteristic_succeed_) {
    return nullptr;
  }

  auto* fake_characteristic = AddFakeCharacteristic(
      /*characteristic_id=*/uuid.value(), /*characteristic_uuid=*/uuid,
      properties, permissions);
  return fake_characteristic->GetWeakPtr();
}

}  // namespace bluetooth
