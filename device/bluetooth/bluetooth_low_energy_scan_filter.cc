// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"

#include "base/memory/ptr_util.h"

namespace device {

BluetoothLowEnergyScanFilter::Pattern::Pattern(
    uint8_t start_position,
    AdvertisementDataType data_type,
    const std::vector<uint8_t>& value)
    : start_position_(start_position),
      data_type_(data_type),
      value_(std::move(value)) {}

BluetoothLowEnergyScanFilter::Pattern::Pattern(const Pattern&) = default;
BluetoothLowEnergyScanFilter::Pattern::~Pattern() = default;

// static
std::unique_ptr<BluetoothLowEnergyScanFilter>
BluetoothLowEnergyScanFilter::Create(
    int16_t device_found_rssi_threshold,
    int16_t device_lost_rssi_threshold,
    base::TimeDelta device_found_timeout,
    base::TimeDelta device_lost_timeout,
    const std::vector<BluetoothLowEnergyScanFilter::Pattern>& patterns) {
  // We use WrapUnique() here so that we can call the private constructor.
  return base::WrapUnique(new BluetoothLowEnergyScanFilter(
      device_found_rssi_threshold, device_lost_rssi_threshold,
      device_found_timeout, device_lost_timeout, patterns));
}

BluetoothLowEnergyScanFilter::BluetoothLowEnergyScanFilter(
    int16_t device_found_rssi_threshold,
    int16_t device_lost_rssi_threshold,
    base::TimeDelta device_found_timeout,
    base::TimeDelta device_lost_timeout,
    std::vector<Pattern> patterns)
    : device_found_rssi_threshold_(device_found_rssi_threshold),
      device_lost_rssi_threshold_(device_lost_rssi_threshold),
      device_found_timeout_(device_found_timeout),
      device_lost_timeout_(device_lost_timeout),
      patterns_(patterns) {}

BluetoothLowEnergyScanFilter::~BluetoothLowEnergyScanFilter() = default;

}  // namespace device
