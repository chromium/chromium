// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"

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

BluetoothLowEnergyScanFilter::BluetoothLowEnergyScanFilter(
    int16_t device_found_threshold,
    uint16_t device_found_timeout,
    int16_t device_lost_threshold,
    uint16_t device_lost_timeout)
    : device_found_threshold_(device_found_threshold),
      device_found_timeout_(device_found_timeout),
      device_lost_threshold_(device_lost_threshold),
      device_lost_timeout_(device_lost_timeout) {}

BluetoothLowEnergyScanFilter::~BluetoothLowEnergyScanFilter() = default;

void BluetoothLowEnergyScanFilter::AddPattern(uint8_t start_position,
                                              AdvertisementDataType data_type,
                                              std::vector<uint8_t> value) {
  patterns_.emplace_back(start_position, data_type, value);
}

}  // namespace device
