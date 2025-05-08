// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// UTF8 utilities, implemented to reduce dependencies.

#include "absl/strings/internal/utf8.h"

#include <cstddef>
#include <cstdint>

#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace strings_internal {

size_t EncodeUTF8Char(char *buffer, char32_t utf8_char) {
  if (utf8_char <= 0x7F) {
    *buffer = static_cast<char>(utf8_char);
    return 1;
  } else if (utf8_char <= 0x7FF) {
    buffer[1] = static_cast<char>(0x80 | (utf8_char & 0x3F));
    utf8_char >>= 6;
    buffer[0] = static_cast<char>(0xC0 | utf8_char);
    return 2;
  } else if (utf8_char <= 0xFFFF) {
    buffer[2] = static_cast<char>(0x80 | (utf8_char & 0x3F));
    utf8_char >>= 6;
    buffer[1] = static_cast<char>(0x80 | (utf8_char & 0x3F));
    utf8_char >>= 6;
    buffer[0] = static_cast<char>(0xE0 | utf8_char);
    return 3;
  } else {
    buffer[3] = static_cast<char>(0x80 | (utf8_char & 0x3F));
    utf8_char >>= 6;
    buffer[2] = static_cast<char>(0x80 | (utf8_char & 0x3F));
    utf8_char >>= 6;
    buffer[1] = static_cast<char>(0x80 | (utf8_char & 0x3F));
    utf8_char >>= 6;
    buffer[0] = static_cast<char>(0xF0 | utf8_char);
    return 4;
  }
}

size_t WideToUtf8(wchar_t wc, char *buf, ShiftState &s) {
  const auto v = static_cast<uint32_t>(wc);
  if (v < 0x80) {
    *buf = static_cast<char>(v);
    return 1;
  } else if (v < 0x800) {
    *buf++ = static_cast<char>(0xc0 | (v >> 6));
    *buf = static_cast<char>(0x80 | (v & 0x3f));
    return 2;
  } else if (v < 0xd800 || (v - 0xe000) < 0x2000) {
    *buf++ = static_cast<char>(0xe0 | (v >> 12));
    *buf++ = static_cast<char>(0x80 | ((v >> 6) & 0x3f));
    *buf = static_cast<char>(0x80 | (v & 0x3f));
    return 3;
  } else if ((v - 0x10000) < 0x100000) {
    *buf++ = static_cast<char>(0xf0 | (v >> 18));
    *buf++ = static_cast<char>(0x80 | ((v >> 12) & 0x3f));
    *buf++ = static_cast<char>(0x80 | ((v >> 6) & 0x3f));
    *buf = static_cast<char>(0x80 | (v & 0x3f));
    return 4;
  } else if (v < 0xdc00) {
    s.saw_high_surrogate = true;
    s.bits = static_cast<uint8_t>(v & 0x3);
    const uint8_t high_bits = ((v >> 6) & 0xf) + 1;
    *buf++ = static_cast<char>(0xf0 | (high_bits >> 2));
    *buf =
        static_cast<char>(0x80 | static_cast<uint8_t>((high_bits & 0x3) << 4) |
                          static_cast<uint8_t>((v >> 2) & 0xf));
    return 2;
  } else if (v < 0xe000 && s.saw_high_surrogate) {
    *buf++ = static_cast<char>(0x80 | static_cast<uint8_t>(s.bits << 4) |
                               static_cast<uint8_t>((v >> 6) & 0xf));
    *buf = static_cast<char>(0x80 | (v & 0x3f));
    s.saw_high_surrogate = false;
    s.bits = 0;
    return 2;
  } else {
    return static_cast<size_t>(-1);
  }
}

}  // namespace strings_internal
ABSL_NAMESPACE_END
}  // namespace absl
