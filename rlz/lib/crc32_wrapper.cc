// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A wrapper around ZLib's CRC functions to put them in the rlz_lib namespace
// and use our types.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "rlz/lib/assert.h"
#include "rlz/lib/crc32.h"
#include "rlz/lib/string_utils.h"
#include "third_party/zlib/zlib.h"

namespace rlz_lib {

int Crc32(const unsigned char* buf, int length) {
  return crc32(0L, buf, length);
}

bool Crc32(const char* text, int* crc) {
  if (!crc) {
    ASSERT_STRING("Crc32: crc is NULL.");
    return false;
  }

  *crc = 0;
  for (int i = 0; text[i]; i++) {
    if (!IsAscii(text[i]))
      return false;

    *crc = crc32(*crc, reinterpret_cast<const unsigned char*>(text + i), 1);
  }

  return true;
}

}  // namespace rlz_lib
