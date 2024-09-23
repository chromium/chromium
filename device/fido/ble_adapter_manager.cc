// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble_adapter_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/fido_request_handler_base.h"

namespace device {

namespace {
using BleStatus = FidoRequestHandlerBase::BleStatus;

const char* BleStatusToString(BleStatus ble_status) {
  switch (ble_status) {
    case BleStatus::kPendingPermissionRequest:
      return "Pending permission request";
    case BleStatus::kPermissionDenied:
      return "Permission denied";
    case BleStatus::kOff:
      return "Off";
    case BleStatus::kOn:
      return "On";
  }
}

BleStatus GetBleAdapterStatus(const BluetoothAdapter& adapter) {
  switch (adapter.GetOsPermissionStatus()) {
    case BluetoothAdapter::PermissionStatus::kUndetermined:
      return BleStatus::kPendingPermissionRequest;
    case BluetoothAdapter::PermissionStatus::kDenied:
      return BleStatus::kPermissionDenied;
    case BluetoothAdapter::PermissionStatus::kAllowed:
      return adapter.IsPowered() ? BleStatus::kOn : BleStatus::kOff;
  }
}

}  // namespace

BleAdapterManager::BleAdapterManager(FidoRequestHandlerBase* request_handler)
    : request_handler_(request_handler) {
  BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&BleAdapterManager::Start, weak_factory_.GetWeakPtr()));
}

BleAdapterManager::~BleAdapterManager() {
  if (adapter_) {
    adapter_->RemoveObserver(this);
  }
}

void BleAdapterManager::SetAdapterPower(bool set_power_on) {
  adapter_->SetPowered(set_power_on, base::DoNothing(), base::DoNothing());
}

void BleAdapterManager::RequestBluetoothPermission(
    FidoRequestHandlerBase::BlePermissionCallback callback) {
  adapter_->RequestSystemPermission(
      base::IgnoreArgs<BluetoothAdapter::PermissionStatus>(
          base::BindOnce(&BleAdapterManager::OnHaveBluetoothPermission,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void BleAdapterManager::OnHaveBluetoothPermission(
    FidoRequestHandlerBase::BlePermissionCallback callback) {
  BleStatus ble_status = GetBleAdapterStatus(*adapter_);
  if (ble_status == BleStatus::kPendingPermissionRequest) {
    FIDO_LOG(ERROR) << "Bluetooth API reports status as undetermined after "
                       "requesting permissions, assuming permission granted";
    std::move(callback).Run(adapter_->IsPowered() ? BleStatus::kOn
                                                  : BleStatus::kOff);
    return;
  }
  std::move(callback).Run(ble_status);
}

void BleAdapterManager::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                              bool powered) {
  BleStatus ble_status = GetBleAdapterStatus(*adapter);
  FIDO_LOG(DEBUG) << "Bluetooth status changed: "
                  << BleStatusToString(ble_status);
  request_handler_->OnBluetoothAdapterStatusChanged(ble_status);
}

void BleAdapterManager::Start(scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK(!adapter_);
  adapter_ = std::move(adapter);
  DCHECK(adapter_);
  adapter_->AddObserver(this);

  BleStatus ble_status = GetBleAdapterStatus(*adapter_);
  FIDO_LOG(DEBUG) << "Bluetooth status: " << BleStatusToString(ble_status);
  request_handler_->OnBluetoothAdapterEnumerated(
      adapter_->IsPresent(), ble_status, adapter_->CanPower(),
      adapter_->IsPeripheralRoleSupported());
}

}  // namespace device
