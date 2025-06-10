// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/ble_scan_parser_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "services/data_decoder/ble_scan_parser/parser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Safety: `data` is guaranteed to be at least `size` bytes long.
  auto data_span = UNSAFE_BUFFERS(base::span(data, size));
  // First, verify that both versions of the parser to do not crash on the same
  // input.
  auto cxx_result = data_decoder::BleScanParserImpl::ParseBleScan(data_span);
  auto rs_result = ble_scan_parser::Parse(data_span);
  // And also make sure that the results match: if they do not match, it means
  // the parser implementations disagree, which is bad.
  CHECK(cxx_result == rs_result);
  return 0;
}
