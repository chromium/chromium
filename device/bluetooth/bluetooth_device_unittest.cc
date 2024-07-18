// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/bluetooth/bluetooth_device.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_pairing_delegate.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif BUILDFLAG(IS_APPLE)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "device/bluetooth/test/bluetooth_test_win.h"
#elif defined(USE_CAST_BLUETOOTH_ADAPTER)
#include "device/bluetooth/test/bluetooth_test_cast.h"
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include "device/bluetooth/test/bluetooth_test_bluez.h"
#elif BUILDFLAG(IS_FUCHSIA)
#include "device/bluetooth/test/bluetooth_test_fuchsia.h"
#endif

namespace device {


namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

int8_t ToInt8(BluetoothTest::TestRSSI rssi) {
  return static_cast<int8_t>(rssi);
}

int8_t ToInt8(BluetoothTest::TestTxPower tx_power) {
  return static_cast<int8_t>(tx_power);
}

#if BUILDFLAG(IS_WIN)
void ScheduleAsynchronousCancelPairing(BluetoothDevice* device) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothDevice::CancelPairing,
                                base::Unretained(device)));
}

void ScheduleAsynchronousRejectPairing(BluetoothDevice* device) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BluetoothDevice::RejectPairing,
                                base::Unretained(device)));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace


using UUIDSet = BluetoothDevice::UUIDSet;
using ServiceDataMap = BluetoothDevice::ServiceDataMap;
using ManufacturerDataMap = BluetoothDevice::ManufacturerDataMap;

TEST(BluetoothDeviceTest, CanonicalizeAddressFormat_AcceptsAllValidFormats) {
  // There are three valid separators (':', '-', and none).
  // Case shouldn't matter.
  const char* const kValidFormats[] = {
    "1A:2B:3C:4D:5E:6F",
    "1a:2B:3c:4D:5e:6F",
    "1a:2b:3c:4d:5e:6f",
    "1A-2B-3C-4D-5E-6F",
    "1a-2B-3c-4D-5e-6F",
    "1a-2b-3c-4d-5e-6f",
    "1A2B3C4D5E6F",
    "1a2B3c4D5e6F",
    "1a2b3c4d5e6f",
  };

  for (size_t i = 0; i < std::size(kValidFormats); ++i) {
    SCOPED_TRACE(std::string("Input format: '") + kValidFormats[i] + "'");
    EXPECT_EQ("1A:2B:3C:4D:5E:6F",
              CanonicalizeBluetoothAddress(kValidFormats[i]));

    std::array<uint8_t, 6> parsed;
    EXPECT_TRUE(ParseBluetoothAddress(kValidFormats[i], parsed));
    EXPECT_EQ("\x1a\x2b\x3c\x4d\x5e\x6f",
              std::string(parsed.begin(), parsed.end()));
  }
}

TEST(BluetoothDeviceTest,
     CanonicalizeAddressFormat_AcceptsAllValidFormatsBytes) {
  std::array<uint8_t, 6> kValidBytes = {12, 14, 76, 200, 5, 8};

  EXPECT_EQ("0C:0E:4C:C8:05:08", CanonicalizeBluetoothAddress(kValidBytes));
}

TEST(BluetoothDeviceTest, CanonicalizeAddressFormat_RejectsInvalidFormats) {
  const char* const kInvalidFormats[] = {
      // Empty string.
      "",
      // Too short.
      "1A:2B:3C:4D:5E",
      // Too long.
      "1A:2B:3C:4D:5E:6F:70",
      // Missing a separator.
      "1A:2B:3C:4D:5E6F",
      // Mixed separators.
      "1A:2B-3C:4D-5E:6F",
      // Invalid hex (6X)
      "1A:2B:3C:4D:5E:6X",
      // Separators in the wrong place.
      "1:A2:B3:C4:D5:E6F",
      // Wrong separator
      "1A|2B|3C|4D|5E|6F",
  };

  for (size_t i = 0; i < std::size(kInvalidFormats); ++i) {
    SCOPED_TRACE(std::string("Input format: '") + kInvalidFormats[i] + "'");
    EXPECT_EQ(std::string(), CanonicalizeBluetoothAddress(kInvalidFormats[i]));

    std::array<uint8_t, 6> parsed;
    EXPECT_FALSE(ParseBluetoothAddress(kInvalidFormats[i], parsed));
  }
}

TEST(BluetoothDeviceTest, GattConnectionErrorReentrancy) {
  constexpr char kTestDeviceAddress[] = "00:11:22:33:44:55";

  auto adapter = base::MakeRefCounted<MockBluetoothAdapter>();
  MockBluetoothDevice device(adapter.get(),
                             /*bluetooth_class=*/0, "Test Device",
                             kTestDeviceAddress,
                             /*initially_paired=*/false,
                             /*connected=*/false);

  EXPECT_CALL(*adapter, GetDevice(kTestDeviceAddress))
      .WillRepeatedly(Return(&device));

  EXPECT_CALL(device, CreateGattConnection(_, _))
      .Times(2)
      .WillRepeatedly([&](BluetoothDevice::GattConnectionCallback callback,
                          std::optional<BluetoothUUID> service_uuid) {
        device.BluetoothDevice::CreateGattConnection(std::move(callback),
                                                     service_uuid);
      });
  EXPECT_CALL(device, CreateGattConnectionImpl(_))
      .WillOnce([&](std::optional<BluetoothUUID> service_uuid) {
        device.DidConnectGatt(BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
      });
  EXPECT_CALL(device, IsGattConnected())
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  // Trigger potential re-entrancy problems by calling CreateGattConnection()
  // from within the callback passed to CreateGattConnection().
  device.CreateGattConnection(
      base::BindLambdaForTesting(
          [&](std::unique_ptr<BluetoothGattConnection> connection,
              std::optional<BluetoothDevice::ConnectErrorCode> error_code) {
            EXPECT_FALSE(connection);
            EXPECT_EQ(error_code,
                      BluetoothDevice::ConnectErrorCode::ERROR_FAILED);
            device.CreateGattConnection(
                base::BindLambdaForTesting(
                    [&](std::unique_ptr<BluetoothGattConnection> connection,
                        std::optional<BluetoothDevice::ConnectErrorCode>
                            error_code) {
                      EXPECT_TRUE(connection);
                      EXPECT_FALSE(error_code);
                    }),
                /*service_uuid=*/std::nullopt);
          }),
      /*service_uuid=*/std::nullopt);
}

#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, DeviceIsPaired) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  // By default a device should not be paired.
  EXPECT_FALSE(device->IsPaired());

  // Connect to the device and simulate a paired state.
  ASSERT_TRUE(ConnectGatt(device));
  SimulateDevicePaired(device, true);
  EXPECT_TRUE(device->IsPaired());

  SimulateDevicePaired(device, false);
  EXPECT_FALSE(device->IsPaired());
}

// Tests that providing a correct pin code results in a paired device.
TEST_P(BluetoothTestWinrt, DevicePairRequestPinCodeCorrect) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_FALSE(device->IsPaired());
  EXPECT_FALSE(device->ExpectingPinCode());

  SimulatePairingPinCode(device, "123456");
  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, RequestPinCode)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        device->SetPinCode("123456");
      });

  base::RunLoop run_loop;
  device->Pair(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&](std::optional<BluetoothDevice::ConnectErrorCode> error_code) {
            EXPECT_FALSE(error_code.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(device->IsPaired());
  EXPECT_FALSE(device->ExpectingPinCode());
}

// Tests that providing a wrong pin code does not result in a paired device.
TEST_P(BluetoothTestWinrt, DevicePairRequestPinCodeWrong) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_FALSE(device->IsPaired());
  EXPECT_FALSE(device->ExpectingPinCode());

  SimulatePairingPinCode(device, "123456");
  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, RequestPinCode)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        device->SetPinCode("000000");
      });
  base::RunLoop run_loop;
  device->Pair(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&](std::optional<BluetoothDevice::ConnectErrorCode> error_code) {
            EXPECT_EQ(BluetoothDevice::ERROR_FAILED, error_code);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_FALSE(device->IsPaired());
  EXPECT_FALSE(device->ExpectingPinCode());
}

// Tests that rejecting the pairing does not result in a paired device.
TEST_P(BluetoothTestWinrt, DevicePairRequestPinCodeRejectPairing) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_FALSE(device->IsPaired());
  EXPECT_FALSE(device->ExpectingPinCode());

  SimulatePairingPinCode(device, "123456");
  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, RequestPinCode)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        ScheduleAsynchronousRejectPairing(device);
      });

  base::RunLoop run_loop;
  device->Pair(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&](std::optional<BluetoothDevice::ConnectErrorCode> error_code) {
            EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, error_code);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_FALSE(device->IsPaired());
  EXPECT_FALSE(device->ExpectingPinCode());
}

// Tests that cancelling the pairing does not result in a paired device.
TEST_P(BluetoothTestWinrt, DevicePairRequestPinCodeCancelPairing) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_FALSE(device->IsPaired());
  EXPECT_FALSE(device->ExpectingPinCode());

  SimulatePairingPinCode(device, "123456");
  StrictMock<MockPairingDelegate> pairing_delegate;

  EXPECT_CALL(pairing_delegate, RequestPinCode)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        ScheduleAsynchronousCancelPairing(device);
      });

  base::RunLoop run_loop;
  device->Pair(
      &pairing_delegate,
      base::BindLambdaForTesting(
          [&](std::optional<BluetoothDevice::ConnectErrorCode> error_code) {
            EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, error_code);
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_FALSE(device->IsPaired());
  EXPECT_FALSE(device->ExpectingPinCode());
}

TEST_P(BluetoothTestWinrt, DevicePairRequestConfirmOnlyAccept) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();

  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_FALSE(device->IsPaired());

  SimulateConfirmOnly(device);
  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, AuthorizePairing)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        device->ConfirmPairing();
      });

  base::test::TestFuture<std::optional<BluetoothDevice::ConnectErrorCode>>
      error_code_future;
  device->Pair(&pairing_delegate, error_code_future.GetCallback());

  EXPECT_FALSE(error_code_future.Get().has_value());
  EXPECT_TRUE(device->IsPaired());
}

