// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_SCAN_FILTER_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_SCAN_FILTER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

class DEVICE_BLUETOOTH_EXPORT BluetoothLowEnergyScanFilter {
 public:
  // Standard data types found in the Bluetooth Assigned Numbers Specification
  // for Generic Access Profile (GAP).
  enum class AdvertisementDataType {
    kFlags = 0x01,
    kServiceUUIDs = 0x03,
    kShortenedLocalName = 0x08,
    kCompleteLocalName = 0x09,
    kListOf16BitServiceSolicitationUUIDs = 0x14,
    kListOf128BitServiceSolicitationUUIDs = 0x15,
    kServiceData = 0x16,
    kServiceData32BitUUID = 0x20,
    kServiceData128BitUUID = 0x21,
    kListOf32BitServiceSolicitationUUIDs = 0x1F,
    kBroadcastCode = 0x2D,
    kManufacturerSpecificData = 0xFF
  };

  // Used as an alternative to specifying RSSI threshold values directly. Since
  // measured signal strength will vary with different combinations of devices
  // and environmental conditions, distances are provided as rough guidelines.
  // However the goal is that there will be no false negatives at the listed
  // distance.
  enum class Range {
    // Roughly 1.5 ft.
    kImmediate,
    // Roughly 6 ft.
    kNear,
    // Roughly 20 ft.
    kFar
  };

  class Pattern {
   public:
    explicit Pattern(uint8_t start_position,
                     AdvertisementDataType data_type,
                     const std::vector<uint8_t>& value);
    Pattern(const Pattern&);
    ~Pattern();

    bool IsValid() const;

    uint8_t start_position() const { return start_position_; }
    AdvertisementDataType data_type() const { return data_type_; }
    const std::vector<uint8_t>& value() const { return value_; }

   private:
    uint8_t start_position_;
    AdvertisementDataType data_type_;
    std::vector<uint8_t> value_;
  };

  // Returns nullptr if the provided parameters fail validation. See
  // documentation on instance variables for details.
  static std::unique_ptr<BluetoothLowEnergyScanFilter> Create(
      Range device_range,
      base::TimeDelta device_found_timeout,
      base::TimeDelta device_lost_timeout,
      const std::vector<Pattern>& patterns,
      std::optional<base::TimeDelta> rssi_sampling_period);

  static std::unique_ptr<BluetoothLowEnergyScanFilter> Create(
      int16_t device_found_rssi_threshold,
      int16_t device_lost_rssi_threshold,
      base::TimeDelta device_found_timeout,
      base::TimeDelta device_lost_timeout,
      const std::vector<Pattern>& patterns,
      std::optional<base::TimeDelta> rssi_sampling_period);

  BluetoothLowEnergyScanFilter(const BluetoothLowEnergyScanFilter&) = delete;
  BluetoothLowEnergyScanFilter& operator=(const BluetoothLowEnergyScanFilter&) =
      delete;
  ~BluetoothLowEnergyScanFilter();

  int16_t device_found_rssi_threshold() const {
    return device_found_rssi_threshold_;
  }
  int16_t device_lost_rssi_threshold() const {
    return device_lost_rssi_threshold_;
  }
  base::TimeDelta device_found_timeout() const { return device_found_timeout_; }
  base::TimeDelta device_lost_timeout() const { return device_lost_timeout_; }
  const std::vector<Pattern>& patterns() const { return patterns_; }
  const std::optional<base::TimeDelta>& rssi_sampling_period() const {
    return rssi_sampling_period_;
  }

 private:
  BluetoothLowEnergyScanFilter(
      int16_t device_found_rssi_threshold,
      int16_t device_lost_rssi_threshold,
      base::TimeDelta device_found_timeout,
      base::TimeDelta device_lost_timeout,
      std::vector<Pattern> patterns,
      std::optional<base::TimeDelta> rssi_sampling_period);

  bool IsValid() const;

  // Must be between -127 and 20, inclusive, and must be greater than to
  // |device_lost_rssi_threshold_|.
  int16_t device_found_rssi_threshold_;

  // Must be between -127 and 20, inclusive, and must be less than
  // |device_found_rssi_threshold_|.
  int16_t device_lost_rssi_threshold_;

  // Must be between 1 and 300 seconds, inclusive. A device must be above
  // |device_found_rssi_threshold_| for |device_found_timeout_| seconds before
  // it is reported as found.
  base::TimeDelta device_found_timeout_;

  // Must be between 1 and 300 seconds, inclusive. A device must be below
  // |device_lost_rssi_threshold_| for |device_lost_timeout_| seconds before it
  // is reported as lost.
  base::TimeDelta device_lost_timeout_;

  // Must not be empty. For each pattern, Pattern::IsValid() must also pass.
  std::vector<Pattern> patterns_;

  // Must be between 0 and 254000 ms, inclusive. Will be rounded up to the
  // nearest 100 ms. If set to 0 all advertisement packets from in-range devices
  // are propagated. If unset only the first advertisement packet of in-range
  // devices are propagated. If set between 1 and 254000 ms advertisements are
  // propagated after the specified time period (rounded up to the nearest 100
  // ms). A lower sampling period will result in higher power consumption, with
  // the default setting being the most power-efficient.
  std::optional<base::TimeDelta> rssi_sampling_period_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_SCAN_FILTER_H_
