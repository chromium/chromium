// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLE_SCAN_PARSER_SCAN_RECORD_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLE_SCAN_PARSER_SCAN_RECORD_H_

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace bluez {

// This is a parsed version of the BLE advertising packet. The data matches the
// fields defined in the BLE specification: https://bit.ly/2DUTnsk
struct ScanRecord {
  // TODO(dcheng): These type aliases are normally defined via
  // bluetooth_device.h, but that would introduce a circular dependency. Perhaps
  // these aliases should be defined in //device/bluetooth/public/cpp instead.
  using ServiceDataMap = std::unordered_map<device::BluetoothUUID,
                                            std::vector<uint8_t>,
                                            device::BluetoothUUIDHash>;
  using ManufacturerId = uint16_t;
  using ManufacturerData = std::vector<uint8_t>;
  using ManufacturerDataMap =
      std::unordered_map<ManufacturerId, ManufacturerData>;

  ScanRecord();
  ~ScanRecord();

  ScanRecord(const ScanRecord&);
  ScanRecord& operator=(const ScanRecord&);

  ScanRecord(ScanRecord&&);
  ScanRecord& operator=(ScanRecord&&);

  // Defines the discovery mode and EDR support
  int8_t advertising_flags;
  // The transmit power in dBm
  int8_t tx_power;
  // Device name
  std::string advertisement_name;
  // UUIDs for services offered.
  std::vector<device::BluetoothUUID> service_uuids;
  // Service data: 16-bit service UUID, service data
  ServiceDataMap service_data_map;
  // Manufacturer data: company code, data
  ManufacturerDataMap manufacturer_data_map;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLE_SCAN_PARSER_SCAN_RECORD_H_
