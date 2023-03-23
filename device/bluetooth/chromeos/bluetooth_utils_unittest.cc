// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/chromeos/bluetooth_utils.h"

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace device {

namespace {

constexpr char kTestBluetoothDisplayName[] = "test_device_name";
constexpr char kTestBluetoothDeviceAddress[] = "01:02:03:04:05:06";
constexpr char kHIDServiceUUID[] = "1812";
constexpr char kSecurityKeyServiceUUID[] = "FFFD";
constexpr char kUnexpectedServiceUUID[] = "1234";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Note: The first 3 hex bytes represent the OUI portion of the address, which
// indicates the device vendor. In this case, "64:16:7F:**:**:**" represents a
// device manufactured by Poly.
constexpr char kFakePolyDeviceAddress[] = "64:16:7F:12:34:56";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const size_t kMaxDevicesForFilter = 5;

}  // namespace

class BluetoothUtilsTest : public testing::Test {
 protected:
  BluetoothUtilsTest() = default;

  BluetoothUtilsTest(const BluetoothUtilsTest&) = delete;
  BluetoothUtilsTest& operator=(const BluetoothUtilsTest&) = delete;

  base::HistogramTester histogram_tester;

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void AddMockPolyDeviceToAdapter() {
    MockBluetoothDevice* mock_bluetooth_device =
        AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_CLASSIC);
    ON_CALL(*mock_bluetooth_device, GetName)
        .WillByDefault(testing::Return(absl::nullopt));
    ON_CALL(*mock_bluetooth_device, GetDeviceType)
        .WillByDefault(testing::Return(BluetoothDeviceType::UNKNOWN));
    ON_CALL(*mock_bluetooth_device, GetAddress)
        .WillByDefault(testing::Return(kFakePolyDeviceAddress));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockBluetoothAdapter> adapter_ =
      base::MakeRefCounted<testing::NiceMock<MockBluetoothAdapter>>();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::ScopedLacrosServiceTestHelper scoped_lacros_service_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
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

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(BluetoothUtilsTest,
       TestFilterBluetoothDeviceList_FilterKnown_AlwaysKeepBondedDevices) {
  auto* mock_bluetooth_device =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_INVALID);
  EXPECT_CALL(*mock_bluetooth_device, IsPaired)
      .WillRepeatedly(testing::Return(true));
  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  0u /* num_expected_remaining_devices */);

  EXPECT_CALL(*mock_bluetooth_device, IsBonded)
      .WillRepeatedly(testing::Return(true));
  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  1u /* num_expected_remaining_devices */);
}
#else
TEST_F(BluetoothUtilsTest,
       TestFilterBluetoothDeviceList_FilterKnown_AlwaysKeepPairedDevices) {
  auto* mock_bluetooth_device =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_INVALID);
  EXPECT_CALL(*mock_bluetooth_device, IsPaired)
      .WillRepeatedly(testing::Return(true));

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  1u /* num_expected_remaining_devices */);
}
#endif

TEST_F(BluetoothUtilsTest,
       TestFilterBluetoothDeviceList_FilterKnown_FilterPairedPhone) {
  auto* mock_bluetooth_device =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_INVALID);
  EXPECT_CALL(*mock_bluetooth_device, IsPaired)
      .WillRepeatedly(testing::Return(true));
  ON_CALL(*mock_bluetooth_device, GetDeviceType)
      .WillByDefault(testing::Return(BluetoothDeviceType::PHONE));

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  0u /* num_expected_remaining_devices */);
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Regression test for b/228118615.
TEST_F(BluetoothUtilsTest, ShowPolyDevice_PolyFlagEnabled) {
  // Poly devices should not be filtered out, regardless of device type.
  AddMockPolyDeviceToAdapter();
  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  1u /* num_expected_remaining_devices */);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(
    BluetoothUtilsTest,
    TestFilterBluetoothDeviceList_FilterKnown_RemoveClassicDevicesWithoutNames) {
  auto* mock_bluetooth_device =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_CLASSIC);
  EXPECT_CALL(*mock_bluetooth_device, GetName)
      .WillOnce(testing::Return(absl::nullopt));

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
    TestFilterBluetoothDeviceList_FilterKnown_DualDevicesWithoutAppearances_RemoveWithFilterFlagEnabled) {
  AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_DUAL);

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  0u /* num_expected_remaining_devices */);
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

