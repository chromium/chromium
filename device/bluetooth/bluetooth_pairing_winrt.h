// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_PAIRING_WINRT_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_PAIRING_WINRT_H_

#include <windows.devices.enumeration.h>
#include <windows.foundation.h>
#include <wrl/client.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

class BluetoothDeviceWinrt;

// This class encapsulates logic required to perform a custom pairing on WinRT.
// Currently only pairing with a pin code is supported.
class BluetoothPairingWinrt {
 public:
  using Callback = base::OnceClosure;
  using ErrorCallback =
      base::OnceCallback<void(BluetoothDevice::ConnectErrorCode)>;

  BluetoothPairingWinrt(
      BluetoothDeviceWinrt* device,
      BluetoothDevice::PairingDelegate* pairing_delegate,
      Microsoft::WRL::ComPtr<
          ABI::Windows::Devices::Enumeration::IDeviceInformationCustomPairing>
          custom_pairing,
      Callback callback,
      ErrorCallback error_callback);

  ~BluetoothPairingWinrt();

  // Initiates the pairing procedure.
  void StartPairing();

  // Indicates whether the device is currently pairing and expecting a
  // PIN Code to be returned.
  bool ExpectingPinCode() const;

  // Sends the PIN code |pin_code| to the remote device during pairing.
  void SetPinCode(base::StringPiece pin_code);

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

  // Weak. This is the device object that owns this pairing instance.
  BluetoothDeviceWinrt* device_;

  // Weak. This is the pairing delegate provided to BluetoothDevice::Pair.
  // Clients need to ensure the delegate stays alive during the pairing
  // procedure.
  BluetoothDevice::PairingDelegate* pairing_delegate_;

  // Boolean indicating whether the device is currently pairing and expecting a
  // PIN Code to be returned.
  bool expecting_pin_code_ = false;

  Microsoft::WRL::ComPtr<
      ABI::Windows::Devices::Enumeration::IDeviceInformationCustomPairing>
      custom_pairing_;
  Callback callback_;
  ErrorCallback error_callback_;

  base::Optional<EventRegistrationToken> pairing_requested_token_;

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IDeferral> pairing_deferral_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Devices::Enumeration::IDevicePairingRequestedEventArgs>
      pairing_requested_;

  base::WeakPtrFactory<BluetoothPairingWinrt> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothPairingWinrt);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_PAIRING_WINRT_H_
