// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "services/data_decoder/ble_scan_parser_impl.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  data_decoder::BleScanParserImpl::ParseBleScan(base::make_span(data, size));
  return 0;
}
