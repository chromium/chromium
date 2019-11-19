// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_pairing_delegate.h"

#include <memory>

#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

constexpr char kTestBluetoothDeviceAddress[] = "test_device_address";
constexpr char kTestFidoBleDeviceId[] = "ble:test_device_address";
constexpr char kTestPinCode[] = "1234_abcd";
constexpr uint32_t kTestPassKey = 1234;
constexpr char kTestBluetoothDeviceName[] = "device_name";

}  // namespace

class FidoBlePairingDelegateTest : public ::testing::Test {
 public:
  FidoBlePairingDelegate* pairing_delegate() { return pairing_delegate_.get(); }
  MockBluetoothDevice* mock_bluetooth_device() {
    return mock_bluetooth_device_.get();
  }

  const base::flat_map<std::string, std::string>&
  pairing_delegate_pincode_map() {
    return pairing_delegate_->bluetooth_device_pincode_map_;
  }

 private:
  std::unique_ptr<FidoBlePairingDelegate> pairing_delegate_ =
      std::make_unique<FidoBlePairingDelegate>();
  std::unique_ptr<MockBluetoothDevice> mock_bluetooth_device_ =
      std::make_unique<MockBluetoothDevice>(nullptr /* adapter */,
                                            0 /* bluetooth_class */,
                                            kTestBluetoothDeviceName,
                                            kTestBluetoothDeviceAddress,
                                            false /* paired */,
                                            false /* connected */);
};

TEST_F(FidoBlePairingDelegateTest, PairingWithPin) {
  pairing_delegate()->StoreBlePinCodeForDevice(kTestFidoBleDeviceId,
                                               kTestPinCode);
  EXPECT_CALL(*mock_bluetooth_device(), SetPinCode(kTestPinCode));
  pairing_delegate()->RequestPinCode(mock_bluetooth_device());
}

TEST_F(FidoBlePairingDelegateTest, PairingFailsForUnknownDevice) {
  static constexpr char kTestUnknownBleDeviceAddress[] =
      "ble:test_device_address_unknown";

  pairing_delegate()->StoreBlePinCodeForDevice(kTestUnknownBleDeviceAddress,
                                               kTestPinCode);
  EXPECT_CALL(*mock_bluetooth_device(), SetPinCode).Times(0);
  EXPECT_CALL(*mock_bluetooth_device(), CancelPairing);
  pairing_delegate()->RequestPinCode(mock_bluetooth_device());
}

TEST_F(FidoBlePairingDelegateTest, PairWithPassKey) {
  static constexpr char kTestPassKeyAsString[] = "1234";
  pairing_delegate()->StoreBlePinCodeForDevice(kTestFidoBleDeviceId,
                                               kTestPassKeyAsString);
  EXPECT_CALL(*mock_bluetooth_device(), SetPasskey(kTestPassKey));
  pairing_delegate()->RequestPasskey(mock_bluetooth_device());
}

TEST_F(FidoBlePairingDelegateTest, PairingFailsWithMalformedPassKey) {
  static constexpr char kTestIncorrectlyFormattedPassKey[] =
      "123_also_contains_alphabet";

  pairing_delegate()->StoreBlePinCodeForDevice(
      kTestFidoBleDeviceId, kTestIncorrectlyFormattedPassKey);
  EXPECT_CALL(*mock_bluetooth_device(), SetPasskey(kTestPassKey)).Times(0);
  EXPECT_CALL(*mock_bluetooth_device(), CancelPairing);
  pairing_delegate()->RequestPasskey(mock_bluetooth_device());
}

TEST_F(FidoBlePairingDelegateTest, RejectAuthorizePairing) {
  EXPECT_CALL(*mock_bluetooth_device(), CancelPairing);
  pairing_delegate()->AuthorizePairing(mock_bluetooth_device());
}

TEST_F(FidoBlePairingDelegateTest, RejectConfirmPassKey) {
  static constexpr uint32_t kTestPassKey = 0;
  EXPECT_CALL(*mock_bluetooth_device(), CancelPairing);
  pairing_delegate()->ConfirmPasskey(mock_bluetooth_device(), kTestPassKey);
}

TEST_F(FidoBlePairingDelegateTest, ChangeStoredDeviceAddress) {
  static constexpr char kTestNewBleDeviceAddress[] =
      "ble:test_changed_device_address";
  pairing_delegate()->StoreBlePinCodeForDevice(kTestFidoBleDeviceId,
                                               kTestPinCode);
  EXPECT_TRUE(
      base::Contains(pairing_delegate_pincode_map(), kTestFidoBleDeviceId));
  EXPECT_FALSE(
      base::Contains(pairing_delegate_pincode_map(), kTestNewBleDeviceAddress));

  pairing_delegate()->ChangeStoredDeviceAddress(kTestFidoBleDeviceId,
                                                kTestNewBleDeviceAddress);
  EXPECT_FALSE(
      base::Contains(pairing_delegate_pincode_map(), kTestFidoBleDeviceId));
  EXPECT_TRUE(
      base::Contains(pairing_delegate_pincode_map(), kTestNewBleDeviceAddress));
}

}  // namespace device
