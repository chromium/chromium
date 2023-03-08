// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
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

class BluetoothRemoteGattServiceTest :
#if BUILDFLAG(IS_WIN)
    public BluetoothTestWinrt {
#else
    public BluetoothTest {
#endif
 public:
  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    BluetoothTestWinrt::SetUp();
#else
    BluetoothTest::SetUp();
#endif
    if (!PlatformSupportsLowEnergy()) {
      GTEST_SKIP() << "Bluetooth Low Energy unavailable.";
    }
  }
};

#if BUILDFLAG(IS_WIN)
using BluetoothRemoteGattServiceTestWinrt = BluetoothRemoteGattServiceTest;
#endif

// Android is excluded because it fires a single discovery event per device.
#if BUILDFLAG(IS_APPLE)
#define MAYBE_IsDiscoveryComplete IsDiscoveryComplete
#else
#define MAYBE_IsDiscoveryComplete DISABLED_IsDiscoveryComplete
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothRemoteGattServiceTestWinrt, IsDiscoveryComplete) {
#else
TEST_F(BluetoothRemoteGattServiceTest, MAYBE_IsDiscoveryComplete) {
#endif
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(1);
  device->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));
  SimulateGattConnection(device);
  base::RunLoop().RunUntilIdle();
  SimulateGattServicesDiscovered(
      device, std::vector<std::string>(
                  {kTestUUIDGenericAccess, kTestUUIDGenericAccess}));
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattService* service = device->GetGattServices()[0];
  EXPECT_TRUE(service->IsDiscoveryComplete());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_GetIdentifier GetIdentifier
#else
#define MAYBE_GetIdentifier DISABLED_GetIdentifier
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothRemoteGattServiceTestWinrt, GetIdentifier) {
#else
TEST_F(BluetoothRemoteGattServiceTest, MAYBE_GetIdentifier) {
#endif
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  // 2 devices to verify unique IDs across them.
  BluetoothDevice* device1 = SimulateLowEnergyDevice(3);
  BluetoothDevice* device2 = SimulateLowEnergyDevice(4);
  device1->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));
  device2->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));
  SimulateGattConnection(device1);
  SimulateGattConnection(device2);
  base::RunLoop().RunUntilIdle();

  // 2 duplicate UUIDs creating 2 service instances on each device.
  SimulateGattServicesDiscovered(
      device1, std::vector<std::string>(
                   {kTestUUIDGenericAccess, kTestUUIDGenericAccess}));
  SimulateGattServicesDiscovered(
      device2, std::vector<std::string>(
                   {kTestUUIDGenericAccess, kTestUUIDGenericAccess}));
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattService* service1 = device1->GetGattServices()[0];
  BluetoothRemoteGattService* service2 = device1->GetGattServices()[1];
  BluetoothRemoteGattService* service3 = device2->GetGattServices()[0];
  BluetoothRemoteGattService* service4 = device2->GetGattServices()[1];

  // All IDs are unique, even though they have the same UUID.
  EXPECT_NE(service1->GetIdentifier(), service2->GetIdentifier());
  EXPECT_NE(service1->GetIdentifier(), service3->GetIdentifier());
  EXPECT_NE(service1->GetIdentifier(), service4->GetIdentifier());

  EXPECT_NE(service2->GetIdentifier(), service3->GetIdentifier());
  EXPECT_NE(service2->GetIdentifier(), service4->GetIdentifier());

  EXPECT_NE(service3->GetIdentifier(), service4->GetIdentifier());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_GetUUID GetUUID
#else
#define MAYBE_GetUUID DISABLED_GetUUID
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothRemoteGattServiceTestWinrt, GetUUID) {
#else
TEST_F(BluetoothRemoteGattServiceTest, MAYBE_GetUUID) {
#endif
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));
  SimulateGattConnection(device);
  base::RunLoop().RunUntilIdle();

  // Create multiple instances with the same UUID.
  BluetoothUUID uuid(kTestUUIDGenericAccess);
  std::vector<std::string> services;
  services.push_back(uuid.canonical_value());
  services.push_back(uuid.canonical_value());
  SimulateGattServicesDiscovered(device, services);
  base::RunLoop().RunUntilIdle();

  // Each has the same UUID.
  EXPECT_EQ(uuid, device->GetGattServices()[0]->GetUUID());
  EXPECT_EQ(uuid, device->GetGattServices()[1]->GetUUID());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_GetCharacteristics_FindNone GetCharacteristics_FindNone