TEST_P(BluetoothTestWinrt, DevicePairRequestConfirmOnlyCancel) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();

  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_FALSE(device->IsPaired());

  SimulateConfirmOnly(device);
  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, AuthorizePairing)
      .WillOnce([](BluetoothDevice* device) {
        ASSERT_NE(device, nullptr);
        ScheduleAsynchronousCancelPairing(device);
      });

  base::test::TestFuture<std::optional<BluetoothDevice::ConnectErrorCode>>
      error_code_future;
  device->Pair(&pairing_delegate, error_code_future.GetCallback());

  EXPECT_EQ(error_code_future.Get(), BluetoothDevice::ERROR_AUTH_CANCELED);
  EXPECT_FALSE(device->IsPaired());
}

TEST_P(BluetoothTestWinrt, DevicePairRequestConfirmPinAccept) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();

  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_FALSE(device->IsPaired());

  SimulateDisplayPin(device, "123456");
  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, ConfirmPasskey)
      .WillOnce([](BluetoothDevice* device, uint32_t passkey) {
        ASSERT_NE(device, nullptr);
        ASSERT_EQ(passkey, 123456u);
        device->ConfirmPairing();
      });

  base::test::TestFuture<std::optional<BluetoothDevice::ConnectErrorCode>>
      error_code_future;
  device->Pair(&pairing_delegate, error_code_future.GetCallback());

  EXPECT_FALSE(error_code_future.Get().has_value());
  EXPECT_TRUE(device->IsPaired());
}

TEST_P(BluetoothTestWinrt, DevicePairRequestConfirmPinCancel) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();

  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_FALSE(device->IsPaired());

  SimulateDisplayPin(device, "123456");
  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, ConfirmPasskey)
      .WillOnce([](BluetoothDevice* device, uint32_t passkey) {
        ASSERT_NE(device, nullptr);
        ASSERT_EQ(passkey, 123456u);
        ScheduleAsynchronousCancelPairing(device);
      });

  base::test::TestFuture<std::optional<BluetoothDevice::ConnectErrorCode>>
      error_code_future;
  device->Pair(&pairing_delegate, error_code_future.GetCallback());

  EXPECT_EQ(error_code_future.Get(), BluetoothDevice::ERROR_AUTH_CANCELED);
  EXPECT_FALSE(device->IsPaired());
}

TEST_P(BluetoothTestWinrt, DevicePairRequestConfirmPinLeadingZeroAccept) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();

  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_FALSE(device->IsPaired());

  SimulateDisplayPin(device, "000001");
  StrictMock<MockPairingDelegate> pairing_delegate;
  EXPECT_CALL(pairing_delegate, ConfirmPasskey)
      .WillOnce([](BluetoothDevice* device, uint32_t passkey) {
        ASSERT_NE(device, nullptr);
        ASSERT_EQ(passkey, 1u);
        device->ConfirmPairing();
      });

  base::test::TestFuture<std::optional<BluetoothDevice::ConnectErrorCode>>
      error_code_future;
  device->Pair(&pairing_delegate, error_code_future.GetCallback());

  EXPECT_FALSE(error_code_future.Get().has_value());
  EXPECT_TRUE(device->IsPaired());
}

TEST_P(BluetoothTestWinrt, DevicePairRequestConfirmPinInvalid) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();

  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_FALSE(device->IsPaired());

  SimulateDisplayPin(device, "1000000");
  StrictMock<MockPairingDelegate> pairing_delegate;

  EXPECT_CALL(pairing_delegate, ConfirmPasskey).Times(0);

  base::test::TestFuture<std::optional<BluetoothDevice::ConnectErrorCode>>
      error_code_future;
  device->Pair(&pairing_delegate, error_code_future.GetCallback());

  EXPECT_EQ(error_code_future.Get(), BluetoothDevice::ERROR_AUTH_FAILED);
  EXPECT_FALSE(device->IsPaired());
}
#endif  // BUILDFLAG(IS_WIN)

// Verifies basic device properties, e.g. GetAddress, GetName, ...
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, LowEnergyDeviceProperties) {
#else
TEST_F(BluetoothTest, LowEnergyDeviceProperties) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);
  ASSERT_TRUE(device);
// Bluetooth class information for BLE device is not available on Windows.
#if !BUILDFLAG(IS_WIN)
  EXPECT_EQ(0x1F00u, device->GetBluetoothClass());
#endif
  EXPECT_EQ(kTestDeviceAddress1, device->GetAddress());
  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());
  EXPECT_EQ(base::UTF8ToUTF16(kTestDeviceName), device->GetNameForDisplay());
  EXPECT_FALSE(device->IsPaired());
  UUIDSet uuids = device->GetUUIDs();
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID(kTestUUIDGenericAccess)));
  EXPECT_TRUE(base::Contains(uuids, BluetoothUUID(kTestUUIDGenericAttribute)));
}

// Verifies that the device name can be populated by later advertisement
// packets and is persistent.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, LowEnergyDeviceNameDelayed) {
#else
// This test does not yet pass on any other platform.
TEST_F(BluetoothTest, DISABLED_LowEnergyDeviceNameDelayed) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ASSERT_TRUE(device);
  // GetName() returns a std::optional<std:string> however some backends still
  // return an empty string rather than nullopt when no name is available.
  EXPECT_TRUE(!device->GetName().has_value() || device->GetName()->empty());

  SimulateLowEnergyDevice(1);
  EXPECT_EQ(base::UTF8ToUTF16(kTestDeviceName), device->GetNameForDisplay());
}

// Device with no advertised Service UUIDs.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, LowEnergyDeviceNoUUIDs) {
#else
TEST_F(BluetoothTest, LowEnergyDeviceNoUUIDs) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ASSERT_TRUE(device);
  UUIDSet uuids = device->GetUUIDs();
  EXPECT_EQ(0u, uuids.size());
}

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
#define MAYBE_GetServiceDataUUIDs_GetServiceDataForUUID \
  GetServiceDataUUIDs_GetServiceDataForUUID
#else
#define MAYBE_GetServiceDataUUIDs_GetServiceDataForUUID \
  DISABLED_GetServiceDataUUIDs_GetServiceDataForUUID
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, GetServiceDataUUIDs_GetServiceDataForUUID) {
#else
TEST_F(BluetoothTest, MAYBE_GetServiceDataUUIDs_GetServiceDataForUUID) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();

#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/41309944): Remove #if once StartLowEnergyDiscoverySession is
  // implemented for bluez.
  StartLowEnergyDiscoverySession();
#endif  // !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)

  // Receive Advertisement with empty service data.
  BluetoothDevice* device1 = SimulateLowEnergyDevice(4);
  EXPECT_FALSE(device1->GetAdvertisingDataFlags().has_value());
  EXPECT_TRUE(device1->GetServiceData().empty());
  EXPECT_TRUE(device1->GetServiceDataUUIDs().empty());
  EXPECT_TRUE(device1->GetManufacturerData().empty());

  // Receive Advertisement with service data.
  BluetoothDevice* device2 = SimulateLowEnergyDevice(1);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(device2->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x04, device2->GetAdvertisingDataFlags().value());
#endif
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {1}}}),
            device2->GetServiceData());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDHeartRate)}),
            device2->GetServiceDataUUIDs());
  EXPECT_EQ(std::vector<uint8_t>({1}),
            *device2->GetServiceDataForUUID(BluetoothUUID(kTestUUIDHeartRate)));
  EXPECT_EQ(std::vector<uint8_t>({1, 2, 3, 4}),
            *device2->GetManufacturerDataForID(kTestManufacturerId));
  // Receive Advertisement with no flags and no service and manufacturer data.
  SimulateLowEnergyDevice(3);

// TODO(crbug.com/41310506): Remove #if once the BlueZ caching behavior is
// changed.
#if (BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)) && \
    !defined(USE_CAST_BLUETOOTH_ADAPTER)
  // On ChromeOS and Linux, BlueZ persists all service data meaning if
  // a device stops advertising service data for a UUID, BlueZ will
  // still return the cached value for that UUID.
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {1}}}),
            device2->GetServiceData());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDHeartRate)}),
            device2->GetServiceDataUUIDs());
  EXPECT_EQ(std::vector<uint8_t>({1}),
            *device2->GetServiceDataForUUID(BluetoothUUID(kTestUUIDHeartRate)));
#else
  EXPECT_FALSE(device2->GetAdvertisingDataFlags().has_value());
  EXPECT_TRUE(device2->GetServiceData().empty());
  EXPECT_TRUE(device2->GetServiceDataUUIDs().empty());
  EXPECT_TRUE(device2->GetManufacturerData().empty());
  EXPECT_EQ(nullptr,
            device2->GetServiceDataForUUID(BluetoothUUID(kTestUUIDHeartRate)));
#endif

  // Receive Advertisement with new service data and empty manufacturer data.
  SimulateLowEnergyDevice(2);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(device2->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x05, device2->GetAdvertisingDataFlags().value());
