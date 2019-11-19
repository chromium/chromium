// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <functional>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/test_bluetooth_adapter_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "device/bluetooth/test/bluetooth_test_android.h"
#elif defined(OS_MACOSX)
#include "device/bluetooth/test/bluetooth_test_mac.h"
#elif defined(OS_WIN)
#include "device/bluetooth/test/bluetooth_test_win.h"
#elif defined(USE_CAST_BLUETOOTH_ADAPTER)
#include "device/bluetooth/test/bluetooth_test_cast.h"
#elif defined(OS_CHROMEOS) || defined(OS_LINUX)
#include "device/bluetooth/test/bluetooth_test_bluez.h"
#elif defined(OS_FUCHSIA)
#include "device/bluetooth/test/bluetooth_test_fuchsia.h"
#endif

using testing::_;
using testing::Invoke;

namespace device {

class BluetoothRemoteGattCharacteristicTest :
#if defined(OS_WIN)
    public BluetoothTestWinrt {
#else
    public BluetoothTest {
#endif
 public:
  // Creates adapter_, device_, service_, characteristic1_, & characteristic2_.
  // |properties| will be used for each characteristic.
  void FakeCharacteristicBoilerplate(int properties = 0) {
    InitWithFakeAdapter();
    StartLowEnergyDiscoverySession();
    device_ = SimulateLowEnergyDevice(3);
    device_->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                  GetConnectErrorCallback(Call::NOT_EXPECTED));
    SimulateGattConnection(device_);
    base::RunLoop().RunUntilIdle();

    SimulateGattServicesDiscovered(
        device_, std::vector<std::string>({kTestUUIDGenericAccess}));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(1u, device_->GetGattServices().size());
    service_ = device_->GetGattServices()[0];
    SimulateGattCharacteristic(service_, kTestUUIDDeviceName, properties);
    SimulateGattCharacteristic(service_, kTestUUIDDeviceName, properties);
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(2u, service_->GetCharacteristics().size());
    characteristic1_ = service_->GetCharacteristics()[0];
    characteristic2_ = service_->GetCharacteristics()[1];
    ResetEventCounts();
  }

  enum class StartNotifySetupError {
    CHARACTERISTIC_PROPERTIES,
    CONFIG_DESCRIPTOR_MISSING,
    CONFIG_DESCRIPTOR_DUPLICATE,
    SET_NOTIFY,
    WRITE_DESCRIPTOR,
    NONE
  };
  // Constructs characteristics with |properties|, calls StartNotifySession,
  // and verifies the appropriate |expected_config_descriptor_value| is written.
  // Error scenarios in this boilerplate are tested by setting |error| to the
  // setup stage to test.
  void StartNotifyBoilerplate(
      int properties,
      NotifyValueState notify_value_state,
      StartNotifySetupError error = StartNotifySetupError::NONE) {
    if (error == StartNotifySetupError::CHARACTERISTIC_PROPERTIES) {
      properties = 0;
    }
    ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(properties));

    size_t expected_descriptors_count = 0;
    if (error != StartNotifySetupError::CONFIG_DESCRIPTOR_MISSING) {
      SimulateGattDescriptor(
          characteristic1_,
          BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
              .canonical_value());
      expected_descriptors_count++;
    }
    if (error == StartNotifySetupError::CONFIG_DESCRIPTOR_DUPLICATE) {
      SimulateGattDescriptor(
          characteristic1_,
          BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
              .canonical_value());
      expected_descriptors_count++;
    }
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(expected_descriptors_count,
              characteristic1_->GetDescriptors().size());

    if (error == StartNotifySetupError::SET_NOTIFY) {
      SimulateGattCharacteristicSetNotifyWillFailSynchronouslyOnce(
          characteristic1_);
    }

    if (error == StartNotifySetupError::WRITE_DESCRIPTOR) {
      SimulateGattDescriptorWriteWillFailSynchronouslyOnce(
          characteristic1_->GetDescriptors()[0]);
    }

    if (error != StartNotifySetupError::NONE) {
      characteristic1_->StartNotifySession(
          GetNotifyCallback(Call::NOT_EXPECTED),
          GetGattErrorCallback(Call::EXPECTED));
      return;
    }

    characteristic1_->StartNotifySession(
        GetNotifyCallback(Call::EXPECTED),
        GetGattErrorCallback(Call::NOT_EXPECTED));

    EXPECT_EQ(0, callback_count_);
    SimulateGattNotifySessionStarted(characteristic1_);
    base::RunLoop().RunUntilIdle();
    ExpectedChangeNotifyValueAttempts(1);
    EXPECT_EQ(1, callback_count_);
    EXPECT_EQ(0, error_callback_count_);
    ASSERT_EQ(1u, notify_sessions_.size());
    ASSERT_TRUE(notify_sessions_[0]);
    EXPECT_EQ(characteristic1_->GetIdentifier(),
              notify_sessions_[0]->GetCharacteristicIdentifier());
    EXPECT_TRUE(notify_sessions_[0]->IsActive());

    // Verify the Client Characteristic Configuration descriptor was written to.
    ExpectedNotifyValue(notify_value_state);
  }

  // A few tests below don't behave correctly on Classic Windows, but do for
  // WinRT. Since a #if defined(OS_WIN) guard is not sufficient to distinguish
  // these two cases, this small utility function is added.
  bool IsClassicWin() {
#if defined(OS_WIN)
    return !GetParam();
#else
    return false;
#endif
  }

  BluetoothDevice* device_ = nullptr;
  BluetoothRemoteGattService* service_ = nullptr;
  BluetoothRemoteGattCharacteristic* characteristic1_ = nullptr;
  BluetoothRemoteGattCharacteristic* characteristic2_ = nullptr;
};

#if defined(OS_WIN)
using BluetoothRemoteGattCharacteristicTestWinrt =
    BluetoothRemoteGattCharacteristicTest;
using BluetoothRemoteGattCharacteristicTestWin32Only =
    BluetoothRemoteGattCharacteristicTest;
using BluetoothRemoteGattCharacteristicTestWinrtOnly =
    BluetoothRemoteGattCharacteristicTest;
#endif

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_GetIdentifier GetIdentifier
#else
#define MAYBE_GetIdentifier DISABLED_GetIdentifier
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt, GetIdentifier) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_GetIdentifier) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();

  StartLowEnergyDiscoverySession();
  // 2 devices to verify unique IDs across them.
  BluetoothDevice* device1 = SimulateLowEnergyDevice(3);
  BluetoothDevice* device2 = SimulateLowEnergyDevice(4);
  device1->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                GetConnectErrorCallback(Call::NOT_EXPECTED));
  device2->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                                GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device1);
  SimulateGattConnection(device2);
  base::RunLoop().RunUntilIdle();

  // 3 services (all with same UUID).
  //   1 on the first device (to test characteristic instances across devices).
  //   2 on the second device (to test same device, multiple service instances).
  SimulateGattServicesDiscovered(
      device1, std::vector<std::string>({kTestUUIDGenericAccess}));
  base::RunLoop().RunUntilIdle();
  SimulateGattServicesDiscovered(
      device2, std::vector<std::string>(
                   {kTestUUIDGenericAccess, kTestUUIDGenericAccess}));
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattService* service1 = device1->GetGattServices()[0];
  BluetoothRemoteGattService* service2 = device2->GetGattServices()[0];
  BluetoothRemoteGattService* service3 = device2->GetGattServices()[1];
  // 6 characteristics (same UUID), 2 on each service.
  SimulateGattCharacteristic(service1, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service1, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service2, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service2, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service3, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service3, kTestUUIDDeviceName, /* properties */ 0);
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattCharacteristic* char1 = service1->GetCharacteristics()[0];
  BluetoothRemoteGattCharacteristic* char2 = service1->GetCharacteristics()[1];
  BluetoothRemoteGattCharacteristic* char3 = service2->GetCharacteristics()[0];
  BluetoothRemoteGattCharacteristic* char4 = service2->GetCharacteristics()[1];
  BluetoothRemoteGattCharacteristic* char5 = service3->GetCharacteristics()[0];
  BluetoothRemoteGattCharacteristic* char6 = service3->GetCharacteristics()[1];

  // All IDs are unique, even though they have the same UUID.
  EXPECT_NE(char1->GetIdentifier(), char2->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char3->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char4->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char1->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char2->GetIdentifier(), char3->GetIdentifier());
  EXPECT_NE(char2->GetIdentifier(), char4->GetIdentifier());
  EXPECT_NE(char2->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char2->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char3->GetIdentifier(), char4->GetIdentifier());
  EXPECT_NE(char3->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char3->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char4->GetIdentifier(), char5->GetIdentifier());
  EXPECT_NE(char4->GetIdentifier(), char6->GetIdentifier());

  EXPECT_NE(char5->GetIdentifier(), char6->GetIdentifier());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_GetUUID GetUUID
#else
#define MAYBE_GetUUID DISABLED_GetUUID
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt, GetUUID) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_GetUUID) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  base::RunLoop().RunUntilIdle();
  SimulateGattServicesDiscovered(
      device, std::vector<std::string>({kTestUUIDGenericAccess}));
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattService* service = device->GetGattServices()[0];

  // Create 3 characteristics. Two of them are duplicates.
  BluetoothUUID uuid1(kTestUUIDDeviceName);
  BluetoothUUID uuid2(kTestUUIDAppearance);
  SimulateGattCharacteristic(service, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service, kTestUUIDAppearance, /* properties */ 0);
  SimulateGattCharacteristic(service, kTestUUIDAppearance, /* properties */ 0);
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattCharacteristic* char1 = service->GetCharacteristics()[0];
  BluetoothRemoteGattCharacteristic* char2 = service->GetCharacteristics()[1];
  BluetoothRemoteGattCharacteristic* char3 = service->GetCharacteristics()[2];

  // Swap as needed to have char1 point to the the characteristic with uuid1.
  if (char2->GetUUID() == uuid1) {
    std::swap(char1, char2);
  } else if (char3->GetUUID() == uuid1) {
    std::swap(char1, char3);
  }

  EXPECT_EQ(uuid1, char1->GetUUID());
  EXPECT_EQ(uuid2, char2->GetUUID());
  EXPECT_EQ(uuid2, char3->GetUUID());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_GetProperties GetProperties
#else
#define MAYBE_GetProperties DISABLED_GetProperties
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt, GetProperties) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_GetProperties) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));
  SimulateGattConnection(device);
  base::RunLoop().RunUntilIdle();
  SimulateGattServicesDiscovered(
      device, std::vector<std::string>({kTestUUIDGenericAccess}));
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattService* service = device->GetGattServices()[0];

  // Create two characteristics with different properties:
  SimulateGattCharacteristic(service, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service, kTestUUIDDeviceName, /* properties */ 7);
  base::RunLoop().RunUntilIdle();

  // Read the properties. Because ordering is unknown swap as necessary.
  int properties1 = service->GetCharacteristics()[0]->GetProperties();
  int properties2 = service->GetCharacteristics()[1]->GetProperties();
  if (properties2 == 0)
    std::swap(properties1, properties2);
  EXPECT_EQ(0, properties1);
  EXPECT_EQ(7, properties2);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_GetService GetService
#else
#define MAYBE_GetService DISABLED_GetService
#endif
// Tests GetService.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt, GetService) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_GetService) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  EXPECT_EQ(service_, characteristic1_->GetService());
  EXPECT_EQ(service_, characteristic2_->GetService());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteCharacteristic_Empty ReadRemoteCharacteristic_Empty
#else
#define MAYBE_ReadRemoteCharacteristic_Empty \
  DISABLED_ReadRemoteCharacteristic_Empty
#endif
// Tests ReadRemoteCharacteristic and GetValue with empty value buffer.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       ReadRemoteCharacteristic_Empty) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_ReadRemoteCharacteristic_Empty) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  base::RunLoop().RunUntilIdle();

  // Duplicate read reported from OS shouldn't cause a problem:
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  EXPECT_EQ(empty_vector, last_read_value_);
  EXPECT_EQ(empty_vector, characteristic1_->GetValue());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteRemoteCharacteristic_Empty WriteRemoteCharacteristic_Empty
#else
#define MAYBE_WriteRemoteCharacteristic_Empty \
  DISABLED_WriteRemoteCharacteristic_Empty
