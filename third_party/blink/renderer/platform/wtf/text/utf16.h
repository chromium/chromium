// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_UTF16_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_UTF16_H_

#include <unicode/utf16.h>

#include <ranges>
#include <type_traits>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace blink {

namespace internal {

// A helper for ContainsOnlyLatin1().
// The compiler will conveniently combine this into a single 64-bit load for us,
// as long as it is reasonably obvious that it can elide the bounds checks.
ALWAYS_INLINE uint64_t Read4Chars(base::span<const UChar> chars, size_t start) {
  static_assert(std::is_unsigned_v<UChar>);
  return static_cast<uint64_t>(chars[start]) |
         (static_cast<uint64_t>(chars[start + 1]) << 16) |
         (static_cast<uint64_t>(chars[start + 2]) << 32) |
         (static_cast<uint64_t>(chars[start + 3]) << 48);
}

}  // namespace internal

// U16_GET() for base::span.
//  - If text[offset] is a leading surrogate and text[offset + 1] is a
//    trailing surrogate, a code point computed from text[offset] and
//    text[offset + 1] is returned.
//  - If text[offset] is a trailing surrogate and text[offset - 1] is a
//    leading surrogate, a code point computed from text[offset - 1] and
//    text[offset] is returned.
//  - Otherwise, text[offset] is returned;
inline UChar32 CodePointAt(base::span<const UChar> text, size_t offset) {
  UChar32 code_point;
  U16_GET(text, 0, offset, text.size(), code_point);
  return code_point;
}

// U16_NEXT() for base::span.
// The return value is same as CodePointAt()'s. `offset` argument is updated
// to point the next of the read character.
template <typename T>
UChar32 CodePointAtAndNext(base::span<const UChar> text, T& offset) {
  UChar32 code_point;
  U16_NEXT(text, offset, text.size(), code_point);
  return code_point;
}
template <typename T>
UChar32 CodePointAtAndNext(base::span<const LChar> text, T& offset) {
  return text[offset++];
}

// This is U16_PREV() for base::span.
// Returns a code point ending with text[offset - 1].  That is to say,
//  - Returns a code point computed from text[offset - 2] and text[offset - 1]
//    if offset-1 is greater than start_offset and text[offset - 2] is a
//    leading surrogate and text[offset - 1] is a trailing surrogate.
//  - Otherwise, text[offset - 1] is returned.
//
// `offset` argument is updated to point the first code unit of the read
//  character.  `offset` won't be smaller than `start_offset`.
template <typename T>
UChar32 CodePointAtAndPrevious(base::span<const UChar> text,
                               T start_offset,
                               T& offset) {
  DCHECK_LT(start_offset, offset);
  UChar32 code_point;
  U16_PREV(text, start_offset, offset, code_point);
  return code_point;
}

// True if `text` only contains Latin1 characters [0,255], or is empty.
ALWAYS_INLINE bool ContainsOnlyLatin1(base::span<const UChar> text) {
  if (text.size() >= 4) {
    constexpr uint64_t kNonLatin1Mask = UINT64_C(0xFF00FF00FF00FF00);
    for (size_t i = 0; i + 3 < text.size(); i += 4) {
      if (internal::Read4Chars(text, i) & kNonLatin1Mask) {
        return false;
      }
    }
    // NOTE: The tail will overlap already-tested characters,
    // but that is completely OK.
    return !(internal::Read4Chars(text, text.size() - 4) & kNonLatin1Mask);
  } else {
    return !std::ranges::any_of(text, [](UChar ch) { return ch & 0xFF00; });
  }
}

// True if `text` is well-formed UTF-16, i.e. it contains no unpaired
// surrogates: every leading surrogate is immediately followed by a trailing
// surrogate, and there are no lone trailing surrogates. This mirrors the
// JavaScript String.prototype.isWellFormed() notion of well-formedness.
WTF_EXPORT
bool IsWellFormed(base::span<const UChar> text);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_UTF16_H_
