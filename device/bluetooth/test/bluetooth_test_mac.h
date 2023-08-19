// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_MAC_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_MAC_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/test_simple_task_runner.h"
#include "device/bluetooth/test/bluetooth_test.h"

#if __OBJC__
@class MockCBDescriptor;
@class MockCBCharacteristic;
@class MockCBPeripheral;
#endif  // __OBJC__

namespace device {

class BluetoothLowEnergyAdapterApple;

// Mac implementation of BluetoothTestBase.
class BluetoothTestMac : public BluetoothTestBase {
 public:
  static const char kTestPeripheralUUID1[];
  static const char kTestPeripheralUUID2[];

  BluetoothTestMac();
  ~BluetoothTestMac() override;

  // Test overrides:
  void SetUp() override;

  // BluetoothTestBase overrides:
  bool PlatformSupportsLowEnergy() override;
  void InitWithDefaultAdapter() override;
  void InitWithoutDefaultAdapter() override;
  void InitWithFakeAdapter() override;
  void ResetEventCounts() override;
  void SimulateAdapterPoweredOff() override;
  BluetoothDevice* SimulateLowEnergyDevice(int device_ordinal) override;
  void SimulateConnectedLowEnergyDevice(
      ConnectedDeviceType device_ordinal) override;
  void SimulateGattConnection(BluetoothDevice* device) override;
  void SimulateGattConnectionError(
      BluetoothDevice* device,
      BluetoothDevice::ConnectErrorCode errorCode) override;
  void SimulateGattDisconnection(BluetoothDevice* device) override;
  void SimulateGattDisconnectionError(BluetoothDevice* device) override;
  void SimulateGattServicesDiscovered(
      BluetoothDevice* device,
      const std::vector<std::string>& uuids,
      const std::vector<std::string>& blocked_uuids = {}) override;
  void SimulateGattServicesChanged(BluetoothDevice* device) override;
  void SimulateGattServiceRemoved(BluetoothRemoteGattService* service) override;
  void SimulateGattCharacteristic(BluetoothRemoteGattService* service,
                                  const std::string& uuid,
                                  int properties) override;
  void SimulateGattCharacteristicRead(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;
  void SimulateGattCharacteristicReadError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode) override;
  void SimulateGattCharacteristicWrite(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattCharacteristicWriteError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode error_code) override;
  void SimulateGattDescriptor(BluetoothRemoteGattCharacteristic* characteristic,
                              const std::string& uuid) override;
  void SimulateGattNotifySessionStarted(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattNotifySessionStartError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode error_code) override;
  void SimulateGattNotifySessionStopped(
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattNotifySessionStopError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode error_code) override;
  void SimulateGattCharacteristicChanged(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;
  void SimulateGattCharacteristicRemoved(
      BluetoothRemoteGattService* service,
      BluetoothRemoteGattCharacteristic* characteristic) override;
  void SimulateGattDescriptorRead(BluetoothRemoteGattDescriptor* descriptor,
                                  const std::vector<uint8_t>& value) override;
  void SimulateGattDescriptorReadError(
      BluetoothRemoteGattDescriptor* descriptor,
      BluetoothGattService::GattErrorCode error_code) override;
  void SimulateGattDescriptorWrite(
      BluetoothRemoteGattDescriptor* descriptor) override;
  void SimulateGattDescriptorWriteError(
      BluetoothRemoteGattDescriptor* descriptor,
      BluetoothGattService::GattErrorCode error_code) override;
  void SimulateGattDescriptorUpdateError(
      BluetoothRemoteGattDescriptor* descriptor,
      BluetoothGattService::GattErrorCode error_code) override;
  void ExpectedChangeNotifyValueAttempts(int attempts) override;
  void ExpectedNotifyValue(NotifyValueState expected_value_state) override;

  // Apple devices have the only platform for which we need to discover each set
  // of attributes individually so we need a method to simulate discovering each
  // set of attributes.
  // Simulates service discovery for a device.
  void SimulateDidDiscoverServicesMac(BluetoothDevice* device);
  // Simulates error in service discovery for a device.
  void SimulateDidDiscoverServicesMacWithError(BluetoothDevice* device);
  // Simulates characteristic discovery for a service.
  void SimulateDidDiscoverCharacteristicsMac(
      BluetoothRemoteGattService* service);
  // Simulates error in characteristic discovery for a service.
  void SimulateDidDiscoverCharacteristicsWithErrorMac(
      BluetoothRemoteGattService* service);
  // Simulates descriptor discovery for a characteristic.
  void SimulateDidDiscoverDescriptorsMac(
      BluetoothRemoteGattCharacteristic* characteristic);
  // Simulates error in descriptor discovery for a characteristic.
  void SimulateDidDiscoverDescriptorsWithErrorMac(
      BluetoothRemoteGattCharacteristic* characteristic);
  // CoreBluetooth can return NSData when reading remote gatt descriptors.
  // This methods simulate receiving NSData from CoreBluetooth.
  void SimulateGattDescriptorReadNSDataMac(
      BluetoothRemoteGattDescriptor* descriptor,
      const std::vector<uint8_t>& value);
  // CoreBluetooth can return NSString when reading remote gatt descriptors.
  // This methods simulate receiving NSString from CoreBluetooth.
  void SimulateGattDescriptorReadNSStringMac(
      BluetoothRemoteGattDescriptor* descriptor,
      const std::string& value);
  // CoreBluetooth can return NSString when reading remote gatt descriptors.
  // This methods simulate receiving NSString from CoreBluetooth.
  void SimulateGattDescriptorReadNSNumberMac(
      BluetoothRemoteGattDescriptor* descriptor,
      short value);

#if !BUILDFLAG(IS_IOS)
  // Sets the power state of the mock controller to |powered|. Used to override
  // BluetoothAdapterMac's SetControllerPowerStateFunction.
  void SetMockControllerPowerState(int powered);
#endif

  // Adds services in MockCBPeripheral.
  void AddServicesToDeviceMac(BluetoothDevice* device,
                              const std::vector<std::string>& uuids);
  // Adds a characteristic in MockCBService.
  void AddCharacteristicToServiceMac(BluetoothRemoteGattService* service,
                                     const std::string& characteristic_uuid,
                                     int properties);
  // Adds a descriptor in MockCBCharacteristic.
  void AddDescriptorToCharacteristicMac(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::string& uuid);

  // Callback for the bluetooth central manager mock.
  void OnFakeBluetoothDeviceConnectGattCalled();
  void OnFakeBluetoothGattDisconnect();

  // Callback for the bluetooth peripheral mock.
  void OnFakeBluetoothServiceDiscovery();
  void OnFakeBluetoothCharacteristicDiscovery();
  void OnFakeBluetoothCharacteristicReadValue();
  void OnFakeBluetoothCharacteristicWriteValue(std::vector<uint8_t> value);
  void OnFakeBluetoothGattSetCharacteristicNotification(bool notify_value);
  void OnFakeBluetoothDescriptorReadValue();
  void OnFakeBluetoothDescriptorWriteValue(std::vector<uint8_t> value);

  // Returns the service UUIDs used to retrieve connected peripherals.
  BluetoothDevice::UUIDSet RetrieveConnectedPeripheralServiceUUIDs();
  // Reset RetrieveConnectedPeripheralServiceUUIDs set.
  void ResetRetrieveConnectedPeripheralServiceUUIDs();

 protected:
  class ScopedMockCentralManager;

#if __OBJC__

  // Returns MockCBPeripheral from BluetoothDevice.
  MockCBPeripheral* GetMockCBPeripheral(BluetoothDevice* device) const;
  // Returns MockCBPeripheral from BluetoothRemoteGattService.
  MockCBPeripheral* GetMockCBPeripheral(
      BluetoothRemoteGattService* service) const;
  // Returns MockCBPeripheral from BluetoothRemoteGattCharacteristic.
  MockCBPeripheral* GetMockCBPeripheral(
      BluetoothRemoteGattCharacteristic* characteristic) const;
  // Returns MockCBPeripheral from BluetoothRemoteGattDescriptor.
  MockCBPeripheral* GetMockCBPeripheral(
      BluetoothRemoteGattDescriptor* descriptor) const;
  // Returns MockCBCharacteristic from BluetoothRemoteGattCharacteristic.
  MockCBCharacteristic* GetCBMockCharacteristic(
      BluetoothRemoteGattCharacteristic* characteristic) const;
  // Returns MockCBDescriptor from BluetoothRemoteGattDescriptor.
  MockCBDescriptor* GetCBMockDescriptor(
      BluetoothRemoteGattDescriptor* descriptor) const;

#endif  // __OBJC__

  // Utility function for finding CBUUIDs with relatively nice SHA256 hashes.
  std::string FindCBUUIDForHashTarget();

  raw_ptr<BluetoothLowEnergyAdapterApple, DanglingUntriaged>
      adapter_low_energy_ = nullptr;
  std::unique_ptr<ScopedMockCentralManager> mock_central_manager_;

  // Value set by -[CBPeripheral setNotifyValue:forCharacteristic:] call.
  bool last_notify_value_ = false;
  int gatt_characteristic_discovery_attempts_ = 0;
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
using BluetoothTest = BluetoothTestMac;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_MAC_H_
