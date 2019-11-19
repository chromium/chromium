// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_DESCRIPTOR_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_DESCRIPTOR_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockBluetoothGattCharacteristic;

class MockBluetoothGattDescriptor : public BluetoothRemoteGattDescriptor {
 public:
  MockBluetoothGattDescriptor(
      MockBluetoothGattCharacteristic* characteristic,
      const std::string& identifier,
      const BluetoothUUID& uuid,
      bool is_local,
      BluetoothRemoteGattCharacteristic::Permissions permissions);
  ~MockBluetoothGattDescriptor() override;

  MOCK_CONST_METHOD0(GetIdentifier, std::string());
  MOCK_CONST_METHOD0(GetUUID, BluetoothUUID());
  MOCK_CONST_METHOD0(IsLocal, bool());
  MOCK_CONST_METHOD0(GetValue, const std::vector<uint8_t>&());
  MOCK_CONST_METHOD0(GetCharacteristic, BluetoothRemoteGattCharacteristic*());
  MOCK_CONST_METHOD0(GetPermissions,
                     BluetoothRemoteGattCharacteristic::Permissions());
  void ReadRemoteDescriptor(ValueCallback c, ErrorCallback ec) override {
    ReadRemoteDescriptor_(c, ec);
  }
  MOCK_METHOD2(ReadRemoteDescriptor_, void(ValueCallback&, ErrorCallback&));
  void WriteRemoteDescriptor(const std::vector<uint8_t>& v,
                             base::OnceClosure c,
                             ErrorCallback ec) override {
    WriteRemoteDescriptor_(v, c, ec);
  }
  MOCK_METHOD3(WriteRemoteDescriptor_,
               void(const std::vector<uint8_t>&,
                    base::OnceClosure&,
                    ErrorCallback&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBluetoothGattDescriptor);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_GATT_DESCRIPTOR_H_
