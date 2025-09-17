// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/ble_scan_parser/wrapper_functions.h"

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "base/containers/to_vector.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace ble_scan_parser_bridge {

UuidListBuilderForTest::UuidListBuilderForTest() = default;
UuidListBuilderForTest::~UuidListBuilderForTest() = default;

void UuidListBuilderForTest::add_uuid(const std::array<uint8_t, 16>& uuid) {
  uuids.push_back(device::BluetoothUUID(uuid));
}

void set_advertising_flags(ScanRecord& record, int8_t flags) {
  record.advertising_flags = flags;
}

void set_tx_power(ScanRecord& record, int8_t power) {
  record.tx_power = power;
}

void set_advertisement_name(ScanRecord& record,
                            rust::Slice<const uint8_t> name) {
  record.advertisement_name = std::string(name.begin(), name.end());
}

void add_service_uuid(ScanRecord& record, const std::array<uint8_t, 16>& uuid) {
  record.service_uuids.push_back(device::BluetoothUUID(uuid));
}

void add_service_data(ScanRecord& record,
                      const std::array<uint8_t, 16>& uuid,
                      rust::Slice<const uint8_t> data) {
  record.service_data_map[device::BluetoothUUID(uuid)] = base::ToVector(data);
}

void add_manufacturer_data(ScanRecord& record,
                           uint16_t company_code,
                           rust::Slice<const uint8_t> data) {
  record.manufacturer_data_map[company_code] = base::ToVector(data);
}

}  // namespace ble_scan_parser_bridge
