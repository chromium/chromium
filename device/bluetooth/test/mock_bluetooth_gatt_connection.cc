// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"

#include "device/bluetooth/bluetooth_adapter.h"

using ::testing::Return;

namespace device {

MockBluetoothGattConnection::MockBluetoothGattConnection(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address)
    : BluetoothGattConnection(adapter, device_address) {
  ON_CALL(*this, GetDeviceAddress()).WillByDefault(Return(device_address));
  ON_CALL(*this, IsConnected()).WillByDefault(Return(true));
}

MockBluetoothGattConnection::~MockBluetoothGattConnection() = default;

}  // namespace device