#endif
// Tests WriteRemoteCharacteristic with empty value buffer.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       WriteRemoteCharacteristic_Empty) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_WriteRemoteCharacteristic_Empty) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  SimulateGattCharacteristicWrite(characteristic1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);

  // Duplicate write reported from OS shouldn't cause a problem:
  SimulateGattCharacteristicWrite(characteristic1_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty_vector, last_write_value_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_Retry_ReadRemoteCharacteristic_DuringDestruction_Fails \
  Retry_ReadRemoteCharacteristic_DuringDestruction_Fails
#else
#define MAYBE_Retry_ReadRemoteCharacteristic_DuringDestruction_Fails \
  DISABLED_Retry_ReadRemoteCharacteristic_DuringDestruction_Fails
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       Retry_ReadRemoteCharacteristic_DuringDestruction_Fails) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_Retry_ReadRemoteCharacteristic_DuringDestruction_Fails) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  bool read_error_callback_called = false;
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      base::BindLambdaForTesting(
          [&](BluetoothRemoteGattService::GattErrorCode error_code) {
            EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
                      error_code);
            read_error_callback_called = true;
            // Retrying Read should fail:
            characteristic1_->ReadRemoteCharacteristic(
                GetReadValueCallback(Call::NOT_EXPECTED),
                GetGattErrorCallback(Call::EXPECTED));
          }));

  DeleteDevice(device_);  // TODO(576906) delete only the characteristic.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(read_error_callback_called);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_Retry_WriteRemoteCharacteristic_DuringDestruction_Fails \
  Retry_WriteRemoteCharacteristic_DuringDestruction_Fails
#else
#define MAYBE_Retry_WriteRemoteCharacteristic_DuringDestruction_Fails \
  DISABLED_Retry_WriteRemoteCharacteristic_DuringDestruction_Fails
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       Retry_WriteRemoteCharacteristic_DuringDestruction_Fails) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_Retry_WriteRemoteCharacteristic_DuringDestruction_Fails) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  bool write_error_callback_called = false;
  characteristic1_->WriteRemoteCharacteristic(
      {} /* value */, GetCallback(Call::NOT_EXPECTED),
      base::BindLambdaForTesting(
          [&](BluetoothRemoteGattService::GattErrorCode error_code) {
            EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
                      error_code);
            write_error_callback_called = true;
            // Retrying Write should fail:
            characteristic1_->WriteRemoteCharacteristic(
                {} /* value */, GetCallback(Call::NOT_EXPECTED),
                GetGattErrorCallback(Call::EXPECTED));
          }));

  DeleteDevice(device_);  // TODO(576906) delete only the characteristic.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(write_error_callback_called);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);
}

#if defined(OS_ANDROID)
#define MAYBE_ReadRemoteCharacteristic_AfterDeleted \
  ReadRemoteCharacteristic_AfterDeleted
#else
#define MAYBE_ReadRemoteCharacteristic_AfterDeleted \
  DISABLED_ReadRemoteCharacteristic_AfterDeleted
#endif
// Tests ReadRemoteCharacteristic completing after Chrome objects are deleted.
// macOS: Not applicable: This can never happen if CBPeripheral delegate is set
// to nil.
// WinRT: Not applicable: Pending callbacks won't fire once the underlying
// object is destroyed.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWin32Only,
       ReadRemoteCharacteristic_AfterDeleted) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_ReadRemoteCharacteristic_AfterDeleted) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

  RememberCharacteristicForSubsequentAction(characteristic1_);
  DeleteDevice(device_);  // TODO(576906) delete only the characteristic.

  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(/* use remembered characteristic */ nullptr,
                                 empty_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE("Did not crash!");
}

// TODO(crbug.com/663131): Enable test on windows when disconnection is
// implemented.
#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteCharacteristic_Disconnected \
  ReadRemoteCharacteristic_Disconnected
#else
#define MAYBE_ReadRemoteCharacteristic_Disconnected \
  DISABLED_ReadRemoteCharacteristic_Disconnected
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       ReadRemoteCharacteristic_Disconnected) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_ReadRemoteCharacteristic_Disconnected) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

// Set up for receiving a read response after disconnection.
// On macOS or WinRT no events arrive after disconnection so there is no point
// in building the infrastructure to test this behavior. FYI
// the code CHECKs that responses arrive only when the device is connected.
#if defined(OS_ANDROID)
  RememberCharacteristicForSubsequentAction(characteristic1_);
#endif

  ASSERT_EQ(1u, adapter_->GetDevices().size());
  SimulateDeviceBreaksConnection(adapter_->GetDevices()[0]);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);

// Dispatch read response after disconnection. See above explanation for why
// we don't do this in macOS on WinRT.
#if defined(OS_ANDROID)
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(nullptr /* use remembered characteristic */,
                                 empty_vector);
  base::RunLoop().RunUntilIdle();
#endif
}

#if defined(OS_ANDROID)
#define MAYBE_WriteRemoteCharacteristic_AfterDeleted \
  WriteRemoteCharacteristic_AfterDeleted
#else
#define MAYBE_WriteRemoteCharacteristic_AfterDeleted \
  DISABLED_WriteRemoteCharacteristic_AfterDeleted
#endif
// Tests WriteRemoteCharacteristic completing after Chrome objects are deleted.
// macOS: Not applicable: This can never happen if CBPeripheral delegate is set
// to nil.
// WinRT: Not applicable: Pending callbacks won't fire once the underlying
// object is destroyed.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWin32Only,
       WriteRemoteCharacteristic_AfterDeleted) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_WriteRemoteCharacteristic_AfterDeleted) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

  RememberCharacteristicForSubsequentAction(characteristic1_);
  DeleteDevice(device_);  // TODO(576906) delete only the characteristic.

  SimulateGattCharacteristicWrite(/* use remembered characteristic */ nullptr);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE("Did not crash!");
}

// TODO(crbug.com/663131): Enable test on windows when disconnection is
// implemented.
#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteRemoteCharacteristic_Disconnected \
  WriteRemoteCharacteristic_Disconnected
#else
#define MAYBE_WriteRemoteCharacteristic_Disconnected \
  DISABLED_WriteRemoteCharacteristic_Disconnected
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       WriteRemoteCharacteristic_Disconnected) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_WriteRemoteCharacteristic_Disconnected) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

// Set up for receiving a write response after disconnection.
// On macOS and WinRT no events arrive after disconnection so there is no point
// in building the infrastructure to test this behavior. FYI
// the code CHECKs that responses arrive only when the device is connected.
#if defined(OS_ANDROID)
  RememberCharacteristicForSubsequentAction(characteristic1_);
#endif  // defined(OS_ANDROID)

  ASSERT_EQ(1u, adapter_->GetDevices().size());
  SimulateDeviceBreaksConnection(adapter_->GetDevices()[0]);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);

// Dispatch write response after disconnection. See above explanation for why
// we don't do this in macOS and WinRT.
#if defined(OS_ANDROID)
  SimulateGattCharacteristicWrite(/* use remembered characteristic */ nullptr);
  base::RunLoop().RunUntilIdle();
#endif  // defined(OS_ANDROID)
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteCharacteristic ReadRemoteCharacteristic
#else
#define MAYBE_ReadRemoteCharacteristic DISABLED_ReadRemoteCharacteristic
#endif
// Tests ReadRemoteCharacteristic and GetValue with non-empty value buffer.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt, ReadRemoteCharacteristic) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_ReadRemoteCharacteristic) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  std::vector<uint8_t> test_vector = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  SimulateGattCharacteristicRead(characteristic1_, test_vector);
  base::RunLoop().RunUntilIdle();

  // Duplicate read reported from OS shouldn't cause a problem:
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  EXPECT_EQ(test_vector, last_read_value_);
  EXPECT_EQ(test_vector, characteristic1_->GetValue());
}

