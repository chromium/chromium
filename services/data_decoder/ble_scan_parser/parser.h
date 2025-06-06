// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_BLE_SCAN_PARSER_PARSER_H_
#define SERVICES_DATA_DECODER_BLE_SCAN_PARSER_PARSER_H_

#include <stdint.h>

#include "base/containers/span.h"
#include "services/data_decoder/public/mojom/ble_scan_parser.mojom.h"

namespace ble_scan_parser {

data_decoder::mojom::ScanRecordPtr Parse(base::span<const uint8_t> bytes);

}

#endif  // SERVICES_DATA_DECODER_BLE_SCAN_PARSER_PARSER_H_
