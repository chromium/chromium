// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/crc.h"

#include <stdint.h>
#include <stddef.h>

#ifdef COURGETTE_USE_CRC_LIB
#  include "zlib.h"
#else
extern "C" {
#include "third_party/lzma_sdk/C/7zCrc.h"
}
#endif


namespace courgette {

uint32_t CalculateCrc(const uint8_t* buffer, size_t size) {
  uint32_t crc;

#ifdef COURGETTE_USE_CRC_LIB
  // Calculate Crc by calling CRC method in zlib
  crc = crc32(0, buffer, size);
#else
  // Calculate Crc by calling CRC method in LZMA SDK
  CrcGenerateTable();
  crc = CrcCalc(buffer, size);
#endif

  return ~crc;
}

}  // namespace
