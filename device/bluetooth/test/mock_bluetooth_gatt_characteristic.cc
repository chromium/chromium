// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"

#include <utility>

#include "device/bluetooth/test/mock_bluetooth_gatt_descriptor.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"

using testing::Invoke;
using testing::Return;
using testing::ReturnRefOfCopy;
using testing::_;

namespace device {

MockBluetoothGattCharacteristic::MockBluetoothGattCharacteristic(
    MockBluetoothGattService* service,
    const std::string& identifier,
    const BluetoothUUID& uuid,
    Properties properties,
    Permissions permissions) {
  ON_CALL(*this, GetIdentifier()).WillByDefault(Return(identifier));
  ON_CALL(*this, GetUUID()).WillByDefault(Return(uuid));
  ON_CALL(*this, GetValue())
      .WillByDefault(ReturnRefOfCopy(std::vector<uint8_t>()));
  ON_CALL(*this, GetService()).WillByDefault(Return(service));
  ON_CALL(*this, GetProperties()).WillByDefault(Return(properties));
  ON_CALL(*this, GetPermissions()).WillByDefault(Return(permissions));
  ON_CALL(*this, IsNotifying()).WillByDefault(Return(false));
  ON_CALL(*this, GetDescriptors()).WillByDefault(Invoke([this] {
    return BluetoothRemoteGattCharacteristic::GetDescriptors();
  }));
  ON_CALL(*this, GetDescriptor(_))
      .WillByDefault(Invoke([this](const std::string& id) {
        return BluetoothRemoteGattCharacteristic::GetDescriptor(id);
      }));
}

MockBluetoothGattCharacteristic::~MockBluetoothGattCharacteristic() = default;

void MockBluetoothGattCharacteristic::AddMockDescriptor(
    std::unique_ptr<MockBluetoothGattDescriptor> mock_descriptor) {
  AddDescriptor(std::move(mock_descriptor));
}

}  // namespace device
