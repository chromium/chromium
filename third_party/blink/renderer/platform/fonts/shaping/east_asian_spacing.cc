// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/east_asian_spacing.h"

#include <unicode/uchar.h>

#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

namespace {

inline bool IsCjkSymbolsAndPunctuationOrEastAsianFullwidth(UChar ch) {
  return (ch >= 0x3000 && ch <= 0x303F) ||
         static_cast<UEastAsianWidth>(u_getIntPropertyValue(
             ch, UCHAR_EAST_ASIAN_WIDTH)) == UEastAsianWidth::U_EA_FULLWIDTH;
}

}  // namespace

// Compute the character class.
// See Text Spacing Character Classes:
// https://drafts.csswg.org/css-text-4/#text-spacing-classes
EastAsianSpacing::CharType EastAsianSpacing::GetCharType(UChar ch) {
  // TODO(crbug.com/1463890): This logic is only for prototyping.
  switch (ch) {
    case kLeftSingleQuotationMarkCharacter:  // U+2018
    case kLeftDoubleQuotationMarkCharacter:  // U+201C
      return CharType::kOpen;
    case kRightSingleQuotationMarkCharacter:  // U+2019
    case kRightDoubleQuotationMarkCharacter:  // U+201D
      return CharType::kClose;
    case kIdeographicSpaceCharacter:  // U+3000
    case kKatakanaMiddleDot:          // U+30FB
      return CharType::kMiddle;
  }
  const auto gc = static_cast<UCharCategory>(u_charType(ch));
  if (gc == UCharCategory::U_START_PUNCTUATION &&
      IsCjkSymbolsAndPunctuationOrEastAsianFullwidth(ch)) {
    return CharType::kOpen;
  }
  if (gc == UCharCategory::U_END_PUNCTUATION &&
      IsCjkSymbolsAndPunctuationOrEastAsianFullwidth(ch)) {
    return CharType::kClose;
  }
  return CharType::kOther;
}

inline bool EastAsianSpacing::ShouldKern(CharType type, CharType last_type) {
  return type == CharType::kOpen &&
         (last_type == CharType::kOpen || last_type == CharType::kMiddle ||
          last_type == CharType::kClose);
}

inline bool EastAsianSpacing::ShouldKernLast(CharType type,
                                             CharType last_type) {
  return last_type == CharType::kClose &&
         (type == CharType::kClose || type == CharType::kMiddle);
}

// Compute kerning and apply features.
// See Fullwidth Punctuation Collapsing:
// https://drafts.csswg.org/css-text-4/#fullwidth-collapsing
void EastAsianSpacing::ComputeKerning(const String& text,
                                      wtf_size_t start,
                                      wtf_size_t end,
                                      const SimpleFontData& font_data,
                                      FontFeatures& features) {
  // Compute for the first character.
  Vector<wtf_size_t, 32> indices;
  CharType last_type;
  if (start) {
    last_type = GetCharType(text[start - 1]);
    const CharType type = GetCharType(text[start]);
    if (ShouldKern(type, last_type)) {
      indices.push_back(start);
    }
    last_type = type;
  } else {
    last_type = GetCharType(text[start]);
  }

  // Compute for characters in the middle.
  // TODO(crbug.com/1463891): This part can be skipped if the font has `chws`.
  CharType type;
  for (wtf_size_t i = start + 1; i < end; ++i, last_type = type) {
    const UChar ch = text[i];
    type = GetCharType(ch);
    if (ShouldKernLast(type, last_type)) {
      DCHECK_GT(i, 0u);
      indices.push_back(i - 1);
    } else if (ShouldKern(type, last_type)) {
      indices.push_back(i);
    }
  }

  // Compute for the last character.
  if (end < text.length()) {
    type = GetCharType(text[end]);
    if (ShouldKernLast(type, last_type)) {
      indices.push_back(end - 1);
    }
  }

  // Append to `features`.
  if (indices.empty()) {
    return;
  }
  DCHECK(std::is_sorted(indices.begin(), indices.end(), std::less_equal<>()));
  const hb_tag_t tag = HB_TAG('h', 'a', 'l', 't');
  features.Reserve(features.size() + indices.size());
  for (const wtf_size_t i : indices) {
    features.Append({tag, 1, i, i + 1});
  }
}

}  // namespace blink
