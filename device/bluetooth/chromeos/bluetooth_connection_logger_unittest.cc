// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/chromeos/bluetooth_connection_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

const char kTestBluetoothDeviceId[] = "bluetooth_device_id";

}

class BluetoothConnectionLoggerTest : public testing::Test {
 protected:
  BluetoothConnectionLoggerTest() = default;
  base::HistogramTester histogram_tester;

  void SetUp() override { device::BluetoothConnectionLogger::Shutdown(); }
};

TEST_F(BluetoothConnectionLoggerTest, TestDeviceConnectionMetric) {
  device::BluetoothConnectionLogger::RecordDeviceConnected(
      kTestBluetoothDeviceId, BluetoothDeviceType::MOUSE);

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.DeviceConnected.AllConnections",
      BluetoothDeviceType::MOUSE, 1);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.DeviceConnected.UniqueConnectionsInSession",
      BluetoothDeviceType::MOUSE, 1);

  device::BluetoothConnectionLogger::RecordDeviceConnected(
      kTestBluetoothDeviceId, BluetoothDeviceType::MOUSE);

  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.DeviceConnected.AllConnections",
      BluetoothDeviceType::MOUSE, 2);
  histogram_tester.ExpectBucketCount(
      "Bluetooth.ChromeOS.DeviceConnected.UniqueConnectionsInSession",
      BluetoothDeviceType::MOUSE, 1);
}

}  // namespace device
