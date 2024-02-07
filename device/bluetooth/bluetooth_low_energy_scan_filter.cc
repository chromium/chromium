// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"

#include <optional>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace {

// These values can be found in the "BlueZ D-Bus Advertisement Monitor API
// Description": //third_party/bluez/current/doc/advertisement-monitor-api.txt
constexpr int16_t kRSSIThresholdMin = -127;
constexpr int16_t kRSSIThresholdMax = 20;
constexpr base::TimeDelta kTimeoutMin = base::Seconds(1);
constexpr base::TimeDelta kTimeoutMax = base::Seconds(300);
constexpr uint8_t kPatternValueMaxLength = 31;
constexpr base::TimeDelta kRSSISamplingPeriodMin = base::Milliseconds(0);
constexpr base::TimeDelta kRSSISamplingPeriodMax = base::Milliseconds(254000);

// These values are based on real-world testing with the goal that they will be
// as high as possible without any false negatives at 1.5/6/20 feet,
// respectively.
constexpr int16_t kImmediateDeviceFoundRSSIThreshold = -60;
constexpr int16_t kImmediateDeviceLostRSSIThreshold = -75;
constexpr int16_t kNearDeviceFoundRSSIThreshold = -65;
constexpr int16_t kNearDeviceLostRSSIThreshold = -80;
constexpr int16_t kFarDeviceFoundRSSIThreshold = -85;
constexpr int16_t kFarDeviceLostRSSIThreshold = -100;

int16_t GetDeviceFoundRSSIThreshold(
    device::BluetoothLowEnergyScanFilter::Range range) {
  switch (range) {
    case device::BluetoothLowEnergyScanFilter::Range::kImmediate:
      return kImmediateDeviceFoundRSSIThreshold;
    case device::BluetoothLowEnergyScanFilter::Range::kNear:
      return kNearDeviceFoundRSSIThreshold;
    case device::BluetoothLowEnergyScanFilter::Range::kFar:
      return kFarDeviceFoundRSSIThreshold;
  }
}

int16_t GetDeviceLostRSSIThreshold(
    device::BluetoothLowEnergyScanFilter::Range range) {
  switch (range) {
    case device::BluetoothLowEnergyScanFilter::Range::kImmediate:
      return kImmediateDeviceLostRSSIThreshold;
    case device::BluetoothLowEnergyScanFilter::Range::kNear:
      return kNearDeviceLostRSSIThreshold;
    case device::BluetoothLowEnergyScanFilter::Range::kFar:
      return kFarDeviceLostRSSIThreshold;
  }
}

base::TimeDelta GetRoundedRSSISamplingPeriod(
    base::TimeDelta rssi_sampling_period) {
  // Do not round negative numbers to zero.
  if (rssi_sampling_period.InMilliseconds() < 0)
    return rssi_sampling_period;

  return rssi_sampling_period.CeilToMultiple(base::Milliseconds(100));
}

}  // namespace

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

bool BluetoothLowEnergyScanFilter::Pattern::IsValid() const {
  // Start position plus size of value must be less than or equal to 31 bytes.
  if (start_position_ + value_.size() > kPatternValueMaxLength) {
    DVLOG(1) << "Invalid filter configuration. Pattern start position plus "
                "length must be less than or equal to "
             << kPatternValueMaxLength << " bytes.";
    return false;
  }

  return true;
}

// static
std::unique_ptr<BluetoothLowEnergyScanFilter>
BluetoothLowEnergyScanFilter::Create(
    Range device_range,
    base::TimeDelta device_found_timeout,
    base::TimeDelta device_lost_timeout,
    const std::vector<Pattern>& patterns,
    std::optional<base::TimeDelta> rssi_sampling_period) {
  return Create(GetDeviceFoundRSSIThreshold(device_range),
                GetDeviceLostRSSIThreshold(device_range), device_found_timeout,
                device_lost_timeout, patterns, rssi_sampling_period);
}

