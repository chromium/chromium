// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/chromeos/bluetooth_utils.h"

#include "ash/constants/ash_pref_names.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_client_uuids.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/ble_constants.h"
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
constexpr uint8_t kLimitedDiscoveryFlag = 0x01;
constexpr uint8_t kGeneralDiscoveryFlag = 0x02;
constexpr char kTimeIntervalBetweenConnectionsHistogramName[] =
    "Bluetooth.ChromeOS.TimeIntervalBetweenConnections";
const BluetoothDevice::ServiceDataMap kTestServiceDataMap = {
    {BluetoothUUID(kHIDServiceUUID), {1}}};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Note: The first 3 hex bytes represent the OUI portion of the address, which
// indicates the device vendor. In this case, "64:16:7F:**:**:**" represents a
// device manufactured by Poly.
constexpr char kFakePolyDeviceAddress[] = "64:16:7F:12:34:56";
constexpr char kConnectionToastShownLast24HoursCountHistogramName[] =
    "Bluetooth.ChromeOS.ConnectionToastShownIn24Hours.Count";
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
        .WillByDefault(testing::Return(std::nullopt));
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
  EXPECT_CALL(*mock_bluetooth_device, GetDeviceType)
      .WillRepeatedly(testing::Return(BluetoothDeviceType::PHONE));
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
      .WillOnce(testing::Return(std::nullopt));

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
    TestFilterBluetoothDeviceList_FilterKnown_RemoveBleDevicesNonDiscoverable) {
  auto* mock_bluetooth_device_1 =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_LE);
  mock_bluetooth_device_1->AddUUID(device::BluetoothUUID(kHIDServiceUUID));
  mock_bluetooth_device_1->UpdateAdvertisementData(
      1 /* rssi */, 0 /* flags */, BluetoothDevice::UUIDList(),
      std::nullopt /* tx_power */, kTestServiceDataMap,
      BluetoothDevice::ManufacturerDataMap());

  auto* mock_bluetooth_device_2 =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_LE);
  mock_bluetooth_device_2->AddUUID(device::BluetoothUUID(kHIDServiceUUID));
  mock_bluetooth_device_2->UpdateAdvertisementData(
      1 /* rssi */, kLimitedDiscoveryFlag /* flags */,
      BluetoothDevice::UUIDList(), std::nullopt /* tx_power */,
      kTestServiceDataMap, BluetoothDevice::ManufacturerDataMap());

  auto* mock_bluetooth_device_3 =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_LE);
  mock_bluetooth_device_3->AddUUID(device::BluetoothUUID(kHIDServiceUUID));
  mock_bluetooth_device_3->UpdateAdvertisementData(
      1 /* rssi */, kGeneralDiscoveryFlag /* flags */,
      BluetoothDevice::UUIDList(), std::nullopt /* tx_power */,
      kTestServiceDataMap, BluetoothDevice::ManufacturerDataMap());

  auto* mock_bluetooth_device_4 =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_LE);
  mock_bluetooth_device_4->AddUUID(device::BluetoothUUID(kHIDServiceUUID));
  mock_bluetooth_device_4->UpdateAdvertisementData(
      1 /* rssi */, kLimitedDiscoveryFlag | kGeneralDiscoveryFlag /* flags */,
      BluetoothDevice::UUIDList(), std::nullopt /* tx_power */,
      kTestServiceDataMap, BluetoothDevice::ManufacturerDataMap());

  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  3u /* num_expected_remaining_devices */);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(
    BluetoothUtilsTest,
    TestFilterBluetoothDeviceList_FilterKnown_RemoveDevicesWithUnsupportedUuids) {
  // These UUIDs are specific to Nearby Share and Phone Hub and are used to
  // identify devices that should be filtered from the UI that otherwise would
  // not have been correctly identified. These devices should always be filtered
  // from the UI. For more information see b/219627324.
  std::vector<BluetoothUUID> unsupported_uuids =
      ash::nearby::GetNearbyClientUuids();
  unsupported_uuids.push_back(
      BluetoothUUID(ash::secure_channel::kGattServerUuid));

  for (const auto& uuid : unsupported_uuids) {
    auto* mock_bluetooth_device =
        AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_DUAL);
    mock_bluetooth_device->AddUUID(uuid);
    VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                    0u /* num_expected_remaining_devices */);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(BluetoothUtilsTest,
       TestFilterBluetoothDeviceList_FilterKnown_DualWithRandomAddressIsLE) {
  auto* mock_bluetooth_device =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_DUAL);
  EXPECT_CALL(*mock_bluetooth_device, GetAddressType)
      .WillRepeatedly(
          testing::Return(BluetoothDevice::AddressType::ADDR_TYPE_RANDOM));

  // Test RemoveBleDevicesWithoutExpectedUuids
  mock_bluetooth_device->AddUUID(device::BluetoothUUID(kUnexpectedServiceUUID));
  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  0u /* num_expected_remaining_devices */);

  // Test KeepBleDevicesWithExpectedUuids
  mock_bluetooth_device->AddUUID(device::BluetoothUUID(kHIDServiceUUID));
  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  1u /* num_expected_remaining_devices */);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(BluetoothUtilsTest,
       TestFilterBluetoothDeviceList_FilterKnown_DualWithKnownTypeIsClassic) {
  auto* mock_bluetooth_device =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_DUAL);
  EXPECT_CALL(*mock_bluetooth_device, GetDeviceType)
      .WillRepeatedly(testing::Return(BluetoothDeviceType::AUDIO));

  // Test RemoveClassicDevicesWithoutNames
  EXPECT_CALL(*mock_bluetooth_device, GetName)
      .WillRepeatedly(testing::Return(std::nullopt));
  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  0u /* num_expected_remaining_devices */);

  // Test KeepClassicDevicesWithNames
  EXPECT_CALL(*mock_bluetooth_device, GetName)
      .WillRepeatedly(testing::Return(kTestBluetoothDisplayName));
  VerifyFilterBluetoothDeviceList(BluetoothFilterType::KNOWN,
                                  1u /* num_expected_remaining_devices */);
}