// Callback that make sure GattCharacteristicValueChanged has been called
// before the callback runs.
static void TestCallback(
    BluetoothRemoteGattCharacteristic::ValueCallback callback,
    const TestBluetoothAdapterObserver& callback_observer,
    const std::vector<uint8_t>& value) {
  EXPECT_EQ(0, callback_observer.gatt_characteristic_value_changed_count());
  std::move(callback).Run(value);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteCharacteristic_GattCharacteristicValueChangedNotCalled \
  ReadRemoteCharacteristic_GattCharacteristicValueChangedNotCalled
#else
#define MAYBE_ReadRemoteCharacteristic_GattCharacteristicValueChangedNotCalled \
  DISABLED_ReadRemoteCharacteristic_GattCharacteristicValueChangedNotCalled
#endif
// Tests that ReadRemoteCharacteristic doesn't result in a
// GattCharacteristicValueChanged call.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       ReadRemoteCharacteristic_GattCharacteristicValueChangedNotCalled) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_ReadRemoteCharacteristic_GattCharacteristicValueChangedNotCalled) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  TestBluetoothAdapterObserver observer(adapter_);

  characteristic1_->ReadRemoteCharacteristic(
      base::BindOnce(TestCallback, GetReadValueCallback(Call::EXPECTED),
                     std::cref(observer)),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  std::vector<uint8_t> test_vector = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  SimulateGattCharacteristicRead(characteristic1_, test_vector);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
// TODO(https://crbug.com/699694): Remove this #if once the bug on Windows is
// fixed.
  if (IsClassicWin()) {
    EXPECT_FALSE(observer.last_gatt_characteristic_id().empty());
    EXPECT_TRUE(observer.last_gatt_characteristic_uuid().IsValid());
  } else {
    EXPECT_TRUE(observer.last_gatt_characteristic_id().empty());
    EXPECT_FALSE(observer.last_gatt_characteristic_uuid().IsValid());
  }
  EXPECT_TRUE(observer.last_changed_characteristic_value().empty());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteRemoteCharacteristic WriteRemoteCharacteristic
#else
#define MAYBE_WriteRemoteCharacteristic DISABLED_WriteRemoteCharacteristic
#endif
// Tests WriteRemoteCharacteristic with non-empty value buffer.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt, WriteRemoteCharacteristic) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_WriteRemoteCharacteristic) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  TestBluetoothAdapterObserver observer(adapter_);

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + base::size(values));
  characteristic1_->WriteRemoteCharacteristic(
      test_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  SimulateGattCharacteristicWrite(characteristic1_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, gatt_write_characteristic_attempts_);
  // TODO(crbug.com/653291): remove this if once the bug on windows is fixed.
  if (!IsClassicWin())
    EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(test_vector, last_write_value_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteCharacteristic_Twice ReadRemoteCharacteristic_Twice
#else
#define MAYBE_ReadRemoteCharacteristic_Twice \
  DISABLED_ReadRemoteCharacteristic_Twice
#endif
// Tests ReadRemoteCharacteristic and GetValue multiple times.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       ReadRemoteCharacteristic_Twice) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_ReadRemoteCharacteristic_Twice) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + base::size(values));
  SimulateGattCharacteristicRead(characteristic1_, test_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector, last_read_value_);
  EXPECT_EQ(test_vector, characteristic1_->GetValue());

  // Read again, with different value:
  ResetEventCounts();
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(empty_vector, last_read_value_);
  EXPECT_EQ(empty_vector, characteristic1_->GetValue());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteRemoteCharacteristic_Twice WriteRemoteCharacteristic_Twice
#else
#define MAYBE_WriteRemoteCharacteristic_Twice \
  DISABLED_WriteRemoteCharacteristic_Twice
#endif
// Tests WriteRemoteCharacteristic multiple times.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       WriteRemoteCharacteristic_Twice) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_WriteRemoteCharacteristic_Twice) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  uint8_t values[] = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  std::vector<uint8_t> test_vector(values, values + base::size(values));
  characteristic1_->WriteRemoteCharacteristic(
      test_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  SimulateGattCharacteristicWrite(characteristic1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector, last_write_value_);

  // Write again, with different value:
  ResetEventCounts();
  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  SimulateGattCharacteristicWrite(characteristic1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(empty_vector, last_write_value_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteCharacteristic_MultipleCharacteristics \
  ReadRemoteCharacteristic_MultipleCharacteristics
#else
#define MAYBE_ReadRemoteCharacteristic_MultipleCharacteristics \
  DISABLED_ReadRemoteCharacteristic_MultipleCharacteristics
#endif
// Tests ReadRemoteCharacteristic on two characteristics.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       ReadRemoteCharacteristic_MultipleCharacteristics) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_ReadRemoteCharacteristic_MultipleCharacteristics) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic2_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  std::vector<uint8_t> test_vector1;
  test_vector1.push_back(111);
  SimulateGattCharacteristicRead(characteristic1_, test_vector1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_vector1, last_read_value_);

  std::vector<uint8_t> test_vector2;
  test_vector2.push_back(222);
  SimulateGattCharacteristicRead(characteristic2_, test_vector2);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(test_vector2, last_read_value_);

  EXPECT_EQ(2, gatt_read_characteristic_attempts_);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector1, characteristic1_->GetValue());
  EXPECT_EQ(test_vector2, characteristic2_->GetValue());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteRemoteCharacteristic_MultipleCharacteristics \
  WriteRemoteCharacteristic_MultipleCharacteristics
#else
#define MAYBE_WriteRemoteCharacteristic_MultipleCharacteristics \
  DISABLED_WriteRemoteCharacteristic_MultipleCharacteristics
#endif
// Tests WriteRemoteCharacteristic on two characteristics.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       WriteRemoteCharacteristic_MultipleCharacteristics) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_WriteRemoteCharacteristic_MultipleCharacteristics) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> test_vector1;
  test_vector1.push_back(111);
  characteristic1_->WriteRemoteCharacteristic(
      test_vector1, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  if (!IsClassicWin())
    EXPECT_EQ(test_vector1, last_write_value_);

  std::vector<uint8_t> test_vector2;
  test_vector2.push_back(222);
  characteristic2_->WriteRemoteCharacteristic(
      test_vector2, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  if (!IsClassicWin())
    EXPECT_EQ(test_vector2, last_write_value_);

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  SimulateGattCharacteristicWrite(characteristic1_);
  base::RunLoop().RunUntilIdle();
  if (IsClassicWin())
    EXPECT_EQ(test_vector1, last_write_value_);

  SimulateGattCharacteristicWrite(characteristic2_);
  base::RunLoop().RunUntilIdle();
  if (IsClassicWin())
    EXPECT_EQ(test_vector2, last_write_value_);

  EXPECT_EQ(2, gatt_write_characteristic_attempts_);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // TODO(591740): Remove if define for OS_ANDROID in this test.
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_RemoteCharacteristic_Nested_Read_Read \
  RemoteCharacteristic_Nested_Read_Read
#else
#define MAYBE_RemoteCharacteristic_Nested_Read_Read \
  DISABLED_RemoteCharacteristic_Nested_Read_Read
#endif
// Tests a nested ReadRemoteCharacteristic from within another
// ReadRemoteCharacteristic.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       RemoteCharacteristic_Nested_Read_Read) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_RemoteCharacteristic_Nested_Read_Read) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  std::vector<uint8_t> test_vector_1 = {0, 1, 2, 3, 4};
  std::vector<uint8_t> test_vector_2 = {0xf, 0xf0, 0xff};

  characteristic1_->ReadRemoteCharacteristic(
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& data) {
        GetReadValueCallback(Call::EXPECTED).Run(data);

        EXPECT_EQ(1, gatt_read_characteristic_attempts_);
        EXPECT_EQ(1, callback_count_);
        EXPECT_EQ(0, error_callback_count_);
        EXPECT_EQ(test_vector_1, last_read_value_);
        EXPECT_EQ(test_vector_1, characteristic1_->GetValue());

        characteristic1_->ReadRemoteCharacteristic(
            GetReadValueCallback(Call::EXPECTED),
            GetGattErrorCallback(Call::NOT_EXPECTED));
        SimulateGattCharacteristicRead(characteristic1_, test_vector_2);
      }),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  SimulateGattCharacteristicRead(characteristic1_, test_vector_1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, gatt_read_characteristic_attempts_);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector_2, last_read_value_);
  EXPECT_EQ(test_vector_2, characteristic1_->GetValue());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_RemoteCharacteristic_Nested_Write_Write \
  RemoteCharacteristic_Nested_Write_Write
#else
#define MAYBE_RemoteCharacteristic_Nested_Write_Write \
  DISABLED_RemoteCharacteristic_Nested_Write_Write
#endif
// Tests a nested WriteRemoteCharacteristic from within another
// WriteRemoteCharacteristic.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       RemoteCharacteristic_Nested_Write_Write) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_RemoteCharacteristic_Nested_Write_Write) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> test_vector_1 = {0, 1, 2, 3, 4};
  std::vector<uint8_t> test_vector_2 = {0xf, 0xf0, 0xff};

  characteristic1_->WriteRemoteCharacteristic(
      test_vector_1, base::BindLambdaForTesting([&] {
        GetCallback(Call::EXPECTED).Run();

        EXPECT_EQ(1, gatt_write_characteristic_attempts_);
        EXPECT_EQ(1, callback_count_);
        EXPECT_EQ(0, error_callback_count_);
        EXPECT_EQ(test_vector_1, last_write_value_);

        characteristic1_->WriteRemoteCharacteristic(
            test_vector_2, GetCallback(Call::EXPECTED),
            GetGattErrorCallback(Call::NOT_EXPECTED));
        SimulateGattCharacteristicWrite(characteristic1_);
      }),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  SimulateGattCharacteristicWrite(characteristic1_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, gatt_write_characteristic_attempts_);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector_2, last_write_value_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_RemoteCharacteristic_Nested_Read_Write \
  RemoteCharacteristic_Nested_Read_Write
#else
#define MAYBE_RemoteCharacteristic_Nested_Read_Write \
  DISABLED_RemoteCharacteristic_Nested_Read_Write
#endif
// Tests a nested WriteRemoteCharacteristic from within a
// ReadRemoteCharacteristic.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       RemoteCharacteristic_Nested_Read_Write) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_RemoteCharacteristic_Nested_Read_Write) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ |
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> test_vector_1 = {0, 1, 2, 3, 4};
  std::vector<uint8_t> test_vector_2 = {0xf, 0xf0, 0xff};

  characteristic1_->ReadRemoteCharacteristic(
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& data) {
        GetReadValueCallback(Call::EXPECTED).Run(data);

        EXPECT_EQ(1, gatt_read_characteristic_attempts_);
        EXPECT_EQ(0, gatt_write_characteristic_attempts_);
        EXPECT_EQ(1, callback_count_);
        EXPECT_EQ(0, error_callback_count_);
        EXPECT_EQ(test_vector_1, last_read_value_);
        EXPECT_EQ(test_vector_1, characteristic1_->GetValue());

        characteristic1_->WriteRemoteCharacteristic(
            test_vector_2, GetCallback(Call::EXPECTED),
            GetGattErrorCallback(Call::NOT_EXPECTED));
        SimulateGattCharacteristicWrite(characteristic1_);
      }),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  SimulateGattCharacteristicRead(characteristic1_, test_vector_1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector_2, last_write_value_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_RemoteCharacteristic_Nested_Write_Read \
  RemoteCharacteristic_Nested_Write_Read
#else
#define MAYBE_RemoteCharacteristic_Nested_Write_Read \
  DISABLED_RemoteCharacteristic_Nested_Write_Read
#endif
// Tests a nested ReadRemoteCharacteristic from within a
// WriteRemoteCharacteristic.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       RemoteCharacteristic_Nested_Write_Read) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_RemoteCharacteristic_Nested_Write_Read) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ |
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> test_vector_1 = {0, 1, 2, 3, 4};
  std::vector<uint8_t> test_vector_2 = {0xf, 0xf0, 0xff};

  characteristic1_->WriteRemoteCharacteristic(
      test_vector_1, base::BindLambdaForTesting([&] {
        GetCallback(Call::EXPECTED).Run();

        EXPECT_EQ(0, gatt_read_characteristic_attempts_);
        EXPECT_EQ(1, gatt_write_characteristic_attempts_);
        EXPECT_EQ(1, callback_count_);
        EXPECT_EQ(0, error_callback_count_);
        EXPECT_EQ(test_vector_1, last_write_value_);

        characteristic1_->ReadRemoteCharacteristic(
            GetReadValueCallback(Call::EXPECTED),
            GetGattErrorCallback(Call::NOT_EXPECTED));
        SimulateGattCharacteristicRead(characteristic1_, test_vector_2);
      }),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  SimulateGattCharacteristicWrite(characteristic1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(test_vector_2, last_read_value_);
  EXPECT_EQ(test_vector_2, characteristic1_->GetValue());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadError ReadError
#else
#define MAYBE_ReadError DISABLED_ReadError
#endif
// Tests ReadRemoteCharacteristic asynchronous error.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt, ReadError) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_ReadError) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  TestBluetoothAdapterObserver observer(adapter_);

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  SimulateGattCharacteristicReadError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH);
  SimulateGattCharacteristicReadError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_FAILED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH,
            last_gatt_error_code_);
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteError WriteError
#else
#define MAYBE_WriteError DISABLED_WriteError
#endif
// Tests WriteRemoteCharacteristic asynchronous error.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt, WriteError) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_WriteError) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  SimulateGattCharacteristicWriteError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH);
  SimulateGattCharacteristicWriteError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_FAILED);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH,
            last_gatt_error_code_);
}

#if defined(OS_ANDROID)
#define MAYBE_ReadSynchronousError ReadSynchronousError
#else
#define MAYBE_ReadSynchronousError DISABLED_ReadSynchronousError
#endif
// Tests ReadRemoteCharacteristic synchronous error.
// Test not relevant for macOS and WinRT since characteristic read cannot
// generate synchronous error.
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_ReadSynchronousError) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  SimulateGattCharacteristicReadWillFailSynchronouslyOnce(characteristic1_);
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  EXPECT_EQ(0, gatt_read_characteristic_attempts_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);

  // After failing once, can succeed:
  ResetEventCounts();
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_read_characteristic_attempts_);
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

#if defined(OS_ANDROID)
#define MAYBE_WriteSynchronousError WriteSynchronousError
#else
#define MAYBE_WriteSynchronousError DISABLED_WriteSynchronousError
#endif
// Tests WriteRemoteCharacteristic synchronous error.
// This test doesn't apply to macOS and WinRT since a synchronous API does not
// exist.
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_WriteSynchronousError) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  SimulateGattCharacteristicWriteWillFailSynchronouslyOnce(characteristic1_);
  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  EXPECT_EQ(0, gatt_write_characteristic_attempts_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);

  // After failing once, can succeed:
  ResetEventCounts();
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(1, gatt_write_characteristic_attempts_);
  SimulateGattCharacteristicWrite(characteristic1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteCharacteristic_ReadPending \
  ReadRemoteCharacteristic_ReadPending
#else
#define MAYBE_ReadRemoteCharacteristic_ReadPending \
  DISABLED_ReadRemoteCharacteristic_ReadPending
#endif
// Tests ReadRemoteCharacteristic error with a pending read operation.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       ReadRemoteCharacteristic_ReadPending) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_ReadRemoteCharacteristic_ReadPending) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial read should still succeed:
  ResetEventCounts();
  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteRemoteCharacteristic_WritePending \
  WriteRemoteCharacteristic_WritePending
#else
#define MAYBE_WriteRemoteCharacteristic_WritePending \
  DISABLED_WriteRemoteCharacteristic_WritePending
#endif
// Tests WriteRemoteCharacteristic error with a pending write operation.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       WriteRemoteCharacteristic_WritePending) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_WriteRemoteCharacteristic_WritePending) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial write should still succeed:
  ResetEventCounts();
  SimulateGattCharacteristicWrite(characteristic1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_ReadRemoteCharacteristic_WritePending \
  ReadRemoteCharacteristic_WritePending
#else
#define MAYBE_ReadRemoteCharacteristic_WritePending \
  DISABLED_ReadRemoteCharacteristic_WritePending
#endif
// Tests ReadRemoteCharacteristic error with a pending write operation.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       ReadRemoteCharacteristic_WritePending) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_ReadRemoteCharacteristic_WritePending) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ |
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> empty_vector;
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial write should still succeed:
  ResetEventCounts();
  SimulateGattCharacteristicWrite(characteristic1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteRemoteCharacteristic_ReadPending \
  WriteRemoteCharacteristic_ReadPending
#else
#define MAYBE_WriteRemoteCharacteristic_ReadPending \
  DISABLED_WriteRemoteCharacteristic_ReadPending
#endif
// Tests WriteRemoteCharacteristic error with a pending Read operation.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       WriteRemoteCharacteristic_ReadPending) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_WriteRemoteCharacteristic_ReadPending) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ |
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> empty_vector;
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->WriteRemoteCharacteristic(
      empty_vector, GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS,
            last_gatt_error_code_);

  // Initial read should still succeed:
  ResetEventCounts();
  SimulateGattCharacteristicRead(characteristic1_, empty_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_Notification_During_ReadRemoteCharacteristic \
  Notification_During_ReadRemoteCharacteristic
#else
#define MAYBE_Notification_During_ReadRemoteCharacteristic \
  DISABLED_Notification_During_ReadRemoteCharacteristic
#endif
// TODO(crbug.com/713991): Enable on windows once it better matches
// how other platforms set global variables.
// Tests that a notification arriving during a pending read doesn't
// cause a crash.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       Notification_During_ReadRemoteCharacteristic) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_Notification_During_ReadRemoteCharacteristic) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY |
          BluetoothRemoteGattCharacteristic::PROPERTY_READ,
      NotifyValueState::NOTIFY));

  TestBluetoothAdapterObserver observer(adapter_);

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  std::vector<uint8_t> notification_value = {111};
  SimulateGattCharacteristicChanged(characteristic1_, notification_value);
  base::RunLoop().RunUntilIdle();

#if defined(OS_MACOSX)
  // Because macOS uses the same event for notifications and read value
  // responses, we can't know what the event was for. Because there is a pending
  // read request we assume is a read request on macOS.
  EXPECT_EQ(notification_value, last_read_value_);
  EXPECT_EQ(notification_value, characteristic1_->GetValue());
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
#else   // !defined(OS_MACOSX)
  EXPECT_EQ(std::vector<uint8_t>(), last_read_value_);
  EXPECT_EQ(notification_value, characteristic1_->GetValue());
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
#endif  // defined(OS_MACOSX)

  observer.Reset();
  std::vector<uint8_t> read_value = {222};
  SimulateGattCharacteristicRead(characteristic1_, read_value);
  base::RunLoop().RunUntilIdle();
#if defined(OS_MACOSX)
  // There are no pending read requests anymore so we assume the event
  // was a notification.
  EXPECT_EQ(notification_value, last_read_value_);
  EXPECT_EQ(read_value, characteristic1_->GetValue());
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
#else   // !defined(OS_MACOSX)
  EXPECT_EQ(read_value, last_read_value_);
  EXPECT_EQ(read_value, characteristic1_->GetValue());
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
#endif  // defined(OS_MACOSX)
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_Notification_During_WriteRemoteCharacteristic \
  Notification_During_WriteRemoteCharacteristic
#else
#define MAYBE_Notification_During_WriteRemoteCharacteristic \
  DISABLED_Notification_During_WriteRemoteCharacteristic
#endif
// Tests that a notification arriving during a pending write doesn't
// cause a crash.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       Notification_During_WriteRemoteCharacteristic) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_Notification_During_WriteRemoteCharacteristic) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY |
          BluetoothRemoteGattCharacteristic::PROPERTY_WRITE,
      NotifyValueState::NOTIFY));

  TestBluetoothAdapterObserver observer(adapter_);

  std::vector<uint8_t> write_value = {111};
  characteristic1_->WriteRemoteCharacteristic(
      write_value, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  std::vector<uint8_t> notification_value = {222};
  SimulateGattCharacteristicChanged(characteristic1_, notification_value);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(notification_value, characteristic1_->GetValue());
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());

  observer.Reset();
  SimulateGattCharacteristicWrite(characteristic1_);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(write_value, last_write_value_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StartNotifySession_NoNotifyOrIndicate \
  StartNotifySession_NoNotifyOrIndicate
#else
#define MAYBE_StartNotifySession_NoNotifyOrIndicate \
  DISABLED_StartNotifySession_NoNotifyOrIndicate
#endif
// StartNotifySession fails if characteristic doesn't have Notify or Indicate
// property.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       StartNotifySession_NoNotifyOrIndicate) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StartNotifySession_NoNotifyOrIndicate) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY,
      StartNotifySetupError::CHARACTERISTIC_PROPERTIES));

  ExpectedChangeNotifyValueAttempts(0);

  // The expected error callback is asynchronous:
  EXPECT_EQ(0, error_callback_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED,
            last_gatt_error_code_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StartNotifySession_NoConfigDescriptor \
  StartNotifySession_NoConfigDescriptor
#else
#define MAYBE_StartNotifySession_NoConfigDescriptor \
  DISABLED_StartNotifySession_NoConfigDescriptor
#endif
// StartNotifySession fails if the characteristic is missing the Client
// Characteristic Configuration descriptor.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       StartNotifySession_NoConfigDescriptor) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StartNotifySession_NoConfigDescriptor) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY,
      StartNotifySetupError::CONFIG_DESCRIPTOR_MISSING));

  ExpectedChangeNotifyValueAttempts(0);

  // The expected error callback is asynchronous:
  EXPECT_EQ(0, error_callback_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED,
            last_gatt_error_code_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StartNotifySession_MultipleConfigDescriptor \
  StartNotifySession_MultipleConfigDescriptor
#else
#define MAYBE_StartNotifySession_MultipleConfigDescriptor \
  DISABLED_StartNotifySession_MultipleConfigDescriptor
#endif
// StartNotifySession fails if the characteristic has multiple Client
// Characteristic Configuration descriptors.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       StartNotifySession_MultipleConfigDescriptor) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StartNotifySession_MultipleConfigDescriptor) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY,
      StartNotifySetupError::CONFIG_DESCRIPTOR_DUPLICATE));

  ExpectedChangeNotifyValueAttempts(0);

  // The expected error callback is asynchronous:
  EXPECT_EQ(0, error_callback_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);
}

