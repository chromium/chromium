// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_PAIRING_REQUESTED_EVENT_ARGS_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_PAIRING_REQUESTED_EVENT_ARGS_WINRT_H_

#include <windows.devices.enumeration.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "device/bluetooth/test/fake_device_information_custom_pairing_winrt.h"

namespace device {

class FakeDevicePairingRequestedEventArgsWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Enumeration::
              IDevicePairingRequestedEventArgs> {
 public:
  explicit FakeDevicePairingRequestedEventArgsWinrt(
      Microsoft::WRL::ComPtr<FakeDeviceInformationCustomPairingWinrt>
          custom_pairing);

  FakeDevicePairingRequestedEventArgsWinrt(
      const FakeDevicePairingRequestedEventArgsWinrt&) = delete;
  FakeDevicePairingRequestedEventArgsWinrt& operator=(
      const FakeDevicePairingRequestedEventArgsWinrt&) = delete;

  ~FakeDevicePairingRequestedEventArgsWinrt() override;

  // IDevicePairingRequestedEventArgs:
  IFACEMETHODIMP get_DeviceInformation(
      ABI::Windows::Devices::Enumeration::IDeviceInformation** value) override;
  IFACEMETHODIMP get_PairingKind(
      ABI::Windows::Devices::Enumeration::DevicePairingKinds* value) override;
  IFACEMETHODIMP get_Pin(HSTRING* value) override;
  IFACEMETHODIMP Accept() override;
  IFACEMETHODIMP AcceptWithPin(HSTRING pin) override;
  IFACEMETHODIMP GetDeferral(
      ABI::Windows::Foundation::IDeferral** result) override;

  void Complete();

 private:
  Microsoft::WRL::ComPtr<FakeDeviceInformationCustomPairingWinrt>
      custom_pairing_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_PAIRING_REQUESTED_EVENT_ARGS_WINRT_H_