#endif
  EXPECT_EQ(ServiceDataMap(
                {{BluetoothUUID(kTestUUIDHeartRate), std::vector<uint8_t>({})},
                 {BluetoothUUID(kTestUUIDImmediateAlert), {0, 2}}}),
            device2->GetServiceData());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDHeartRate),
                     BluetoothUUID(kTestUUIDImmediateAlert)}),
            device2->GetServiceDataUUIDs());
  EXPECT_EQ(std::vector<uint8_t>({}),
            *device2->GetServiceDataForUUID(BluetoothUUID(kTestUUIDHeartRate)));
  EXPECT_EQ(std::vector<uint8_t>({}),
            *device2->GetManufacturerDataForID(kTestManufacturerId));
  EXPECT_EQ(
      std::vector<uint8_t>({0, 2}),
      *device2->GetServiceDataForUUID(BluetoothUUID(kTestUUIDImmediateAlert)));

#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/41309944): Remove #if once StartLowEnergyDiscoverySession is
  // implemented for bluez.
  // Stop discovery.
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());

  EXPECT_FALSE(device2->GetAdvertisingDataFlags().has_value());
  EXPECT_TRUE(device2->GetServiceData().empty());
  EXPECT_TRUE(device2->GetServiceDataUUIDs().empty());
  EXPECT_EQ(nullptr,
            device2->GetServiceDataForUUID(BluetoothUUID(kTestUUIDHeartRate)));
  EXPECT_EQ(nullptr, device2->GetServiceDataForUUID(
                         BluetoothUUID(kTestUUIDImmediateAlert)));
#endif  // !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_AdvertisementData_Discovery AdvertisementData_Discovery
#else
#define MAYBE_AdvertisementData_Discovery DISABLED_AdvertisementData_Discovery
#endif
// Tests that the Advertisement Data fields are correctly updated during
// discovery.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, AdvertisementData_Discovery) {
#else
TEST_F(BluetoothTest, MAYBE_AdvertisementData_Discovery) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  // Start Discovery Session and receive Advertisement, should
  // not notify of device changed because the device is new.
  //  - GetInquiryRSSI: Should return the packet's rssi.
  //  - GetAdvertisingDataFlags: Should return advertised flags.
  //  - GetUUIDs: Should return Advertised UUIDs.
  //  - GetServiceData: Should return advertised Service Data.
  //  - GetInquiryTxPower: Should return the packet's advertised Tx Power.
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(0, observer.device_changed_count());

  EXPECT_EQ(ToInt8(TestRSSI::LOWEST), device->GetInquiryRSSI().value());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x04, device->GetAdvertisingDataFlags().value());
#endif
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {1}}}),
            device->GetServiceData());
  EXPECT_EQ(ManufacturerDataMap({{kTestManufacturerId, {1, 2, 3, 4}}}),
            device->GetManufacturerData());
  EXPECT_EQ(ToInt8(TestTxPower::LOWEST), device->GetInquiryTxPower().value());

  // Receive Advertisement with no flags, no UUIDs, Service Data, or Tx Power,
  // should notify device changed.
  //  - GetInquiryRSSI: Should return packet's rssi.
  //  - GetAdvertisingDataFlags: Should return nullopt because of no flags.
  //  - GetUUIDs: Should return no UUIDs.
  //  - GetServiceData: Should return empty map.
  //  - GetInquiryTxPower: Should return nullopt because of no Tx Power.
  SimulateLowEnergyDevice(3);
  EXPECT_EQ(1, observer.device_changed_count());

  EXPECT_EQ(ToInt8(TestRSSI::LOW), device->GetInquiryRSSI().value());
  EXPECT_FALSE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_TRUE(device->GetUUIDs().empty());
  EXPECT_TRUE(device->GetServiceData().empty());
  EXPECT_TRUE(device->GetManufacturerData().empty());
  EXPECT_FALSE(device->GetInquiryTxPower());

  // Receive Advertisement with different UUIDs, Service Data, and Tx Power,
  // should notify device changed.
  //  - GetInquiryRSSI: Should return last packet's rssi.
  //  - GetAdvertisingDataFlags: Should return last advertised flags.
  //  - GetUUIDs: Should return latest Advertised UUIDs.
  //  - GetServiceData: Should return last advertised Service Data.
  //  - GetInquiryTxPower: Should return last advertised Tx Power.
  SimulateLowEnergyDevice(2);
  EXPECT_EQ(2, observer.device_changed_count());

  EXPECT_EQ(ToInt8(TestRSSI::LOWER), device->GetInquiryRSSI().value());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x05, device->GetAdvertisingDataFlags().value());
#endif
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDImmediateAlert),
                     BluetoothUUID(kTestUUIDLinkLoss)}),
            device->GetUUIDs());

  EXPECT_EQ(ServiceDataMap(
                {{BluetoothUUID(kTestUUIDHeartRate), std::vector<uint8_t>({})},
                 {BluetoothUUID(kTestUUIDImmediateAlert), {0, 2}}}),
            device->GetServiceData());
  EXPECT_EQ(ManufacturerDataMap({{kTestManufacturerId, {}}}),
            device->GetManufacturerData());
  EXPECT_EQ(ToInt8(TestTxPower::LOWER), device->GetInquiryTxPower().value());

  // Stop discovery session, should notify of device changed.
  //  - GetInquiryRSSI: Should return nullopt because we are no longer
  //    discovering.
  //  - GetAdvertisingDataFlags: Should return no flags.
  //  - GetUUIDs: Should not return any UUIDs.
  //  - GetServiceData: Should return empty map.
  //  - GetMAnufacturerData: Should return empty map.
  //  - GetInquiryTxPower: Should return nullopt because we are no longer
  //    discovering.
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());

  EXPECT_EQ(3, observer.device_changed_count());

  EXPECT_FALSE(device->GetInquiryRSSI());
  EXPECT_FALSE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_TRUE(device->GetUUIDs().empty());
  EXPECT_TRUE(device->GetServiceData().empty());
  EXPECT_TRUE(device->GetManufacturerData().empty());
  EXPECT_FALSE(device->GetInquiryTxPower());

  // Discover the device again with different UUIDs, should notify of device
  // changed.
  //  - GetInquiryRSSI: Should return last packet's rssi.
  //  - GetAdvertisingDataFlags: Should return last advertised flags.
  //  - GetUUIDs: Should return only the latest Advertised UUIDs.
  //  - GetServiceData: Should return last advertise Service Data.
  //  - GetInquiryTxPower: Should return last advertised Tx Power.
  StartLowEnergyDiscoverySession();
  device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(4, observer.device_changed_count());

  EXPECT_EQ(ToInt8(TestRSSI::LOWEST), device->GetInquiryRSSI().value());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x04, device->GetAdvertisingDataFlags().value());
#endif
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {1}}}),
            device->GetServiceData());
  EXPECT_EQ(ManufacturerDataMap({{kTestManufacturerId, {1, 2, 3, 4}}}),
            device->GetManufacturerData());
  EXPECT_EQ(ToInt8(TestTxPower::LOWEST), device->GetInquiryTxPower().value());
}

// TODO(dougt) As I turn on new platforms for WebBluetooth Scanning,
// I will relax this #ifdef
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
#define MAYBE_DeviceAdvertisementReceived DeviceAdvertisementReceived
#else
#define MAYBE_DeviceAdvertisementReceived DISABLED_DeviceAdvertisementReceived
#endif
// Tests that the Bluetooth adapter observer is notified when a device
// advertisement is received.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, DeviceAdvertisementReceived) {
#else
TEST_F(BluetoothTest, MAYBE_DeviceAdvertisementReceived) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  StartLowEnergyDiscoverySession();
  SimulateLowEnergyDevice(1);

  ASSERT_EQ(1, observer.device_advertisement_raw_received_count());
  EXPECT_EQ(kTestDeviceName, observer.last_device_name().value_or(""));
  EXPECT_EQ(kTestDeviceName, observer.last_advertisement_name().value_or(""));
  EXPECT_EQ(static_cast<int>(TestRSSI::LOWEST),
            observer.last_rssi().value_or(-1));
  EXPECT_EQ(static_cast<int>(TestTxPower::LOWEST),
            observer.last_tx_power().value_or(-1));

  // BluetoothDevice::GetAppearance() is not implemented on all platforms.
  // TODO(crbug.com/41240161): Check this property when it is implemented.

  const device::BluetoothDevice::UUIDList kTestAdvertisedUUIDs = {
      BluetoothUUID(kTestUUIDGenericAccess),
      BluetoothUUID(kTestUUIDGenericAttribute)};
  EXPECT_EQ(kTestAdvertisedUUIDs, observer.last_advertised_uuids());

  const device::BluetoothDevice::ServiceDataMap kTestServiceDataMap = {
      {BluetoothUUID(kTestUUIDHeartRate), {1}}};
  EXPECT_EQ(kTestServiceDataMap, observer.last_service_data_map());

  const device::BluetoothDevice::ManufacturerDataMap kTestManufacturerDataMap =
      {{kTestManufacturerId, {1, 2, 3, 4}}};
  EXPECT_EQ(kTestManufacturerDataMap, observer.last_manufacturer_data_map());

  // Double check that we can receive another advertisement.
  SimulateLowEnergyDevice(2);
  EXPECT_EQ(2, observer.device_advertisement_raw_received_count());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_GetUUIDs_Connection GetUUIDs_Connection
