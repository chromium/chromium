// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/crc32.h"

#include <array>

#include "base/check_op.h"

namespace zucchini {

namespace {

std::array<uint32_t, 256> MakeCrc32Table() {
  constexpr uint32_t kCrc32Poly = 0xEDB88320;

  std::array<uint32_t, 256> crc32Table;
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t r = i;
    for (int j = 0; j < 8; ++j)
      r = (r >> 1) ^ (kCrc32Poly & ~((r & 1) - 1));
    crc32Table[i] = r;
  }
  return crc32Table;
}

}  // namespace

// Minimalistic CRC-32 implementation for Zucchini usage. Adapted from LZMA SDK
// (found at third_party/lzma_sdk/C/7zCrc.c), which is public domain.
uint32_t CalculateCrc32(const uint8_t* first, const uint8_t* last) {
  DCHECK_GE(last, first);

  static const std::array<uint32_t, 256> kCrc32Table = MakeCrc32Table();

  uint32_t ret = 0xFFFFFFFF;
  for (; first != last; ++first)
    ret = kCrc32Table[(ret ^ *first) & 0xFF] ^ (ret >> 8);
  return ret ^ 0xFFFFFFFF;
}

}  // namespace zucchini
