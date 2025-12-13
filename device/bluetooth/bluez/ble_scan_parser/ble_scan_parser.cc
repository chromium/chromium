// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/ble_scan_parser/ble_scan_parser.h"

#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/containers/span_rust.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "device/bluetooth/bluez/ble_scan_parser/cxx.rs.h"

namespace bluez {

std::optional<ScanRecord> ParseBleScan(base::span<const uint8_t> bytes) {
  ScanRecord record;
  bool result =
      ble_scan_parser_bridge::parse(base::SpanToRustSlice(bytes), record);
  if (!result) {
    return std::nullopt;
  }
  base::UmaHistogramBoolean("Bluetooth.LocalNameIsUtf8",
                            base::IsStringUTF8(record.advertisement_name));
  return std::move(record);
}

}  // namespace bluez
