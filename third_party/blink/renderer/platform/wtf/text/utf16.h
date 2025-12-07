// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_UTF16_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_UTF16_H_

#include <unicode/utf16.h>

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace blink {

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

// True if `text` only contains Latin1 characters [0,255].
WTF_EXPORT
bool ContainsOnlyLatin1(base::span<const UChar> text);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_UTF16_H_
