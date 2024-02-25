// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_PAIRING_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_PAIRING_WINRT_H_

#include <windows.devices.enumeration.h>
#include <windows.foundation.h>
#include <wrl/client.h>

#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

class BluetoothDeviceWinrt;

// This class encapsulates logic required to perform a custom pairing on WinRT.
// Currently only pairing with a pin code is supported.
class BluetoothPairingWinrt {
 public:
  // On error |error_code| will have a value, otherwise successful.
  using ConnectCallback = base::OnceCallback<void(
      std::optional<BluetoothDevice::ConnectErrorCode> error_code)>;

  BluetoothPairingWinrt(
      BluetoothDeviceWinrt* device,
      BluetoothDevice::PairingDelegate* pairing_delegate,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Enumeration::IDeviceInformationCustomPairing>
          custom_pairing,
      ConnectCallback callback);

  BluetoothPairingWinrt(const BluetoothPairingWinrt&) = delete;
  BluetoothPairingWinrt& operator=(const BluetoothPairingWinrt&) = delete;

  ~BluetoothPairingWinrt();

  // Initiates the pairing procedure.
  void StartPairing();

  // Indicates whether the device is currently pairing and expecting a
  // PIN Code to be returned.
  bool ExpectingPinCode() const;

  // Sends the PIN code |pin_code| to the remote device during pairing.
  void SetPinCode(std::string_view pin_code);

  // User consented to continue pairing the remote device.
  void ConfirmPairing();

  // Rejects a pairing or connection request from a remote device.
  void RejectPairing();

  // Cancels a pairing or connection attempt to a remote device.
  void CancelPairing();

 private:
  void OnPairingRequested(
      ABI::Windows::Devices::Enumeration::IDeviceInformationCustomPairing*
          custom_pairing,
      ABI::Windows::Devices::Enumeration::IDevicePairingRequestedEventArgs*
          pairing_requested);

  void OnPair(Microsoft::WRL::ComPtr<
              ABI::Windows::Devices::Enumeration::IDevicePairingResult>
                  pairing_result);

  void OnSetPinCodeDeferralCompletion(HRESULT hr);
  void OnConfirmPairingDeferralCompletion(HRESULT hr);
  void OnRejectPairing(HRESULT hr);
  void OnCancelPairing(HRESULT hr);

  // Weak. This is the device object that owns this pairing instance.
  raw_ptr<BluetoothDeviceWinrt> device_;

  // Weak. This is the pairing delegate provided to BluetoothDevice::Pair.
  // Clients need to ensure the delegate stays alive during the pairing
  // procedure.
  raw_ptr<BluetoothDevice::PairingDelegate> pairing_delegate_;

  // Boolean indicating whether the device is currently pairing and expecting a
  // PIN Code to be returned.
  bool expecting_pin_code_ = false;

  Microsoft::WRL::ComPtr<
      ABI::Windows::Devices::Enumeration::IDeviceInformationCustomPairing>
      custom_pairing_;
  ConnectCallback callback_;

  std::optional<EventRegistrationToken> pairing_requested_token_;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IDeferral> pairing_deferral_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Devices::Enumeration::IDevicePairingRequestedEventArgs>
      pairing_requested_;

  bool was_cancelled_ = false;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BluetoothPairingWinrt> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_PAIRING_WINRT_H_