#else
#define MAYBE_GetUUIDs_Connection DISABLED_GetUUIDs_Connection
#endif
// Tests Advertisement Data is updated correctly during a connection.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, GetUUIDs_Connection) {
#else
TEST_F(BluetoothTest, MAYBE_GetUUIDs_Connection) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));

  // Connect to the device.
  //  - GetUUIDs: Should return no UUIDs because Services have not been
  //    discovered.
  ASSERT_TRUE(ConnectGatt(device));
  ASSERT_TRUE(device->IsConnected());

  EXPECT_TRUE(device->GetUUIDs().empty());

  observer.Reset();

  // Discover services, should notify of device changed.
  //  - GetUUIDs: Should return the device's services' UUIDs.
  std::vector<std::string> services;
  services.push_back(kTestUUIDGenericAccess);
  SimulateGattServicesDiscovered(device, services);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, observer.device_changed_count());

  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess)}),
            device->GetUUIDs());

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  // TODO(ortuno): Enable in Android and classic Windows.
  // Android and Windows don't yet support service changed events.
  // http://crbug.com/548280
  // http://crbug.com/579202

  observer.Reset();

  // Notify of services changed, should notify of device changed.
  //  - GetUUIDs: Should return no UUIDs because we no longer know what services
  //    the device has.
  SimulateGattServicesChanged(device);

  ASSERT_FALSE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_TRUE(device->GetUUIDs().empty());

  // Services discovered again, should notify of device changed.
  //  - GetUUIDs: Should return Service UUIDs.
  SimulateGattServicesDiscovered(device, {} /* services */);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess)}),
            device->GetUUIDs());

#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)

  observer.Reset();

  // Disconnect, should notify device changed.
  //  - GetUUIDs: Should return no UUIDs since we no longer know what services
  //    the device holds and notify of device changed.
  gatt_connections_[0]->Disconnect();
  SimulateGattDisconnection(device);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(device->IsGattConnected());

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_TRUE(device->GetUUIDs().empty());
}

#if BUILDFLAG(IS_APPLE)
// Tests that receiving 2 notifications in a row from macOS that services has
// changed is handled correctly. Each notification should generate a
// notification that the gatt device has changed, and each notification should
// ask to macOS to scan for services. Only after the second service scan is
// received, the device changed notification should be sent and the
// characteristic discovery procedure should be started.
// Android: This test doesn't apply to Android because there is no services
// changed event that could arrive during a discovery procedure.
TEST_F(BluetoothTest, TwoPendingServiceDiscoveryRequests) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  BluetoothDevice* device = SimulateLowEnergyDevice(1);
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_FALSE(device->IsGattServicesDiscoveryComplete());

  observer.Reset();
  SimulateGattServicesChanged(device);
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_FALSE(device->IsGattServicesDiscoveryComplete());

  // First system call to
  // -[id<CBPeripheralDelegate> peripheral:didDiscoverServices:] using
  // SimulateDidDiscoverServicesMac().
  observer.Reset();
  AddServicesToDeviceMac(device, {kTestUUIDHeartRate});
  SimulateDidDiscoverServicesMac(device);
  EXPECT_EQ(0, observer.device_changed_count());
  EXPECT_FALSE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(gatt_characteristic_discovery_attempts_, 0);

  // Second system call to
  // -[id<CBPeripheralDelegate> peripheral:didDiscoverServices:] using the
  // generic call to SimulateGattServicesDiscovered(). This method triggers
  // the full discovery cycles (services, characteristics and descriptors),
  // which includes -[id<CBPeripheralDelegate> peripheral:didDiscoverServices:].
  SimulateGattServicesDiscovered(
      device, std::vector<std::string>({kTestUUIDImmediateAlert}));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());
  // Characteristics are discovered once for each service.
  EXPECT_EQ(gatt_characteristic_discovery_attempts_, 2);

  EXPECT_EQ(2u, device->GetGattServices().size());
}

// Simulate an unexpected call to -[id<CBPeripheralDelegate>
// peripheral:didDiscoverServices:]. This should not happen, but if it does
// (buggy device?), a discovery cycle should be done.
TEST_F(BluetoothTest, ExtraDidDiscoverServicesCall) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  BluetoothDevice* device = SimulateLowEnergyDevice(1);
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_FALSE(device->IsGattServicesDiscoveryComplete());

  // Legitimate system call to
  // -[id<CBPeripheralDelegate> peripheral:didDiscoverServices:].
  observer.Reset();
  SimulateGattServicesDiscovered(
      device, std::vector<std::string>({kTestUUIDHeartRate}));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(gatt_characteristic_discovery_attempts_, 1);
  EXPECT_EQ(1u, device->GetGattServices().size());

  // Unexpected system call to
  // -[id<CBPeripheralDelegate> peripheral:didDiscoverServices:]:
  // This system call is expected only once after -[CBCentralManager
  // discoverServices:]. The call to -[CBCentralManager discoverServices:] and
  // its answer with -[id<CBPeripheralDelegate> peripheral:didDiscoverServices:]
  // is done with SimulateGattServicesDiscovered(). So a second system call to
  // -[id<CBPeripheralDelegate> peripheral:didDiscoverServices:] is not expected
  // and should be ignored.
  AddServicesToDeviceMac(device, {kTestUUIDImmediateAlert});
  SimulateDidDiscoverServicesMac(device);
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());

  EXPECT_EQ(1u, device->GetGattServices().size());
}
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_AdvertisementData_DiscoveryDuringConnection \
  AdvertisementData_DiscoveryDuringConnection
#else
#define MAYBE_AdvertisementData_DiscoveryDuringConnection \
  DISABLED_AdvertisementData_DiscoveryDuringConnection
#endif
// Tests Advertisement Data is updated correctly when we start discovery
// during a connection.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, AdvertisementData_DiscoveryDuringConnection) {
#else
TEST_F(BluetoothTest, MAYBE_AdvertisementData_DiscoveryDuringConnection) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());
  ASSERT_EQ(0u, device->GetUUIDs().size());
  discovery_sessions_.clear();

  // Connect.
  ASSERT_TRUE(ConnectGatt(device));
  ASSERT_TRUE(device->IsConnected());

  observer.Reset();

  // Start Discovery and receive advertisement during connection,
  // should notify of device changed.
  //  - GetInquiryRSSI: Should return the packet's rssi.
  //  - GetAdvertisingDataFlags: Should return last advertised flags.
  //  - GetUUIDs: Should return only Advertised UUIDs since services haven't
  //    been discovered yet.
  //  - GetServiceData: Should return last advertised Service Data.
  //  - GetInquiryTxPower: Should return the packet's advertised Tx Power.
  StartLowEnergyDiscoverySession();
  ASSERT_TRUE(adapter_->IsDiscovering());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());
  device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(1, observer.device_changed_count());

  EXPECT_EQ(ToInt8(TestRSSI::LOWEST), device->GetInquiryRSSI().value());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x04, device->GetAdvertisingDataFlags().value());
#endif
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {1}}}),
            device->GetServiceData());
  EXPECT_EQ(ToInt8(TestTxPower::LOWEST), device->GetInquiryTxPower().value());

  // Discover services, should notify of device changed.
  //  - GetUUIDs: Should return both Advertised UUIDs and Service UUIDs.
  std::vector<std::string> services;
  services.push_back(kTestUUIDHeartRate);
  SimulateGattServicesDiscovered(device, services);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, observer.device_changed_count());

  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute),
                     BluetoothUUID(kTestUUIDHeartRate)}),
            device->GetUUIDs());

  // Receive advertisement again, notify of device changed.
  //  - GetInquiryRSSI: Should return last packet's rssi.
  //  - GetAdvertisingDataFlags: Should return last advertised flags.
  //  - GetUUIDs: Should return only new Advertised UUIDs and Service UUIDs.
  //  - GetServiceData: Should return last advertised Service Data.
  //  - GetInquiryTxPower: Should return the last packet's advertised Tx Power.
  device = SimulateLowEnergyDevice(2);

  EXPECT_EQ(3, observer.device_changed_count());
  EXPECT_EQ(ToInt8(TestRSSI::LOWER), device->GetInquiryRSSI().value());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x05, device->GetAdvertisingDataFlags().value());
#endif
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDLinkLoss),
                     BluetoothUUID(kTestUUIDImmediateAlert),
                     BluetoothUUID(kTestUUIDHeartRate)}),
            device->GetUUIDs());
  EXPECT_EQ(ServiceDataMap(
                {{BluetoothUUID(kTestUUIDHeartRate), std::vector<uint8_t>({})},
                 {BluetoothUUID(kTestUUIDImmediateAlert), {0, 2}}}),
            device->GetServiceData());
  EXPECT_EQ(ToInt8(TestTxPower::LOWER), device->GetInquiryTxPower().value());

  // Stop discovery session, should notify of device changed.
  //  - GetInquiryRSSI: Should return nullopt because we are no longer
  //    discovering.
  //  - GetAdvertisingDataFlags: Should return no flags since we are no longer
  //    discovering.
  //  - GetUUIDs: Should only return Service UUIDs.
  //  - GetServiceData: Should return an empty map since we are no longer
  //    discovering.
  //  - GetInquiryTxPower: Should return nullopt because we are no longer
  //    discovering.
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(adapter_->IsDiscovering());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());

  EXPECT_EQ(4, observer.device_changed_count());
  EXPECT_FALSE(device->GetInquiryRSSI());
  EXPECT_FALSE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDHeartRate)}), device->GetUUIDs());
  EXPECT_EQ(ServiceDataMap(), device->GetServiceData());
  EXPECT_FALSE(device->GetInquiryTxPower());

  // Disconnect device, should notify of device changed.
  //  - GetUUIDs: Should return no UUIDs.
  gatt_connections_[0]->Disconnect();
  SimulateGattDisconnection(device);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(device->IsGattConnected());

  EXPECT_EQ(5, observer.device_changed_count());

  EXPECT_TRUE(device->GetUUIDs().empty());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_AdvertisementData_ConnectionDuringDiscovery \
  AdvertisementData_ConnectionDuringDiscovery
