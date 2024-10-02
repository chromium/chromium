// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_type_conversion.h"

#include "base/check_op.h"

namespace media {

// Support ISO-639-2/T language.
uint16_t ConvertIso639LanguageCodeToU16(std::string_view language) {
  // Handle undefined or unsupported format.
  if (language.size() != 3) {
    return kUndefinedLanguageCode;
  }

  // 5 bits ASCII.
  uint16_t code = 0;
  for (int i = 0; i < 3; i++) {
    uint8_t c = language[i];
    c -= 0x60;
    if (c > 0x1f) {
      // Invalid character, it will write as an undefined.
      return kUndefinedLanguageCode;
    }
    code <<= 5;
    code |= c;
  }

  return code;
}

uint64_t ConvertToTimescale(base::TimeDelta time_diff, uint32_t timescale) {
  return time_diff.InSecondsF() * timescale;
}

}  // namespace media
