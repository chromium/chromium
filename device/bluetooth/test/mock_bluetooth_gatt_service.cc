// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"

#include <memory>
#include <utility>

#include "device/bluetooth/test/mock_bluetooth_device.h"

using testing::Return;
using testing::Invoke;
using testing::_;

namespace device {

MockBluetoothGattService::MockBluetoothGattService(
    MockBluetoothDevice* device,
    const std::string& identifier,
    const BluetoothUUID& uuid,
    bool is_primary,
    bool is_local) {
  ON_CALL(*this, GetIdentifier()).WillByDefault(Return(identifier));
  ON_CALL(*this, GetUUID()).WillByDefault(Return(uuid));
  ON_CALL(*this, IsLocal()).WillByDefault(Return(is_local));
  ON_CALL(*this, IsPrimary()).WillByDefault(Return(is_primary));
  ON_CALL(*this, GetDevice()).WillByDefault(Return(device));
  ON_CALL(*this, GetCharacteristics()).WillByDefault(Invoke([this] {
    return BluetoothRemoteGattService::GetCharacteristics();
  }));
  ON_CALL(*this, GetIncludedServices())
      .WillByDefault(Return(std::vector<BluetoothRemoteGattService*>()));
  ON_CALL(*this, GetCharacteristic(_))
      .WillByDefault(Invoke([this](const std::string& id) {
        return BluetoothRemoteGattService::GetCharacteristic(id);
      }));
  ON_CALL(*this, GetCharacteristicsByUUID(_))
      .WillByDefault(Invoke([this](const BluetoothUUID& uuid) {
        return BluetoothRemoteGattService::GetCharacteristicsByUUID(uuid);
      }));
}

MockBluetoothGattService::~MockBluetoothGattService() = default;

void MockBluetoothGattService::AddMockCharacteristic(
    std::unique_ptr<MockBluetoothGattCharacteristic> mock_characteristic) {
  AddCharacteristic(std::move(mock_characteristic));
}

}  // namespace device
