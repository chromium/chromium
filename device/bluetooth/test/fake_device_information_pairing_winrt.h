// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_PAIRING_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_PAIRING_WINRT_H_

#include <Windows.Devices.Enumeration.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <string>
#include <string_view>

namespace device {

class FakeDeviceInformationPairingWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Enumeration::IDeviceInformationPairing,
          ABI::Windows::Devices::Enumeration::IDeviceInformationPairing2> {
 public:
  explicit FakeDeviceInformationPairingWinrt(bool is_paired);
  explicit FakeDeviceInformationPairingWinrt(std::string pin);
  explicit FakeDeviceInformationPairingWinrt(
      ABI::Windows::Devices::Enumeration::DevicePairingKinds pairing_kind);
  explicit FakeDeviceInformationPairingWinrt(
      ABI::Windows::Devices::Enumeration::DevicePairingKinds pairing_kind,
      std::string_view display_pin);

  FakeDeviceInformationPairingWinrt(const FakeDeviceInformationPairingWinrt&) =
      delete;
  FakeDeviceInformationPairingWinrt& operator=(
      const FakeDeviceInformationPairingWinrt&) = delete;

  ~FakeDeviceInformationPairingWinrt() override;

  // IDeviceInformationPairing:
  IFACEMETHODIMP get_IsPaired(boolean* value) override;
  IFACEMETHODIMP get_CanPair(boolean* value) override;
  IFACEMETHODIMP PairAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DevicePairingResult*>** result)
      override;
  IFACEMETHODIMP PairWithProtectionLevelAsync(
      ABI::Windows::Devices::Enumeration::DevicePairingProtectionLevel
          min_protection_level,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DevicePairingResult*>** result)
      override;

  // IDeviceInformationPairing2:
  IFACEMETHODIMP get_ProtectionLevel(
      ABI::Windows::Devices::Enumeration::DevicePairingProtectionLevel* value)
      override;
  IFACEMETHODIMP get_Custom(
      ABI::Windows::Devices::Enumeration::IDeviceInformationCustomPairing**
          value) override;
  IFACEMETHODIMP PairWithProtectionLevelAndSettingsAsync(
      ABI::Windows::Devices::Enumeration::DevicePairingProtectionLevel
          min_protection_level,
      ABI::Windows::Devices::Enumeration::IDevicePairingSettings*
          device_pairing_settings,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DevicePairingResult*>** result)
      override;
  IFACEMETHODIMP UnpairAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DeviceUnpairingResult*>** result)
      override;

  void set_paired(bool is_paired) { is_paired_ = is_paired; }

 private:
  bool is_paired_ = false;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Devices::Enumeration::IDeviceInformationCustomPairing>
      custom_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_PAIRING_WINRT_H_
