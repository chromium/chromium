// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/web_bluetooth_pairing_manager.h"

#include <utility>

#include "base/callback_helpers.h"
#include "content/browser/bluetooth/web_bluetooth_pairing_manager_delegate.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"

using blink::mojom::WebBluetoothService;
using device::BluetoothDevice;

namespace content {

constexpr int WebBluetoothPairingManager::kMaxPairAttempts;

// TODO(960258): Ensure this delegate outlives any in-progress pairing operation
// for which it is used. Additionally review use of WebBluetoothDeviceId vs.
// BluetoothDevice as well as how to deal with simultaneous pairing requests
// for the same device.
WebBluetoothPairingManager::WebBluetoothPairingManager(
    WebBluetoothPairingManagerDelegate* pairing_manager_delegate)
    : pairing_manager_delegate_(pairing_manager_delegate) {
  DCHECK(pairing_manager_delegate_);
}

WebBluetoothPairingManager::~WebBluetoothPairingManager() = default;

void WebBluetoothPairingManager::PairForCharacteristicReadValue(
    const std::string& characteristic_instance_id,
    int num_pair_attempts,
    WebBluetoothService::RemoteCharacteristicReadValueCallback read_callback) {
  blink::WebBluetoothDeviceId device_id =
      pairing_manager_delegate_->GetCharacteristicDeviceID(
          characteristic_instance_id);
  if (!device_id.IsValid()) {
    std::move(read_callback)
        .Run(WebBluetoothServiceImpl::TranslateConnectErrorAndRecord(
                 BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN),
             /*value=*/absl::nullopt);
    return;
  }

  auto split_read_callback = base::SplitOnceCallback(std::move(read_callback));
  pairing_manager_delegate_->PairDevice(
      device_id, this,
      base::BindOnce(
          &WebBluetoothPairingManager::OnReadCharacteristicValuePairSuccess,
          weak_ptr_factory_.GetWeakPtr(), characteristic_instance_id,
          std::move(split_read_callback.first)),
      base::BindOnce(
          &WebBluetoothPairingManager::OnReadCharacteristicValuePairFailure,
          weak_ptr_factory_.GetWeakPtr(), characteristic_instance_id,
          num_pair_attempts + 1, std::move(split_read_callback.second)));
}

void WebBluetoothPairingManager::OnReadCharacteristicValuePairSuccess(
    std::string characteristic_instance_id,
    WebBluetoothService::RemoteCharacteristicReadValueCallback read_callback) {
  pairing_manager_delegate_->RemoteCharacteristicReadValue(
      characteristic_instance_id, std::move(read_callback));
}

void WebBluetoothPairingManager::OnReadCharacteristicValuePairFailure(
    std::string characteristic_instance_id,
    int num_pair_attempts,
    WebBluetoothService::RemoteCharacteristicReadValueCallback read_callback,
    BluetoothDevice::ConnectErrorCode error_code) {
  if (error_code == BluetoothDevice::ConnectErrorCode::ERROR_AUTH_REJECTED &&
      num_pair_attempts < kMaxPairAttempts) {
    PairForCharacteristicReadValue(characteristic_instance_id,
                                   num_pair_attempts, std::move(read_callback));
    return;
  }

  std::move(read_callback)
      .Run(WebBluetoothServiceImpl::TranslateConnectErrorAndRecord(error_code),
           /*value=*/absl::nullopt);
}

void WebBluetoothPairingManager::RequestPinCode(BluetoothDevice* device) {
  NOTIMPLEMENTED();
  // Upcoming CL will replace the hardcoded cancel with UI PIN prompt.
  // Cancelling the pairing operation fails with:
  // Unexpected failure: SecurityError: GATT operation not authorized.
  device->CancelPairing();
}

void WebBluetoothPairingManager::RequestPasskey(BluetoothDevice* device) {
  device->CancelPairing();
  NOTREACHED() << "Passkey pairing not supported.";
}

void WebBluetoothPairingManager::DisplayPinCode(BluetoothDevice* device,
                                                const std::string& pincode) {
  NOTIMPLEMENTED();
}

void WebBluetoothPairingManager::DisplayPasskey(BluetoothDevice* device,
                                                uint32_t passkey) {
  NOTIMPLEMENTED();
}

void WebBluetoothPairingManager::KeysEntered(BluetoothDevice* device,
                                             uint32_t entered) {
  NOTIMPLEMENTED();
}

void WebBluetoothPairingManager::ConfirmPasskey(BluetoothDevice* device,
                                                uint32_t passkey) {
  NOTIMPLEMENTED();
}

void WebBluetoothPairingManager::AuthorizePairing(BluetoothDevice* device) {
  NOTIMPLEMENTED();
}

}  // namespace content
