// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/web_bluetooth_pairing_manager.h"

#include <utility>

#include "base/callback_helpers.h"
#include "content/browser/bluetooth/web_bluetooth_pairing_manager_delegate.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"


namespace content {

using ::blink::mojom::WebBluetoothService;
using ::device::BluetoothDevice;

namespace {
void OnPairForReadCharacteristicCallback(
    std::string characteristic_instance_id,
    WebBluetoothPairingManagerDelegate* pairing_manager_delegate,
    WebBluetoothService::RemoteCharacteristicReadValueCallback callback,
    absl::optional<BluetoothDevice::ConnectErrorCode> error_code) {
  if (error_code) {
    std::move(callback).Run(
        WebBluetoothServiceImpl::TranslateConnectErrorAndRecord(*error_code),
        /*value=*/absl::nullopt);
    return;
  }
  pairing_manager_delegate->RemoteCharacteristicReadValue(
      characteristic_instance_id, std::move(callback));
}

void OnPairForWriteCharacteristicCallback(
    std::string characteristic_instance_id,
    WebBluetoothPairingManagerDelegate* pairing_manager_delegate,
    std::vector<uint8_t> value,
    blink::mojom::WebBluetoothWriteType write_type,
    WebBluetoothService::RemoteCharacteristicWriteValueCallback callback,
    absl::optional<BluetoothDevice::ConnectErrorCode> error_code) {
  if (error_code) {
    std::move(callback).Run(
        WebBluetoothServiceImpl::TranslateConnectErrorAndRecord(*error_code));
    return;
  }
  pairing_manager_delegate->RemoteCharacteristicWriteValue(
      characteristic_instance_id, value, write_type, std::move(callback));
}

void OnPairForReadDescriptorCallback(
    const std::string& descriptor_instance_id,
    WebBluetoothPairingManagerDelegate* pairing_manager_delegate,
    WebBluetoothService::RemoteDescriptorReadValueCallback callback,
    absl::optional<BluetoothDevice::ConnectErrorCode> error_code) {
  if (error_code) {
    std::move(callback).Run(
        WebBluetoothServiceImpl::TranslateConnectErrorAndRecord(*error_code),
        /*value=*/absl::nullopt);
    return;
  }
  pairing_manager_delegate->RemoteDescriptorReadValue(descriptor_instance_id,
                                                      std::move(callback));
}

void OnPairForWriteDescriptorCallback(
    const std::string& descriptor_instance_id,
    WebBluetoothPairingManagerDelegate* pairing_manager_delegate,
    std::vector<uint8_t> value,
    WebBluetoothService::RemoteDescriptorWriteValueCallback callback,
    absl::optional<BluetoothDevice::ConnectErrorCode> error_code) {
  if (error_code) {
    std::move(callback).Run(
        WebBluetoothServiceImpl::TranslateConnectErrorAndRecord(*error_code));
    return;
  }
  pairing_manager_delegate->RemoteDescriptorWriteValue(
      descriptor_instance_id, value, std::move(callback));
}
}  // namespace

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

WebBluetoothPairingManager::~WebBluetoothPairingManager() {
  auto pending_pair_device_ids = std::move(pending_pair_device_ids_);
  for (const auto& device_id : pending_pair_device_ids) {
    pairing_manager_delegate_->CancelPairing(device_id);
  }
}

void WebBluetoothPairingManager::PairForCharacteristicReadValue(
    const std::string& characteristic_instance_id,
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

  PairDevice(
      device_id, /*num_pair_attempts=*/0,
      base::BindOnce(&OnPairForReadCharacteristicCallback,
                     characteristic_instance_id, pairing_manager_delegate_,
                     std::move(read_callback)));
}

void WebBluetoothPairingManager::PairForCharacteristicWriteValue(
    const std::string& characteristic_instance_id,
    const std::vector<uint8_t>& value,
    blink::mojom::WebBluetoothWriteType write_type,
    WebBluetoothService::RemoteCharacteristicWriteValueCallback callback) {
  blink::WebBluetoothDeviceId device_id =
      pairing_manager_delegate_->GetCharacteristicDeviceID(
          characteristic_instance_id);
  if (!device_id.IsValid()) {
    std::move(callback).Run(
        WebBluetoothServiceImpl::TranslateConnectErrorAndRecord(
            BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN));
    return;
  }

  PairDevice(
      device_id, /*num_pair_attempts=*/0,
      base::BindOnce(&OnPairForWriteCharacteristicCallback,
                     characteristic_instance_id, pairing_manager_delegate_,
                     value, write_type, std::move(callback)));
}

void WebBluetoothPairingManager::PairForDescriptorReadValue(
    const std::string& descriptor_instance_id,
    WebBluetoothService::RemoteDescriptorReadValueCallback read_callback) {
  blink::WebBluetoothDeviceId device_id =
      pairing_manager_delegate_->GetDescriptorDeviceId(descriptor_instance_id);
  if (!device_id.IsValid()) {
    std::move(read_callback)
        .Run(WebBluetoothServiceImpl::TranslateConnectErrorAndRecord(
                 BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN),
             /*value=*/absl::nullopt);
    return;
  }

  PairDevice(
      device_id, /*num_pair_attempts=*/0,
      base::BindOnce(&OnPairForReadDescriptorCallback, descriptor_instance_id,
                     pairing_manager_delegate_, std::move(read_callback)));
}

void WebBluetoothPairingManager::PairForDescriptorWriteValue(
    const std::string& descriptor_instance_id,
    const std::vector<uint8_t>& value,
    WebBluetoothService::RemoteDescriptorWriteValueCallback callback) {
  blink::WebBluetoothDeviceId device_id =
      pairing_manager_delegate_->GetDescriptorDeviceId(descriptor_instance_id);
  if (!device_id.IsValid()) {
    std::move(callback).Run(
        WebBluetoothServiceImpl::TranslateConnectErrorAndRecord(
            BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN));
    return;
  }

  PairDevice(device_id, /*num_pair_attempts=*/0,
             base::BindOnce(&OnPairForWriteDescriptorCallback,
                            descriptor_instance_id, pairing_manager_delegate_,
                            std::move(value), std::move(callback)));
}

void WebBluetoothPairingManager::PairDevice(
    blink::WebBluetoothDeviceId device_id,
    int num_pair_attempts,
    device::BluetoothDevice::ConnectCallback callback) {
  DCHECK(device_id.IsValid());
  if (pending_pair_device_ids_.contains(device_id)) {
    std::move(callback).Run(
        BluetoothDevice::ConnectErrorCode::ERROR_AUTH_CANCELED);
    return;
  }
  pending_pair_device_ids_.insert(device_id);

  pairing_manager_delegate_->PairDevice(
      device_id, /*pairing_delegate=*/this,
      base::BindOnce(&WebBluetoothPairingManager::OnPairDevice,
                     weak_ptr_factory_.GetWeakPtr(), device_id,
                     num_pair_attempts + 1, std::move(callback)));
}

void WebBluetoothPairingManager::OnPairDevice(
    blink::WebBluetoothDeviceId device_id,
    int num_pair_attempts,
    BluetoothDevice::ConnectCallback callback,
    absl::optional<BluetoothDevice::ConnectErrorCode> error_code) {
  pending_pair_device_ids_.erase(device_id);
  if (!error_code) {
    std::move(callback).Run(/*error_code=*/absl::nullopt);
    return;
  }
  if (*error_code == BluetoothDevice::ConnectErrorCode::ERROR_AUTH_REJECTED &&
      num_pair_attempts < kMaxPairAttempts) {
    PairDevice(device_id, num_pair_attempts, std::move(callback));
    return;
  }
  std::move(callback).Run(error_code);
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