#if defined(OS_ANDROID)
#define MAYBE_StartNotifySession_FailToSetCharacteristicNotification \
  StartNotifySession_FailToSetCharacteristicNotification
#else
#define MAYBE_StartNotifySession_FailToSetCharacteristicNotification \
  DISABLED_StartNotifySession_FailToSetCharacteristicNotification
#endif
// StartNotifySession fails synchronously when failing to set a characteristic
// to enable notifications.
// Android: This is mBluetoothGatt.setCharacteristicNotification failing.
// macOS: Not applicable: CoreBluetooth doesn't support synchronous API.
// Windows: Synchronous Test Not Applicable: OS calls are all made
// asynchronously from BluetoothTaskManagerWin.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StartNotifySession_FailToSetCharacteristicNotification) {
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY,
      StartNotifySetupError::SET_NOTIFY));

  // The expected error callback is asynchronous:
  EXPECT_EQ(0, error_callback_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, error_callback_count_);

  ExpectedChangeNotifyValueAttempts(0);
  ASSERT_EQ(0u, notify_sessions_.size());
}

#if defined(OS_ANDROID)
#define MAYBE_StartNotifySession_WriteDescriptorSynchronousError \
  StartNotifySession_WriteDescriptorSynchronousError
#else
#define MAYBE_StartNotifySession_WriteDescriptorSynchronousError \
  DISABLED_StartNotifySession_WriteDescriptorSynchronousError
#endif
// Tests StartNotifySession descriptor write synchronous failure.
// macOS: Not applicable: No need to write to the descriptor manually.
// -[CBPeripheral setNotifyValue:forCharacteristic:] takes care of it.
// Windows: Synchronous Test Not Applicable: OS calls are all made
// asynchronously from BluetoothTaskManagerWin.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StartNotifySession_WriteDescriptorSynchronousError) {
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY,
      StartNotifySetupError::WRITE_DESCRIPTOR));

  // The expected error callback is asynchronous:
  EXPECT_EQ(0, error_callback_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(1, gatt_notify_characteristic_attempts_);
  EXPECT_EQ(0, gatt_write_descriptor_attempts_);
  ASSERT_EQ(0u, last_write_value_.size());
  ASSERT_EQ(0u, notify_sessions_.size());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StartNotifySession StartNotifySession
#else
#define MAYBE_StartNotifySession DISABLED_StartNotifySession
#endif
// Tests StartNotifySession success on a characteristic that enabled Notify.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt, StartNotifySession) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_StartNotifySession) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StartNotifySession_OnIndicate StartNotifySession_OnIndicate
#else
#define MAYBE_StartNotifySession_OnIndicate \
  DISABLED_StartNotifySession_OnIndicate
#endif
// Tests StartNotifySession success on a characteristic that enabled Indicate.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       StartNotifySession_OnIndicate) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StartNotifySession_OnIndicate) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: INDICATE */ 0x20, NotifyValueState::INDICATE));
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StartNotifySession_OnNotifyAndIndicate \
  StartNotifySession_OnNotifyAndIndicate
#else
#define MAYBE_StartNotifySession_OnNotifyAndIndicate \
  DISABLED_StartNotifySession_OnNotifyAndIndicate
#endif
// Tests StartNotifySession success on a characteristic that enabled Notify &
// Indicate.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       StartNotifySession_OnNotifyAndIndicate) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StartNotifySession_OnNotifyAndIndicate) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY and INDICATE bits set */ 0x30,
      NotifyValueState::NOTIFY));
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StartNotifySession_Multiple StartNotifySession_Multiple
#else
#define MAYBE_StartNotifySession_Multiple DISABLED_StartNotifySession_Multiple
#endif
// Tests multiple StartNotifySession success.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       StartNotifySession_Multiple) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StartNotifySession_Multiple) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  characteristic1_->StartNotifySession(
      GetNotifyCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->StartNotifySession(
      GetNotifyCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  ASSERT_EQ(2u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  ASSERT_TRUE(notify_sessions_[1]);
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[1]->GetCharacteristicIdentifier());
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
  EXPECT_TRUE(notify_sessions_[1]->IsActive());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StartNotifySessionError_Multiple StartNotifySessionError_Multiple
#else
#define MAYBE_StartNotifySessionError_Multiple \
  DISABLED_StartNotifySessionError_Multiple
#endif
// Tests multiple StartNotifySessions pending and then an error.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StartNotifySessionError_Multiple) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StartNotifySessionError_Multiple) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  characteristic1_->StartNotifySession(GetNotifyCallback(Call::NOT_EXPECTED),
                                       GetGattErrorCallback(Call::EXPECTED));
  characteristic1_->StartNotifySession(GetNotifyCallback(Call::NOT_EXPECTED),
                                       GetGattErrorCallback(Call::EXPECTED));
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStartError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_FAILED);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  ASSERT_EQ(0u, notify_sessions_.size());
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);
}

#if defined(OS_ANDROID)
#define MAYBE_StartNotifySession_AfterDeleted StartNotifySession_AfterDeleted
#else
#define MAYBE_StartNotifySession_AfterDeleted \
  DISABLED_StartNotifySession_AfterDeleted
#endif
// Tests StartNotifySession completing after chrome objects are deleted.
// macOS: Not applicable: This can never happen if CBPeripheral delegate is set
// to nil.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StartNotifySession_AfterDeleted) {
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  characteristic1_->StartNotifySession(GetNotifyCallback(Call::NOT_EXPECTED),
                                       GetGattErrorCallback(Call::EXPECTED));
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);
  EXPECT_EQ(0, callback_count_);

  RememberCharacteristicForSubsequentAction(characteristic1_);
  RememberCCCDescriptorForSubsequentAction(characteristic1_);
  DeleteDevice(device_);  // TODO(576906) delete only the characteristic.

  SimulateGattNotifySessionStarted(/* use remembered characteristic */ nullptr);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  ASSERT_EQ(0u, notify_sessions_.size());
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StartNotifySession_BeforeDeleted StartNotifySession_BeforeDeleted
#else
#define MAYBE_StartNotifySession_BeforeDeleted \
  DISABLED_StartNotifySession_BeforeDeleted
#endif
// Tests StartNotifySession completing before chrome objects are deleted.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StartNotifySession_BeforeDeleted) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StartNotifySession_BeforeDeleted) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  characteristic1_->StartNotifySession(
      GetNotifyCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);
  EXPECT_EQ(0, callback_count_);

  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, notify_sessions_.size());

  std::string characteristic_identifier = characteristic1_->GetIdentifier();

  EXPECT_EQ(characteristic_identifier,
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_EQ(characteristic1_, notify_sessions_[0]->GetCharacteristic());
  EXPECT_TRUE(notify_sessions_[0]->IsActive());

  DeleteDevice(device_);  // TODO(576906) delete only the characteristic.

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE("Did not crash!");
  ASSERT_TRUE(notify_sessions_[0]);
  EXPECT_EQ(characteristic_identifier,
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
}

#if defined(OS_MACOSX)
#define MAYBE_StartNotifySession_Reentrant_Success_Success \
  StartNotifySession_Reentrant_Success_Success
#else
#define MAYBE_StartNotifySession_Reentrant_Success_Success \
  DISABLED_StartNotifySession_Reentrant_Success_Success
#endif
// Tests StartNotifySession reentrant in start notify session success callback
// and the reentrant start notify session success.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       StartNotifySession_Reentrant_Success_Success) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StartNotifySession_Reentrant_Success_Success) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  characteristic1_->StartNotifySession(
      GetReentrantStartNotifySessionSuccessCallback(Call::EXPECTED,
                                                    characteristic1_),
      GetReentrantStartNotifySessionErrorCallback(
          Call::NOT_EXPECTED, characteristic1_,
          false /* error_in_reentrant */));
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);

  // Simulate reentrant StartNotifySession request from
  // BluetoothTestBase::ReentrantStartNotifySessionSuccessCallback.
  base::RunLoop().RunUntilIdle();
  ExpectedChangeNotifyValueAttempts(1);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  ASSERT_EQ(2u, notify_sessions_.size());
  for (unsigned int i = 0; i < notify_sessions_.size(); i++) {
    ASSERT_TRUE(notify_sessions_[i]);
    EXPECT_EQ(characteristic1_->GetIdentifier(),
              notify_sessions_[i]->GetCharacteristicIdentifier());
    EXPECT_TRUE(notify_sessions_[i]->IsActive());
  }
}

