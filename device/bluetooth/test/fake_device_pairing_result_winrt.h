// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_PAIRING_RESULT_WINRT_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_PAIRING_RESULT_WINRT_H_

#include <windows.devices.enumeration.h>
#include <wrl/implements.h>

namespace device {

class FakeDevicePairingResultWinrt
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRt | Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::Devices::Enumeration::IDevicePairingResult> {
 public:
  explicit FakeDevicePairingResultWinrt(
      ABI::Windows::Devices::Enumeration::DevicePairingResultStatus status);

  FakeDevicePairingResultWinrt(const FakeDevicePairingResultWinrt&) = delete;
  FakeDevicePairingResultWinrt& operator=(const FakeDevicePairingResultWinrt&) =
      delete;

  ~FakeDevicePairingResultWinrt() override;

  // IDevicePairingResult:
  IFACEMETHODIMP get_Status(
      ABI::Windows::Devices::Enumeration::DevicePairingResultStatus* status)
      override;
  IFACEMETHODIMP get_ProtectionLevelUsed(
      ABI::Windows::Devices::Enumeration::DevicePairingProtectionLevel* value)
      override;

 private:
  ABI::Windows::Devices::Enumeration::DevicePairingResultStatus status_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_DEVICE_PAIRING_RESULT_WINRT_H_
