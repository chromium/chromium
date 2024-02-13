// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_CAST_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_CAST_H_

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "chromecast/device/bluetooth/le/mock_le_scan_manager.h"
#include "chromecast/public/bluetooth/bluetooth_types.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_local_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_local_gatt_descriptor.h"
#include "device/bluetooth/test/bluetooth_test.h"

namespace device {

// Cast implementation of BluetoothTestBase.
class BluetoothTestCast : public BluetoothTestBase {
 public:
  BluetoothTestCast();

  BluetoothTestCast(const BluetoothTestCast&) = delete;
  BluetoothTestCast& operator=(const BluetoothTestCast&) = delete;

  ~BluetoothTestCast() override;

  // BluetoothTestBase overrides:
  bool PlatformSupportsLowEnergy() override;
  void InitWithFakeAdapter() override;
  BluetoothDevice* SimulateLowEnergyDevice(int device_ordinal) override;

 private:
  class GattClientManager;

  void UpdateAdapter(
      const std::string& address,
      const std::optional<std::string>& name,
      const std::vector<std::string>& service_uuids,
      const std::map<std::string, std::vector<uint8_t>>& service_data,
      const std::map<uint16_t, std::vector<uint8_t>>& manufacturer_data);

  const std::unique_ptr<GattClientManager> gatt_client_manager_;
  ::chromecast::bluetooth::MockLeScanManager le_scan_manager_;
};

// Defines common test fixture name. Use TEST_F(BluetoothTest, YourTestName).
using BluetoothTest = BluetoothTestCast;

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_CAST_H_