#else
#define MAYBE_AdvertisementData_ConnectionDuringDiscovery \
  DISABLED_AdvertisementData_ConnectionDuringDiscovery
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, AdvertisementData_ConnectionDuringDiscovery) {
#else
TEST_F(BluetoothTest, MAYBE_AdvertisementData_ConnectionDuringDiscovery) {
#endif
  // Tests that the Advertisement Data is correctly updated when
  // the device connects during discovery.
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);

  // Start discovery session and receive and advertisement. No device changed
  // notification because it's a new device.
  //  - GetInquiryRSSI: Should return the packet's rssi.
  //  - GetAdvertisingDataFlags: Should return advertised flags.
  //  - GetUUIDs: Should return Advertised UUIDs.
  //  - GetServiceData: Should return advertised Service Data.
  //  - GetInquiryTxPower: Should return the packet's advertised Tx Power.
  StartLowEnergyDiscoverySession();
  ASSERT_TRUE(adapter_->IsDiscovering());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(0, observer.device_changed_count());
  EXPECT_EQ(ToInt8(TestRSSI::LOWEST), device->GetInquiryRSSI().value());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x04, device->GetAdvertisingDataFlags().value());
#endif
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {1}}}),
            device->GetServiceData());
  EXPECT_EQ(ToInt8(TestTxPower::LOWEST), device->GetInquiryTxPower().value());

  // Connect, should notify of device changed.
  //  - GetUUIDs: Should return Advertised UUIDs even before GATT Discovery.
  ASSERT_TRUE(ConnectGatt(device));
  ASSERT_TRUE(device->IsConnected());

  observer.Reset();
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());

  // Receive Advertisement with new UUIDs, should notify of device changed.
  //  - GetInquiryRSSI: Should return the packet's rssi.
  //  - GetAdvertisingDataFlags: Should return advertised flags.
  //  - GetUUIDs: Should return new Advertised UUIDs.
  //  - GetServiceData: Should return new advertised Service Data.
  //  - GetInquiryTxPower: Should return the packet's advertised Tx Power.
  device = SimulateLowEnergyDevice(2);

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(ToInt8(TestRSSI::LOWER), device->GetInquiryRSSI().value());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x05, device->GetAdvertisingDataFlags().value());
#endif
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDLinkLoss),
                     BluetoothUUID(kTestUUIDImmediateAlert)}),
            device->GetUUIDs());
  EXPECT_EQ(ServiceDataMap(
                {{BluetoothUUID(kTestUUIDHeartRate), std::vector<uint8_t>({})},
                 {BluetoothUUID(kTestUUIDImmediateAlert), {0, 2}}}),
            device->GetServiceData());
  EXPECT_EQ(ToInt8(TestTxPower::LOWER), device->GetInquiryTxPower().value());

  // Discover Services, should notify of device changed.
  //  - GetUUIDs: Should return Advertised UUIDs and Service UUIDs.
  std::vector<std::string> services;
  services.push_back(kTestUUIDHeartRate);
  SimulateGattServicesDiscovered(device, services);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, observer.device_changed_count());

  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDLinkLoss),
                     BluetoothUUID(kTestUUIDImmediateAlert),
                     BluetoothUUID(kTestUUIDHeartRate)}),
            device->GetUUIDs());

  // Disconnect, should notify of device changed.
  //  - GetInquiryRSSI: Should return last packet's rssi.
  //  - GetAdvertisingDataFlags: Should return same advertised flags.
  //  - GetUUIDs: Should return only Advertised UUIDs.
  //  - GetServiceData: Should still return same advertised Service Data.
  //  - GetInquiryTxPower: Should return the last packet's advertised Tx Power.
  gatt_connections_[0]->Disconnect();
  SimulateGattDisconnection(device);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(device->IsGattConnected());

  EXPECT_EQ(3, observer.device_changed_count());
  EXPECT_EQ(ToInt8(TestRSSI::LOWER), device->GetInquiryRSSI().value());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x05, device->GetAdvertisingDataFlags().value());
#endif
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDLinkLoss),
                     BluetoothUUID(kTestUUIDImmediateAlert)}),
            device->GetUUIDs());

  EXPECT_EQ(ServiceDataMap(
                {{BluetoothUUID(kTestUUIDHeartRate), std::vector<uint8_t>({})},
                 {BluetoothUUID(kTestUUIDImmediateAlert), {0, 2}}}),
            device->GetServiceData());
  EXPECT_EQ(ToInt8(TestTxPower::LOWER), device->GetInquiryTxPower().value());

  // Receive Advertisement with new UUIDs, should notify of device changed.
  //  - GetInquiryRSSI: Should return last packet's rssi.
  //  - GetAdvertisingDataFlags: Should return the new advertised flags.
  //  - GetUUIDs: Should return only new Advertised UUIDs.
  //  - GetServiceData: Should return only new advertised Service Data.
  //  - GetInquiryTxPower: Should return the last packet's advertised Tx Power.
  device = SimulateLowEnergyDevice(1);

  EXPECT_EQ(4, observer.device_changed_count());
  EXPECT_EQ(ToInt8(TestRSSI::LOWEST), device->GetInquiryRSSI().value());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_EQ(0x04, device->GetAdvertisingDataFlags().value());
#endif
  EXPECT_EQ(UUIDSet({BluetoothUUID(kTestUUIDGenericAccess),
                     BluetoothUUID(kTestUUIDGenericAttribute)}),
            device->GetUUIDs());
  EXPECT_EQ(ServiceDataMap({{BluetoothUUID(kTestUUIDHeartRate), {1}}}),
            device->GetServiceData());
  EXPECT_EQ(ToInt8(TestTxPower::LOWEST), device->GetInquiryTxPower().value());

  // Stop discovery session, should notify of device changed.
  //  - GetInquiryRSSI: Should return nullopt because we are no longer
  //    discovering.
  //  - GetAdvertisingDataFlags: Should return no advertised flags since we are
  //    no longer discovering.
  //  - GetUUIDs: Should return no UUIDs.
  //  - GetServiceData: Should return no UUIDs since we are no longer
  //    discovering.
  //  - GetInquiryTxPower: Should return nullopt because we are no longer
  //    discovering.
  discovery_sessions_[0]->Stop(GetCallback(Call::EXPECTED),
                               GetErrorCallback(Call::NOT_EXPECTED));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5, observer.device_changed_count());

  EXPECT_FALSE(device->GetInquiryRSSI());
  EXPECT_FALSE(device->GetAdvertisingDataFlags().has_value());
  EXPECT_TRUE(device->GetUUIDs().empty());
  EXPECT_TRUE(device->GetServiceData().empty());
  EXPECT_FALSE(device->GetInquiryTxPower());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_APPLE) || \
    BUILDFLAG(IS_LINUX)
#define MAYBE_GetName_NullName GetName_NullName
#else
#define MAYBE_GetName_NullName DISABLED_GetName_NullName
#endif
// GetName for Device with no name.
TEST_F(BluetoothTest, MAYBE_GetName_NullName) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();

// StartLowEnergyDiscoverySession is not yet implemented on ChromeOS|bluez,
// and is non trivial to implement. On ChromeOS, it is not essential for
// this test to operate, and so it is simply skipped. Android at least
// does require this step.
#if !BUILDFLAG(IS_CHROMEOS)
  StartLowEnergyDiscoverySession();
#endif  // !BUILDFLAG(IS_CHROMEOS)

  BluetoothDevice* device = SimulateLowEnergyDevice(5);
  EXPECT_FALSE(device->GetName());

  // The check below is not currently working on Android and Mac because the
  // GetAppearance() method is not implemented on those platforms.
  // TODO(crbug.com/41240161): Enable the check below when GetAppearance()
  // is implemented for Android and Mac.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_APPLE)
  EXPECT_EQ(device->GetNameForDisplay(),
            u"Unknown or Unsupported Device (01:00:00:90:1E:BE)");
#endif
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_CreateGattConnection CreateGattConnection
#else
#define MAYBE_CreateGattConnection DISABLED_CreateGattConnection
#endif
// Basic CreateGattConnection test.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, CreateGattConnection) {
#else
TEST_F(BluetoothTest, MAYBE_CreateGattConnection) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));

  ASSERT_EQ(1u, gatt_connections_.size());
  EXPECT_TRUE(device->IsGattConnected());
  EXPECT_TRUE(gatt_connections_[0]->IsConnected());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_DisconnectionNotifiesDeviceChanged \
  DisconnectionNotifiesDeviceChanged
