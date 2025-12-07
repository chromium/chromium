// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/ble_scan_parser/ble_scan_parser.h"

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Safety: `data` is guaranteed to be at least `size` bytes long.
  auto data_span = UNSAFE_BUFFERS(base::span(data, size));
  // Check that the parser does not crash or do anything surprising.
  (void)bluez::ParseBleScan(data_span);
  return 0;
}