#else
#define MAYBE_GetCharacteristics_FindNone DISABLED_GetCharacteristics_FindNone
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothRemoteGattServiceTestWinrt, GetCharacteristics_FindNone) {
#else
TEST_F(BluetoothRemoteGattServiceTest, MAYBE_GetCharacteristics_FindNone) {
#endif
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));
  SimulateGattConnection(device);
  base::RunLoop().RunUntilIdle();

  // Simulate a service, with no Characteristics:
  SimulateGattServicesDiscovered(
      device, std::vector<std::string>({kTestUUIDGenericAccess}));
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattService* service = device->GetGattServices()[0];

  EXPECT_EQ(0u, service->GetCharacteristics().size());
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_GetCharacteristics_and_GetCharacteristic \
  GetCharacteristics_and_GetCharacteristic
#else
#define MAYBE_GetCharacteristics_and_GetCharacteristic \
  DISABLED_GetCharacteristics_and_GetCharacteristic
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothRemoteGattServiceTestWinrt,
       GetCharacteristics_and_GetCharacteristic) {
#else
TEST_F(BluetoothRemoteGattServiceTest,
       MAYBE_GetCharacteristics_and_GetCharacteristic) {
#endif
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));
  SimulateGattConnection(device);
  base::RunLoop().RunUntilIdle();

  // Simulate a service, with several Characteristics:
  SimulateGattServicesDiscovered(
      device, std::vector<std::string>({kTestUUIDGenericAccess}));
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattService* service = device->GetGattServices()[0];
  SimulateGattCharacteristic(service, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service, kTestUUIDAppearance,
                             /* properties */ 0);
  // Duplicate UUID.
  SimulateGattCharacteristic(service, kTestUUIDAppearance,
                             /* properties */ 0);
  SimulateGattCharacteristic(service, kTestUUIDReconnectionAddress,
                             /* properties */ 0);

  base::RunLoop().RunUntilIdle();
  // Verify that GetCharacteristic can retrieve characteristics again by ID,
  // and that the same Characteristics come back.
  EXPECT_EQ(4u, service->GetCharacteristics().size());
  std::string char_id1 = service->GetCharacteristics()[0]->GetIdentifier();
  std::string char_id2 = service->GetCharacteristics()[1]->GetIdentifier();
  std::string char_id3 = service->GetCharacteristics()[2]->GetIdentifier();
  std::string char_id4 = service->GetCharacteristics()[3]->GetIdentifier();
  BluetoothUUID char_uuid1 = service->GetCharacteristics()[0]->GetUUID();
  BluetoothUUID char_uuid2 = service->GetCharacteristics()[1]->GetUUID();
  BluetoothUUID char_uuid3 = service->GetCharacteristics()[2]->GetUUID();
  BluetoothUUID char_uuid4 = service->GetCharacteristics()[3]->GetUUID();
  EXPECT_EQ(char_uuid1, service->GetCharacteristic(char_id1)->GetUUID());
  EXPECT_EQ(char_uuid2, service->GetCharacteristic(char_id2)->GetUUID());
  EXPECT_EQ(char_uuid3, service->GetCharacteristic(char_id3)->GetUUID());
  EXPECT_EQ(char_uuid4, service->GetCharacteristic(char_id4)->GetUUID());

  // GetCharacteristics & GetCharacteristic return the same object for the same
  // ID:
  EXPECT_EQ(service->GetCharacteristics()[0],
            service->GetCharacteristic(char_id1));
  EXPECT_EQ(service->GetCharacteristic(char_id1),
            service->GetCharacteristic(char_id1));
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
#define MAYBE_GetCharacteristicsByUUID GetCharacteristicsByUUID
#else
#define MAYBE_GetCharacteristicsByUUID DISABLED_GetCharacteristicsByUUID
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothRemoteGattServiceTestWinrt, GetCharacteristicsByUUID) {
#else
TEST_F(BluetoothRemoteGattServiceTest, MAYBE_GetCharacteristicsByUUID) {
#endif
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));
  SimulateGattConnection(device);
  base::RunLoop().RunUntilIdle();

  // Simulate two primary GATT services.
  SimulateGattServicesDiscovered(
      device,
      std::vector<std::string>({kTestUUIDGenericAccess, kTestUUIDHeartRate}));
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattService* service1 = device->GetGattServices()[0];
  BluetoothRemoteGattService* service2 = device->GetGattServices()[1];
  SimulateGattCharacteristic(service1, kTestUUIDDeviceName,
                             /* properties */ 0);
  // 2 duplicate UUIDs creating 2 instances.
  SimulateGattCharacteristic(service2, kTestUUIDHeartRateMeasurement,
                             /* properties */ 0);
  SimulateGattCharacteristic(service2, kTestUUIDHeartRateMeasurement,
                             /* properties */ 0);
  base::RunLoop().RunUntilIdle();

  {
    std::vector<BluetoothRemoteGattCharacteristic*> characteristics =
        service1->GetCharacteristicsByUUID(BluetoothUUID(kTestUUIDDeviceName));
    EXPECT_EQ(1u, characteristics.size());
    EXPECT_EQ(kTestUUIDDeviceName,
              characteristics[0]->GetUUID().canonical_value());
  }

  {
    std::vector<BluetoothRemoteGattCharacteristic*> characteristics =
        service2->GetCharacteristicsByUUID(
            BluetoothUUID(kTestUUIDHeartRateMeasurement));
    EXPECT_EQ(2u, characteristics.size());
    EXPECT_EQ(kTestUUIDHeartRateMeasurement,
              characteristics[0]->GetUUID().canonical_value());
    EXPECT_EQ(kTestUUIDHeartRateMeasurement,
              characteristics[1]->GetUUID().canonical_value());
    EXPECT_NE(characteristics[0]->GetIdentifier(),
              characteristics[1]->GetIdentifier());
  }

  BluetoothUUID characteristic_uuid_not_exist_in_setup(
      "33333333-0000-1000-8000-00805f9b34fb");
  EXPECT_TRUE(
      service1->GetCharacteristicsByUUID(characteristic_uuid_not_exist_in_setup)
          .empty());
  EXPECT_TRUE(
      service2->GetCharacteristicsByUUID(characteristic_uuid_not_exist_in_setup)
          .empty());
}

