// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <vector>

#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Values for a valid filter configuration.
constexpr int16_t kDeviceFoundRSSIThreshold = -80;
constexpr int16_t kDeviceLostRSSIThreshold = -100;
constexpr base::TimeDelta kDeviceFoundTimeout = base::TimeDelta::FromSeconds(1);
constexpr base::TimeDelta kDeviceLostTimeout = base::TimeDelta::FromSeconds(5);
const std::vector<uint8_t> kPatternValue = {0xff};

device::BluetoothLowEnergyScanFilter::Pattern GetPattern() {
  return device::BluetoothLowEnergyScanFilter::Pattern(
      /*start_position=*/0,
      device::BluetoothLowEnergyScanFilter::AdvertisementDataType::kServiceData,
      kPatternValue);
}

}  // namespace

namespace device {

TEST(BluetoothLowEnergyScanFilterTest, Valid) {
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      kDeviceLostTimeout, {GetPattern()});
  EXPECT_TRUE(filter);
}

TEST(BluetoothLowEnergyScanFilterTest, InvalidNoPattern) {
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      kDeviceLostTimeout, {});
  EXPECT_FALSE(filter);
}

TEST(BluetoothLowEnergyScanFilterTest, InvalidPatternTooLong) {
  device::BluetoothLowEnergyScanFilter::Pattern pattern(
      /*start_position=*/64,
      device::BluetoothLowEnergyScanFilter::AdvertisementDataType::kServiceData,
      kPatternValue);
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      kDeviceLostTimeout, {pattern});
  EXPECT_FALSE(filter);
}

TEST(BluetoothLowEnergyScanFilterTest, InvalidBadTimeout) {
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      base::TimeDelta::FromSeconds(0), {GetPattern()});
  EXPECT_FALSE(filter);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      base::TimeDelta::FromSeconds(301), {GetPattern()});
  EXPECT_FALSE(filter);
}

TEST(BluetoothLowEnergyScanFilterTest, InvalidBadThresholds) {
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      /*device_found_rssi_threshold=*/-128, kDeviceLostRSSIThreshold,
      kDeviceFoundTimeout, kDeviceLostTimeout, {GetPattern()});
  EXPECT_FALSE(filter);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      /*device_found_rssi_threshold=*/21, kDeviceLostRSSIThreshold,
      kDeviceFoundTimeout, kDeviceLostTimeout, {GetPattern()});
  EXPECT_FALSE(filter);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, /*device_lost_rssi_threshold=*/-128,
      kDeviceFoundTimeout, kDeviceLostTimeout, {GetPattern()});
  EXPECT_FALSE(filter);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, /*device_lost_rssi_threshold=*/21,
      kDeviceFoundTimeout, kDeviceLostTimeout, {GetPattern()});
  EXPECT_FALSE(filter);

  // Expect a failure if the "device lost" threshold is greater than the "device
  // found" threshold.
  filter = device::BluetoothLowEnergyScanFilter::Create(
      /*device_found_rssi_threshold=*/-80, /*device_lost_rssi_threshold=*/-60,
      kDeviceFoundTimeout, kDeviceLostTimeout, {GetPattern()});
  EXPECT_FALSE(filter);
}

TEST(BluetoothLowEnergyScanFilterTest, ValidUsingRange) {
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      device::BluetoothLowEnergyScanFilter::Range::kImmediate,
      kDeviceFoundTimeout, kDeviceLostTimeout, {GetPattern()});
  EXPECT_TRUE(filter);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      device::BluetoothLowEnergyScanFilter::Range::kNear, kDeviceFoundTimeout,
      kDeviceLostTimeout, {GetPattern()});
  EXPECT_TRUE(filter);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      device::BluetoothLowEnergyScanFilter::Range::kFar, kDeviceFoundTimeout,
      kDeviceLostTimeout, {GetPattern()});
  EXPECT_TRUE(filter);
}

}  // namespace device