#if defined(OS_WIN)
// Tests StartNotifySession reentrant in start notify session error callback
// and the reentrant start notify session error.
TEST_P(BluetoothRemoteGattCharacteristicTestWin32Only,
       StartNotifySession_Reentrant_Error_Error) {
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothGattDescriptor::ClientCharacteristicConfigurationUuid().value());
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  SimulateGattNotifySessionStartError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);

  characteristic1_->StartNotifySession(
      GetReentrantStartNotifySessionSuccessCallback(Call::NOT_EXPECTED,
                                                    characteristic1_),
      GetReentrantStartNotifySessionErrorCallback(
          Call::EXPECTED, characteristic1_, true /* error_in_reentrant */));
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();
  ExpectedChangeNotifyValueAttempts(0);

  // Simulate reentrant StartNotifySession request from
  // BluetoothTestBase::ReentrantStartNotifySessionErrorCallback.
  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();
  ExpectedChangeNotifyValueAttempts(0);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(2, error_callback_count_);
  ASSERT_EQ(0u, notify_sessions_.size());
}
#endif

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession StopNotifySession
#else
#define MAYBE_StopNotifySession DISABLED_StopNotifySession
#endif
// Tests StopNotifySession success on a characteristic that enabled Notify.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly, StopNotifySession) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_StopNotifySession) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);

  notify_sessions_[0]->Stop(GetStopNotifyCallback(Call::EXPECTED));
  SimulateGattNotifySessionStopped(characteristic1_);
  base::RunLoop().RunUntilIdle();

  ExpectedChangeNotifyValueAttempts(2);
  ExpectedNotifyValue(NotifyValueState::NONE);

  // Check that the notify session is inactive.
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  EXPECT_FALSE(characteristic1_->IsNotifying());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_SessionDeleted StopNotifySession_SessionDeleted
#else
#define MAYBE_StopNotifySession_SessionDeleted \
  DISABLED_StopNotifySession_SessionDeleted
#endif
// Tests that deleted sessions are stopped.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StopNotifySession_SessionDeleted) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_SessionDeleted) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);

  notify_sessions_.clear();
  SimulateGattNotifySessionStopped(characteristic1_);
  base::RunLoop().RunUntilIdle();

  ExpectedChangeNotifyValueAttempts(2);
  ExpectedNotifyValue(NotifyValueState::NONE);

  // Check that the notify session is inactive.
  EXPECT_FALSE(characteristic1_->IsNotifying());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_SessionDeleted2 \
  StopNotifySession_SessionDeleted2
#else
#define MAYBE_StopNotifySession_SessionDeleted2 \
  DISABLED_StopNotifySession_SessionDeleted2
#endif
// Tests that deleting the sessions before the stop callbacks have been
// invoked does not cause problems.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StopNotifySession_SessionDeleted2) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_SessionDeleted2) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));

  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  // Start notify sessions.
  characteristic1_->StartNotifySession(
      GetNotifyCheckForPrecedingCalls(0),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->StartNotifySession(
      GetNotifyCheckForPrecedingCalls(1),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  ASSERT_EQ(2u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  ASSERT_TRUE(notify_sessions_[1]);
  EXPECT_EQ(characteristic1_, notify_sessions_[0]->GetCharacteristic());
  EXPECT_EQ(characteristic1_, notify_sessions_[1]->GetCharacteristic());
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
  EXPECT_TRUE(notify_sessions_[1]->IsActive());
  EXPECT_TRUE(characteristic1_->IsNotifying());

  // Queue up stop events.
  notify_sessions_[1]->Stop(GetStopNotifyCallback(Call::EXPECTED));
  notify_sessions_[0]->Stop(GetStopNotifyCallback(Call::EXPECTED));

  // Delete the notify sessions.
  notify_sessions_.clear();

  // Run the stop events.
  base::RunLoop().RunUntilIdle();
  SimulateGattNotifySessionStopped(characteristic1_);
  base::RunLoop().RunUntilIdle();

  // Check that the state is correct.
  EXPECT_TRUE("Did not crash!");
  EXPECT_FALSE(characteristic1_->IsNotifying());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_Cancelled StopNotifySession_Cancelled
#else
#define MAYBE_StopNotifySession_Cancelled DISABLED_StopNotifySession_Cancelled
#endif
// Tests that cancelling StopNotifySession works.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       StopNotifySession_Cancelled) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_Cancelled) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));

  // Check that the session is correctly setup.
  std::string characteristic_identifier = characteristic1_->GetIdentifier();
  EXPECT_EQ(characteristic_identifier,
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_EQ(characteristic1_, notify_sessions_[0]->GetCharacteristic());
  EXPECT_TRUE(notify_sessions_[0]->IsActive());

  // Queue a Stop request.
  notify_sessions_[0]->Stop(GetStopNotifyCallback(Call::EXPECTED));

  // Cancel Stop by deleting the device before Stop finishes.
  DeleteDevice(device_);  // TODO(576906) delete only the characteristic.
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_AfterDeleted StopNotifySession_AfterDeleted
#else
#define MAYBE_StopNotifySession_AfterDeleted \
  DISABLED_StopNotifySession_AfterDeleted
#endif
// Tests that deleted sessions are stopped.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       StopNotifySession_AfterDeleted) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_AfterDeleted) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));

  // Check that the session is correctly setup
  std::string characteristic_identifier = characteristic1_->GetIdentifier();
  EXPECT_EQ(characteristic_identifier,
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_EQ(characteristic1_, notify_sessions_[0]->GetCharacteristic());
  EXPECT_TRUE(notify_sessions_[0]->IsActive());

  DeleteDevice(device_);  // TODO(576906) delete only the characteristic.

  ResetEventCounts();
  notify_sessions_[0]->Stop(GetStopNotifyCallback(Call::EXPECTED));

  // Check that the callback does not arrive synchronously.
  EXPECT_EQ(0, callback_count_);

  // Trigger the stop callback
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);

  EXPECT_TRUE("Did not crash!");
  ASSERT_TRUE(notify_sessions_[0]);
  EXPECT_EQ(characteristic_identifier,
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_OnIndicate StopNotifySession_OnIndicate
#else
#define MAYBE_StopNotifySession_OnIndicate DISABLED_StopNotifySession_OnIndicate
#endif
// Tests StopNotifySession success on a characteristic that enabled Indicate.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StopNotifySession_OnIndicate) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_OnIndicate) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: INDICATE */ 0x20, NotifyValueState::INDICATE));
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::INDICATE);

  notify_sessions_[0]->Stop(GetStopNotifyCallback(Call::EXPECTED));
  SimulateGattNotifySessionStopped(characteristic1_);
  base::RunLoop().RunUntilIdle();

  ExpectedChangeNotifyValueAttempts(2);
  ExpectedNotifyValue(NotifyValueState::NONE);

  // Check that the notify session is inactive.
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  EXPECT_FALSE(characteristic1_->IsNotifying());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_OnNotifyAndIndicate \
  StopNotifySession_OnNotifyAndIndicate
#else
#define MAYBE_StopNotifySession_OnNotifyAndIndicate \
  DISABLED_StopNotifySession_OnNotifyAndIndicate
#endif
// Tests StopNotifySession success on a characteristic that enabled Notify &
// Indicate.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StopNotifySession_OnNotifyAndIndicate) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_OnNotifyAndIndicate) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY and INDICATE bits set */ 0x30,
      NotifyValueState::NOTIFY));
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);

  notify_sessions_[0]->Stop(GetStopNotifyCallback(Call::EXPECTED));
  SimulateGattNotifySessionStopped(characteristic1_);
  base::RunLoop().RunUntilIdle();

  ExpectedChangeNotifyValueAttempts(2);
  ExpectedNotifyValue(NotifyValueState::NONE);

  // Check that the notify session is inactive.
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  EXPECT_FALSE(characteristic1_->IsNotifying());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_Error StopNotifySession_Error
#else
#define MAYBE_StopNotifySession_Error DISABLED_StopNotifySession_Error
#endif
// Tests StopNotifySession error
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StopNotifySession_Error) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_StopNotifySession_Error) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));

  // Check that the notify session is active.
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_EQ(characteristic1_, notify_sessions_[0]->GetCharacteristic());
  EXPECT_TRUE(characteristic1_->IsNotifying());

  notify_sessions_[0]->Stop(GetStopNotifyCallback(Call::EXPECTED));
  SimulateGattNotifySessionStopError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_UNKNOWN);
  base::RunLoop().RunUntilIdle();

  // Check that the notify session is inactive.
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  EXPECT_FALSE(characteristic1_->IsNotifying());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_Multiple1 StopNotifySession_Multiple1
#else
#define MAYBE_StopNotifySession_Multiple1 DISABLED_StopNotifySession_Multiple1
#endif
// Tests multiple StopNotifySession calls for a single session.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StopNotifySession_Multiple1) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_Multiple1) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));

  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  // Start notify session
  characteristic1_->StartNotifySession(
      GetNotifyCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  ASSERT_EQ(1u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  EXPECT_EQ(characteristic1_, notify_sessions_[0]->GetCharacteristic());
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
  EXPECT_TRUE(characteristic1_->IsNotifying());

  // Stop the notify session twice
  ResetEventCounts();
  notify_sessions_[0]->Stop(GetStopNotifyCheckForPrecedingCalls(0));
  notify_sessions_[0]->Stop(GetStopNotifyCheckForPrecedingCalls(1));
  SimulateGattNotifySessionStopped(characteristic1_);
  base::RunLoop().RunUntilIdle();

  // Check that the notify session is inactive.
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  EXPECT_FALSE(characteristic1_->IsNotifying());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_Multiple2 StopNotifySession_Multiple2
#else
#define MAYBE_StopNotifySession_Multiple2 DISABLED_StopNotifySession_Multiple2
#endif
// Tests multiple StartNotifySession calls and multiple StopNotifySession calls.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StopNotifySession_Multiple2) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_Multiple2) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));

  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  // Start notify sessions
  characteristic1_->StartNotifySession(
      GetNotifyCheckForPrecedingCalls(0),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  characteristic1_->StartNotifySession(
      GetNotifyCheckForPrecedingCalls(1),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  EXPECT_EQ(0, callback_count_);
  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  ASSERT_EQ(2u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  ASSERT_TRUE(notify_sessions_[1]);
  EXPECT_EQ(characteristic1_, notify_sessions_[0]->GetCharacteristic());
  EXPECT_EQ(characteristic1_, notify_sessions_[1]->GetCharacteristic());
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
  EXPECT_TRUE(notify_sessions_[1]->IsActive());
  EXPECT_TRUE(characteristic1_->IsNotifying());

  // Stop the notify sessions
  notify_sessions_[1]->Stop(GetStopNotifyCheckForPrecedingCalls(2));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
  EXPECT_FALSE(notify_sessions_[1]->IsActive());
  EXPECT_TRUE(characteristic1_->IsNotifying());

  notify_sessions_[0]->Stop(GetStopNotifyCheckForPrecedingCalls(3));
  SimulateGattNotifySessionStopped(characteristic1_);
  base::RunLoop().RunUntilIdle();

  // Check that the notify sessions is inactive.
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  EXPECT_FALSE(notify_sessions_[1]->IsActive());
  EXPECT_FALSE(characteristic1_->IsNotifying());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_StopStart StopNotifySession_StopStart
#else
#define MAYBE_StopNotifySession_StopStart DISABLED_StopNotifySession_StopStart
#endif
// Tests starting a new notify session before the previous stop request
// resolves.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StopNotifySession_StopStart) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_StopStart) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  // Start notify session
  ResetEventCounts();
  characteristic1_->StartNotifySession(
      GetNotifyCheckForPrecedingCalls(0),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  EXPECT_TRUE(notify_sessions_[0]->IsActive());

  // Stop the notify session
  notify_sessions_[0]->Stop(GetStopNotifyCheckForPrecedingCalls(1));

  // Start another notify session
  characteristic1_->StartNotifySession(
      GetNotifyCheckForPrecedingCalls(2),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  SimulateGattNotifySessionStopped(characteristic1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(notify_sessions_[0]->IsActive());

  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  ASSERT_TRUE(notify_sessions_[1]);
  EXPECT_TRUE(notify_sessions_[1]->IsActive());
  EXPECT_TRUE(characteristic1_->IsNotifying());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_StartStopStart StopNotifySession_StartStopStart
#else
#define MAYBE_StopNotifySession_StartStopStart \
  DISABLED_StopNotifySession_StartStopStart
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StopNotifySession_StartStopStart) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_StartStopStart) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));

  // Check that the initial notify session is active.
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_EQ(characteristic1_, notify_sessions_[0]->GetCharacteristic());
  EXPECT_TRUE(characteristic1_->IsNotifying());

  // Queue up the first event.
  ResetEventCounts();
  characteristic1_->StartNotifySession(
      GetNotifyCheckForPrecedingCalls(0),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  // Queue up the second event.
  notify_sessions_[0]->Stop(GetStopNotifyCheckForPrecedingCalls(1));

  // Queue up the third event.
  characteristic1_->StartNotifySession(
      GetNotifyCheckForPrecedingCalls(2),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  // Run the event loop to resolve all three events.
  base::RunLoop().RunUntilIdle();

  // Check the state of all the sessions.
  ASSERT_EQ(3u, notify_sessions_.size());
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_EQ(characteristic1_, notify_sessions_[0]->GetCharacteristic());
  EXPECT_FALSE(notify_sessions_[0]->IsActive());

  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[1]->GetCharacteristicIdentifier());
  EXPECT_EQ(characteristic1_, notify_sessions_[1]->GetCharacteristic());
  EXPECT_TRUE(notify_sessions_[1]->IsActive());

  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[2]->GetCharacteristicIdentifier());
  EXPECT_EQ(characteristic1_, notify_sessions_[2]->GetCharacteristic());
  EXPECT_TRUE(notify_sessions_[2]->IsActive());

  EXPECT_TRUE(characteristic1_->IsNotifying());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_StopStopStart StopNotifySession_StopStopStart
#else
#define MAYBE_StopNotifySession_StopStopStart \
  DISABLED_StopNotifySession_StopStopStart
#endif
// Tests starting a new notify session before the previous stop requests
// resolve.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StopNotifySession_StopStopStart) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_StopStopStart) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  // Start notify session
  ResetEventCounts();
  characteristic1_->StartNotifySession(
      GetNotifyCheckForPrecedingCalls(0),
      GetGattErrorCallback(Call::NOT_EXPECTED));
  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);

  // Stop the notify session twice
  notify_sessions_[0]->Stop(GetStopNotifyCheckForPrecedingCalls(1));
  notify_sessions_[0]->Stop(GetStopNotifyCheckForPrecedingCalls(2));

  ExpectedChangeNotifyValueAttempts(2);
  ExpectedNotifyValue(NotifyValueState::NONE);

  // Start another notify session
  characteristic1_->StartNotifySession(
      GetNotifyCheckForPrecedingCalls(3),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  ExpectedChangeNotifyValueAttempts(2);
  ExpectedNotifyValue(NotifyValueState::NONE);

  SimulateGattNotifySessionStopped(characteristic1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(notify_sessions_[0]->IsActive());

  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();

  ExpectedChangeNotifyValueAttempts(3);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);

  // Check the notify state
  ASSERT_EQ(2u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  ASSERT_TRUE(notify_sessions_[1]);
  EXPECT_TRUE(notify_sessions_[1]->IsActive());
  EXPECT_TRUE(characteristic1_->IsNotifying());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_Reentrant_Success_Stop \
  StopNotifySession_Reentrant_Success_Stop
#else
#define MAYBE_StopNotifySession_Reentrant_Success_Stop \
  DISABLED_StopNotifySession_Reentrant_Success_Stop
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StopNotifySession_Reentrant_Success_Stop) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_Reentrant_Success_Stop) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(
      FakeCharacteristicBoilerplate(/* properties: NOTIFY */ 0x10));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, characteristic1_->GetDescriptors().size());

  // Start notify session
  characteristic1_->StartNotifySession(
      base::BindOnce(
          [](BluetoothRemoteGattCharacteristic::NotifySessionCallback
                 notify_callback,
             base::OnceClosure stop_notify_callback,
             std::unique_ptr<BluetoothGattNotifySession> session) {
            BluetoothGattNotifySession* s = session.get();
            std::move(notify_callback).Run(std::move(session));
            s->Stop(std::move(stop_notify_callback));
          },
          GetNotifyCallback(Call::EXPECTED),
          GetStopNotifyCallback(Call::EXPECTED)),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  EXPECT_TRUE(characteristic1_->IsNotifying());

  SimulateGattNotifySessionStopped(characteristic1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  EXPECT_FALSE(characteristic1_->IsNotifying());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_Reentrant_Stop_StartSuccess \
  StopNotifySession_Reentrant_Stop_StartSuccess
#else
#define MAYBE_StopNotifySession_Reentrant_Stop_StartSuccess \
  DISABLED_StopNotifySession_Reentrant_Stop_StartSuccess
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StopNotifySession_Reentrant_Stop_StartSuccess) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_Reentrant_Stop_StartSuccess) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));

  // Check that the notify session is active.
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_EQ(characteristic1_, notify_sessions_[0]->GetCharacteristic());
  EXPECT_TRUE(characteristic1_->IsNotifying());

  notify_sessions_[0]->Stop(base::BindOnce(
      [](BluetoothRemoteGattCharacteristic* characteristic,
         BluetoothRemoteGattCharacteristic::NotifySessionCallback
             notify_callback,
         BluetoothRemoteGattCharacteristic::ErrorCallback error_callback) {
        characteristic->StartNotifySession(std::move(notify_callback),
                                           std::move(error_callback));
      },
      characteristic1_, GetNotifyCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED)));

  SimulateGattNotifySessionStopped(characteristic1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  EXPECT_FALSE(characteristic1_->IsNotifying());

  SimulateGattNotifySessionStarted(characteristic1_);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  ASSERT_TRUE(notify_sessions_[1]);
  EXPECT_EQ(characteristic1_, notify_sessions_[0]->GetCharacteristic());
  EXPECT_EQ(characteristic1_, notify_sessions_[1]->GetCharacteristic());
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  EXPECT_TRUE(notify_sessions_[1]->IsActive());
  EXPECT_TRUE(characteristic1_->IsNotifying());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_StopNotifySession_Reentrant_Stop_StartError \
  StopNotifySession_Reentrant_Stop_StartError
#else
#define MAYBE_StopNotifySession_Reentrant_Stop_StartError \
  DISABLED_StopNotifySession_Reentrant_Stop_StartError
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       StopNotifySession_Reentrant_Stop_StartError) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySession_Reentrant_Stop_StartError) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));

  // Check that the notify session is active.
  EXPECT_TRUE(notify_sessions_[0]->IsActive());
  EXPECT_EQ(characteristic1_->GetIdentifier(),
            notify_sessions_[0]->GetCharacteristicIdentifier());
  EXPECT_EQ(characteristic1_, notify_sessions_[0]->GetCharacteristic());
  EXPECT_TRUE(characteristic1_->IsNotifying());

  notify_sessions_[0]->Stop(base::BindOnce(
      [](BluetoothRemoteGattCharacteristic* characteristic,
         BluetoothRemoteGattCharacteristic::NotifySessionCallback
             notify_callback,
         BluetoothRemoteGattCharacteristic::ErrorCallback error_callback) {
        characteristic->StartNotifySession(std::move(notify_callback),
                                           std::move(error_callback));
      },
      characteristic1_, GetNotifyCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED)));

  SimulateGattNotifySessionStopped(characteristic1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  EXPECT_FALSE(characteristic1_->IsNotifying());

  SimulateGattNotifySessionStartError(
      characteristic1_, BluetoothRemoteGattService::GATT_ERROR_FAILED);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, notify_sessions_.size());
  ASSERT_TRUE(notify_sessions_[0]);
  EXPECT_EQ(characteristic1_, notify_sessions_[0]->GetCharacteristic());
  EXPECT_FALSE(notify_sessions_[0]->IsActive());
  EXPECT_FALSE(characteristic1_->IsNotifying());
}