TEST_F(
    BluetoothUtilsTest,
    TestFilterBluetoothDeviceList_FilterKnown_DualWithoutKnownTypeIsInvalid) {
  auto* mock_bluetooth_device =
      AddMockBluetoothDeviceToAdapter(BLUETOOTH_TRANSPORT_DUAL);
  EXPECT_CALL(*mock_bluetooth_device, GetName)
      .WillRepeatedly(testing::Return(kTestBluetoothDisplayName));
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

  auto assert_filtered_user_error_histograms =
      [&](device::ConnectionFailureReason failure_reason) {
        histogram_tester.ExpectBucketCount(
            "Bluetooth.ChromeOS.Pairing.Result.UserErrorsFiltered2", 0,
            total_count - 1);
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

  RecordPairingResult(device::ConnectionFailureReason::kNotFound,
                      device::BluetoothTransport::BLUETOOTH_TRANSPORT_CLASSIC,
                      base::Seconds(2));
  total_count++;
  assert_histograms(device::ConnectionFailureReason::kNotFound);
  assert_filtered_user_error_histograms(
      device::ConnectionFailureReason::kNotFound);

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

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Result."
      "UserErrorsFiltered",
      0, 0);

  RecordUserInitiatedReconnectionAttemptResult(
      device::ConnectionFailureReason::kFailed,
      device::UserInitiatedReconnectionUISurfaces::kSettings);

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Result."
      "UserErrorsFiltered",
      0, 1);

  RecordUserInitiatedReconnectionAttemptResult(
      device::ConnectionFailureReason::kInprogress,
      device::UserInitiatedReconnectionUISurfaces::kSettings);

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.UserInitiatedReconnectionAttempt.Result."
      "UserErrorsFiltered",
      0, 1);
}

TEST_F(BluetoothUtilsTest, TestDisconnectMetric) {
  RecordDeviceDisconnect(BluetoothDeviceType::MOUSE);
  histogram_tester.ExpectBucketCount("Bluetooth.ChromeOS.DeviceDisconnect",
                                     BluetoothDeviceType::MOUSE, 1);
}

