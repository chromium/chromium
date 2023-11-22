// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_GATT_CHARACTERISTIC_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_GATT_CHARACTERISTIC_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <windows.foundation.collections.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <stdint.h>

#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_gatt_service.h"

namespace device {

class BluetoothTestWinrt;
class FakeGattDescriptorWinrt;

class FakeGattCharacteristicWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattCharacteristic,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattCharacteristic3> {
 public:
  FakeGattCharacteristicWinrt(BluetoothTestWinrt* bluetooth_test_winrt,
                              int properties,
                              std::string_view uuid,
                              uint16_t attribute_handle);

  FakeGattCharacteristicWinrt(const FakeGattCharacteristicWinrt&) = delete;
  FakeGattCharacteristicWinrt& operator=(const FakeGattCharacteristicWinrt&) =
      delete;

  ~FakeGattCharacteristicWinrt() override;

  // IGattCharacteristic:
  IFACEMETHODIMP GetDescriptors(
      GUID descriptor_uuid,
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDescriptor*>** value) override;
  IFACEMETHODIMP get_CharacteristicProperties(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCharacteristicProperties* value) override;
  IFACEMETHODIMP get_ProtectionLevel(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattProtectionLevel* value) override;
  IFACEMETHODIMP put_ProtectionLevel(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattProtectionLevel value) override;
  IFACEMETHODIMP get_UserDescription(HSTRING* value) override;
  IFACEMETHODIMP get_Uuid(GUID* value) override;
  IFACEMETHODIMP get_AttributeHandle(uint16_t* value) override;
  IFACEMETHODIMP get_PresentationFormats(
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattPresentationFormat*>** value) override;
  IFACEMETHODIMP ReadValueAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattReadResult*>** value) override;
  IFACEMETHODIMP ReadValueWithCacheModeAsync(
      ABI::Windows::Devices::Bluetooth::BluetoothCacheMode cache_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattReadResult*>** value) override;
  IFACEMETHODIMP WriteValueAsync(
      ABI::Windows::Storage::Streams::IBuffer* value,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCommunicationStatus>** async_op) override;
  IFACEMETHODIMP WriteValueWithOptionAsync(
      ABI::Windows::Storage::Streams::IBuffer* value,
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattWriteOption
          write_option,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCommunicationStatus>** async_op) override;
  IFACEMETHODIMP ReadClientCharacteristicConfigurationDescriptorAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattReadClientCharacteristicConfigurationDescriptorResult*>**
          async_op) override;
  IFACEMETHODIMP WriteClientCharacteristicConfigurationDescriptorAsync(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattClientCharacteristicConfigurationDescriptorValue
              client_characteristic_configuration_descriptor_value,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCommunicationStatus>** async_op) override;
  IFACEMETHODIMP add_ValueChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattCharacteristic*,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattValueChangedEventArgs*>* value_changed_handler,
      EventRegistrationToken* value_changed_event_cookie) override;
  IFACEMETHODIMP remove_ValueChanged(
      EventRegistrationToken value_changed_event_cookie) override;

  // IGattCharacteristic3:
  IFACEMETHODIMP GetDescriptorsAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDescriptorsResult*>** operation) override;
  IFACEMETHODIMP GetDescriptorsWithCacheModeAsync(
      ABI::Windows::Devices::Bluetooth::BluetoothCacheMode cache_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDescriptorsResult*>** operation) override;
  IFACEMETHODIMP GetDescriptorsForUuidAsync(
      GUID descriptor_uuid,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDescriptorsResult*>** operation) override;
  IFACEMETHODIMP GetDescriptorsForUuidWithCacheModeAsync(
      GUID descriptor_uuid,
      ABI::Windows::Devices::Bluetooth::BluetoothCacheMode cache_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDescriptorsResult*>** operation) override;
  IFACEMETHODIMP WriteValueWithResultAsync(
      ABI::Windows::Storage::Streams::IBuffer* value,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattWriteResult*>** operation) override;
  IFACEMETHODIMP WriteValueWithResultAndOptionAsync(
      ABI::Windows::Storage::Streams::IBuffer* value,
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattWriteOption
          write_option,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattWriteResult*>** operation) override;
  IFACEMETHODIMP
  WriteClientCharacteristicConfigurationDescriptorWithResultAsync(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattClientCharacteristicConfigurationDescriptorValue
              client_characteristic_configuration_descriptor_value,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattWriteResult*>** operation) override;

  void SimulateGattCharacteristicRead(const std::vector<uint8_t>& data);
  void SimulateGattCharacteristicReadError(
      BluetoothGattService::GattErrorCode error_code);
  void SimulateGattCharacteristicWrite();
  void SimulateGattCharacteristicWriteError(
      BluetoothGattService::GattErrorCode error_code);
  void SimulateGattDescriptor(std::string_view uuid);
  void SimulateGattNotifySessionStarted();
  void SimulateGattNotifySessionStartError(
      BluetoothGattService::GattErrorCode error_code);
  void SimulateGattNotifySessionStopped();
  void SimulateGattNotifySessionStopError(
      BluetoothGattService::GattErrorCode error_code);
  void SimulateGattCharacteristicChanged(const std::vector<uint8_t>& value);

 private:
  raw_ptr<BluetoothTestWinrt> bluetooth_test_winrt_;
  ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
      GattCharacteristicProperties properties_;
  GUID uuid_;
  uint16_t attribute_handle_;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattCharacteristic*,
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattValueChangedEventArgs*>>
      value_changed_handler_;

  base::OnceCallback<void(
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattReadResult>)>
      read_value_callback_;
  base::OnceCallback<void(
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattWriteResult>)>
      write_value_callback_;
  base::OnceCallback<void(
      Microsoft::WRL::ComPtr<ABI::Windows::Devices::Bluetooth::
                                 GenericAttributeProfile::IGattWriteResult>)>
      notify_session_callback_;

  std::vector<Microsoft::WRL::ComPtr<FakeGattDescriptorWinrt>>
      fake_descriptors_;
  uint16_t last_descriptor_attribute_handle_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_GATT_CHARACTERISTIC_WINRT_H_
