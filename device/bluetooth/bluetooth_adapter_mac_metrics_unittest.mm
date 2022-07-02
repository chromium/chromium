// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "device/bluetooth/test/bluetooth_test_mac.h"

namespace device {

class BluetoothAdapterMacMetricsTest : public BluetoothTest {
 public:
  void FakeDeviceBoilerPlate() {
    if (!PlatformSupportsLowEnergy()) {
      LOG(WARNING) << "Low Energy Bluetooth unavailable, skipping unit test.";
      return;
    }
    InitWithFakeAdapter();
    StartLowEnergyDiscoverySession();
    device_ = SimulateLowEnergyDevice(3);
  }

  void FakeServiceBoilerPlate() {
    ASSERT_NO_FATAL_FAILURE(FakeDeviceBoilerPlate());

    device_->CreateGattConnection(
        GetGattConnectionCallback(Call::EXPECTED, Result::SUCCESS));
    SimulateGattConnection(device_);
    base::RunLoop().RunUntilIdle();
    SimulateGattServicesDiscovered(
        device_, std::vector<std::string>({kTestUUIDGenericAccess}));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1u, device_->GetGattServices().size());
    service_ = device_->GetGattServices()[0];
  }

  void FakeCharacteristicBoilerplate(int property = 0) {
    ASSERT_NO_FATAL_FAILURE(FakeServiceBoilerPlate());

    SimulateGattCharacteristic(service_, kTestUUIDDeviceName, property);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1u, service_->GetCharacteristics().size());
    characteristic_ = service_->GetCharacteristics()[0];
  }

  raw_ptr<BluetoothDevice> device_ = nullptr;
  raw_ptr<BluetoothRemoteGattService> service_ = nullptr;
  raw_ptr<BluetoothRemoteGattCharacteristic> characteristic_ = nullptr;
};

TEST_F(BluetoothAdapterMacMetricsTest, DidFailToConnectToPeripheralError) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(FakeDeviceBoilerPlate());
  SimulateGattConnectionError(device_, BluetoothDevice::ERROR_FAILED);
  histogram_tester.ExpectTotalCount(
      "Bluetooth.MacOS.Errors.DidFailToConnectToPeripheral", 1);
}

TEST_F(BluetoothAdapterMacMetricsTest, DidDisconnectPeripheral) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(FakeDeviceBoilerPlate());
  SimulateGattDisconnectionError(device_);
  histogram_tester.ExpectTotalCount(
      "Bluetooth.MacOS.Errors.DidDisconnectPeripheral", 1);
}

TEST_F(BluetoothAdapterMacMetricsTest, DidDiscoverPrimaryServicesError) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(FakeDeviceBoilerPlate());
  SimulateGattConnection(device_);
  SimulateDidDiscoverServicesMacWithError(device_);
  histogram_tester.ExpectTotalCount(
      "Bluetooth.MacOS.Errors.DidDiscoverPrimaryServices", 1);
}

TEST_F(BluetoothAdapterMacMetricsTest, DidDiscoverCharacteristicsError) {
  ASSERT_NO_FATAL_FAILURE(FakeServiceBoilerPlate());
  base::HistogramTester histogram_tester;
  SimulateDidDiscoverCharacteristicsWithErrorMac(service_);
  histogram_tester.ExpectTotalCount(
      "Bluetooth.MacOS.Errors.DidDiscoverCharacteristics", 1);
}

TEST_F(BluetoothAdapterMacMetricsTest, DidUpdateValueError) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  SimulateGattCharacteristicReadError(characteristic_,
                                      BluetoothGattService::GATT_ERROR_FAILED);
  histogram_tester.ExpectTotalCount("Bluetooth.MacOS.Errors.DidUpdateValue", 1);
}

TEST_F(BluetoothAdapterMacMetricsTest, DidWriteValueError) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  SimulateGattCharacteristicWriteError(characteristic_,
                                       BluetoothGattService::GATT_ERROR_FAILED);
  histogram_tester.ExpectTotalCount("Bluetooth.MacOS.Errors.DidWriteValue", 1);
}

TEST_F(BluetoothAdapterMacMetricsTest, DidUpdateNotificationStateError) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY));
  SimulateGattDescriptor(
      characteristic_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, characteristic_->GetDescriptors().size());
  characteristic_->StartNotifySession(GetNotifyCallback(Call::NOT_EXPECTED),
                                      GetGattErrorCallback(Call::EXPECTED));
  ExpectedChangeNotifyValueAttempts(1);
  ExpectedNotifyValue(NotifyValueState::NOTIFY);
  SimulateGattNotifySessionStartError(characteristic_,
                                      BluetoothGattService::GATT_ERROR_FAILED);
  histogram_tester.ExpectTotalCount(
      "Bluetooth.MacOS.Errors.DidUpdateNotificationState", 1);
}

TEST_F(BluetoothAdapterMacMetricsTest, DidDiscoverDescriptorsError) {
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));
  base::HistogramTester histogram_tester;
  SimulateDidDiscoverDescriptorsWithErrorMac(characteristic_);
  histogram_tester.ExpectTotalCount(
      "Bluetooth.MacOS.Errors.DidDiscoverDescriptors", 1);
}

TEST_F(BluetoothAdapterMacMetricsTest, DidUpdateValueForDescriptorError) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));
  SimulateGattDescriptor(
      characteristic_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  EXPECT_EQ(1u, characteristic_->GetDescriptors().size());
  BluetoothRemoteGattDescriptor* descriptor =
      characteristic_->GetDescriptors()[0];
  descriptor->ReadRemoteDescriptor(
      GetReadValueCallback(Call::EXPECTED, Result::FAILURE));
  SimulateGattDescriptorUpdateError(descriptor,
                                    BluetoothGattService::GATT_ERROR_FAILED);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Bluetooth.MacOS.Errors.DidUpdateValueForDescriptor", 1);
}

TEST_F(BluetoothAdapterMacMetricsTest, DidWriteValueForDescriptorError) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(FakeCharacteristicBoilerplate(
      BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));
  SimulateGattDescriptor(
      characteristic_,
      BluetoothRemoteGattDescriptor::ClientCharacteristicConfigurationUuid()
          .canonical_value());
  EXPECT_EQ(1u, characteristic_->GetDescriptors().size());
  BluetoothRemoteGattDescriptor* descriptor =
      characteristic_->GetDescriptors()[0];
  std::vector<uint8_t> empty_vector;
  descriptor->WriteRemoteDescriptor(empty_vector,
                                    GetCallback(Call::NOT_EXPECTED),
                                    GetGattErrorCallback(Call::EXPECTED));
  SimulateGattDescriptorWriteError(descriptor,
                                   BluetoothGattService::GATT_ERROR_FAILED);
  histogram_tester.ExpectTotalCount(
      "Bluetooth.MacOS.Errors.DidWriteValueForDescriptor", 1);
}

}  // namespace device
