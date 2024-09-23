// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/ble_scan_parser_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  data_decoder::BleScanParserImpl::ParseBleScan(
      // Safety: `data` is guaranteed to be at least `size` bytes long.
      UNSAFE_BUFFERS(base::make_span(data, size)));
  return 0;
}