#else
#define MAYBE_DisconnectionNotifiesDeviceChanged \
  DISABLED_DisconnectionNotifiesDeviceChanged
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, DisconnectionNotifiesDeviceChanged) {
#else
TEST_F(BluetoothTest, MAYBE_DisconnectionNotifiesDeviceChanged) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ASSERT_TRUE(ConnectGatt(device));

  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_TRUE(device->IsConnected());
  EXPECT_TRUE(device->IsGattConnected());

  SimulateDeviceBreaksConnection(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, observer.device_changed_count());
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsGattConnected());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_BluetoothGattConnection BluetoothGattConnection
#else
#define MAYBE_BluetoothGattConnection DISABLED_BluetoothGattConnection
#endif
// Creates BluetoothGattConnection instances and tests that the interface
// functions even when some Disconnect and the BluetoothDevice is destroyed.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, BluetoothGattConnection) {
#else
TEST_F(BluetoothTest, MAYBE_BluetoothGattConnection) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  std::string device_address = device->GetAddress();

  // CreateGattConnection
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  ASSERT_EQ(1u, gatt_connections_.size());
  EXPECT_TRUE(device->IsGattConnected());
  EXPECT_TRUE(gatt_connections_[0]->IsConnected());

  // Connect again once already connected.
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(0, gatt_connection_attempts_);
  ASSERT_EQ(3u, gatt_connections_.size());

  // Test GetDeviceAddress
  EXPECT_EQ(device_address, gatt_connections_[0]->GetDeviceAddress());

  // Test IsConnected
  EXPECT_TRUE(gatt_connections_[0]->IsConnected());
  EXPECT_TRUE(gatt_connections_[1]->IsConnected());
  EXPECT_TRUE(gatt_connections_[2]->IsConnected());

  // Disconnect & Delete connection objects. Device stays connected.
  gatt_connections_[0]->Disconnect();  // Disconnect first.
  gatt_connections_.pop_back();        // Delete last.
  EXPECT_FALSE(gatt_connections_[0]->IsConnected());
  EXPECT_TRUE(gatt_connections_[1]->IsConnected());
  EXPECT_TRUE(device->IsGattConnected());
  EXPECT_EQ(0, gatt_disconnection_attempts_);

  // Delete device, connection objects should all be disconnected.
  gatt_disconnection_attempts_ = 0;
  DeleteDevice(device);
  EXPECT_EQ(1, gatt_disconnection_attempts_);
  EXPECT_FALSE(gatt_connections_[0]->IsConnected());
  EXPECT_FALSE(gatt_connections_[1]->IsConnected());

  // Test GetDeviceAddress after device deleted.
  EXPECT_EQ(device_address, gatt_connections_[0]->GetDeviceAddress());
  EXPECT_EQ(device_address, gatt_connections_[1]->GetDeviceAddress());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_BluetoothGattConnection_ConnectWithMultipleOSConnections \
  BluetoothGattConnection_ConnectWithMultipleOSConnections
#else
#define MAYBE_BluetoothGattConnection_ConnectWithMultipleOSConnections \
  DISABLED_BluetoothGattConnection_ConnectWithMultipleOSConnections
#endif
// Calls CreateGattConnection then simulates multiple connections from platform.
TEST_F(BluetoothTest,
       MAYBE_BluetoothGattConnection_ConnectWithMultipleOSConnections) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  TestBluetoothAdapterObserver observer(adapter_);
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  // CreateGattConnection, & multiple connections from platform only invoke
  // callbacks once:
  ResetEventCounts();

  ASSERT_TRUE(
      ConnectGatt(device, /*service_uuid=*/std::nullopt,
                  base::BindLambdaForTesting([this](BluetoothDevice* device) {
                    SimulateGattConnection(device);
                    SimulateGattConnection(device);
                  })));

  EXPECT_EQ(1, gatt_discovery_attempts_);
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(1, gatt_connection_attempts_);
  EXPECT_TRUE(gatt_connections_[0]->IsConnected());

  // Become disconnected:
  SimulateGattDisconnection(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(gatt_connections_[0]->IsConnected());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_BluetoothGattConnection_AlreadyConnected \
  BluetoothGattConnection_AlreadyConnected
#else
#define MAYBE_BluetoothGattConnection_AlreadyConnected \
  DISABLED_BluetoothGattConnection_AlreadyConnected
#endif
// Calls CreateGattConnection after already connected.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, BluetoothGattConnection_AlreadyConnected) {
#else
TEST_F(BluetoothTest, MAYBE_BluetoothGattConnection_AlreadyConnected) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  // Be already connected:
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_TRUE(gatt_connections_[0]->IsConnected());

  // Then CreateGattConnection:
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(0, gatt_connection_attempts_);
  EXPECT_TRUE(gatt_connections_[1]->IsConnected());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_BluetoothGattConnection_NewConnectionLeavesPreviousDisconnected \
  BluetoothGattConnection_NewConnectionLeavesPreviousDisconnected
#else
#define MAYBE_BluetoothGattConnection_NewConnectionLeavesPreviousDisconnected \
  DISABLED_BluetoothGattConnection_NewConnectionLeavesPreviousDisconnected
#endif
// Creates BluetoothGattConnection after one exists that has disconnected.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt,
       BluetoothGattConnection_NewConnectionLeavesPreviousDisconnected) {
#else
TEST_F(BluetoothTest,
       MAYBE_BluetoothGattConnection_NewConnectionLeavesPreviousDisconnected) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  // Create connection:
  ASSERT_TRUE(ConnectGatt(device));

  // Disconnect connection:
  gatt_connections_[0]->Disconnect();
  SimulateGattDisconnection(device);
  base::RunLoop().RunUntilIdle();

  // Create 2nd connection:
  ASSERT_TRUE(ConnectGatt(device));

  EXPECT_FALSE(gatt_connections_[0]->IsConnected())
      << "The disconnected connection shouldn't become connected when another "
         "connection is created.";
  EXPECT_TRUE(gatt_connections_[1]->IsConnected());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_BluetoothGattConnection_DisconnectWhenObjectsDestroyed \
  BluetoothGattConnection_DisconnectWhenObjectsDestroyed
#else
#define MAYBE_BluetoothGattConnection_DisconnectWhenObjectsDestroyed \
  DISABLED_BluetoothGattConnection_DisconnectWhenObjectsDestroyed
#endif
// Deletes BluetoothGattConnection causing disconnection.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt,
       BluetoothGattConnection_DisconnectWhenObjectsDestroyed) {
#else
TEST_F(BluetoothTest,
       MAYBE_BluetoothGattConnection_DisconnectWhenObjectsDestroyed) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  // Create multiple connections and simulate connection complete:
  ASSERT_TRUE(
      ConnectGatt(device,
                  /*service_uuid=*/std::nullopt,
                  base::BindLambdaForTesting([this](BluetoothDevice* device) {
                    ConnectGatt(device);
                  })));
  EXPECT_EQ(2u, gatt_connections_.size());

  // Delete all CreateGattConnection objects, observe disconnection:
  ResetEventCounts();
  gatt_connections_.clear();
  EXPECT_EQ(1, gatt_disconnection_attempts_);
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_BluetoothGattConnection_DisconnectInProgress \
  BluetoothGattConnection_DisconnectInProgress
#else
#define MAYBE_BluetoothGattConnection_DisconnectInProgress \
  DISABLED_BluetoothGattConnection_DisconnectInProgress
#endif
// Starts process of disconnecting and then calls BluetoothGattConnection.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, BluetoothGattConnection_DisconnectInProgress) {
#else
TEST_F(BluetoothTest, MAYBE_BluetoothGattConnection_DisconnectInProgress) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  // Create multiple connections and simulate connection complete:
  ASSERT_TRUE(
      ConnectGatt(device,
                  /*service_uuid=*/std::nullopt,
                  base::BindLambdaForTesting([this](BluetoothDevice* device) {
                    ConnectGatt(device);
                  })));
  EXPECT_EQ(2u, gatt_connections_.size());

  // Disconnect all CreateGattConnection objects & create a new connection.
  // But, don't yet simulate the device disconnecting:
  ResetEventCounts();
  for (const auto& connection : gatt_connections_)
    connection->Disconnect();
  EXPECT_EQ(1, gatt_disconnection_attempts_);

  // Create a connection.
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(0, gatt_connection_attempts_);  // No connection attempt.
  EXPECT_FALSE(gatt_connections_.front()->IsConnected());
  EXPECT_TRUE(gatt_connections_.back()->IsConnected());

  // Actually disconnect:
  SimulateGattDisconnection(device);
  base::RunLoop().RunUntilIdle();
  for (const auto& connection : gatt_connections_)
    EXPECT_FALSE(connection->IsConnected());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_BluetoothGattConnection_SimulateDisconnect \
  BluetoothGattConnection_SimulateDisconnect
#else
#define MAYBE_BluetoothGattConnection_SimulateDisconnect \
  DISABLED_BluetoothGattConnection_SimulateDisconnect
#endif
// Calls CreateGattConnection but receives notice that the device disconnected
// before it ever connects.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, BluetoothGattConnection_SimulateDisconnect) {
#else
TEST_F(BluetoothTest, MAYBE_BluetoothGattConnection_SimulateDisconnect) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  ResetEventCounts();
  EXPECT_FALSE(
      ConnectGatt(device,
                  /*service_uuid=*/std::nullopt,
                  base::BindLambdaForTesting([this](BluetoothDevice* device) {
                    SimulateGattDisconnection(device);
                  })));

  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_code_);
  for (const auto& connection : gatt_connections_)
    EXPECT_FALSE(connection->IsConnected());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_BluetoothGattConnection_DisconnectGatt_SimulateConnect \
  BluetoothGattConnection_DisconnectGatt_SimulateConnect
#else
#define MAYBE_BluetoothGattConnection_DisconnectGatt_SimulateConnect \
  DISABLED_BluetoothGattConnection_DisconnectGatt_SimulateConnect