#if BUILDFLAG(IS_APPLE)
#define MAYBE_GattCharacteristics_ObserversCalls \
  GattCharacteristics_ObserversCalls
#else
#define MAYBE_GattCharacteristics_ObserversCalls \
  DISABLED_GattCharacteristics_ObserversCalls
#endif
// The GattServicesRemoved event is not implemented for WinRT.
TEST_F(BluetoothRemoteGattServiceTest,
       MAYBE_GattCharacteristics_ObserversCalls) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));
  SimulateGattConnection(device);
  base::RunLoop().RunUntilIdle();

  TestBluetoothAdapterObserver observer(adapter_);

  // Simulate a service, with several Characteristics:
  SimulateGattServicesDiscovered(
      device, std::vector<std::string>({kTestUUIDGenericAccess}));
  base::RunLoop().RunUntilIdle();
  BluetoothRemoteGattService* service = device->GetGattServices()[0];
  SimulateGattCharacteristic(service, kTestUUIDDeviceName, /* properties */ 0);
  SimulateGattCharacteristic(service, kTestUUIDAppearance,
                             /* properties */ 0);
  // Duplicate UUID.
  SimulateGattCharacteristic(service, kTestUUIDAppearance,
                             /* properties */ 0);
  SimulateGattCharacteristic(service, kTestUUIDReconnectionAddress,
                             /* properties */ 0);

  // Simulate remove of characteristics one by one.
  EXPECT_EQ(4u, service->GetCharacteristics().size());
  std::string removed_char = service->GetCharacteristics()[0]->GetIdentifier();
  SimulateGattCharacteristicRemoved(service,
                                    service->GetCharacteristic(removed_char));
  EXPECT_EQ(1, observer.gatt_characteristic_removed_count());
  EXPECT_FALSE(service->GetCharacteristic(removed_char));
  EXPECT_EQ(3u, service->GetCharacteristics().size());
  removed_char = service->GetCharacteristics()[0]->GetIdentifier();
  SimulateGattCharacteristicRemoved(service,
                                    service->GetCharacteristic(removed_char));
  EXPECT_EQ(2, observer.gatt_characteristic_removed_count());
  EXPECT_FALSE(service->GetCharacteristic(removed_char));
  EXPECT_EQ(2u, service->GetCharacteristics().size());
  removed_char = service->GetCharacteristics()[0]->GetIdentifier();
  SimulateGattCharacteristicRemoved(service,
                                    service->GetCharacteristic(removed_char));
  EXPECT_EQ(3, observer.gatt_characteristic_removed_count());
  EXPECT_FALSE(service->GetCharacteristic(removed_char));
  EXPECT_EQ(1u, service->GetCharacteristics().size());
  removed_char = service->GetCharacteristics()[0]->GetIdentifier();
  SimulateGattCharacteristicRemoved(service,
                                    service->GetCharacteristic(removed_char));
  EXPECT_EQ(4, observer.gatt_characteristic_removed_count());
  EXPECT_FALSE(service->GetCharacteristic(removed_char));
  EXPECT_EQ(0u, service->GetCharacteristics().size());
}

