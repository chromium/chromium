// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_pairing_bluez.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"

using device::BluetoothDevice;

namespace {

// Histogram enumerations for pairing methods.
enum UMAPairingMethod {
  UMA_PAIRING_METHOD_NONE,
  UMA_PAIRING_METHOD_REQUEST_PINCODE,
  UMA_PAIRING_METHOD_REQUEST_PASSKEY,
  UMA_PAIRING_METHOD_DISPLAY_PINCODE,
  UMA_PAIRING_METHOD_DISPLAY_PASSKEY,
  UMA_PAIRING_METHOD_CONFIRM_PASSKEY,
  // NOTE: Add new pairing methods immediately above this line. Make sure to
  // update the enum list in tools/histogram/histograms.xml accordingly.
  UMA_PAIRING_METHOD_COUNT
};

// Number of keys that will be entered for a passkey, six digits plus the
// final enter.
const uint16_t kPasskeyMaxKeysEntered = 7;

}  // namespace

namespace bluez {

BluetoothPairingBlueZ::BluetoothPairingBlueZ(
    BluetoothDeviceBlueZ* device,
    BluetoothDevice::PairingDelegate* pairing_delegate)
    : device_(device),
      pairing_delegate_(pairing_delegate),
      pairing_delegate_used_(false) {
  VLOG(1) << "Created BluetoothPairingBlueZ for " << device_->GetAddress();
}

BluetoothPairingBlueZ::~BluetoothPairingBlueZ() {
  VLOG(1) << "Destroying BluetoothPairingBlueZ for " << device_->GetAddress();

  if (!pairing_delegate_used_) {
    UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingMethod",
                              UMA_PAIRING_METHOD_NONE,
                              UMA_PAIRING_METHOD_COUNT);
  }

  if (!pincode_callback_.is_null()) {
    std::move(pincode_callback_)
        .Run(bluez::BluetoothAgentServiceProvider::Delegate::CANCELLED, "");
  }

  if (!passkey_callback_.is_null()) {
    std::move(passkey_callback_)
        .Run(bluez::BluetoothAgentServiceProvider::Delegate::CANCELLED, 0);
  }

  if (!confirmation_callback_.is_null()) {
    std::move(confirmation_callback_)
        .Run(bluez::BluetoothAgentServiceProvider::Delegate::CANCELLED);
  }

  pairing_delegate_ = NULL;
}

void BluetoothPairingBlueZ::RequestPinCode(
    bluez::BluetoothAgentServiceProvider::Delegate::PinCodeCallback callback) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingMethod",
                            UMA_PAIRING_METHOD_REQUEST_PINCODE,
                            UMA_PAIRING_METHOD_COUNT);

  ResetCallbacks();
  pincode_callback_ = std::move(callback);
  pairing_delegate_used_ = true;
  pairing_delegate_->RequestPinCode(device_);
}

bool BluetoothPairingBlueZ::ExpectingPinCode() const {
  return !pincode_callback_.is_null();
}

void BluetoothPairingBlueZ::SetPinCode(const std::string& pincode) {
  if (pincode_callback_.is_null())
    return;

  std::move(pincode_callback_)
      .Run(bluez::BluetoothAgentServiceProvider::Delegate::SUCCESS, pincode);

  // If this is not an outgoing connection to the device, clean up the pairing
  // context since the pairing is done. The outgoing connection case is cleaned
  // up in the callback for the underlying Pair() call.
  if (!device_->IsConnecting())
    device_->EndPairing();
}

void BluetoothPairingBlueZ::DisplayPinCode(const std::string& pincode) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingMethod",
                            UMA_PAIRING_METHOD_DISPLAY_PINCODE,
                            UMA_PAIRING_METHOD_COUNT);

  ResetCallbacks();
  pairing_delegate_used_ = true;
  pairing_delegate_->DisplayPinCode(device_, pincode);

  // If this is not an outgoing connection to the device, the pairing context
  // needs to be cleaned up again as there's no reliable indication of
  // completion of incoming pairing.
  if (!device_->IsConnecting())
    device_->EndPairing();
}

void BluetoothPairingBlueZ::RequestPasskey(
    bluez::BluetoothAgentServiceProvider::Delegate::PasskeyCallback callback) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingMethod",
                            UMA_PAIRING_METHOD_REQUEST_PASSKEY,
                            UMA_PAIRING_METHOD_COUNT);

  ResetCallbacks();
  passkey_callback_ = std::move(callback);
  pairing_delegate_used_ = true;
  pairing_delegate_->RequestPasskey(device_);
}

bool BluetoothPairingBlueZ::ExpectingPasskey() const {
  return !passkey_callback_.is_null();
}

