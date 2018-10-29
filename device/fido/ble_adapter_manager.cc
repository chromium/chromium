// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble_adapter_manager.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/ble/fido_ble_device.h"

namespace device {

BleAdapterManager::BleAdapterManager(FidoRequestHandlerBase* request_handler)
    : request_handler_(request_handler), weak_factory_(this) {
  BluetoothAdapterFactory::Get().GetAdapter(base::BindRepeating(
      &BleAdapterManager::Start, weak_factory_.GetWeakPtr()));
}

BleAdapterManager::~BleAdapterManager() {
  if (adapter_powered_on_programmatically_)
    SetAdapterPower(false /* set_power_on */);

  if (adapter_) {
    adapter_->RemoveObserver(this);
    pairing_delegate_.CancelPairingOnAllKnownDevices(adapter_.get());
  }
}

void BleAdapterManager::SetAdapterPower(bool set_power_on) {
  if (set_power_on)
    adapter_powered_on_programmatically_ = true;

  adapter_->SetPowered(set_power_on, base::DoNothing(), base::DoNothing());
}

void BleAdapterManager::InitiatePairing(std::string authenticator_id,
                                        std::string pin_code,
                                        base::OnceClosure success_callback,
                                        base::OnceClosure error_callback) {
  DCHECK(adapter_);
  auto device_list = adapter_->GetDevices();
  auto device_it = std::find_if(
      device_list.begin(), device_list.end(),
      [&authenticator_id](const auto& bluetooth_device) {
        return FidoBleDevice::GetId(bluetooth_device->GetAddress()) ==
               authenticator_id;
      });

  if (device_it == device_list.end() ||
      !request_handler_->HasAuthenticator(authenticator_id)) {
    std::move(error_callback).Run();
    return;
  }

  pairing_delegate_.StoreBlePinCodeForDevice(std::move(authenticator_id),
                                             std::move(pin_code));

  auto failure_callback = base::BindOnce(
      [](base::OnceClosure callback,
         BluetoothDevice::ConnectErrorCode error_code) {
        std::move(callback).Run();
      },
      std::move(error_callback));

  (*device_it)
      ->Pair(&pairing_delegate_,
             base::AdaptCallbackForRepeating(std::move(success_callback)),
             base::AdaptCallbackForRepeating(std::move(failure_callback)));
}

void BleAdapterManager::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                              bool powered) {
  request_handler_->OnBluetoothAdapterPowerChanged(powered);
}

void BleAdapterManager::DeviceAddressChanged(BluetoothAdapter* adapter,
                                             BluetoothDevice* device,
                                             const std::string& old_address) {
  pairing_delegate_.ChangeStoredDeviceAddress(
      FidoBleDevice::GetId(old_address),
      FidoBleDevice::GetId(device->GetAddress()));
}

void BleAdapterManager::Start(scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK(!adapter_);
  adapter_ = std::move(adapter);
  DCHECK(adapter_);
  adapter_->AddObserver(this);

  request_handler_->OnBluetoothAdapterEnumerated(
      adapter_->IsPresent(), adapter_->IsPowered(), adapter_->CanPower());
}

}  // namespace device
