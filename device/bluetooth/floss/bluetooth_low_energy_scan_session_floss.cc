// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_low_energy_scan_session_floss.h"

#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#include "device/bluetooth/floss/bluetooth_device_floss.h"

namespace floss {

BluetoothLowEnergyScanSessionFloss::BluetoothLowEnergyScanSessionFloss(
    base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate,
    base::OnceCallback<void(const std::string&)> destructor_callback)
    : delegate_(std::move(delegate)),
      destructor_callback_(std::move(destructor_callback)) {}

BluetoothLowEnergyScanSessionFloss::~BluetoothLowEnergyScanSessionFloss() {
  std::move(destructor_callback_).Run(uuid_.value());
}

void BluetoothLowEnergyScanSessionFloss::OnActivate(uint8_t scanner_id,
                                                    bool success) {
  scanner_id_ = scanner_id;
  if (!delegate_) {
    return;
  }

  if (!success) {
    delegate_->OnSessionStarted(
        this, BluetoothLowEnergyScanSession::ErrorCode::kFailed);
    return;
  }

  has_activated_ = true;
  delegate_->OnSessionStarted(this, /*error_code=*/absl::nullopt);
}

void BluetoothLowEnergyScanSessionFloss::OnRelease() {
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

void BluetoothLowEnergyScanSessionFloss::OnDeviceFound(
    device::BluetoothDevice* device) {
  if (!delegate_ || !device) {
    return;
  }

  delegate_->OnDeviceFound(this, device);
}

void BluetoothLowEnergyScanSessionFloss::OnDeviceLost(
    device::BluetoothDevice* device) {
  if (!delegate_ || !device) {
    return;
  }

  delegate_->OnDeviceLost(this, device);
}

void BluetoothLowEnergyScanSessionFloss::OnRegistered(
    device::BluetoothUUID uuid) {
  uuid_ = uuid;
}

}  // namespace floss
