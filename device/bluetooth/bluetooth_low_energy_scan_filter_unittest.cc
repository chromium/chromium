// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"

#include <stdint.h>

#include <optional>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Values for a valid filter configuration.
constexpr int16_t kDeviceFoundRSSIThreshold = -80;
constexpr int16_t kDeviceLostRSSIThreshold = -100;
constexpr base::TimeDelta kDeviceFoundTimeout = base::Seconds(1);
constexpr base::TimeDelta kDeviceLostTimeout = base::Seconds(5);
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
      kDeviceLostTimeout, {GetPattern()},
      /*rssi_sampling_period=*/std::nullopt);
  EXPECT_TRUE(filter);
}

TEST(BluetoothLowEnergyScanFilterTest, InvalidNoPattern) {
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      kDeviceLostTimeout, {}, /*rssi_sampling_period=*/std::nullopt);
  EXPECT_FALSE(filter);
}

TEST(BluetoothLowEnergyScanFilterTest, InvalidPatternTooLong) {
  device::BluetoothLowEnergyScanFilter::Pattern pattern(
      /*start_position=*/64,
      device::BluetoothLowEnergyScanFilter::AdvertisementDataType::kServiceData,
      kPatternValue);
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      kDeviceLostTimeout, {pattern}, /*rssi_sampling_period=*/std::nullopt);
  EXPECT_FALSE(filter);
}

TEST(BluetoothLowEnergyScanFilterTest, InvalidBadTimeout) {
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      base::Seconds(0), {GetPattern()}, /*rssi_sampling_period=*/std::nullopt);
  EXPECT_FALSE(filter);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      base::Seconds(301), {GetPattern()},
      /*rssi_sampling_period=*/std::nullopt);
  EXPECT_FALSE(filter);
}

TEST(BluetoothLowEnergyScanFilterTest, InvalidBadRssiSamplingPeriod) {
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      base::Seconds(0), {GetPattern()},
      /*rssi_sampling_period=*/base::Milliseconds(-1));
  EXPECT_FALSE(filter);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      base::Seconds(301), {GetPattern()},
      /*rssi_sampling_period=*/base::Milliseconds(254001));
  EXPECT_FALSE(filter);
}

TEST(BluetoothLowEnergyScanFilterTest, ValidRssiSamplingPeriod) {
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      kDeviceLostTimeout, {GetPattern()},
      /*rssi_sampling_period=*/base::Milliseconds(0));
  EXPECT_TRUE(filter);
  EXPECT_EQ(filter->rssi_sampling_period().value().InMilliseconds(), 0);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      kDeviceLostTimeout, {GetPattern()},
      /*rssi_sampling_period=*/base::Milliseconds(254000));
  EXPECT_TRUE(filter);
  EXPECT_EQ(filter->rssi_sampling_period().value().InMilliseconds(), 254000);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      kDeviceLostTimeout, {GetPattern()},
      /*rssi_sampling_period=*/base::Milliseconds(23));
  EXPECT_TRUE(filter);
  EXPECT_EQ(filter->rssi_sampling_period().value().InMilliseconds(), 100);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, kDeviceLostRSSIThreshold, kDeviceFoundTimeout,
      kDeviceLostTimeout, {GetPattern()},
      /*rssi_sampling_period=*/std::nullopt);
  EXPECT_TRUE(filter);
  EXPECT_FALSE(filter->rssi_sampling_period().has_value());
}

TEST(BluetoothLowEnergyScanFilterTest, InvalidBadThresholds) {
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      /*device_found_rssi_threshold=*/-128, kDeviceLostRSSIThreshold,
      kDeviceFoundTimeout, kDeviceLostTimeout, {GetPattern()},
      /*rssi_sampling_period=*/std::nullopt);
  EXPECT_FALSE(filter);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      /*device_found_rssi_threshold=*/21, kDeviceLostRSSIThreshold,
      kDeviceFoundTimeout, kDeviceLostTimeout, {GetPattern()},
      /*rssi_sampling_period=*/std::nullopt);
  EXPECT_FALSE(filter);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, /*device_lost_rssi_threshold=*/-128,
      kDeviceFoundTimeout, kDeviceLostTimeout, {GetPattern()},
      /*rssi_sampling_period=*/std::nullopt);
  EXPECT_FALSE(filter);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      kDeviceFoundRSSIThreshold, /*device_lost_rssi_threshold=*/21,
      kDeviceFoundTimeout, kDeviceLostTimeout, {GetPattern()},
      /*rssi_sampling_period=*/std::nullopt);
  EXPECT_FALSE(filter);

  // Expect a failure if the "device lost" threshold is greater than the "device
  // found" threshold.
  filter = device::BluetoothLowEnergyScanFilter::Create(
      /*device_found_rssi_threshold=*/-80, /*device_lost_rssi_threshold=*/-60,
      kDeviceFoundTimeout, kDeviceLostTimeout, {GetPattern()},
      /*rssi_sampling_period=*/std::nullopt);
  EXPECT_FALSE(filter);
}

TEST(BluetoothLowEnergyScanFilterTest, ValidUsingRange) {
  auto filter = device::BluetoothLowEnergyScanFilter::Create(
      device::BluetoothLowEnergyScanFilter::Range::kImmediate,
      kDeviceFoundTimeout, kDeviceLostTimeout, {GetPattern()},
      /*rssi_sampling_period=*/std::nullopt);
  EXPECT_TRUE(filter);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      device::BluetoothLowEnergyScanFilter::Range::kNear, kDeviceFoundTimeout,
      kDeviceLostTimeout, {GetPattern()},
      /*rssi_sampling_period=*/std::nullopt);
  EXPECT_TRUE(filter);

  filter = device::BluetoothLowEnergyScanFilter::Create(
      device::BluetoothLowEnergyScanFilter::Range::kFar, kDeviceFoundTimeout,
      kDeviceLostTimeout, {GetPattern()},
      /*rssi_sampling_period=*/std::nullopt);
  EXPECT_TRUE(filter);
}

}  // namespace device
