// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_remote_gatt_service.h"

#include <utility>

#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace device {

BluetoothRemoteGattService::BluetoothRemoteGattService() = default;

BluetoothRemoteGattService::~BluetoothRemoteGattService() = default;

std::vector<BluetoothRemoteGattCharacteristic*>
BluetoothRemoteGattService::GetCharacteristics() const {
  std::vector<BluetoothRemoteGattCharacteristic*> characteristics;
  characteristics.reserve(characteristics_.size());
  for (const auto& characteristic : characteristics_)
    characteristics.push_back(characteristic.second.get());
  return characteristics;
}

BluetoothRemoteGattCharacteristic*
BluetoothRemoteGattService::GetCharacteristic(
    const std::string& identifier) const {
  auto iter = characteristics_.find(identifier);
  return iter != characteristics_.end() ? iter->second.get() : nullptr;
}

std::vector<BluetoothRemoteGattCharacteristic*>
BluetoothRemoteGattService::GetCharacteristicsByUUID(
    const BluetoothUUID& characteristic_uuid) const {
  std::vector<BluetoothRemoteGattCharacteristic*> result;
  for (const auto& characteristic : characteristics_) {
    if (characteristic.second->GetUUID() == characteristic_uuid)
      result.push_back(characteristic.second.get());
  }

  return result;
}

bool BluetoothRemoteGattService::IsDiscoveryComplete() const {
  return discovery_complete_;
}

void BluetoothRemoteGattService::SetDiscoveryComplete(bool complete) {
  discovery_complete_ = complete;
}

bool BluetoothRemoteGattService::AddCharacteristic(
    std::unique_ptr<BluetoothRemoteGattCharacteristic> characteristic) {
  if (!characteristic)
    return false;

  const auto& characteristic_raw = *characteristic;
  return characteristics_
      .try_emplace(characteristic_raw.GetIdentifier(),
                   std::move(characteristic))
      .second;
}

}  // namespace device