#if defined(OS_MACOSX) || defined(OS_WIN)
#define MAYBE_GattCharacteristicAdded GattCharacteristicAdded
#else
#define MAYBE_GattCharacteristicAdded DISABLED_GattCharacteristicAdded
#endif
// TODO(786473) Android should report that services are discovered when a
// characteristic is added, but currently does not.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt, GattCharacteristicAdded) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_GattCharacteristicAdded) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());
  TestBluetoothAdapterObserver observer(adapter_);

  SimulateGattCharacteristic(service_, kTestUUIDDeviceName, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.gatt_services_discovered_count());
}

#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_GattCharacteristicValueChanged GattCharacteristicValueChanged
#else
#define MAYBE_GattCharacteristicValueChanged \
  DISABLED_GattCharacteristicValueChanged
#endif
// Tests Characteristic Value changes during a Notify Session.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       GattCharacteristicValueChanged) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_GattCharacteristicValueChanged) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));

  TestBluetoothAdapterObserver observer(adapter_);

  std::vector<uint8_t> test_vector1, test_vector2;
  test_vector1.push_back(111);
  test_vector2.push_back(222);

  SimulateGattCharacteristicChanged(characteristic1_, test_vector1);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(test_vector1, characteristic1_->GetValue());

  SimulateGattCharacteristicChanged(characteristic1_, test_vector2);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(test_vector2, characteristic1_->GetValue());
}

#if defined(OS_ANDROID)
#define MAYBE_TwoGattCharacteristicValueChanges \
  TwoGattCharacteristicValueChanges
#else
#define MAYBE_TwoGattCharacteristicValueChanges \
  DISABLED_TwoGattCharacteristicValueChanges
#endif
// Tests that Characteristic value changes arriving consecutively result in
// two notifications with correct values.
// macOS: Does not apply. All events arrive on the UI Thread.
// TODO(crbug.com/694102): Enable this test on Windows.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_TwoGattCharacteristicValueChanges) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));

  TestBluetoothAdapterObserver observer(adapter_);

  std::vector<uint8_t> test_vector1({111});
  std::vector<uint8_t> test_vector2({222});

  SimulateGattCharacteristicChanged(characteristic1_, test_vector1);
  SimulateGattCharacteristicChanged(characteristic1_, test_vector2);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, observer.gatt_characteristic_value_changed_count());
  EXPECT_EQ(test_vector2, characteristic1_->GetValue());
  EXPECT_EQ(std::vector<std::vector<uint8_t>>({test_vector1, test_vector2}),
            observer.previous_characteristic_value_changed_values());
}

#if defined(OS_ANDROID)
#define MAYBE_GattCharacteristicValueChanged_AfterDeleted \
  GattCharacteristicValueChanged_AfterDeleted
#else
#define MAYBE_GattCharacteristicValueChanged_AfterDeleted \
  DISABLED_GattCharacteristicValueChanged_AfterDeleted
#endif
// Tests Characteristic Value changing after a Notify Session and objects being
// destroyed.
// macOS: Not applicable: This can never happen if CBPeripheral delegate is set
// to nil.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWin32Only,
       GattCharacteristicValueChanged_AfterDeleted) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_GattCharacteristicValueChanged_AfterDeleted) {
#endif
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));
  TestBluetoothAdapterObserver observer(adapter_);

  RememberCharacteristicForSubsequentAction(characteristic1_);
  DeleteDevice(device_);  // TODO(576906) delete only the characteristic.

  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicChanged(/* use remembered characteristic */ nullptr,
                                    empty_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE("Did not crash!");
  EXPECT_EQ(0, observer.gatt_characteristic_value_changed_count());
}

// Tests that closing the GATT connection during a characteristic
// value notification is safe.
#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_GattCharacteristicValueChanged_DisconnectDuring \
  GattCharacteristicValueChanged_DisconnectDuring
#else
#define MAYBE_GattCharacteristicValueChanged_DisconnectDuring \
  DISABLED_GattCharacteristicValueChanged_DisconnectDuring
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       GattCharacteristicValueChanged_DisconnectDuring) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_GattCharacteristicValueChanged_DisconnectDuring) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));
  MockBluetoothAdapter::Observer observer1(adapter_);
  MockBluetoothAdapter::Observer observer2(adapter_);

  // |observer1| will be notified first and close the GATT connection which
  // may prevent |observer2| from being notified if |characteristic1_| has been
  // freed.
  base::RunLoop loop;
  EXPECT_CALL(observer1, GattCharacteristicValueChanged(adapter_.get(),
                                                        characteristic1_, _))
      .WillOnce(
          Invoke([&](BluetoothAdapter*, BluetoothRemoteGattCharacteristic*,
                     const std::vector<uint8_t>& value) {
            gatt_connections_[0]->Disconnect();
            loop.Quit();
          }));
  EXPECT_CALL(observer2, GattCharacteristicValueChanged(adapter_.get(),
                                                        characteristic1_, _))
      .Times(testing::AtMost(1))
      .WillRepeatedly(
          Invoke([&](BluetoothAdapter*,
                     BluetoothRemoteGattCharacteristic* characteristic,
                     const std::vector<uint8_t>& value) {
            // Call a method on |characteristic| to check the pointer is still
            // valid.
            EXPECT_EQ(value, characteristic->GetValue());
          }));

  std::vector<uint8_t> empty_vector;
  SimulateGattCharacteristicChanged(characteristic1_, empty_vector);
  loop.Run();
}

