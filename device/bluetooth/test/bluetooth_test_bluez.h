// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_BLUEZ_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_BLUEZ_H_

#include <cstdint>
#include <vector>

#include "base/callback_forward.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/test/bluetooth_test.h"

namespace bluez {
class FakeBluetoothDeviceClient;
class FakeBluetoothAdapterClient;
}

namespace device {

// BlueZ implementation of BluetoothTestBase.
class BluetoothTestBlueZ : public BluetoothTestBase {
 public:
  BluetoothTestBlueZ();
  ~BluetoothTestBlueZ() override;

  // Test overrides:
  void SetUp() override;
  void TearDown() override;

  // BluetoothTestBase overrides:
  bool PlatformSupportsLowEnergy() override;
  void InitWithFakeAdapter() override;
  BluetoothDevice* SimulateLowEnergyDevice(int device_ordinal) override;
  BluetoothDevice* SimulateClassicDevice() override;
  void SimulateLocalGattCharacteristicValueReadRequest(
      BluetoothDevice* from_device,
      BluetoothLocalGattCharacteristic* characteristic,
      BluetoothLocalGattService::Delegate::ValueCallback value_callback,
      base::OnceClosure error_callback) override;
  void SimulateLocalGattCharacteristicValueWriteRequest(
      BluetoothDevice* from_device,
      BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value_to_write,
      base::OnceClosure success_callback,
      base::OnceClosure error_callback) override;
  void SimulateLocalGattCharacteristicValuePrepareWriteRequest(
      BluetoothDevice* from_device,
      BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value_to_write,
      int offset,
      bool has_subsequent_write,
      base::OnceClosure success_callback,
      base::OnceClosure error_callback) override;
  void SimulateLocalGattDescriptorValueReadRequest(
      BluetoothDevice* from_device,
      BluetoothLocalGattDescriptor* descriptor,
      BluetoothLocalGattService::Delegate::ValueCallback value_callback,
      base::OnceClosure error_callback) override;
  void SimulateLocalGattDescriptorValueWriteRequest(
      BluetoothDevice* from_device,
      BluetoothLocalGattDescriptor* descriptor,
      const std::vector<uint8_t>& value_to_write,
      base::OnceClosure success_callback,
      base::OnceClosure error_callback) override;
  bool SimulateLocalGattCharacteristicNotificationsRequest(
      BluetoothLocalGattCharacteristic* characteristic,
      bool start) override;
  std::vector<uint8_t> LastNotifactionValueForCharacteristic(
      BluetoothLocalGattCharacteristic* characteristic) override;
  std::vector<BluetoothLocalGattService*> RegisteredGattServices() override;

 private:
  bluez::FakeBluetoothDeviceClient* fake_bluetooth_device_client_;
  bluez::FakeBluetoothAdapterClient* fake_bluetooth_adapter_client_;
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
using BluetoothTest = BluetoothTestBlueZ;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_BLUEZ_H_
