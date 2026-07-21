// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A wrapper around ZLib's CRC functions to put them in the rlz_lib namespace
// and use our types.

#include "rlz/lib/assert.h"
#include "rlz/lib/crc32.h"
#include "rlz/lib/string_utils.h"
#include "third_party/zlib/zlib.h"

namespace rlz_lib {

uint32_t Crc32(base::span<const uint8_t> data) {
  return static_cast<uint32_t>(
      crc32(0L, data.data(), static_cast<uInt>(data.size())));
}

bool Crc32(std::string_view text, uint32_t* crc) {
  if (!crc) {
    ASSERT_STRING("Crc32: crc is NULL.");
    return false;
  }

  for (char c : text) {
    if (!IsAscii(c)) {
      return false;
    }
  }

  *crc = Crc32(base::as_byte_span(text));
  return true;
}

}  // namespace rlz_lib
