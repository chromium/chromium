// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/east_asian_spacing.h"

#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"

namespace blink {

// Compute the character class.
// See Text Spacing Character Classes:
// https://drafts.csswg.org/css-text-4/#text-spacing-classes
EastAsianSpacing::CharType EastAsianSpacing::GetCharType(UChar ch) {
  // TODO(crbug.com/1463890): This logic is only for prototyping.
  switch (ch) {
    case 0xFF09:  // Fullwidth Right Parenthesis
      return CharType::Close;
  }
  return CharType::Other;
}

// Compute kerning and apply features.
// See Fullwidth Punctuation Collapsing:
// https://drafts.csswg.org/css-text-4/#fullwidth-collapsing
void EastAsianSpacing::ComputeKerning(const String& text,
                                      wtf_size_t start,
                                      wtf_size_t end,
                                      const SimpleFontData& font_data,
                                      FontFeatures& features) {
  // TODO(crbug.com/1463890): This logic is only for prototyping.
  const hb_tag_t tag = HB_TAG('h', 'a', 'l', 't');
  CharType last_type = start ? GetCharType(text[start - 1]) : CharType::Other;
  for (wtf_size_t i = start; i < end; ++i) {
    const UChar ch = text[i];
    CharType type = GetCharType(ch);
    if (i > start && last_type == CharType::Close && type == CharType::Close) {
      DCHECK_GT(i, 0u);
      features.Append({tag, 1, i - 1, i});
    }
    last_type = type;
  }
}

}  // namespace blink
