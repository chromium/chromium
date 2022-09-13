// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_gatt_descriptor.h"

#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"

using testing::Return;
using testing::ReturnRefOfCopy;

namespace device {

MockBluetoothGattDescriptor::MockBluetoothGattDescriptor(
    MockBluetoothGattCharacteristic* characteristic,
    const std::string& identifier,
    const BluetoothUUID& uuid,
    BluetoothRemoteGattCharacteristic::Permissions permissions) {
  ON_CALL(*this, GetIdentifier()).WillByDefault(Return(identifier));
  ON_CALL(*this, GetUUID()).WillByDefault(Return(uuid));
  ON_CALL(*this, GetValue())
      .WillByDefault(ReturnRefOfCopy(std::vector<uint8_t>()));
  ON_CALL(*this, GetCharacteristic()).WillByDefault(Return(characteristic));
  ON_CALL(*this, GetPermissions()).WillByDefault(Return(permissions));
}

MockBluetoothGattDescriptor::~MockBluetoothGattDescriptor() = default;

}  // namespace device
