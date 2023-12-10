// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_GATT_DEVICE_SERVICE_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_GATT_DEVICE_SERVICE_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <windows.foundation.collections.h>
#include <wrl/implements.h>

#include <stdint.h>

#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"

namespace device {

class BluetoothTestWinrt;
class FakeBluetoothLEDeviceWinrt;
class FakeGattCharacteristicWinrt;

class FakeGattDeviceServiceWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattDeviceService,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattDeviceService3> {
 public:
  FakeGattDeviceServiceWinrt(
      BluetoothTestWinrt* bluetooth_test_winrt,
      Microsoft::WRL::ComPtr<FakeBluetoothLEDeviceWinrt> fake_device,
      std::string_view uuid,
      uint16_t attribute_handle,
      bool allowed);

  FakeGattDeviceServiceWinrt(const FakeGattDeviceServiceWinrt&) = delete;
  FakeGattDeviceServiceWinrt& operator=(const FakeGattDeviceServiceWinrt&) =
      delete;

  ~FakeGattDeviceServiceWinrt() override;

  // IGattDeviceService:
  IFACEMETHODIMP GetCharacteristics(
      GUID characteristic_uuid,
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCharacteristic*>** value) override;
  IFACEMETHODIMP GetIncludedServices(
      GUID service_uuid,
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceService*>** value) override;
  IFACEMETHODIMP get_DeviceId(HSTRING* value) override;
  IFACEMETHODIMP get_Uuid(GUID* value) override;
  IFACEMETHODIMP get_AttributeHandle(uint16_t* value) override;

  // IGattDeviceService3:
  IFACEMETHODIMP get_DeviceAccessInformation(
      ABI::Windows::Devices::Enumeration::IDeviceAccessInformation** value)
      override;
  IFACEMETHODIMP get_Session(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::IGattSession**
          value) override;
  IFACEMETHODIMP get_SharingMode(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattSharingMode* value) override;
  IFACEMETHODIMP RequestAccessAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DeviceAccessStatus>** value)
      override;
  IFACEMETHODIMP OpenAsync(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattSharingMode
          sharing_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattOpenStatus>** operation) override;
  IFACEMETHODIMP GetCharacteristicsAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCharacteristicsResult*>** operation) override;
  IFACEMETHODIMP GetCharacteristicsWithCacheModeAsync(
      ABI::Windows::Devices::Bluetooth::BluetoothCacheMode cache_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCharacteristicsResult*>** operation) override;
  IFACEMETHODIMP GetCharacteristicsForUuidAsync(
      GUID characteristic_uuid,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCharacteristicsResult*>** operation) override;
  IFACEMETHODIMP GetCharacteristicsForUuidWithCacheModeAsync(
      GUID characteristic_uuid,
      ABI::Windows::Devices::Bluetooth::BluetoothCacheMode cache_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCharacteristicsResult*>** operation) override;
  IFACEMETHODIMP GetIncludedServicesAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceServicesResult*>** operation) override;
  IFACEMETHODIMP GetIncludedServicesWithCacheModeAsync(
      ABI::Windows::Devices::Bluetooth::BluetoothCacheMode cache_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceServicesResult*>** operation) override;
  IFACEMETHODIMP GetIncludedServicesForUuidAsync(
      GUID service_uuid,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceServicesResult*>** operation) override;
  IFACEMETHODIMP GetIncludedServicesForUuidWithCacheModeAsync(
      GUID service_uuid,
      ABI::Windows::Devices::Bluetooth::BluetoothCacheMode cache_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceServicesResult*>** operation) override;

  void SimulateGattCharacteristic(std::string_view uuid, int proporties);

 private:
  const raw_ptr<BluetoothTestWinrt> bluetooth_test_winrt_;
  const Microsoft::WRL::ComPtr<FakeBluetoothLEDeviceWinrt> fake_device_;
  const GUID uuid_;
  const uint16_t attribute_handle_;
  const bool allowed_;
  bool opened_ = false;

  std::vector<Microsoft::WRL::ComPtr<FakeGattCharacteristicWinrt>>
      fake_characteristics_;
  uint16_t characteristic_attribute_handle_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_GATT_DEVICE_SERVICE_WINRT_H_
