// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/emulation/fake_remote_gatt_service.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/emulation/fake_remote_gatt_characteristic.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/public/mojom/emulation/fake_bluetooth.mojom.h"

namespace bluetooth {

FakeRemoteGattService::FakeRemoteGattService(
    const std::string& service_id,
    const device::BluetoothUUID& service_uuid,
    bool is_primary,
    device::BluetoothDevice* device)
    : service_id_(service_id),
      service_uuid_(service_uuid),
      is_primary_(is_primary),
      device_(device) {}

FakeRemoteGattService::~FakeRemoteGattService() = default;

bool FakeRemoteGattService::AllResponsesConsumed() {
  return base::ranges::all_of(characteristics_, [](const auto& e) {
    return static_cast<FakeRemoteGattCharacteristic*>(e.second.get())
        ->AllResponsesConsumed();
  });
}

std::string FakeRemoteGattService::AddFakeCharacteristic(
    const device::BluetoothUUID& characteristic_uuid,
    mojom::CharacteristicPropertiesPtr properties) {
  // Attribute instance Ids need to be unique.
  std::string new_characteristic_id = base::StringPrintf(
      "%s_%zu", GetIdentifier().c_str(), ++last_characteristic_id_);

  auto [it, inserted] = characteristics_.emplace(
      new_characteristic_id, std::make_unique<FakeRemoteGattCharacteristic>(
                                 new_characteristic_id, characteristic_uuid,
                                 std::move(properties), this));

  DCHECK(inserted);
  return it->second->GetIdentifier();
}

bool FakeRemoteGattService::RemoveFakeCharacteristic(
    const std::string& identifier) {
  return characteristics_.erase(identifier) != 0u;
}

std::string FakeRemoteGattService::GetIdentifier() const {
  return service_id_;
}

device::BluetoothUUID FakeRemoteGattService::GetUUID() const {
  return service_uuid_;
}

bool FakeRemoteGattService::IsPrimary() const {
  return is_primary_;
}

device::BluetoothDevice* FakeRemoteGattService::GetDevice() const {
  return device_;
}

std::vector<device::BluetoothRemoteGattService*>
FakeRemoteGattService::GetIncludedServices() const {
  NOTREACHED();
}

}  // namespace bluetooth
