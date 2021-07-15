// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_SCAN_FILTER_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_SCAN_FILTER_H_

#include <stddef.h>
#include <stdint.h>
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

  class Pattern {
   public:
    explicit Pattern(uint8_t start_position,
                     AdvertisementDataType data_type,
                     const std::vector<uint8_t>& value);
    Pattern(const Pattern&);
    ~Pattern();

    uint8_t start_position() const { return start_position_; }
    AdvertisementDataType data_type() const { return data_type_; }
    const std::vector<uint8_t>& value() const { return value_; }

   private:
    uint8_t start_position_;
    AdvertisementDataType data_type_;
    std::vector<uint8_t> value_;
  };

  static std::unique_ptr<BluetoothLowEnergyScanFilter> Create(
      int16_t device_found_rssi_threshold,
      int16_t device_lost_rssi_threshold,
      base::TimeDelta device_found_timeout,
      base::TimeDelta device_lost_timeout,
      const std::vector<Pattern>& patterns);

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

 private:
  BluetoothLowEnergyScanFilter(int16_t device_found_rssi_threshold,
                               int16_t device_lost_rssi_threshold,
                               base::TimeDelta device_found_timeout,
                               base::TimeDelta device_lost_timeout,
                               std::vector<Pattern> patterns);

  int16_t device_found_rssi_threshold_;
  int16_t device_lost_rssi_threshold_;
  base::TimeDelta device_found_timeout_;
  base::TimeDelta device_lost_timeout_;
  std::vector<Pattern> patterns_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_SCAN_FILTER_H_
