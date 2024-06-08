// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/services/auction_worklet/auction_v8_inspector_util.h"

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/strings/utf_string_conversions.h"

namespace auction_worklet {

std::vector<uint8_t> GetStringBytes(const v8_inspector::StringView& s) {
  if (s.is8Bit()) {
    return std::vector<uint8_t>(s.characters8(), s.characters8() + s.length());
  } else {
    std::string converted = base::UTF16ToUTF8(std::u16string_view(
        reinterpret_cast<const char16_t*>(s.characters16()), s.length()));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(converted.data());
    return std::vector<uint8_t>(data, data + converted.size());
  }
}

}  // namespace auction_worklet