void BluetoothPairingBlueZ::SetPasskey(uint32_t passkey) {
  if (passkey_callback_.is_null())
    return;

  std::move(passkey_callback_)
      .Run(bluez::BluetoothAgentServiceProvider::Delegate::SUCCESS, passkey);

  // If this is not an outgoing connection to the device, clean up the pairing
  // context since the pairing is done. The outgoing connection case is cleaned
  // up in the callback for the underlying Pair() call.
  if (!device_->IsConnecting())
    device_->EndPairing();
}

void BluetoothPairingBlueZ::DisplayPasskey(uint32_t passkey) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingMethod",
                            UMA_PAIRING_METHOD_DISPLAY_PASSKEY,
                            UMA_PAIRING_METHOD_COUNT);

  ResetCallbacks();
  pairing_delegate_used_ = true;
  pairing_delegate_->DisplayPasskey(device_, passkey);
}

void BluetoothPairingBlueZ::KeysEntered(uint16_t entered) {
  pairing_delegate_used_ = true;
  pairing_delegate_->KeysEntered(device_, entered);

  // If this is not an outgoing connection to the device, the pairing context
  // needs to be cleaned up again as there's no reliable indication of
  // completion of incoming pairing.
  if (entered >= kPasskeyMaxKeysEntered && !device_->IsConnecting())
    device_->EndPairing();
}

void BluetoothPairingBlueZ::RequestConfirmation(
    uint32_t passkey,
    bluez::BluetoothAgentServiceProvider::Delegate::ConfirmationCallback
        callback) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingMethod",
                            UMA_PAIRING_METHOD_CONFIRM_PASSKEY,
                            UMA_PAIRING_METHOD_COUNT);

  ResetCallbacks();
  confirmation_callback_ = std::move(callback);
  pairing_delegate_used_ = true;
  pairing_delegate_->ConfirmPasskey(device_, passkey);
}

void BluetoothPairingBlueZ::RequestAuthorization(
    bluez::BluetoothAgentServiceProvider::Delegate::ConfirmationCallback
        callback) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.PairingMethod", UMA_PAIRING_METHOD_NONE,
                            UMA_PAIRING_METHOD_COUNT);

  ResetCallbacks();
  confirmation_callback_ = std::move(callback);
  pairing_delegate_used_ = true;
  pairing_delegate_->AuthorizePairing(device_);
}

bool BluetoothPairingBlueZ::ExpectingConfirmation() const {
  return !confirmation_callback_.is_null();
}

void BluetoothPairingBlueZ::ConfirmPairing() {
  if (confirmation_callback_.is_null())
    return;

  std::move(confirmation_callback_)
      .Run(bluez::BluetoothAgentServiceProvider::Delegate::SUCCESS);

  // If this is not an outgoing connection to the device, clean up the pairing
  // context since the pairing is done. The outgoing connection case is cleaned
  // up in the callback for the underlying Pair() call.
  if (!device_->IsConnecting())
    device_->EndPairing();
}

bool BluetoothPairingBlueZ::RejectPairing() {
  return RunPairingCallbacks(
      bluez::BluetoothAgentServiceProvider::Delegate::REJECTED);
}

bool BluetoothPairingBlueZ::CancelPairing() {
  return RunPairingCallbacks(
      bluez::BluetoothAgentServiceProvider::Delegate::CANCELLED);
}

BluetoothDevice::PairingDelegate* BluetoothPairingBlueZ::GetPairingDelegate()
    const {
  return pairing_delegate_;
}

void BluetoothPairingBlueZ::ResetCallbacks() {
  pincode_callback_.Reset();
  passkey_callback_.Reset();
  confirmation_callback_.Reset();
}

bool BluetoothPairingBlueZ::RunPairingCallbacks(
    bluez::BluetoothAgentServiceProvider::Delegate::Status status) {
  pairing_delegate_used_ = true;

  bool callback_run = false;
  if (!pincode_callback_.is_null()) {
    std::move(pincode_callback_).Run(status, "");
    callback_run = true;
  }

  if (!passkey_callback_.is_null()) {
    std::move(passkey_callback_).Run(status, 0);
    callback_run = true;
  }

  if (!confirmation_callback_.is_null()) {
    std::move(confirmation_callback_).Run(status);
    callback_run = true;
  }

  // If this is not an outgoing connection to the device, clean up the pairing
  // context since the pairing is done. The outgoing connection case is cleaned
  // up in the callback for the underlying Pair() call.
  if (!device_->IsConnecting())
    device_->EndPairing();

  return callback_run;
}

}  // namespace bluez
