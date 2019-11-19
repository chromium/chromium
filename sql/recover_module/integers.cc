// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recover_module/integers.h"

#include "base/logging.h"

namespace sql {
namespace recover {

std::pair<int64_t, const uint8_t*> ParseVarint(const uint8_t* buffer,
                                               const uint8_t* buffer_end) {
  DCHECK(buffer != nullptr);
  DCHECK(buffer_end != nullptr);

  DCHECK_LT(buffer, buffer_end);
  const uint8_t* const regular_buffer_end =
      (buffer_end - buffer > kMaxVarintSize - 1) ? buffer + kMaxVarintSize - 1
                                                 : buffer_end;

  uint64_t value = 0;
  uint8_t last_byte;
  while (buffer < regular_buffer_end) {
    last_byte = *buffer;
    ++buffer;
    value = (value << 7) | (last_byte & 0x7f);
    if ((last_byte & 0x80) == 0)
      break;
  }
  if (buffer < buffer_end && (last_byte & 0x80) != 0) {
    value = (value << 8) | *buffer;
    ++buffer;
  }
  return {value, buffer};
}

}  // namespace recover
}  // namespace sql