TEST_F(BluetoothUtilsTest, TestTimeIntervalBetweenConnectionsMetric) {
  // Verify no initial histogram entries.
  histogram_tester.ExpectTotalCount(
      kTimeIntervalBetweenConnectionsHistogramName, 0);

  // Record a time interval of 50 minutes between connections, this should not
  // be recorded as it exceeds the threshold for recording.
  RecordTimeIntervalBetweenConnections(base::Minutes(50));
  histogram_tester.ExpectTotalCount(
      kTimeIntervalBetweenConnectionsHistogramName, 0);

  // Record a time interval of 1 minute between connections.
  RecordTimeIntervalBetweenConnections(base::Minutes(1));
  histogram_tester.ExpectTotalCount(
      kTimeIntervalBetweenConnectionsHistogramName, 1);
  histogram_tester.ExpectTimeBucketCount(
      kTimeIntervalBetweenConnectionsHistogramName, base::Minutes(1), 1);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(BluetoothUtilsTest, TestConnectionToastShownCount24HoursMetric) {
  // Verify no initial histogram entries.
  histogram_tester.ExpectTotalCount(
      kConnectionToastShownLast24HoursCountHistogramName, 0);

  // Initialize pref.
  std::unique_ptr<TestingPrefServiceSimple> local_state =
      std::make_unique<TestingPrefServiceSimple>();
  local_state->registry()->RegisterIntegerPref(
      ash::prefs::kBluetoothConnectionToastShownCount, 0);
  local_state->registry()->RegisterTimePref(
      ash::prefs::kBluetoothToastCountStartTime,
      base::Time::Now().LocalMidnight());

  // Simulate 1 minute passing and connect the Bluetooth device.
  local_state->SetTime(ash::prefs::kBluetoothToastCountStartTime,
                       base::Time::Now().LocalMidnight() - base::Minutes(1));
  MaybeRecordConnectionToastShownCount(local_state.get(),
                                       /*triggered_by_connect=*/true);

  // Verify the toast count increments to 1, and no metric is recorded since
  // it is within 24 hours.
  EXPECT_EQ(1, local_state->GetInteger(
                   ash::prefs::kBluetoothConnectionToastShownCount));
  histogram_tester.ExpectTotalCount(
      kConnectionToastShownLast24HoursCountHistogramName, 0);

  // Simulate 15 minutes passing and connect again.
  local_state->SetTime(ash::prefs::kBluetoothToastCountStartTime,
                       base::Time::Now().LocalMidnight() - base::Minutes(15));
  MaybeRecordConnectionToastShownCount(local_state.get(),
                                       /*triggered_by_connect=*/true);

  // Verify the toast count increments to 2, but still no metrics recorded.
  EXPECT_EQ(2, local_state->GetInteger(
                   ash::prefs::kBluetoothConnectionToastShownCount));
  histogram_tester.ExpectTotalCount(
      kConnectionToastShownLast24HoursCountHistogramName, 0);

  // Simulate 30 hours passing and connect.
  local_state->SetTime(ash::prefs::kBluetoothToastCountStartTime,
                       base::Time::Now().LocalMidnight() - base::Hours(30));
  MaybeRecordConnectionToastShownCount(local_state.get(),
                                       /*triggered_by_connect=*/true);

  // Verify the metric is emitted after the 24-hour threshold is crossed.
  histogram_tester.ExpectTotalCount(
      kConnectionToastShownLast24HoursCountHistogramName, 1);
  histogram_tester.ExpectBucketCount(
      kConnectionToastShownLast24HoursCountHistogramName, /*sample=*/2,
      /*expected_count=*/1);

  // Verify the toast count and start time are reset after emitting the metric.
  EXPECT_EQ(1, local_state->GetInteger(
                   ash::prefs::kBluetoothConnectionToastShownCount));
  EXPECT_EQ(base::Time::Now().LocalMidnight(),
            local_state->GetTime(ash::prefs::kBluetoothToastCountStartTime));

  // Simulate passing more than 24 hours, but this time, triggered by device
  // start.
  local_state->SetTime(ash::prefs::kBluetoothToastCountStartTime,
                       base::Time::Now().LocalMidnight() - base::Hours(30));
  MaybeRecordConnectionToastShownCount(local_state.get(),
                                       /*triggered_by_connect=*/false);

  // Verify the metric is emitted.
  histogram_tester.ExpectTotalCount(
      kConnectionToastShownLast24HoursCountHistogramName, 2);
  histogram_tester.ExpectBucketCount(
      kConnectionToastShownLast24HoursCountHistogramName, /*sample=*/1,
      /*expected_count=*/1);

  // Verify the count is reset to 0, instead of 1 this time.
  EXPECT_EQ(0, local_state->GetInteger(
                   ash::prefs::kBluetoothConnectionToastShownCount));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(BluetoothUtilsTest, TestFlossManagerClientInitMetric) {
  static int expected_num_successes = 0, expected_num_failures = 0;
  auto assert_histograms = [&]() {
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.FlossManagerClientInit.Result", 1,
        expected_num_successes);
    histogram_tester.ExpectBucketCount(
        "Bluetooth.ChromeOS.FlossManagerClientInit.Result", 0,
        expected_num_failures);
  };
  RecordFlossManagerClientInit(/*success=*/true, base::Seconds(1));
  expected_num_successes++;
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.FlossManagerClientInit.Duration.Success", 1000, 1);
  assert_histograms();

  RecordFlossManagerClientInit(/*success=*/false, base::Seconds(2));
  expected_num_failures++;
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.FlossManagerClientInit.Duration.Failure", 2000, 1);
  assert_histograms();
}

}  // namespace device