#endif
// Calls CreateGattConnection & DisconnectGatt, then simulates connection.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt,
       BluetoothGattConnection_DisconnectGatt_SimulateConnect) {
#else
TEST_F(BluetoothTest,
       MAYBE_BluetoothGattConnection_DisconnectGatt_SimulateConnect) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  ResetEventCounts();
  EXPECT_TRUE(
      ConnectGatt(device,
                  /*service_uuid=*/std::nullopt,
                  base::BindLambdaForTesting([this](BluetoothDevice* device) {
#if !BUILDFLAG(IS_WIN)
                    // On Windows there is currently no way to cancel a
                    // pending GATT connection from the caller's side.
                    device->DisconnectGatt();
#endif
                    SimulateGattConnection(device);
                  })));

#if !BUILDFLAG(IS_WIN)
  EXPECT_EQ(1, gatt_disconnection_attempts_);
#endif
  EXPECT_EQ(1, gatt_connection_attempts_);

  EXPECT_TRUE(gatt_connections_.back()->IsConnected());
  ResetEventCounts();
  SimulateGattDisconnection(device);
  base::RunLoop().RunUntilIdle();
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_BluetoothGattConnection_DisconnectGatt_SimulateDisconnect \
  BluetoothGattConnection_DisconnectGatt_SimulateDisconnect
#else
#define MAYBE_BluetoothGattConnection_DisconnectGatt_SimulateDisconnect \
  DISABLED_BluetoothGattConnection_DisconnectGatt_SimulateDisconnect
#endif
// Calls CreateGattConnection & DisconnectGatt, then simulates disconnection.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt,
       BluetoothGattConnection_DisconnectGatt_SimulateDisconnect) {
#else
TEST_F(BluetoothTest,
       MAYBE_BluetoothGattConnection_DisconnectGatt_SimulateDisconnect) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  ResetEventCounts();
  ASSERT_FALSE(
      ConnectGatt(device,
                  /*service_uuid=*/std::nullopt,
                  base::BindLambdaForTesting([this](BluetoothDevice* device) {
                    device->DisconnectGatt();
                    SimulateGattDisconnection(device);
                  })));
  EXPECT_EQ(1, gatt_connection_attempts_);
  EXPECT_EQ(
#if BUILDFLAG(IS_ANDROID)
      // Closing a GATT connection also disconnects on Android.
      // TODO(crbug.com/40670359): this value probably shouldn't be different on
      // Android.
      2,
#else
      1,
#endif
      gatt_disconnection_attempts_);
  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_code_);
  for (const auto& connection : gatt_connections_)
    EXPECT_FALSE(connection->IsConnected());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_BluetoothGattConnection_DisconnectGatt_Cleanup \
  BluetoothGattConnection_DisconnectGatt_Cleanup
#else
#define MAYBE_BluetoothGattConnection_DisconnectGatt_Cleanup \
  DISABLED_BluetoothGattConnection_DisconnectGatt_Cleanup
#endif
// Calls CreateGattConnection & DisconnectGatt, then checks that gatt services
// have been cleaned up.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, BluetoothGattConnection_DisconnectGatt_Cleanup) {
#else
TEST_F(BluetoothTest, MAYBE_BluetoothGattConnection_DisconnectGatt_Cleanup) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  EXPECT_FALSE(device->IsConnected());

  // Connect to the device
  ResetEventCounts();
  TestBluetoothAdapterObserver observer(adapter_);
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_TRUE(device->IsConnected());

  // Discover services
  SimulateGattServicesDiscovered(
      device,
      std::vector<std::string>({kTestUUIDGenericAccess, kTestUUIDHeartRate}));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(2u, device->GetGattServices().size());
  EXPECT_EQ(1, observer.gatt_services_discovered_count());

  // Disconnect from the device
  device->DisconnectGatt();
  SimulateGattDisconnection(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(0u, device->GetGattServices().size());

  // Verify that the device can be connected to again
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_TRUE(device->IsConnected());

  // Verify that service discovery can be done again
  SimulateGattServicesDiscovered(
      device, std::vector<std::string>({kTestUUIDGenericAttribute}));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(1u, device->GetGattServices().size());
  EXPECT_EQ(2, observer.gatt_services_discovered_count());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_BluetoothGattConnection_ErrorAfterConnection \
  BluetoothGattConnection_ErrorAfterConnection
#else
#define MAYBE_BluetoothGattConnection_ErrorAfterConnection \
  DISABLED_BluetoothGattConnection_ErrorAfterConnection
#endif
// Calls CreateGattConnection, but simulate errors connecting. Also, verifies
// multiple errors should only invoke callbacks once.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, BluetoothGattConnection_ErrorAfterConnection) {
#else
TEST_F(BluetoothTest, MAYBE_BluetoothGattConnection_ErrorAfterConnection) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);

  ResetEventCounts();
  EXPECT_FALSE(ConnectGatt(
      device,
      /*service_uuid=*/std::nullopt,
      base::BindLambdaForTesting([this](BluetoothDevice* device) {
        SimulateGattConnectionError(device, BluetoothDevice::ERROR_AUTH_FAILED);
        SimulateGattConnectionError(device, BluetoothDevice::ERROR_FAILED);
      })));

  EXPECT_EQ(1, gatt_connection_attempts_);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
  // TODO(crbug.com/40452547): Change to ERROR_AUTH_FAILED. We should be getting
  // a callback only with the first error, but our android framework doesn't yet
  // support sending different errors.
  // On Windows, any GattConnectionError will result in ERROR_FAILED.
  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_code_);
#else
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_FAILED, last_connect_error_code_);
#endif
  for (const auto& connection : gatt_connections_)
    EXPECT_FALSE(connection->IsConnected());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
#define MAYBE_GattServices_ObserversCalls GattServices_ObserversCalls
#else
#define MAYBE_GattServices_ObserversCalls DISABLED_GattServices_ObserversCalls
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, GattServices_ObserversCalls) {
#else
TEST_F(BluetoothTest, MAYBE_GattServices_ObserversCalls) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  TestBluetoothAdapterObserver observer(adapter_);
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(1, gatt_discovery_attempts_);

  SimulateGattServicesDiscovered(
      device,
      std::vector<std::string>({kTestUUIDGenericAccess, kTestUUIDHeartRate}));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, observer.gatt_services_discovered_count());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE)
#define MAYBE_GattServicesDiscovered_Success GattServicesDiscovered_Success
#else
#define MAYBE_GattServicesDiscovered_Success \
  DISABLED_GattServicesDiscovered_Success
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, GattServicesDiscovered_Success) {
#else
TEST_F(BluetoothTest, MAYBE_GattServicesDiscovered_Success) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  TestBluetoothAdapterObserver observer(adapter_);
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(1, gatt_discovery_attempts_);
  EXPECT_EQ(0, observer.gatt_services_discovered_count());

  SimulateGattServicesDiscovered(
      device,
      std::vector<std::string>({kTestUUIDGenericAccess, kTestUUIDHeartRate}));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(1, observer.gatt_services_discovered_count());
  EXPECT_EQ(2u, device->GetGattServices().size());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_GattServicesDiscovered_AfterDeleted \
  GattServicesDiscovered_AfterDeleted
#else
#define MAYBE_GattServicesDiscovered_AfterDeleted \
  DISABLED_GattServicesDiscovered_AfterDeleted
#endif
// macOS: Not applicable: This can never happen because when
// the device gets destroyed the CBPeripheralDelegate is also destroyed
// and no more events are dispatched.
TEST_F(BluetoothTest, MAYBE_GattServicesDiscovered_AfterDeleted) {
  // Tests that we don't crash if services are discovered after
  // the device object is deleted.
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(1, gatt_discovery_attempts_);

  RememberDeviceForSubsequentAction(device);
  DeleteDevice(device);

  SimulateGattServicesDiscovered(
      nullptr /* use remembered device */,
      std::vector<std::string>({kTestUUIDGenericAccess, kTestUUIDHeartRate}));
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
#define MAYBE_GattServicesDiscoveredError_AfterDeleted \
  GattServicesDiscoveredError_AfterDeleted
#else
#define MAYBE_GattServicesDiscoveredError_AfterDeleted \
  DISABLED_GattServicesDiscoveredError_AfterDeleted
#endif
// macOS: Not applicable: This can never happen because when
// the device gets destroyed the CBPeripheralDelegate is also destroyed
// and no more events are dispatched.
TEST_F(BluetoothTest, MAYBE_GattServicesDiscoveredError_AfterDeleted) {
  // Tests that we don't crash if there was an error discoverying services
  // after the device object is deleted.
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(1, gatt_discovery_attempts_);

  RememberDeviceForSubsequentAction(device);
  DeleteDevice(device);

  SimulateGattServicesDiscoveryError(nullptr /* use remembered device */);
  base::RunLoop().RunUntilIdle();
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_GattServicesDiscovered_AfterDisconnection \
  GattServicesDiscovered_AfterDisconnection
#else
#define MAYBE_GattServicesDiscovered_AfterDisconnection \
  DISABLED_GattServicesDiscovered_AfterDisconnection
#endif
// Classic Windows does not support disconnection.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, GattServicesDiscovered_AfterDisconnection) {
#else
TEST_F(BluetoothTest, MAYBE_GattServicesDiscovered_AfterDisconnection) {
#endif
  // Tests that we don't crash if there was an error discovering services after
  // the device disconnects.
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(1, gatt_discovery_attempts_);

  SimulateDeviceBreaksConnection(device);
  base::RunLoop().RunUntilIdle();

  SimulateGattServicesDiscovered(
      device,
      std::vector<std::string>({kTestUUIDGenericAccess, kTestUUIDHeartRate}));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(0u, device->GetGattServices().size());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_GattServicesDiscoveredError_AfterDisconnection \
  GattServicesDiscoveredError_AfterDisconnection
#else
#define MAYBE_GattServicesDiscoveredError_AfterDisconnection \
  DISABLED_GattServicesDiscoveredError_AfterDisconnection
#endif
// Windows does not support disconnecting.
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, GattServicesDiscoveredError_AfterDisconnection) {
#else
TEST_F(BluetoothTest, MAYBE_GattServicesDiscoveredError_AfterDisconnection) {
#endif
  // Tests that we don't crash if services are discovered after
  // the device disconnects.
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(1, gatt_discovery_attempts_);

  SimulateGattDisconnection(device);
  base::RunLoop().RunUntilIdle();

  SimulateGattServicesDiscoveryError(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(0u, device->GetGattServices().size());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_GetGattServices_and_GetGattService \
  GetGattServices_and_GetGattService
#else
#define MAYBE_GetGattServices_and_GetGattService \
  DISABLED_GetGattServices_and_GetGattService
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, GetGattServices_and_GetGattService) {
#else
TEST_F(BluetoothTest, MAYBE_GetGattServices_and_GetGattService) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(1, gatt_discovery_attempts_);

  // 2 duplicate UUIDs creating 2 instances.
  SimulateGattServicesDiscovered(
      device,
      std::vector<std::string>(
          {kTestUUIDGenericAccess, kTestUUIDHeartRate, kTestUUIDHeartRate}));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, device->GetGattServices().size());

  // Test GetGattService:
  std::string service_id1 = device->GetGattServices()[0]->GetIdentifier();
  std::string service_id2 = device->GetGattServices()[1]->GetIdentifier();
  std::string service_id3 = device->GetGattServices()[2]->GetIdentifier();
  EXPECT_TRUE(device->GetGattService(service_id1));
  EXPECT_TRUE(device->GetGattService(service_id2));
  EXPECT_TRUE(device->GetGattService(service_id3));
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_GetGattServices_FindNone GetGattServices_FindNone
#else
#define MAYBE_GetGattServices_FindNone DISABLED_GetGattServices_FindNone
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, GetGattServices_FindNone) {
#else
TEST_F(BluetoothTest, MAYBE_GetGattServices_FindNone) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(1, gatt_discovery_attempts_);

  // Simulate an empty set of discovered services.
  SimulateGattServicesDiscovered(device, {} /* uuids */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, device->GetGattServices().size());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_GetGattServices_DiscoveryError GetGattServices_DiscoveryError
#else
#define MAYBE_GetGattServices_DiscoveryError \
  DISABLED_GetGattServices_DiscoveryError
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, GetGattServices_DiscoveryError) {
#else
TEST_F(BluetoothTest, MAYBE_GetGattServices_DiscoveryError) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(1, gatt_discovery_attempts_);

  SimulateGattServicesDiscoveryError(device);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, device->GetGattServices().size());
}

#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, GattServicesDiscovered_SomeServicesBlocked) {
#else
TEST_F(BluetoothTest, DISABLED_GattServicesDiscovered_SomeServicesBlocked) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  TestBluetoothAdapterObserver observer(adapter_);
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_EQ(1, gatt_discovery_attempts_);
  EXPECT_EQ(0, observer.gatt_services_discovered_count());

  SimulateGattServicesDiscovered(
      device,
      /*uuids=*/{kTestUUIDGenericAccess, kTestUUIDHeartRate},
      /*blocked_uuids=*/{kTestUUIDU2f});
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());
  EXPECT_EQ(1, observer.gatt_services_discovered_count());
  // Even though some services are blocked they should still appear in the list.
  EXPECT_EQ(3u, device->GetGattServices().size());
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
TEST_F(BluetoothTest, GetDeviceTransportType) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);
  EXPECT_EQ(BLUETOOTH_TRANSPORT_LE, device->GetType());

#if !defined(USE_CAST_BLUETOOTH_ADAPTER)
  BluetoothDevice* device2 = SimulateLowEnergyDevice(6);
  EXPECT_EQ(BLUETOOTH_TRANSPORT_DUAL, device2->GetType());

  BluetoothDevice* device3 = SimulateClassicDevice();
  EXPECT_EQ(BLUETOOTH_TRANSPORT_CLASSIC, device3->GetType());
#endif  // !defined(USE_CAST_BLUETOOTH_ADAPTER)
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_GetPrimaryServices GetPrimaryServices
#else
#define MAYBE_GetPrimaryServices DISABLED_GetPrimaryServices
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, GetPrimaryServices) {
#else
TEST_F(BluetoothTest, MAYBE_GetPrimaryServices) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  EXPECT_FALSE(device->IsConnected());

  // Connect to the device.
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_TRUE(device->IsGattConnected());

  // Discover services: Two unique UUIDs, of which the second is duplicated.
  SimulateGattServicesDiscovered(
      device, {kTestUUIDGenericAccess, kTestUUIDHeartRate, kTestUUIDHeartRate});
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());

  EXPECT_EQ(3u, device->GetPrimaryServices().size());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_GetPrimaryServicesByUUID GetPrimaryServicesByUUID
