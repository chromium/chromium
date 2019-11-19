// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/chromeos/bluetooth_utils.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/constants/chromeos_features.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

constexpr char kTestBluetoothDisplayName[] = "test_device_name";
constexpr char kTestBluetoothDeviceAddress[] = "01:02:03:04:05:06";
constexpr char kHIDServiceUUID[] = "1812";
constexpr char kSecurityKeyServiceUUID[] = "FFFD";
constexpr char kUnexpectedServiceUUID[] = "1234";
const size_t kMaxDevicesForFilter = 5;

}  // namespace

class BluetoothUtilsTest : public testing::Test {
 protected:
  BluetoothUtilsTest() = default;

  void SetUp() override {
    BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
  }

  MockBluetoothDevice* AddMockBluetoothDeviceToAdapter(
      BluetoothTransport transport) {
    auto mock_bluetooth_device =
        std::make_unique<testing::NiceMock<MockBluetoothDevice>>(
            adapter_.get(), 0 /* bluetooth_class */, kTestBluetoothDisplayName,
            kTestBluetoothDeviceAddress, false /* paired */,
            false /* connected */);

    ON_CALL(*mock_bluetooth_device, GetType)
        .WillByDefault(testing::Return(transport));

    auto* mock_bluetooth_device_ptr = mock_bluetooth_device.get();
    adapter_->AddMockDevice(std::move(mock_bluetooth_device));
    return mock_bluetooth_device_ptr;
  }

  MockBluetoothAdapter* adapter() { return adapter_.get(); }

  MockBluetoothDevice* GetMockBluetoothDevice(size_t index) {
    return static_cast<MockBluetoothDevice*>(
        adapter()->GetMockDevices()[index]);
  }

  void VerifyFilterBluetoothDeviceList(BluetoothFilterType filter_type,
                                       size_t num_expected_remaining_devices) {
    BluetoothAdapter::DeviceList filtered_device_list =
        FilterBluetoothDeviceList(adapter_->GetMockDevices(), filter_type,
                                  kMaxDevicesForFilter);
    EXPECT_EQ(num_expected_remaining_devices, filtered_device_list.size());
  }

  void DisableAggressiveAppearanceFilter() {
    feature_list_.InitAndDisableFeature(
        chromeos::features::kBluetoothAggressiveAppearanceFilter);
  }

  void SetLongTermKeys(const std::string& keys) {
    feature_list_.InitAndEnableFeatureWithParameters(
        chromeos::features::kBlueZLongTermKeyBlocklist,
        {{chromeos::features::kBlueZLongTermKeyBlocklistParamName, keys}});
  }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockBluetoothAdapter> adapter_ =
      base::MakeRefCounted<testing::NiceMock<MockBluetoothAdapter>>();
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothUtilsTest);
};

TEST_F(BluetoothUtilsTest,
       TestFilterBluetoothDeviceList_FilterAll_NoDevicesFiltered) {
  // If BluetoothFilterType::KNOWN were passed, this device would otherwise be
  // filtered out, but we expect it to not be.
  AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_INVALID);

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::ALL,
                                  1u /* num_expected_remaining_devices */);
}

TEST_F(BluetoothUtilsTest,
       TestFilterBluetoothDeviceList_FilterAll_MaxDevicesExceeded) {
  for (size_t i = 0; i < kMaxDevicesForFilter * 2; ++i)
    AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_INVALID);

  VerifyFilterBluetoothDeviceList(
      BluetoothFilterType::ALL,
      kMaxDevicesForFilter /* num_expected_remaining_devices */);
}

TEST_F(BluetoothUtilsTest,
       TestFilterBluetoothDeviceList_FilterKnown_AlwaysKeepPairedDevices) {
  auto* mock_bluetooth_device =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_INVALID);
  EXPECT_CALL(*mock_bluetooth_device, IsPaired)
      .WillRepeatedly(testing::Return(true));

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  1u /* num_expected_remaining_devices */);
}

TEST_F(BluetoothUtilsTest,
       TestFilterBluetoothDeviceList_FilterKnown_RemoveInvalidDevices) {
  AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_INVALID);

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  0u /* num_expected_remaining_devices */);
}