#if defined(OS_ANDROID)
#define MAYBE_GetDescriptors_FindNone GetDescriptors_FindNone
#else
#define MAYBE_GetDescriptors_FindNone DISABLED_GetDescriptors_FindNone
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt, GetDescriptors_FindNone) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_GetDescriptors_FindNone) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  EXPECT_EQ(0u, characteristic1_->GetDescriptors().size());
}

#if defined(OS_ANDROID)
#define MAYBE_GetDescriptors_and_GetDescriptor GetDescriptors_and_GetDescriptor
#else
#define MAYBE_GetDescriptors_and_GetDescriptor \
  DISABLED_GetDescriptors_and_GetDescriptor
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt,
       GetDescriptors_and_GetDescriptor) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_GetDescriptors_and_GetDescriptor) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  // Add several Descriptors:
  BluetoothUUID uuid1(kTestUUIDCharacteristicUserDescription);
  BluetoothUUID uuid2(kTestUUIDClientCharacteristicConfiguration);
  BluetoothUUID uuid3(kTestUUIDServerCharacteristicConfiguration);
  BluetoothUUID uuid4(kTestUUIDCharacteristicPresentationFormat);
  SimulateGattDescriptor(characteristic1_, uuid1.canonical_value());
  SimulateGattDescriptor(characteristic1_, uuid2.canonical_value());
  SimulateGattDescriptor(characteristic2_, uuid3.canonical_value());
  SimulateGattDescriptor(characteristic2_, uuid4.canonical_value());
  base::RunLoop().RunUntilIdle();

  // Verify that GetDescriptor can retrieve descriptors again by ID,
  // and that the same Descriptor is returned when searched by ID.
  EXPECT_EQ(2u, characteristic1_->GetDescriptors().size());
  EXPECT_EQ(2u, characteristic2_->GetDescriptors().size());
  std::string c1_id1 = characteristic1_->GetDescriptors()[0]->GetIdentifier();
  std::string c1_id2 = characteristic1_->GetDescriptors()[1]->GetIdentifier();
  std::string c2_id1 = characteristic2_->GetDescriptors()[0]->GetIdentifier();
  std::string c2_id2 = characteristic2_->GetDescriptors()[1]->GetIdentifier();
  BluetoothUUID c1_uuid1 = characteristic1_->GetDescriptors()[0]->GetUUID();
  BluetoothUUID c1_uuid2 = characteristic1_->GetDescriptors()[1]->GetUUID();
  BluetoothUUID c2_uuid1 = characteristic2_->GetDescriptors()[0]->GetUUID();
  BluetoothUUID c2_uuid2 = characteristic2_->GetDescriptors()[1]->GetUUID();
  EXPECT_EQ(c1_uuid1, characteristic1_->GetDescriptor(c1_id1)->GetUUID());
  EXPECT_EQ(c1_uuid2, characteristic1_->GetDescriptor(c1_id2)->GetUUID());
  EXPECT_EQ(c2_uuid1, characteristic2_->GetDescriptor(c2_id1)->GetUUID());
  EXPECT_EQ(c2_uuid2, characteristic2_->GetDescriptor(c2_id2)->GetUUID());

  // GetDescriptors & GetDescriptor return the same object for the same ID:
  EXPECT_EQ(characteristic1_->GetDescriptors()[0],
            characteristic1_->GetDescriptor(c1_id1));
  EXPECT_EQ(characteristic1_->GetDescriptor(c1_id1),
            characteristic1_->GetDescriptor(c1_id1));

  // Characteristic 1 has descriptor uuids 1 and 2 (we don't know the order).
  EXPECT_TRUE(c1_uuid1 == uuid1 || c1_uuid2 == uuid1);
  EXPECT_TRUE(c1_uuid1 == uuid2 || c1_uuid2 == uuid2);
  // ... but not uuid 3
  EXPECT_FALSE(c1_uuid1 == uuid3 || c1_uuid2 == uuid3);
}

#if defined(OS_ANDROID)
#define MAYBE_GetDescriptorsByUUID GetDescriptorsByUUID
#else
#define MAYBE_GetDescriptorsByUUID DISABLED_GetDescriptorsByUUID
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrt, GetDescriptorsByUUID) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_GetDescriptorsByUUID) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }

  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate());

  // Add several Descriptors:
  BluetoothUUID id1(kTestUUIDCharacteristicUserDescription);
  BluetoothUUID id2(kTestUUIDClientCharacteristicConfiguration);
  BluetoothUUID id3(kTestUUIDServerCharacteristicConfiguration);
  SimulateGattDescriptor(characteristic1_, id1.canonical_value());
  SimulateGattDescriptor(characteristic1_, id2.canonical_value());
  SimulateGattDescriptor(characteristic2_, id3.canonical_value());
  SimulateGattDescriptor(characteristic2_, id3.canonical_value());
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(characteristic2_->GetDescriptorsByUUID(id3).at(0)->GetIdentifier(),
            characteristic2_->GetDescriptorsByUUID(id3).at(1)->GetIdentifier());

  EXPECT_EQ(id1, characteristic1_->GetDescriptorsByUUID(id1).at(0)->GetUUID());
  EXPECT_EQ(id2, characteristic1_->GetDescriptorsByUUID(id2).at(0)->GetUUID());
  EXPECT_EQ(id3, characteristic2_->GetDescriptorsByUUID(id3).at(0)->GetUUID());
  EXPECT_EQ(id3, characteristic2_->GetDescriptorsByUUID(id3).at(1)->GetUUID());
  EXPECT_EQ(1u, characteristic1_->GetDescriptorsByUUID(id1).size());
  EXPECT_EQ(1u, characteristic1_->GetDescriptorsByUUID(id2).size());
  EXPECT_EQ(2u, characteristic2_->GetDescriptorsByUUID(id3).size());

  EXPECT_EQ(0u, characteristic2_->GetDescriptorsByUUID(id1).size());
  EXPECT_EQ(0u, characteristic2_->GetDescriptorsByUUID(id2).size());
  EXPECT_EQ(0u, characteristic1_->GetDescriptorsByUUID(id3).size());
}

#if defined(OS_ANDROID)
#define MAYBE_ReadDuringDisconnect ReadDuringDisconnect
#else
#define MAYBE_ReadDuringDisconnect DISABLED_ReadDuringDisconnect
#endif
// Tests that read requests after a device disconnects but before the disconnect
// task has a chance to run result in an error.
// macOS: Does not apply. All events arrive on the UI Thread.
// TODO(crbug.com/694102): Enable this test on Windows.
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_ReadDuringDisconnect) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  SimulateGattDisconnection(device_);
  // Do not yet call RunUntilIdle() to process the disconnect task.
  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);
}

#if defined(OS_ANDROID)
#define MAYBE_WriteDuringDisconnect WriteDuringDisconnect
#else
#define MAYBE_WriteDuringDisconnect DISABLED_WriteDuringDisconnect
#endif
// Tests that write requests after a device disconnects but before the
// disconnect task runs result in an error.
// macOS: Does not apply. All events arrive on the UI Thread.
// TODO(crbug.com/694102): Enable this test on Windows.
TEST_F(BluetoothRemoteGattCharacteristicTest, MAYBE_WriteDuringDisconnect) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  SimulateGattDisconnection(device_);
  // Do not yet call RunUntilIdle() to process the disconnect task.
  characteristic1_->WriteRemoteCharacteristic(
      std::vector<uint8_t>(), GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);
}

#if defined(OS_MACOSX)
#define MAYBE_WriteWithoutResponseOnlyCharacteristic_WriteRemoteCharacteristicDuringDisconnect \
  WriteWithoutResponseOnlyCharacteristic_WriteRemoteCharacteristicDuringDisconnect
#else
#define MAYBE_WriteWithoutResponseOnlyCharacteristic_WriteRemoteCharacteristicDuringDisconnect \
  DISABLED_WriteWithoutResponseOnlyCharacteristic_WriteRemoteCharacteristicDuringDisconnect
#endif
// Tests that writing without response during a disconnect results in an error.
// Only applies to macOS and WinRT whose events arrive all on the UI thread. See
// other *DuringDisconnect tests for Android and Windows whose events arrive on
// a different thread.
#if defined(OS_WIN)
TEST_P(
    BluetoothRemoteGattCharacteristicTestWinrtOnly,
    WriteWithoutResponseOnlyCharacteristic_WriteRemoteCharacteristicDuringDisconnect) {
#else
TEST_F(
    BluetoothRemoteGattCharacteristicTest,
    MAYBE_WriteWithoutResponseOnlyCharacteristic_WriteRemoteCharacteristicDuringDisconnect) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE));

  characteristic1_->WriteRemoteCharacteristic(
      std::vector<uint8_t>(), GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  SimulateDeviceBreaksConnection(adapter_->GetDevices()[0]);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);
}

// Tests that closing the GATT connection when a characteristic value write
// fails due to a disconnect is safe.
#if defined(OS_ANDROID) || defined(OS_MACOSX)
#define MAYBE_WriteWithoutResponseOnlyCharacteristic_CloseConnectionDuringDisconnect \
  WriteWithoutResponseOnlyCharacteristic_CloseConnectionDuringDisconnect
#else
#define MAYBE_WriteWithoutResponseOnlyCharacteristic_CloseConnectionDuringDisconnect \
  DISABLED_WriteWithoutResponseOnlyCharacteristic_CloseConnectionDuringDisconnect
#endif
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       WriteWithoutResponseOnlyCharacteristic_CloseConnectionDuringDisconnect) {
#else
TEST_F(
    BluetoothRemoteGattCharacteristicTest,
    MAYBE_WriteWithoutResponseOnlyCharacteristic_CloseConnectionDuringDisconnect) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE));

  base::RunLoop loop;
  characteristic1_->WriteRemoteCharacteristic(
      std::vector<uint8_t>(), GetCallback(Call::NOT_EXPECTED),
      base::BindLambdaForTesting(
          [&](BluetoothGattService::GattErrorCode error_code) {
            EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
                      error_code);
            gatt_connections_[0]->Disconnect();
            loop.Quit();
          }));
  SimulateDeviceBreaksConnection(adapter_->GetDevices()[0]);
  loop.Run();
}

#if defined(OS_MACOSX)
#define MAYBE_WriteWithoutResponseOnlyCharacteristic_DisconnectCalledDuringWriteRemoteCharacteristic \
  WriteWithoutResponseOnlyCharacteristic_DisconnectCalledDuringWriteRemoteCharacteristic
#else
#define MAYBE_WriteWithoutResponseOnlyCharacteristic_DisconnectCalledDuringWriteRemoteCharacteristic \
  DISABLED_WriteWithoutResponseOnlyCharacteristic_DisconnectCalledDuringWriteRemoteCharacteristic
#endif
// Tests that disconnecting right after a write without response results in an
// error.
// TODO(crbug.com/726534): Enable on other platforms depending on the resolution
// of crbug.com/726534.
TEST_F(
    BluetoothRemoteGattCharacteristicTest,
    MAYBE_WriteWithoutResponseOnlyCharacteristic_DisconnectCalledDuringWriteRemoteCharacteristic) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE));

  characteristic1_->WriteRemoteCharacteristic(
      std::vector<uint8_t>(), GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  gatt_connections_[0]->Disconnect();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);

  SimulateGattDisconnection(device_);
  base::RunLoop().RunUntilIdle();
}

#if defined(OS_MACOSX)
#define MAYBE_WriteWithoutResponseOnlyCharacteristic_DisconnectCalledBeforeWriteRemoteCharacteristic \
  WriteWithoutResponseOnlyCharacteristic_DisconnectCalledBeforeWriteRemoteCharacteristic
#else
#define MAYBE_WriteWithoutResponseOnlyCharacteristic_DisconnectCalledBeforeWriteRemoteCharacteristic \
  DISABLED_WriteWithoutResponseOnlyCharacteristic_DisconnectCalledBeforeWriteRemoteCharacteristic
