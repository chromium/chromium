// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "third_party/snappy/src/snappy-sinksource.h"
#include "third_party/snappy/src/snappy.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  snappy::ByteArraySource src(reinterpret_cast<const char*>(data), size);
  uint32_t len;
  // Note: src is invalid after GetUncompressedLength call.
  if (!snappy::GetUncompressedLength(&src, &len) || (len > 1E6)) {
    // We have to bail out, to avoid self-crafted decompression bombs.
    return 0;
  }

  std::string uncompressed_str;
  snappy::Uncompress(reinterpret_cast<const char*>(data), size,
                     &uncompressed_str);
  return 0;
}
