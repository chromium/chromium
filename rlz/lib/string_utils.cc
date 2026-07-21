// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// String manipulation functions used in the RLZ library.

#include "rlz/lib/string_utils.h"

#include "base/strings/string_number_conversions.h"
#include "rlz/lib/assert.h"

namespace rlz_lib {

bool IsAscii(unsigned char letter) {
  return letter < 0x80;
}

bool BytesToString(base::span<const uint8_t> data, std::string* string) {
  if (!string)
    return false;

  string->clear();
  if (data.empty() || !data.data()) {
    return false;
  }

  *string = base::HexEncode(data);
  return true;
}

}  // namespace rlz_lib