TEST_F(BluetoothUtilsTest,
       TestFilterBluetoothDeviceList_FilterKnown_KeepClassicDevicesWithNames) {
  AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_CLASSIC);

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  1u /* num_expected_remaining_devices */);
}

TEST_F(
    BluetoothUtilsTest,
    TestFilterBluetoothDeviceList_FilterKnown_RemoveClassicDevicesWithoutNames) {
  auto* mock_bluetooth_device =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_CLASSIC);
  EXPECT_CALL(*mock_bluetooth_device, GetName)
      .WillOnce(testing::Return(base::nullopt));

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  0u /* num_expected_remaining_devices */);
}

TEST_F(
    BluetoothUtilsTest,
    TestFilterBluetoothDeviceList_FilterKnown_RemoveBleDevicesWithoutExpectedUuids) {
  auto* mock_bluetooth_device =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_LE);
  mock_bluetooth_device->AddUUID(device::BluetoothUUID(kUnexpectedServiceUUID));

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  0u /* num_expected_remaining_devices */);
}

TEST_F(
    BluetoothUtilsTest,
    TestFilterBluetoothDeviceList_FilterKnown_KeepBleDevicesWithExpectedUuids) {
  auto* mock_bluetooth_device_1 =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_LE);
  mock_bluetooth_device_1->AddUUID(device::BluetoothUUID(kHIDServiceUUID));

  auto* mock_bluetooth_device_2 =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_LE);
  mock_bluetooth_device_2->AddUUID(
      device::BluetoothUUID(kSecurityKeyServiceUUID));

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  2u /* num_expected_remaining_devices */);
}

TEST_F(
    BluetoothUtilsTest,
    TestFilterBluetoothDeviceList_FilterKnown_KeepDualDevicesWithNamesAndAppearances) {
  auto* mock_bluetooth_device =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_DUAL);
  EXPECT_CALL(*mock_bluetooth_device, GetDeviceType)
      .WillRepeatedly(testing::Return(BluetoothDeviceType::AUDIO));

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  1u /* num_expected_remaining_devices */);
}

TEST_F(
    BluetoothUtilsTest,
    TestFilterBluetoothDeviceList_FilterKnown_DualDevicesWithoutAppearances_KeepWithFilterFlagDisabled) {
  DisableAggressiveAppearanceFilter();

  AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_DUAL);

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  1u /* num_expected_remaining_devices */);
}

TEST_F(
    BluetoothUtilsTest,
    TestFilterBluetoothDeviceList_FilterKnown_DualDevicesWithoutAppearances_RemoveWithFilterFlagEnabled) {
  AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_DUAL);

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  0u /* num_expected_remaining_devices */);
}

TEST_F(
    BluetoothUtilsTest,
    TestFilterBluetoothDeviceList_FilterKnown_AppearanceComputer_KeepWithFilterFlagDisabled) {
  DisableAggressiveAppearanceFilter();

  auto* mock_bluetooth_device =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_CLASSIC);
  ON_CALL(*mock_bluetooth_device, GetDeviceType)
      .WillByDefault(testing::Return(BluetoothDeviceType::COMPUTER));

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  1u /* num_expected_remaining_devices */);
}

TEST_F(
    BluetoothUtilsTest,
    TestFilterBluetoothDeviceList_FilterKnown_AppearanceComputer_RemoveWithFilterFlagEnabled) {
  auto* mock_bluetooth_device_1 =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_CLASSIC);
  EXPECT_CALL(*mock_bluetooth_device_1, GetDeviceType)
      .WillOnce(testing::Return(BluetoothDeviceType::COMPUTER));

  auto* mock_bluetooth_device_2 =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_LE);
  EXPECT_CALL(*mock_bluetooth_device_2, GetDeviceType)
      .WillOnce(testing::Return(BluetoothDeviceType::COMPUTER));

  auto* mock_bluetooth_device_3 =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_DUAL);
  EXPECT_CALL(*mock_bluetooth_device_3, GetDeviceType)
      .WillOnce(testing::Return(BluetoothDeviceType::COMPUTER));

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  0u /* num_expected_remaining_devices */);
}