#else
#define MAYBE_GetPrimaryServicesByUUID DISABLED_GetPrimaryServicesByUUID
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, GetPrimaryServicesByUUID) {
#else
TEST_F(BluetoothTest, MAYBE_GetPrimaryServicesByUUID) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }

  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  EXPECT_FALSE(device->IsConnected());

  // Connect to the device.
  ResetEventCounts();
  ASSERT_TRUE(ConnectGatt(device));
  EXPECT_TRUE(device->IsGattConnected());

  // Discover services: Two unique UUIDs, of which the second is duplicated.
  SimulateGattServicesDiscovered(
      device, {kTestUUIDGenericAccess, kTestUUIDHeartRate, kTestUUIDHeartRate});
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());

  {
    const BluetoothUUID unique_service_uuid(kTestUUIDGenericAccess);
    std::vector<BluetoothRemoteGattService*> services =
        device->GetPrimaryServicesByUUID(unique_service_uuid);
    EXPECT_EQ(1u, services.size());
    EXPECT_EQ(unique_service_uuid, services[0]->GetUUID());
  }

  {
    const BluetoothUUID duplicate_service_uuid(kTestUUIDHeartRate);
    std::vector<BluetoothRemoteGattService*> services =
        device->GetPrimaryServicesByUUID(duplicate_service_uuid);
    EXPECT_EQ(2u, services.size());
    EXPECT_EQ(duplicate_service_uuid, services[0]->GetUUID());
    EXPECT_EQ(duplicate_service_uuid, services[1]->GetUUID());

    EXPECT_TRUE(
        device
            ->GetPrimaryServicesByUUID(BluetoothUUID(kTestUUIDGenericAttribute))
            .empty());

    EXPECT_NE(services[0]->GetIdentifier(), services[1]->GetIdentifier());
  }
}

#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothTestWinrt, GattConnectedNameChange) {
#else
// The SimulateGattNameChange() function is not yet available on other
// platforms.
TEST_F(BluetoothTest, DISABLED_GattConnectedNameChange) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();

  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  ASSERT_TRUE(ConnectGatt(device));
  // GetName() returns a std::optional<std:string> however some backends still
  // return an empty string rather than nullopt when no name is available.
  EXPECT_TRUE(!device->GetName() || device->GetName()->empty());

  TestBluetoothAdapterObserver observer(adapter_);
  SimulateGattNameChange(device, kTestDeviceName);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.device_changed_count());
  EXPECT_EQ(base::UTF8ToUTF16(kTestDeviceName), device->GetNameForDisplay());
}

#if BUILDFLAG(IS_WIN)
// WinRT sometimes calls OnConnectionStatusChanged when the status is
// initialized and not when changed.
TEST_P(BluetoothTestWinrt, FalseStatusChangedTest) {
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  EXPECT_FALSE(device->IsConnected());
  device->CreateGattConnection(
      GetGattConnectionCallback(Call::NOT_EXPECTED, Result::FAILURE));
  SimulateStatusChangeToDisconnect(device);

  base::RunLoop().RunUntilIdle();
}
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_ServiceSpecificDiscovery ServiceSpecificDiscovery
#else
#define MAYBE_ServiceSpecificDiscovery DISABLED_ServiceSpecificDiscovery
#endif

#if !BUILDFLAG(IS_WIN)
TEST_F(BluetoothTest, MAYBE_ServiceSpecificDiscovery) {
#else
TEST_P(BluetoothTestWinrt, ServiceSpecificDiscovery) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    GTEST_SKIP() << "Low Energy Bluetooth unavailable, skipping unit test.";
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);

  // Create a GATT connection and specify a specific UUID for discovery.
  ASSERT_TRUE(ConnectGatt(device, BluetoothUUID(kTestUUIDGenericAccess)));
  EXPECT_TRUE(device->IsGattConnected());

  SimulateGattServicesDiscovered(device, {kTestUUIDGenericAccess});
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, gatt_connection_attempts_);
  EXPECT_EQ(1, gatt_discovery_attempts_);

#if !BUILDFLAG(IS_WIN)
  // Outside of WinRT, service-specific discovery should be ignored.
  ASSERT_FALSE(device->supports_service_specific_discovery());

  EXPECT_FALSE(GetTargetGattService(device).has_value());
  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());
#else
  ASSERT_TRUE(device->supports_service_specific_discovery());

  std::optional<BluetoothUUID> service_uuid = GetTargetGattService(device);
  ASSERT_TRUE(service_uuid.has_value());
  EXPECT_EQ(*service_uuid, BluetoothUUID(kTestUUIDGenericAccess));
  EXPECT_FALSE(device->IsGattServicesDiscoveryComplete());

  // Next, simulate a second GATT request that requests the same service.
  // The connection request should be ignored because of the existing,
  // compatible connection.
  ASSERT_TRUE(ConnectGatt(device, BluetoothUUID(kTestUUIDGenericAccess)));
  EXPECT_EQ(1, gatt_connection_attempts_);
  EXPECT_EQ(1, gatt_discovery_attempts_);

  // A third GATT request is same without any UUID.
  ASSERT_TRUE(ConnectGatt(device));
  // This should restart discovery.
  EXPECT_EQ(2, gatt_discovery_attempts_);

  SimulateGattServicesDiscovered(device, {kTestUUIDGenericAccess});
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(device->IsGattServicesDiscoveryComplete());

  // Another GATT request with a specific UUID should be ignored because any
  // specific service is a subset of a complete discovery.
  ASSERT_TRUE(ConnectGatt(device, BluetoothUUID(kTestUUIDGenericAccess)));
  EXPECT_EQ(2, gatt_discovery_attempts_);
#endif
}

}  // namespace device
