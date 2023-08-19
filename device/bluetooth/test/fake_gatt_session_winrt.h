// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_GATT_SESSION_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_GATT_SESSION_WINRT_H_

#include <windows.devices.bluetooth.genericattributeprofile.h>
#include <windows.devices.bluetooth.h>
#include <windows.foundation.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/memory/raw_ptr.h"

namespace device {

class BluetoothTestWinrt;

class FakeGattSessionWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattSession,
          ABI::Windows::Foundation::IClosable> {
 public:
  FakeGattSessionWinrt(BluetoothTestWinrt* bluetooth_test_winrt);
  FakeGattSessionWinrt(const FakeGattSessionWinrt&) = delete;
  FakeGattSessionWinrt& operator=(const FakeGattSessionWinrt&) = delete;
  ~FakeGattSessionWinrt() override;

  void SimulateGattConnection();
  void SimulateGattDisconnection();
  void SimulateGattConnectionError();

  // IGattSession:
  IFACEMETHODIMP get_DeviceId(
      ABI::Windows::Devices::Bluetooth::IBluetoothDeviceId** value) override;
  IFACEMETHODIMP get_CanMaintainConnection(::boolean* value) override;
  IFACEMETHODIMP put_MaintainConnection(::boolean value) override;
  IFACEMETHODIMP get_MaintainConnection(::boolean* value) override;
  IFACEMETHODIMP get_MaxPduSize(UINT16* value) override;
  IFACEMETHODIMP get_SessionStatus(
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattSessionStatus* value) override;
  IFACEMETHODIMP add_MaxPduSizeChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattSession*,
          IInspectable*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP
  remove_MaxPduSizeChanged(EventRegistrationToken token) override;
  IFACEMETHODIMP add_SessionStatusChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattSession*,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattSessionStatusChangedEventArgs*>* handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP
  remove_SessionStatusChanged(EventRegistrationToken token) override;

  // IClosable:
  IFACEMETHODIMP Close() override;

 private:
  raw_ptr<BluetoothTestWinrt> bluetooth_test_winrt_ = nullptr;
  bool maintain_connection_ = false;

  ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattSessionStatus
      status_ = ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattSessionStatus_Closed;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::GattSession*,
      ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
          GattSessionStatusChangedEventArgs*>>
      status_changed_handler_;
};

class FakeGattSessionStaticsWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              IGattSessionStatics> {
 public:
  FakeGattSessionStaticsWinrt(BluetoothTestWinrt* bluetooth_test_winrt);
  FakeGattSessionStaticsWinrt(const FakeGattSessionStaticsWinrt&) = delete;
  FakeGattSessionStaticsWinrt& operator=(const FakeGattSessionStaticsWinrt&) =
      delete;
  ~FakeGattSessionStaticsWinrt() override;

  // IGattSessionStatics:
  IFACEMETHODIMP FromDeviceIdAsync(
      ABI::Windows::Devices::Bluetooth::IBluetoothDeviceId* device_id,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Bluetooth::GenericAttributeProfile::
              GattSession*>** operation) override;

 private:
  raw_ptr<BluetoothTestWinrt, DanglingUntriaged> bluetooth_test_winrt_ =
      nullptr;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_GATT_SESSION_WINRT_H_