TEST_F(BluetoothUtilsTest, TestUiSurfaceDisplayedMetric) {
  RecordUiSurfaceDisplayed(BluetoothUiSurface::kSettingsDeviceListSubpage);

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.UiSurfaceDisplayed",
      BluetoothUiSurface::kSettingsDeviceListSubpage, 1);

  RecordUiSurfaceDisplayed(BluetoothUiSurface::kSettingsDeviceDetailSubpage);

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.UiSurfaceDisplayed",
      BluetoothUiSurface::kSettingsDeviceListSubpage, 1);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.UiSurfaceDisplayed",
      BluetoothUiSurface::kSettingsDeviceDetailSubpage, 1);
}

TEST_F(BluetoothUtilsTest, TestPairMetric) {
  size_t total_count = 0;
  auto assert_histograms = [&](device::ConnectionFailureReason failure_reason) {
    histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.Pairing.Result", 0,
                                       total_count);
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.Pairing.Result.Classic", 0, total_count);
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.Pairing.Duration.Failure", 2000, total_count);
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.Pairing.Duration.Failure.Classic", 2000,
        total_count);
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.Pairing.Result.FailureReason", failure_reason, 1);
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.Pairing.Result.FailureReason.Classic",
        failure_reason, 1);
  };

  auto assert_filtered_failure_histograms =
      [&](device::ConnectionFailureReason error, size_t expected_count) {
        histogram_tester.ExpectBucketCount(
            "Bluetooth.ChromeOS.Pairing.Result.FilteredFailureReason", error,
            expected_count);
        histogram_tester.ExpectBucketCount(
            "Bluetooth.ChromeOS.Pairing.Result.FilteredFailureReason.Classic",
            error, expected_count);
      };

  RecordPairingResult(device::ConnectionFailureReason::kAuthFailed,
                      device::BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC,
                      base::Seconds(2));
  total_count++;
  assert_histograms(device::ConnectionFailureReason::kAuthFailed);
  assert_filtered_failure_histograms(
      device::ConnectionFailureReason::kAuthFailed, /*expected_count=*/1);

  RecordPairingResult(device::ConnectionFailureReason::kAuthCanceled,
                      device::BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC,
                      base::Seconds(2));
  total_count++;
  assert_histograms(device::ConnectionFailureReason::kAuthCanceled);
  assert_filtered_failure_histograms(
      device::ConnectionFailureReason::kAuthCanceled,
      /*expected_count=*/0);

  RecordPairingResult(device::ConnectionFailureReason::kAuthRejected,
                      device::BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC,
                      base::Seconds(2));
  total_count++;
  assert_histograms(device::ConnectionFailureReason::kAuthRejected);
  assert_filtered_failure_histograms(
      device::ConnectionFailureReason::kAuthRejected,
      /*expected_count=*/0);

  RecordPairingResult(device::ConnectionFailureReason::kInprogress,
                      device::BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC,
                      base::Seconds(2));
  total_count++;
  assert_histograms(device::ConnectionFailureReason::kInprogress);
  assert_filtered_failure_histograms(
      device::ConnectionFailureReason::kInprogress, /*expected_count=*/1);
}

TEST_F(BluetoothUtilsTest, TestUserAttemptedReconnectionMetric) {
  RecordUserInitiatedReconnectionAttemptDuration(
      device::ConnectionFailureReason::kFailed,
      device::BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC,
      base::Seconds(2));

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Duration.Failure",
      2000, 1);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Duration.Failure."
      "Classic",
      2000, 1);
}

TEST_F(BluetoothUtilsTest, TestDisconnectMetric) {
  RecordDeviceDisconnect(BluetoothDeviceType::MOUSE);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.DeviceDisconnect",
                                     BluetoothDeviceType::MOUSE, 1);
}

}  // namespace device
