// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_pairing_bluez.h"

#include "base/logging.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"

using device::BluetoothDevice;

namespace {

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
  DVLOG(1) << "Created BluetoothPairingBlueZ for " << device_->GetAddress();
}

BluetoothPairingBlueZ::~BluetoothPairingBlueZ() {
  DVLOG(1) << "Destroying BluetoothPairingBlueZ for " << device_->GetAddress();

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

  pairing_delegate_ = nullptr;
}

void BluetoothPairingBlueZ::RequestPinCode(
    bluez::BluetoothAgentServiceProvider::Delegate::PinCodeCallback callback) {
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
  ResetCallbacks();
  confirmation_callback_ = std::move(callback);
  pairing_delegate_used_ = true;
  pairing_delegate_->ConfirmPasskey(device_, passkey);
}

void BluetoothPairingBlueZ::RequestAuthorization(
    bluez::BluetoothAgentServiceProvider::Delegate::ConfirmationCallback
        callback) {
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