// static
std::unique_ptr<BluetoothLowEnergyScanFilter>
BluetoothLowEnergyScanFilter::Create(
    int16_t device_found_rssi_threshold,
    int16_t device_lost_rssi_threshold,
    base::TimeDelta device_found_timeout,
    base::TimeDelta device_lost_timeout,
    const std::vector<BluetoothLowEnergyScanFilter::Pattern>& patterns,
    std::optional<base::TimeDelta> rssi_sampling_period) {
  // We use WrapUnique() here so that we can call the private constructor.
  auto filter = base::WrapUnique(new BluetoothLowEnergyScanFilter(
      device_found_rssi_threshold, device_lost_rssi_threshold,
      device_found_timeout, device_lost_timeout, patterns,
      rssi_sampling_period));
  if (!filter->IsValid()) {
    return nullptr;
  }

  return filter;
}

BluetoothLowEnergyScanFilter::BluetoothLowEnergyScanFilter(
    int16_t device_found_rssi_threshold,
    int16_t device_lost_rssi_threshold,
    base::TimeDelta device_found_timeout,
    base::TimeDelta device_lost_timeout,
    std::vector<Pattern> patterns,
    std::optional<base::TimeDelta> rssi_sampling_period)
    : device_found_rssi_threshold_(device_found_rssi_threshold),
      device_lost_rssi_threshold_(device_lost_rssi_threshold),
      device_found_timeout_(device_found_timeout),
      device_lost_timeout_(device_lost_timeout),
      patterns_(patterns),
      rssi_sampling_period_(rssi_sampling_period) {
  // |rssi_sampling_period_| needs to be rounded because the BlueZ API only
  // supports 100ms increments.
  if (rssi_sampling_period_.has_value()) {
    rssi_sampling_period_ =
        GetRoundedRSSISamplingPeriod(rssi_sampling_period_.value());
  }
}

BluetoothLowEnergyScanFilter::~BluetoothLowEnergyScanFilter() = default;

bool BluetoothLowEnergyScanFilter::IsValid() const {
  if (device_found_rssi_threshold_ < kRSSIThresholdMin ||
      device_lost_rssi_threshold_ < kRSSIThresholdMin) {
    DVLOG(1)
        << "Invalid filter configuration. RSSI thresholds must be larger than "
        << kRSSIThresholdMin << ".";
    return false;
  }

  if (device_found_rssi_threshold_ > kRSSIThresholdMax ||
      device_lost_rssi_threshold_ > kRSSIThresholdMax) {
    DVLOG(1)
        << "Invalid filter configuration. RSSI thresholds must be smaller than "
        << kRSSIThresholdMax << ".";
    return false;
  }

  if (device_found_rssi_threshold_ <= device_lost_rssi_threshold_) {
    DVLOG(1) << "Invalid filter configuration. Device found RSSI threshold "
                "must be larger than device lost RSSI threshold.";
    return false;
  }

  if (device_found_timeout_ < kTimeoutMin ||
      device_lost_timeout_ < kTimeoutMin) {
    DVLOG(1) << "Invalid filter configuration. Timeouts must be larger than "
             << kTimeoutMin << ".";
    return false;
  }

  if (device_found_timeout_ > kTimeoutMax ||
      device_lost_timeout_ > kTimeoutMax) {
    DVLOG(1) << "Invalid filter configuration. Timeouts must be smaller than "
             << kTimeoutMax << ".";
    return false;
  }

  if (patterns_.empty()) {
    DVLOG(1) << "Invalid filter configuration. Patterns must not be empty.";
    return false;
  }

  for (const auto& pattern : patterns_) {
    if (!pattern.IsValid()) {
      return false;
    }
  }

  if (rssi_sampling_period_.has_value() &&
      (rssi_sampling_period_.value() < kRSSISamplingPeriodMin ||
       rssi_sampling_period_.value() > kRSSISamplingPeriodMax)) {
    DVLOG(1) << "Invalid RSSI sampling rate: "
             << rssi_sampling_period_.value().InMilliseconds()
             << " ms . RSSI sampling rate must be between "
             << kRSSISamplingPeriodMin.InMilliseconds() << " ms and "
             << kRSSISamplingPeriodMax.InMilliseconds() << " ms,  inclusive.";
    return false;
  }

  return true;
}

}  // namespace device
