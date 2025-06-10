// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_BLE_SCAN_PARSER_WRAPPER_FUNCTIONS_H_
#define SERVICES_DATA_DECODER_BLE_SCAN_PARSER_WRAPPER_FUNCTIONS_H_

#include <stdint.h>

#include <array>
#include <vector>

#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "services/data_decoder/public/mojom/ble_scan_parser.mojom.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace ble_scan_parser_bridge {

using ScanRecord = data_decoder::mojom::ScanRecord;

void set_advertising_flags(ScanRecord& record, int8_t flags);
void set_tx_power(ScanRecord& record, int8_t power);
void set_advertisement_name(ScanRecord& record,
                            rust::Slice<const uint8_t> name);
void add_service_uuid(ScanRecord& record, const std::array<uint8_t, 16>& uuid);
void add_service_data(ScanRecord& record,
                      const std::array<uint8_t, 16>& uuid,
                      rust::Slice<const uint8_t> data);
void add_manufacturer_data(ScanRecord& record,
                           uint16_t company_code,
                           rust::Slice<const uint8_t> data);

struct UuidListBuilderForTest {
  UuidListBuilderForTest();
  ~UuidListBuilderForTest();

  void add_uuid(const std::array<uint8_t, 16>& uuid);

  std::vector<device::BluetoothUUID> uuids;
};

}  // namespace ble_scan_parser_bridge

#endif  // SERVICES_DATA_DECODER_BLE_SCAN_PARSER_WRAPPER_FUNCTIONS_H_