#if BUILDFLAG(IS_APPLE)
#define MAYBE_SimulateGattServiceRemove SimulateGattServiceRemove
#else
#define MAYBE_SimulateGattServiceRemove DISABLED_SimulateGattServiceRemove
#endif
#if BUILDFLAG(IS_WIN)
TEST_P(BluetoothRemoteGattServiceTestWinrt, SimulateGattServiceRemove) {
#else
TEST_F(BluetoothRemoteGattServiceTest, MAYBE_SimulateGattServiceRemove) {
#endif
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));
  SimulateGattConnection(device);
  base::RunLoop().RunUntilIdle();

  TestBluetoothAdapterObserver observer(adapter_);

  // Simulate two primary GATT services.
  SimulateGattServicesDiscovered(
      device,
      std::vector<std::string>({kTestUUIDGenericAccess, kTestUUIDHeartRate}));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, device->GetGattServices().size());

  // Simulate remove of a primary service.
  BluetoothRemoteGattService* service1 = device->GetGattServices()[0];
  BluetoothRemoteGattService* service2 = device->GetGattServices()[1];
  std::string removed_service = service1->GetIdentifier();
  SimulateGattServiceRemoved(device->GetGattService(removed_service));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, device->GetGattServices().size());
  EXPECT_FALSE(device->GetGattService(removed_service));
  EXPECT_EQ(device->GetGattServices()[0], service2);
}