TEST_F(BluetoothUtilsTest,
       TestFilterBluetoothDeviceList_FilterKnown_RemoveAppearancePhone) {
  auto* mock_bluetooth_device =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_DUAL);
  ON_CALL(*mock_bluetooth_device, GetDeviceType)
      .WillByDefault(testing::Return(BluetoothDeviceType::PHONE));

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  0u /* num_expected_remaining_devices */);
}

TEST_F(BluetoothUtilsTest, TestGetBlockedLongTermKeys_ListIncludesBadLtks) {
  // One nibble too long, one nibble too short, and one nibble just right.
  std::string hex_key_1 = "000000000000000000000000000012345";
  std::string hex_key_2 = "0000000000000000000000000000123";
  std::string hex_key_3 = "00000000000000000000000000001234";
  SetLongTermKeys(hex_key_1 + ',' + hex_key_2 + ',' + hex_key_3);

  std::vector<std::vector<uint8_t>> expected_array;
  std::vector<uint8_t> expected_key = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x12, 0x34};
  expected_array.push_back(expected_key);

  EXPECT_EQ(expected_array, device::GetBlockedLongTermKeys());
}

TEST_F(BluetoothUtilsTest, TestGetBlockedLongTermKeys_ListIncludesNonHexInput) {
  std::string hex_key_1 = "bad00input00but00correct00length";
  std::string hex_key_2 = "00000000000000000000000000001234";
  SetLongTermKeys(hex_key_1 + ',' + hex_key_2);

  std::vector<std::vector<uint8_t>> expected_array;
  std::vector<uint8_t> expected_key = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x12, 0x34};
  expected_array.push_back(expected_key);

  EXPECT_EQ(expected_array, device::GetBlockedLongTermKeys());
}

TEST_F(BluetoothUtilsTest, TestEmptyList) {
  SetLongTermKeys("");

  std::vector<std::vector<uint8_t>> expected_array;

  EXPECT_EQ(expected_array, device::GetBlockedLongTermKeys());
}

TEST_F(BluetoothUtilsTest, TestGetBlockedLongTermKeys_OneElementList) {
  std::string hex_key_1 = "012300004567000089ab0000cdef0000";
  std::vector<uint8_t> expected_key_1 = {0x01, 0x23, 0x00, 0x00, 0x45, 0x67,
                                         0x00, 0x00, 0x89, 0xab, 0x00, 0x00,
                                         0xcd, 0xef, 0x00, 0x00};

  SetLongTermKeys(hex_key_1);

  std::vector<std::vector<uint8_t>> expected_array;
  expected_array.push_back(expected_key_1);

  EXPECT_EQ(expected_array, device::GetBlockedLongTermKeys());
}

TEST_F(BluetoothUtilsTest, TestGetBlockedLongTermKeys_MultipleElementList) {
  std::string hex_key_1 = "012300004567000089ab0000cdef0000";
  std::vector<uint8_t> expected_key_1 = {0x01, 0x23, 0x00, 0x00, 0x45, 0x67,
                                         0x00, 0x00, 0x89, 0xab, 0x00, 0x00,
                                         0xcd, 0xef, 0x00, 0x00};

  std::string hex_key_2 = "00001111222233334444555566667777";
  std::vector<uint8_t> expected_key_2 = {0x00, 0x00, 0x11, 0x11, 0x22, 0x22,
                                         0x33, 0x33, 0x44, 0x44, 0x55, 0x55,
                                         0x66, 0x66, 0x77, 0x77};

  std::string hex_key_3 = "88889999aaaabbbbccccddddeeeeffff";
  std::vector<uint8_t> expected_key_3 = {0x88, 0x88, 0x99, 0x99, 0xaa, 0xaa,
                                         0xbb, 0xbb, 0xcc, 0xcc, 0xdd, 0xdd,
                                         0xee, 0xee, 0xff, 0xff};

  SetLongTermKeys(hex_key_1 + ',' + hex_key_2 + ',' + hex_key_3);

  std::vector<std::vector<uint8_t>> expected_array;
  expected_array.push_back(expected_key_1);
  expected_array.push_back(expected_key_2);
  expected_array.push_back(expected_key_3);

  EXPECT_EQ(expected_array, device::GetBlockedLongTermKeys());
}

}  // namespace device
