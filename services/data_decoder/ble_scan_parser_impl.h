// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_BLE_SCAN_PARSER_IMPL_H_
#define SERVICES_DATA_DECODER_BLE_SCAN_PARSER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/macros.h"
#include "device/bluetooth/public/mojom/uuid.mojom.h"
#include "services/data_decoder/public/mojom/ble_scan_parser.mojom.h"

namespace data_decoder {

enum class UuidFormat {
  // The UUID is the third and fourth bytes of a UUID with this pattern:
  // 0000____-0000-1000-8000-00805F9B34FB
  kFormat16Bit,
  // The UUID is the first four bytes of a UUID with this pattern:
  // ________-0000-1000-8000-00805F9B34FB
  kFormat32Bit,
  // The UUID is a standard UUID
  kFormat128Bit,
  kFormatInvalid
};

class BleScanParserImpl : public mojom::BleScanParser {
 public:
  BleScanParserImpl();
  ~BleScanParserImpl() override;

  // mojom::BleScanParser:
  void Parse(const std::vector<uint8_t>& advertisement_data,
             ParseCallback callback) override;

  static mojom::ScanRecordPtr ParseBleScan(
      base::span<const uint8_t> advertisement_data);

  static std::string ParseUuid(base::span<const uint8_t> bytes,
                               UuidFormat format);

  static bool ParseServiceUuids(
      base::span<const uint8_t> bytes,
      UuidFormat format,
      std::vector<device::BluetoothUUID>* service_uuids);

 private:
  DISALLOW_COPY_AND_ASSIGN(BleScanParserImpl);
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_BLE_SCAN_PARSER_IMPL_H_
