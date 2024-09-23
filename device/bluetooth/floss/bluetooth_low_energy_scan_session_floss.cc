// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/bluetooth_low_energy_scan_session_floss.h"

#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_low_energy_scan_session.h"
#include "device/bluetooth/floss/bluetooth_device_floss.h"

namespace floss {

BluetoothLowEnergyScanSessionFloss::BluetoothLowEnergyScanSessionFloss(
    std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
    base::WeakPtr<device::BluetoothLowEnergyScanSession::Delegate> delegate,
    base::OnceCallback<void(const std::string&)> destructor_callback)
    : filter_(std::move(filter)),
      delegate_(std::move(delegate)),
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
  delegate_->OnSessionStarted(this, /*error_code=*/std::nullopt);
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

std::optional<ScanFilter>
BluetoothLowEnergyScanSessionFloss::GetFlossScanFilter() {
  if (!filter_)
    return std::nullopt;

  ScanFilter filter;
  filter.rssi_high_threshold = filter_->device_found_rssi_threshold();
  filter.rssi_low_threshold = filter_->device_lost_rssi_threshold();
  filter.rssi_low_timeout = filter_->device_lost_timeout().InSeconds();
  if (filter_->rssi_sampling_period().has_value()) {
    filter.rssi_sampling_period =
        filter_->rssi_sampling_period().value().InMilliseconds() / 100;
  } else {
    // If no given value, use default value of reporting only one advertisement
    // per device during monitoring period
    filter.rssi_sampling_period = 0xFF;
  }

  for (auto& pattern : filter_->patterns()) {
    ScanFilterPattern p;
    p.start_position = pattern.start_position();
    p.ad_type = static_cast<uint8_t>(pattern.data_type());
    p.content = pattern.value();

    filter.condition.patterns.push_back(p);
  }

  return filter;
}

}  // namespace floss
