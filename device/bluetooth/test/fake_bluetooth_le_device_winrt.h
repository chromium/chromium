// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_DEVICE_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_DEVICE_WINRT_H_

#include <windows.devices.bluetooth.h>
#include <windows.devices.enumeration.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/fake_device_information_winrt.h"

namespace device {

class BluetoothTestWinrt;
class FakeGattDeviceServiceWinrt;

class FakeBluetoothLEDeviceWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice,
          ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice2,
          ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice3,
          ABI::Windows::Devices::Bluetooth::IBluetoothLEDevice4,
          ABI::Windows::Foundation::IClosable> {
 public:
  explicit FakeBluetoothLEDeviceWinrt(BluetoothTestWinrt* bluetooth_test_winrt);

  FakeBluetoothLEDeviceWinrt(const FakeBluetoothLEDeviceWinrt&) = delete;
  FakeBluetoothLEDeviceWinrt& operator=(const FakeBluetoothLEDeviceWinrt&) =
      delete;

  ~FakeBluetoothLEDeviceWinrt() override;

  // IBluetoothLEDevice:
  IFACEMETHODIMP get_DeviceId(HSTRING* value) override;
  IFACEMETHODIMP get_Name(HSTRING* value) override;
  IFACEMETHODIMP get_GattServices(
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceService*>** value) override;
  IFACEMETHODIMP get_ConnectionStatus(
      ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus* value)
      override;
  IFACEMETHODIMP get_BluetoothAddress(uint64_t* value) override;
  IFACEMETHODIMP GetGattService(
      GUID service_uuid,
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          IGattDeviceService** service) override;
  IFACEMETHODIMP add_NameChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Bluetooth::BluetoothLEDevice*,
          IInspectable*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_NameChanged(EventRegistrationToken token) override;
  IFACEMETHODIMP add_GattServicesChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Bluetooth::BluetoothLEDevice*,
          IInspectable*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_GattServicesChanged(
      EventRegistrationToken token) override;
  IFACEMETHODIMP add_ConnectionStatusChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Bluetooth::BluetoothLEDevice*,
          IInspectable*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_ConnectionStatusChanged(
      EventRegistrationToken token) override;

  // IBluetoothLEDevice2:
  IFACEMETHODIMP get_DeviceInformation(
      ABI::Windows::Devices::Enumeration::IDeviceInformation** value) override;
  IFACEMETHODIMP get_Appearance(
      ABI::Windows::Devices::Bluetooth::IBluetoothLEAppearance** value)
      override;
  IFACEMETHODIMP get_BluetoothAddressType(
      ABI::Windows::Devices::Bluetooth::BluetoothAddressType* value) override;

  // IBluetoothLEDevice3:
  IFACEMETHODIMP get_DeviceAccessInformation(
      ABI::Windows::Devices::Enumeration::IDeviceAccessInformation** value)
      override;
  IFACEMETHODIMP RequestAccessAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DeviceAccessStatus>** operation)
      override;
  IFACEMETHODIMP GetGattServicesAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceServicesResult*>** operation) override;
  IFACEMETHODIMP GetGattServicesWithCacheModeAsync(
      ABI::Windows::Devices::Bluetooth::BluetoothCacheMode cache_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceServicesResult*>** operation) override;
  IFACEMETHODIMP GetGattServicesForUuidAsync(
      GUID service_uuid,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceServicesResult*>** operation) override;
  IFACEMETHODIMP GetGattServicesForUuidWithCacheModeAsync(
      GUID service_uuid,
      ABI::Windows::Devices::Bluetooth::BluetoothCacheMode cache_mode,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattDeviceServicesResult*>** operation) override;

  // IBluetoothLEDevice4:
  IFACEMETHODIMP get_BluetoothDeviceId(
      ABI::Windows::Devices::Bluetooth::IBluetoothDeviceId** value) override;

  // IClosable:
  IFACEMETHODIMP Close() override;

  // We perform explicit reference counting, to be able to query the ref count
  // ourselves. This is required to simulate Gatt disconnection behavior
  // exhibited by the production UWP APIs.
  void AddReference();
  void RemoveReference();

  void SimulateDevicePaired(bool is_paired);
  void SimulatePairingPinCode(std::string pin_code);
  void SimulateConfirmOnly();
  void SimulateDisplayPin(std::string_view display_pin);
  std::optional<BluetoothUUID> GetTargetGattService() const;
  void SimulateGattConnection();
  void SimulateGattConnectionError(
      BluetoothDevice::ConnectErrorCode error_code);
  void SimulateGattDisconnection();
  void SimulateDeviceBreaksConnection();
  void SimulateGattNameChange(const std::string& new_name);
  void SimulateGattServicesDiscovered(
      const std::vector<std::string>& uuids,
      const std::vector<std::string>& blocked_uuids);
  void SimulateGattServicesChanged();
  void SimulateStatusChangeToDisconnect();
  void SimulateGattServiceRemoved(BluetoothRemoteGattService* service);
  void SimulateGattCharacteristic(BluetoothRemoteGattService* service,
                                  const std::string& uuid,
                                  int properties);
  void SimulateGattDescriptor(BluetoothRemoteGattCharacteristic* characteristic,
                              const std::string& uuid);
  void SimulateGattServicesDiscoveryError();

 private:
  raw_ptr<BluetoothTestWinrt> bluetooth_test_winrt_ = nullptr;
  uint32_t reference_count_ = 1u;
  std::optional<std::string> name_;

  ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus status_ =
      ABI::Windows::Devices::Bluetooth::BluetoothConnectionStatus_Disconnected;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::Devices::Bluetooth::BluetoothLEDevice*,
      IInspectable*>>
      connection_status_changed_handler_;

  Microsoft::WRL::ComPtr<ABI::Windows::Devices::Enumeration::IDeviceInformation>
      device_information_ = Microsoft::WRL::Make<FakeDeviceInformationWinrt>();

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::Devices::Bluetooth::BluetoothLEDevice*,
      IInspectable*>>
      gatt_services_changed_handler_;

  base::OnceCallback<void(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattDeviceServicesResult>)>
      gatt_services_callback_;
  // Contains the last GUID passed to GetGattServicesForUuidAsync.
  std::optional<GUID> service_uuid_;

  std::vector<Microsoft::WRL::ComPtr<FakeGattDeviceServiceWinrt>>
      fake_services_;
  uint16_t service_attribute_handle_ = 0;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::Devices::Bluetooth::BluetoothLEDevice*,
      IInspectable*>>
      name_changed_handler_;
};

class FakeBluetoothLEDeviceStaticsWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::IBluetoothLEDeviceStatics> {
 public:
  explicit FakeBluetoothLEDeviceStaticsWinrt(
      BluetoothTestWinrt* bluetooth_test_winrt);

  FakeBluetoothLEDeviceStaticsWinrt(const FakeBluetoothLEDeviceStaticsWinrt&) =
      delete;
  FakeBluetoothLEDeviceStaticsWinrt& operator=(
      const FakeBluetoothLEDeviceStaticsWinrt&) = delete;

  ~FakeBluetoothLEDeviceStaticsWinrt() override;

  // IBluetoothLEDeviceStatics:
  IFACEMETHODIMP FromIdAsync(
      HSTRING device_id,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::BluetoothLEDevice*>** operation)
      override;
  IFACEMETHODIMP FromBluetoothAddressAsync(
      uint64_t bluetooth_address,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::BluetoothLEDevice*>** operation)
      override;
  IFACEMETHODIMP GetDeviceSelector(HSTRING* device_selector) override;

 private:
  raw_ptr<BluetoothTestWinrt> bluetooth_test_winrt_ = nullptr;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_LE_DEVICE_WINRT_H_
