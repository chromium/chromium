// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_CONNECTION_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_CONNECTION_H_

#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class BluetoothAdapter;

class MockBluetoothGattConnection : public BluetoothGattConnection {
 public:
  MockBluetoothGattConnection(scoped_refptr<device::BluetoothAdapter> adapter,
                              const std::string& device_address);
  ~MockBluetoothGattConnection() override;

  MOCK_CONST_METHOD0(GetDeviceAddress, std::string());
  MOCK_METHOD0(IsConnected, bool());
  MOCK_METHOD0(Disconnect, void());
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_CONNECTION_H_