#if BUILDFLAG(IS_APPLE)
// Tests to receive a services changed notification from macOS, while
// discovering characteristics. The gatt device should scan again for services
// and characteristics, before scanning for descriptors. Only after the gatt
// service changed notification should be sent.
// Android: This test doesn't apply to Android because there is no services
// changed event that could arrive during a discovery procedure.
TEST_F(BluetoothRemoteGattServiceTest,
       SimulateDeviceModificationWhileDiscoveringCharacteristics) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));

  TestBluetoothAdapterObserver observer(adapter_);

  // Starts first discovery process.
  SimulateGattConnection(device);
  AddServicesToDeviceMac(device, {kTestUUIDHeartRate});
  SimulateDidDiscoverServicesMac(device);
  EXPECT_EQ(1u, device->GetGattServices().size());
  BluetoothRemoteGattService* service = device->GetGattServices()[0];
  std::string characteristic_uuid1 = "11111111-0000-1000-8000-00805f9b34fb";
  AddCharacteristicToServiceMac(service, characteristic_uuid1,
                                /* properties */ 0);
  // Now waiting for characteristic discovery.

  // Starts second discovery process.
  SimulateGattServicesChanged(device);
  SimulateDidDiscoverServicesMac(device);
  // Now waiting for the second characteristic discovery.

  // First system call to -[id<CBPeripheralDelegate>
  // peripheral:didDiscoverCharacteristicsForService:error:]
  SimulateDidDiscoverCharacteristicsMac(service);
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  EXPECT_EQ(1u, service->GetCharacteristics().size());

  // Finish discovery process.
  std::string characteristic_uuid2 = "22222222-0000-1000-8000-00805f9b34fb";
  AddCharacteristicToServiceMac(service, characteristic_uuid2,
                                /* properties */ 0);
  // Second system call to -[id<CBPeripheralDelegate>
  // peripheral:didDiscoverCharacteristicsForService:error:]
  SimulateDidDiscoverCharacteristicsMac(service);
  EXPECT_EQ(2u, service->GetCharacteristics().size());
  EXPECT_EQ(0, observer.gatt_service_changed_count());
  BluetoothRemoteGattCharacteristic* characteristic1 =
      service->GetCharacteristics()[0];
  BluetoothRemoteGattCharacteristic* characteristic2 =
      service->GetCharacteristics()[1];
  if (characteristic1->GetUUID().canonical_value() == characteristic_uuid2) {
    BluetoothRemoteGattCharacteristic* tmp = characteristic1;
    characteristic1 = characteristic2;
    characteristic2 = tmp;
  }
  EXPECT_EQ(characteristic_uuid1, characteristic1->GetUUID().canonical_value());
  EXPECT_EQ(characteristic_uuid2, characteristic2->GetUUID().canonical_value());
  SimulateDidDiscoverDescriptorsMac(characteristic1);
  SimulateDidDiscoverDescriptorsMac(characteristic2);
  EXPECT_EQ(1, observer.gatt_service_changed_count());
}
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE)
// Simulates to receive an extra discovery characteristic notifications from
// macOS. Those notifications should be ignored.
// Android: This test doesn't apply to Android because there is no services
// changed event that could arrive during a discovery procedure.
TEST_F(BluetoothRemoteGattServiceTest, ExtraDidDiscoverServicesCall) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));

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
  SimulateDidDiscoverServicesMac(device);  // Extra system call.
  SimulateGattServicesChanged(device);
  SimulateDidDiscoverServicesMac(device);
  SimulateDidDiscoverServicesMac(device);  // Extra system call.
  SimulateDidDiscoverCharacteristicsMac(service);
  SimulateDidDiscoverServicesMac(device);  // Extra system call.
  SimulateDidDiscoverDescriptorsMac(characteristic);
  SimulateDidDiscoverServicesMac(device);  // Extra system call.
  EXPECT_EQ(2, observer.device_changed_count());
}
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_APPLE)
// Simulates to receive an extra discovery characteristic notifications from
// macOS. Those notifications should be ignored.
// Android: This test doesn't apply to Android because there is no services
// changed event that could arrive during a discovery procedure.
TEST_F(BluetoothRemoteGattServiceTest, ExtraDidDiscoverCharacteristicsCall) {
  InitWithFakeAdapter();
  StartLowEnergyDiscoverySession();
  BluetoothDevice* device = SimulateLowEnergyDevice(3);
  device->CreateGattConnection(
      GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));

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
  SimulateDidDiscoverCharacteristicsMac(service);  // Extra system call.
  SimulateGattServicesChanged(device);
  SimulateDidDiscoverCharacteristicsMac(service);  // Extra system call.
  SimulateDidDiscoverServicesMac(device);
  SimulateDidDiscoverCharacteristicsMac(service);
  SimulateDidDiscoverCharacteristicsMac(service);  // Extra system call.
  SimulateDidDiscoverDescriptorsMac(characteristic);
  SimulateDidDiscoverCharacteristicsMac(service);  // Extra system call.
  EXPECT_EQ(2, observer.device_changed_count());
}
#endif  // BUILDFLAG(IS_APPLE)

#if BUILDFLAG(IS_WIN)
INSTANTIATE_TEST_SUITE_P(All,
                         BluetoothRemoteGattServiceTestWinrt,
                         ::testing::ValuesIn(kBluetoothTestWinrtParam));
#endif  // BUILDFLAG(IS_WIN)

}  // namespace device