#endif
// Tests that disconnecting right before a write without response results in an
// error.
// TODO(crbug.com/726534): Enable on other platforms depending on the resolution
// of crbug.com/726534.
TEST_F(
    BluetoothRemoteGattCharacteristicTest,
    MAYBE_WriteWithoutResponseOnlyCharacteristic_DisconnectCalledBeforeWriteRemoteCharacteristic) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE));

  gatt_connections_[0]->Disconnect();
  characteristic1_->WriteRemoteCharacteristic(
      std::vector<uint8_t>(), GetCallback(Call::NOT_EXPECTED),
      GetGattErrorCallback(Call::EXPECTED));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);

  SimulateGattDisconnection(device_);
  base::RunLoop().RunUntilIdle();
}

#if defined(OS_MACOSX)
#define MAYBE_WriteWithoutResponse_PropertyNotPresent \
  WriteWithoutResponse_PropertyNotPresent
#else
#define MAYBE_WriteWithoutResponse_PropertyNotPresent \
  DISABLED_WriteWithoutResponse_PropertyNotPresent
#endif
// Tests that WriteWithoutResponse fails when a characteristic does not have the
// required property.
// TODO(https://crbug.com/831524): Enable for other platforms once supported.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       WriteWithoutResponse_PropertyNotPresent) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_WriteWithoutResponse_PropertyNotPresent) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  std::vector<uint8_t> test_vector = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  EXPECT_FALSE(characteristic1_->WriteWithoutResponse(test_vector));
}

#if defined(OS_MACOSX)
#define MAYBE_WriteWithoutResponse_PendingWrite \
  WriteWithoutResponse_PendingWrite
#else
#define MAYBE_WriteWithoutResponse_PendingWrite \
  DISABLED_WriteWithoutResponse_PendingWrite
#endif
// Tests that WriteWithoutResponse fails when a characteristic already has a
// pending write.
// TODO(https://crbug.com/831524): Enable for other platforms once supported.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       WriteWithoutResponse_PendingWrite) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_WriteWithoutResponse_PendingWrite) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE |
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE));

  std::vector<uint8_t> test_vector = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  characteristic1_->WriteRemoteCharacteristic(
      test_vector, GetCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  // Explicitly make sure that the callback has not been invoked yet. Since
  // WriteRemoteCharacteristic is still pending, this results in an error for
  // WriteWithoutResponse.
  EXPECT_EQ(0, callback_count_);
  EXPECT_FALSE(characteristic1_->WriteWithoutResponse(test_vector));

  // Test that the failed WriteWithoutReponse request does not interfere with
  // the pending WriteRemoteCharacteristic request.
  SimulateGattCharacteristicWrite(characteristic1_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(test_vector, last_write_value_);
}

#if defined(OS_MACOSX)
#define MAYBE_WriteWithoutResponse_PendingRead WriteWithoutResponse_PendingRead
#else
#define MAYBE_WriteWithoutResponse_PendingRead \
  DISABLED_WriteWithoutResponse_PendingRead
#endif
// Tests that WriteWithoutResponse fails when a characteristic already has a
// pending read.
// TODO(https://crbug.com/831524): Enable for other platforms once supported.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       WriteWithoutResponse_PendingRead) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_WriteWithoutResponse_PendingRead) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_READ |
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE));

  characteristic1_->ReadRemoteCharacteristic(
      GetReadValueCallback(Call::EXPECTED),
      GetGattErrorCallback(Call::NOT_EXPECTED));

  // Explicitly make sure that the callback has not been invoked yet. Since
  // ReadRemoteCharacteristic is still pending, this results in an error for
  // WriteWithoutResponse.
  EXPECT_EQ(0, callback_count_);
  std::vector<uint8_t> test_vector = {0, 1, 2, 3, 4, 0xf, 0xf0, 0xff};
  EXPECT_FALSE(characteristic1_->WriteWithoutResponse(test_vector));

  // Test that the failed WriteWithoutReponse request does not interfere with
  // the pending ReadRemoteCharacteristic request.
  SimulateGattCharacteristicRead(characteristic1_, test_vector);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(test_vector, last_read_value_);
}

#if defined(OS_MACOSX)
#define MAYBE_WriteWithoutResponse_Success WriteWithoutResponse_Success
#else
#define MAYBE_WriteWithoutResponse_Success DISABLED_WriteWithoutResponse_Success
#endif
// Tests that WriteWithoutResponse indicates success if the proper conditions
// are met.
// TODO(https://crbug.com/831524): Enable for other platforms once supported.
#if defined(OS_WIN)
TEST_P(BluetoothRemoteGattCharacteristicTestWinrtOnly,
       WriteWithoutResponse_Success) {
#else
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_WriteWithoutResponse_Success) {
#endif
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE));

  std::vector<uint8_t> test_vector = {1, 2, 3, 4};
  EXPECT_TRUE(characteristic1_->WriteWithoutResponse(test_vector));
  EXPECT_EQ(test_vector, last_write_value_);
}

#if defined(OS_ANDROID)
#define MAYBE_StartNotifySessionDuringDisconnect \
  StartNotifySessionDuringDisconnect
#else
#define MAYBE_StartNotifySessionDuringDisconnect \
  DISABLED_StartNotifySessionDuringDisconnect
#endif
// Tests that start notifications requests after a device disconnects but before
// the disconnect task runs result in an error.
// macOS: Does not apply. All events arrive on the UI Thread.
// TODO(crbug.com/694102): Enable this test on Windows.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StartNotifySessionDuringDisconnect) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY));
  SimulateGattDescriptor(
      characteristic1_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());

  SimulateGattDisconnection(device_);
  // Don't run the disconnect task.
  characteristic1_->StartNotifySession(GetNotifyCallback(Call::NOT_EXPECTED),
                                       GetGattErrorCallback(Call::EXPECTED));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BluetoothRemoteGattService::GATT_ERROR_FAILED,
            last_gatt_error_code_);
}

#if defined(OS_ANDROID)
#define MAYBE_StopNotifySessionDuringDisconnect \
  StopNotifySessionDuringDisconnect
#else
#define MAYBE_StopNotifySessionDuringDisconnect \
  DISABLED_StopNotifySessionDuringDisconnect
#endif
// Tests that stop notifications requests after a device disconnects but before
// the disconnect task runs do not result in a crash.
// macOS: Does not apply. All events arrive on the UI Thread.
// TODO(crbug.com/694102): Enable this test on Windows.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_StopNotifySessionDuringDisconnect) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));

  SimulateGattDisconnection(device_);
  // Don't run the disconnect task.
  notify_sessions_[0]->Stop(GetStopNotifyCallback(Call::EXPECTED));
  base::RunLoop().RunUntilIdle();
}

#if defined(OS_ANDROID)
#define MAYBE_DeleteNotifySessionDuringDisconnect \
  DeleteNotifySessionDuringDisconnect
#else
#define MAYBE_DeleteNotifySessionDuringDisconnect \
  DISABLED_DeleteNotifySessionDuringDisconnect
#endif
// Tests that deleting notify sessions after a device disconnects but before the
// disconnect task runs do not result in a crash.
// macOS: Does not apply. All events arrive on the UI Thread.
// TODO(crbug.com/694102): Enable this test on Windows.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       MAYBE_DeleteNotifySessionDuringDisconnect) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  ASSERT_NO_FATAL_FAILURE(StartNotifyBoilerplate(
      /* properties: NOTIFY */ 0x10, NotifyValueState::NOTIFY));

  SimulateGattDisconnection(device_);
  // Don't run the disconnect task.
  notify_sessions_.clear();
  base::RunLoop().RunUntilIdle();
}

#if defined(OS_MACOSX)
// Tests to receive a services changed notification from macOS, while
// discovering descriptors. This test simulate having 2 descriptor scan at the
// same time. Only once both descriptor scanning is done, the gatt device is
// ready.
// Android: This test doesn't apply to Android because there is no services
// changed event that could arrive during a discovery procedure.
TEST_F(BluetoothRemoteGattCharacteristicTest,
       SimulateDeviceModificationWhileDiscoveringDescriptors) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));

  TestBluetoothAdapterObserver observer(adapter_);

  // Starts first discovery process.
  SimulateGattConnection(device);
  EXPECT_EQ(1, observer.device_changed_count());
  AddServicesToDeviceMac(device, {kTestUUIDHeartRate});
  SimulateDidDiscoverServicesMac(device);
  EXPECT_EQ(1u, device->GetGattServices().size());
  BluetoothRemoteGattService* service = device->GetGattServices()[0];
  std::string characteristic_uuid = "11111111-0000-1000-8000-00805f9b34fb";
  AddCharacteristicToServiceMac(service, characteristic_uuid,
                                /* properties */ 0);
  SimulateDidDiscoverCharacteristicsMac(service);
  EXPECT_EQ(1u, service->GetCharacteristics().size());
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristics()[0];
  std::string descriptor_uuid = "22222222-0000-1000-8000-00805f9b34fb";
  AddDescriptorToCharacteristicMac(characteristic, descriptor_uuid);
  // Now waiting for descriptor discovery.

  // Starts second discovery process.
  SimulateGattServicesChanged(device);
  EXPECT_EQ(2, observer.device_changed_count());
  SimulateDidDiscoverServicesMac(device);
  SimulateDidDiscoverCharacteristicsMac(service);
  // Now waiting for a second descriptor discovery.

  // Finish discovery process.
  // First system call to -[id<CBPeripheralDelegate>
  // peripheral:didDiscoverDescriptorsForCharacteristic:error:]
  SimulateDidDiscoverDescriptorsMac(characteristic);
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1u, service->GetCharacteristics().size());
  EXPECT_EQ(1u, characteristic->GetDescriptors().size());
  EXPECT_EQ(2, observer.device_changed_count());

  // Second system call to -[id<CBPeripheralDelegate>
  // peripheral:didDiscoverDescriptorsForCharacteristic:error:]
  // Finish second discovery process.
  observer.Reset();
  SimulateDidDiscoverDescriptorsMac(characteristic);
  EXPECT_EQ(1, observer.gatt_service_changed_count());
  EXPECT_EQ(1, observer.device_changed_count());
}
#endif  // defined(OS_MACOSX)

#if defined(OS_MACOSX)
// Simulates to receive an extra discovery descriptor notifications from  macOS.
// Those notifications should be ignored.
// Android: This test doesn't apply to Android because there is no services
// changed event that could arrive during a discovery procedure.
TEST_F(BluetoothRemoteGattCharacteristicTest, ExtraDidDiscoverDescriptorsCall) {
  if (!PlatformSupportsLowEnergy()) {
    LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
    return;
  }
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(GetGattConnectionCallback(Call::EXPECTED),
                               GetConnectErrorCallback(Call::NOT_EXPECTED));

  TestBluetoothAdapterObserver observer(adapter_);

  // Starts first discovery process.
  SimulateGattConnection(device);
  AddServicesToDeviceMac(device, {kTestUUIDHeartRate});
  SimulateDidDiscoverServicesMac(device);
  EXPECT_EQ(1u, device->GetGattServices().size());
  BluetoothRemoteGattService* service = device->GetGattServices()[0];
  std::string characteristic_uuid = "11111111-0000-1000-8000-00805f9b34fb";
  AddCharacteristicToServiceMac(service, characteristic_uuid,
                                /* properties */ 0);
  SimulateDidDiscoverCharacteristicsMac(service);
  EXPECT_EQ(1u, service->GetCharacteristics().size());
  BluetoothRemoteGattCharacteristic* characteristic =
      service->GetCharacteristics()[0];
  std::string descriptor_uuid = "22222222-0000-1000-8000-00805f9b34fb";
  AddDescriptorToCharacteristicMac(characteristic, descriptor_uuid);
  SimulateDidDiscoverDescriptorsMac(characteristic);
  EXPECT_EQ(1, observer.gatt_service_changed_count());
  EXPECT_EQ(1u, service->GetCharacteristics().size());
  EXPECT_EQ(1u, characteristic->GetDescriptors().size());

  observer.Reset();
  SimulateDidDiscoverDescriptorsMac(characteristic);  // Extra system call.
  SimulateGattServicesChanged(device);
  SimulateDidDiscoverDescriptorsMac(characteristic);  // Extra system call.
  SimulateDidDiscoverServicesMac(device);
  SimulateDidDiscoverDescriptorsMac(characteristic);  // Extra system call.
  SimulateDidDiscoverCharacteristicsMac(service);
  SimulateDidDiscoverDescriptorsMac(characteristic);
  SimulateDidDiscoverDescriptorsMac(characteristic);  // Extra system call.
  EXPECT_EQ(2, observer.device_changed_count());
}
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    BluetoothRemoteGattCharacteristicTestWinrt,
    ::testing::Bool());

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    BluetoothRemoteGattCharacteristicTestWin32Only,
    ::testing::Values(false));

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    BluetoothRemoteGattCharacteristicTestWinrtOnly,
    ::testing::Values(true));
#endif  // defined(OS_WIN)

}  // namespace device
