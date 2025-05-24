// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_v8_inspector_util.h"

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"

namespace auction_worklet {

std::vector<uint8_t> GetStringBytes(const v8_inspector::StringView& s) {
  if (s.is8Bit()) {
    // SAFETY: in 8-bit mode, v8_inspector::StringView data is in
    // [s.characters8(), s.characters8() + s.length()).
    return UNSAFE_BUFFERS(
        std::vector<uint8_t>(s.characters8(), s.characters8() + s.length()));
  } else {
    std::string converted = base::UTF16ToUTF8(std::u16string_view(
        reinterpret_cast<const char16_t*>(s.characters16()), s.length()));
    return std::vector<uint8_t>(converted.begin(), converted.end());
  }
}

}  // namespace auction_worklet
