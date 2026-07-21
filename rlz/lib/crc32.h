// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A wrapper around ZLib's CRC function.

#ifndef RLZ_LIB_CRC32_H_
#define RLZ_LIB_CRC32_H_

#include <stdint.h>

#include <string_view>

#include "base/containers/span.h"

namespace rlz_lib {

uint32_t Crc32(base::span<const uint8_t> data);
bool Crc32(std::string_view text, uint32_t* crc);

}  // namespace rlz_lib

#endif  // RLZ_LIB_CRC32_H_
