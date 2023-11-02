// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_PAIRING_DELEGATE_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_PAIRING_DELEGATE_H_

#include <cstdint>
#include <string>

#include "device/bluetooth/bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockPairingDelegate : public BluetoothDevice::PairingDelegate {
 public:
  MockPairingDelegate();
  MockPairingDelegate(const MockPairingDelegate&) = delete;
  ~MockPairingDelegate() override;
  MockPairingDelegate& operator=(const MockPairingDelegate&) = delete;

  MOCK_METHOD1(RequestPinCode, void(BluetoothDevice* device));
  MOCK_METHOD1(RequestPasskey, void(BluetoothDevice* device));
  MOCK_METHOD2(DisplayPinCode,
               void(BluetoothDevice* device, const std::string& pincode));
  MOCK_METHOD2(DisplayPasskey, void(BluetoothDevice* device, uint32_t passkey));
  MOCK_METHOD2(KeysEntered, void(BluetoothDevice* device, uint32_t entered));
  MOCK_METHOD2(ConfirmPasskey, void(BluetoothDevice* device, uint32_t passkey));
  MOCK_METHOD1(AuthorizePairing, void(BluetoothDevice* device));
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_PAIRING_DELEGATE_H_
