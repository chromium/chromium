// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_low_energy_scan_session_bluez.h"

#include <optional>

#include "base/debug/dump_without_crashing.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/dbus/bluetooth_device_client.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"

namespace bluez {

BluetoothLowEnergyScanSessionBlueZ::BluetoothLowEnergyScanSessionBlueZ(
    const std::string& session_id,
    base::WeakPtr<BluetoothAdapterBlueZ> adapter,
    base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate,
    base::OnceCallback<void(const std::string&)> destructor_callback)
    : session_id_(session_id),
      adapter_(std::move(adapter)),
      delegate_(std::move(delegate)),
      destructor_callback_(std::move(destructor_callback)) {}

BluetoothLowEnergyScanSessionBlueZ::~BluetoothLowEnergyScanSessionBlueZ() {
  std::move(destructor_callback_).Run(session_id_);
}

void BluetoothLowEnergyScanSessionBlueZ::OnActivate() {
  has_activated_ = true;
  if (!delegate_) {
    return;
  }

  delegate_->OnSessionStarted(this, /*error_code=*/std::nullopt);
}

void BluetoothLowEnergyScanSessionBlueZ::OnRelease() {
  if (!delegate_) {
    return;
  }

  if (!has_activated_) {
    delegate_->OnSessionStarted(
        this, BluetoothLowEnergyScanSession::ErrorCode::kFailed);
    return;
  }

  delegate_->OnSessionInvalidated(this);
}

void BluetoothLowEnergyScanSessionBlueZ::OnDeviceFound(
    const dbus::ObjectPath& device_path) {
  if (!delegate_) {
    return;
  }

  DCHECK(adapter_);
  device::BluetoothDevice* device = adapter_->GetDeviceWithPath(device_path);
  if (!device) {
    // TODO(b/212643004): Generate crash dumps to understand why the device
    // path is sometimes invalid but avoid notifying observers with a null
    // device.
    base::debug::DumpWithoutCrashing();
    return;
  }

  delegate_->OnDeviceFound(this, device);
}

void BluetoothLowEnergyScanSessionBlueZ::OnDeviceLost(
    const dbus::ObjectPath& device_path) {
  if (!delegate_) {
    return;
  }

  DCHECK(adapter_);
  device::BluetoothDevice* device = adapter_->GetDeviceWithPath(device_path);
  DCHECK(device);

  delegate_->OnDeviceLost(this, device);
}

base::WeakPtr<BluetoothLowEnergyScanSessionBlueZ>
BluetoothLowEnergyScanSessionBlueZ::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace bluez
