// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/ble_scan_parser/parser.h"

#include <utility>

#include "base/containers/span.h"
#include "base/containers/span_rust.h"
#include "services/data_decoder/ble_scan_parser/cxx.rs.h"

namespace ble_scan_parser {

data_decoder::mojom::ScanRecordPtr Parse(base::span<const uint8_t> bytes) {
  data_decoder::mojom::ScanRecordPtr record =
      data_decoder::mojom::ScanRecord::New();
  bool result =
      ble_scan_parser_bridge::parse(base::SpanToRustSlice(bytes), *record);
  return result ? std::move(record) : nullptr;
}

}  // namespace ble_scan_parser
