// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_SERVICE_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class BluetoothRemoteGattCharacteristic;
class MockBluetoothDevice;

class MockBluetoothGattService : public BluetoothRemoteGattService {
 public:
  MockBluetoothGattService(MockBluetoothDevice* device,
                           const std::string& identifier,
                           const BluetoothUUID& uuid,
                           bool is_primary);

  MockBluetoothGattService(const MockBluetoothGattService&) = delete;
  MockBluetoothGattService& operator=(const MockBluetoothGattService&) = delete;

  ~MockBluetoothGattService() override;

  MOCK_CONST_METHOD0(GetIdentifier, std::string());
  MOCK_CONST_METHOD0(GetUUID, BluetoothUUID());
  MOCK_CONST_METHOD0(IsPrimary, bool());
  MOCK_CONST_METHOD0(GetDevice, BluetoothDevice*());
  MOCK_CONST_METHOD0(GetCharacteristics,
                     std::vector<BluetoothRemoteGattCharacteristic*>());
  MOCK_CONST_METHOD0(GetIncludedServices,
                     std::vector<BluetoothRemoteGattService*>());
  MOCK_CONST_METHOD1(GetCharacteristic,
                     BluetoothRemoteGattCharacteristic*(const std::string&));
  MOCK_CONST_METHOD1(
      GetCharacteristicsByUUID,
      std::vector<BluetoothRemoteGattCharacteristic*>(const BluetoothUUID&));

  void AddMockCharacteristic(
      std::unique_ptr<MockBluetoothGattCharacteristic> mock_characteristic);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_SERVICE_H_
