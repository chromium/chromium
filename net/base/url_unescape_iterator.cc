// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/url_unescape_iterator.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/third_party/icu/icu_utf.h"

namespace net {

namespace {

// Returns true if `s` contains any characters whose interpretation may be
// changed by UrlUnescapeIterator. ASCII characters are passed through
// unchanged, except for '+' and '%'.
bool ContainsCharactersChangedByUnescaping(std::string_view s) {
  return std::ranges::any_of(
      s, [](char c) { return c == '+' || c == '%' || (c & 0x80) != 0; });
}

}  // namespace

void UrlUnescapeIterator::IncrementReplacementChar() {
  value_ = kReplacementCharacterInUTF8[replacement_character_byte_];
  ++replacement_character_byte_;
  if (replacement_character_byte_ == std::size(kReplacementCharacterInUTF8)) {
    replacement_character_byte_ = 0;
  }
}

std::pair<char, UrlUnescapeIterator::WrappedIterator>
UrlUnescapeIterator::DecodePercent(WrappedIterator next) {
  if (next == end_) {
    return {'%', next};
  }
  const char most_sig_digit = *next;
  if (!base::IsHexDigit(most_sig_digit)) {
    return {'%', next};
  }
  const auto next2 = std::next(next);
  if (next2 == end_) {
    return {'%', next};
  }
  const char least_sig_digit = *next2;
  if (!base::IsHexDigit(least_sig_digit)) {
    return {'%', next};
  }

  const char value =
      static_cast<char>(base::HexDigitToInt(most_sig_digit) << 4 |
                        base::HexDigitToInt(least_sig_digit));
  return {value, std::next(next2)};
}

void UrlUnescapeIterator::CheckNonAscii() {
  static constexpr size_t kMaxUtf8CharacterLength = 4u;
  // It would be ideal to use base::StreamingUtf8Validator here, but
  // unfortunately it is not compiled into Cronet builds. Instead, we determine
  // the length of the UTF-8 character based on the first byte and then decode
  // it into a temporary buffer so that we can use base::ReadUnicodeCharacter()
  // to check it for validity.
  std::array<char, kMaxUtf8CharacterLength> current_codepoint = {};
  size_t current_codepoint_size = 1u;
  if ((value_ & 0xE0) == 0xC0) {
    current_codepoint_size = 2u;
  } else if ((value_ & 0xF0) == 0xE0) {
    current_codepoint_size = 3u;
  } else if ((value_ & 0xF8) == 0xF0) {
    current_codepoint_size = 4u;
  } else {
    EmitReplacementCharacter();
    return;
  }
  current_codepoint[0] = value_;
  // Since a byte in `current_codepoint` corresponds to 1 or 3 bytes in the
  // input string, we need to keep track of where each byte was found. We don't
  // keep track of the first byte, as we will never set `next_` to point to
  // that, so iterators[0] points to current_codepoint[1] and so on.
  std::array<WrappedIterator, kMaxUtf8CharacterLength> iterators = {next_};
  for (size_t i = 1u; i < current_codepoint_size; ++i) {
    const auto current = iterators[i - 1];
    if (current == end_) {
      // There may have been a bad byte already, so we still need to call
      // ReadUnicodeCharacter() to ensure we emit the correct number of
      // replacement characters.
      break;
    }
    const auto [value, next] = DecodeAt(current);
    current_codepoint[i] = value;
    iterators[i] = next;
  }
  size_t char_index = 0;
  base_icu::UChar32 code_point_out = 0;
  const bool ok = base::ReadUnicodeCharacter(current_codepoint.data(),
                                             current_codepoint_size,
                                             &char_index, &code_point_out);
  if (!ok) {
    next_ = iterators[char_index];
    EmitReplacementCharacter();
    return;
  }

  remaining_checked_output_bytes_ = current_codepoint_size - 1;
}

void UrlUnescapeIterator::EmitReplacementCharacter() {
  value_ = kReplacementCharacterInUTF8[0];
  replacement_character_byte_ = 1;
}

bool EqualsAfterUrlDecoding(std::string_view a, std::string_view b) {
  if (a == b) {
    // UrlUnescapeIterator is deterministic, so if they are the same before
    // decoding they will also be the same afterwards.
    return true;
  }

  if (!ContainsCharactersChangedByUnescaping(a) &&
      !ContainsCharactersChangedByUnescaping(b)) {
    return false;
  }

  return std::ranges::equal(MakeUrlUnescapeRange(a), MakeUrlUnescapeRange(b));
}

}  // namespace net
