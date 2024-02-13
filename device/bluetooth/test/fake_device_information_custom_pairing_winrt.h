// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_CUSTOM_PAIRING_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_CUSTOM_PAIRING_WINRT_H_

#include <windows.devices.enumeration.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/test/fake_device_information_pairing_winrt.h"

namespace base {
class SequencedTaskRunner;
}

namespace device {

class FakeDeviceInformationCustomPairingWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Enumeration::IDeviceInformationCustomPairing> {
 public:
  FakeDeviceInformationCustomPairingWinrt(
      Microsoft::WRL::ComPtr<FakeDeviceInformationPairingWinrt> pairing,
      std::string pin);

  FakeDeviceInformationCustomPairingWinrt(
      Microsoft::WRL::ComPtr<FakeDeviceInformationPairingWinrt> pairing,
      ABI::Windows::Devices::Enumeration::DevicePairingKinds pairing_kind);

  FakeDeviceInformationCustomPairingWinrt(
      Microsoft::WRL::ComPtr<FakeDeviceInformationPairingWinrt> pairing,
      ABI::Windows::Devices::Enumeration::DevicePairingKinds pairing_kind,
      std::string_view display_pin);

  FakeDeviceInformationCustomPairingWinrt(
      const FakeDeviceInformationCustomPairingWinrt&) = delete;
  FakeDeviceInformationCustomPairingWinrt& operator=(
      const FakeDeviceInformationCustomPairingWinrt&) = delete;

  ~FakeDeviceInformationCustomPairingWinrt() override;

  // IDeviceInformationCustomPairing:
  IFACEMETHODIMP PairAsync(
      ABI::Windows::Devices::Enumeration::DevicePairingKinds
          pairing_kinds_supported,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DevicePairingResult*>** result)
      override;
  IFACEMETHODIMP PairWithProtectionLevelAsync(
      ABI::Windows::Devices::Enumeration::DevicePairingKinds
          pairing_kinds_supported,
      ABI::Windows::Devices::Enumeration::DevicePairingProtectionLevel
          min_protection_level,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DevicePairingResult*>** result)
      override;
  IFACEMETHODIMP PairWithProtectionLevelAndSettingsAsync(
      ABI::Windows::Devices::Enumeration::DevicePairingKinds
          pairing_kinds_supported,
      ABI::Windows::Devices::Enumeration::DevicePairingProtectionLevel
          min_protection_level,
      ABI::Windows::Devices::Enumeration::IDevicePairingSettings*
          device_pairing_settings,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Devices::Enumeration::DevicePairingResult*>** result)
      override;
  IFACEMETHODIMP add_PairingRequested(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::Devices::Enumeration::DeviceInformationCustomPairing*,
          ABI::Windows::Devices::Enumeration::DevicePairingRequestedEventArgs*>*
          handler,
      EventRegistrationToken* token) override;
  IFACEMETHODIMP remove_PairingRequested(EventRegistrationToken token) override;

  void AcceptWithPin(std::string pin);
  void Complete();

  ABI::Windows::Devices::Enumeration::DevicePairingKinds pairing_kind() const {
    return pairing_kind_;
  }

  const std::string& pin() const { return display_pin_; }

  void SetConfirmed() { confirmed_ = true; }

 private:
  Microsoft::WRL::ComPtr<FakeDeviceInformationPairingWinrt> pairing_;
  const std::optional<std::string> pin_;
  std::string accepted_pin_;
  bool confirmed_ = false;

  ABI::Windows::Devices::Enumeration::DevicePairingKinds pairing_kind_ =
      ABI::Windows::Devices::Enumeration::DevicePairingKinds_ProvidePin;

  std::string display_pin_;

  base::OnceCallback<void(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Enumeration::IDevicePairingResult>)>
      pair_callback_;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::ITypedEventHandler<
      ABI::Windows::Devices::Enumeration::DeviceInformationCustomPairing*,
      ABI::Windows::Devices::Enumeration::DevicePairingRequestedEventArgs*>>
      pairing_requested_handler_;

  scoped_refptr<base::SequencedTaskRunner> pair_task_runner_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_INFORMATION_CUSTOM_PAIRING_WINRT_H_
