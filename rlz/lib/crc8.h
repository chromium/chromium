// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Crc8 utility functions.

#ifndef RLZ_LIB_CRC8_H_
#define RLZ_LIB_CRC8_H_

#include <stdint.h>

#include "base/containers/span.h"

namespace rlz_lib {
// CRC-8 methods:
class Crc8 {
 public:
  static bool Generate(base::span<const uint8_t> data, uint8_t* check_sum);
  static bool Verify(base::span<const uint8_t> data,
                     uint8_t checksum,
                     bool* matches);
};
}  // namespace rlz_lib

#endif  // RLZ_LIB_CRC8_H_
