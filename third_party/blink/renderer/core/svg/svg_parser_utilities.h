/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PARSER_UTILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PARSER_UTILITIES_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"

namespace blink {

enum WhitespaceMode {
  kDisallowWhitespace = 0,
  kAllowLeadingWhitespace = 0x1,
  kAllowTrailingWhitespace = 0x2,
  kAllowLeadingAndTrailingWhitespace =
      kAllowLeadingWhitespace | kAllowTrailingWhitespace
};

bool ParseNumber(base::span<const LChar>& span,
                 float& number,
                 WhitespaceMode = kAllowLeadingAndTrailingWhitespace);
bool ParseNumber(base::span<const UChar>& span,
                 float& number,
                 WhitespaceMode = kAllowLeadingAndTrailingWhitespace);

bool ParseNumberOptionalNumber(const String& s, float& h, float& v);

template <typename CharType>
inline bool SkipOptionalSVGSpaces(const base::span<const CharType> chars,
                                  size_t& position) {
  while (position < chars.size() && IsHTMLSpace<CharType>(chars[position])) {
    ++position;
  }
  return position < chars.size();
}

template <typename CharType>
constexpr inline bool SkipOptionalSVGSpaces(base::span<const CharType>& span) {
  size_t position = 0;
  const bool result = SkipOptionalSVGSpaces(span, position);
  span = span.subspan(position);
  return result;
}

// Skips optional spaces and an optional delimiter (a comma, by default).
// This is used for parsing separators in lists of values in SVG attributes.
//
// This function starts scanning `chars` at `position`, and returns the new
// position after skipping characters.  If nothing can be skipped, `position`
// is returned.
template <typename CharType>
[[nodiscard]] size_t SkipOptionalSVGSpacesOrDelimiter(
    const base::span<const CharType> chars,
    size_t position,
    char delimiter = ',') {
  if (position < chars.size() && !IsHTMLSpace<CharType>(chars[position]) &&
      chars[position] != delimiter) {
    return position;
  }
  if (SkipOptionalSVGSpaces(chars, position)) {
    if (chars[position] == delimiter) {
      ++position;
      SkipOptionalSVGSpaces(chars, position);
    }
  }
  return position;
}

template <typename CharType>
constexpr inline bool SkipOptionalSVGSpacesOrDelimiter(
    base::span<const CharType>& span,
    char delimiter = ',') {
  const size_t position = SkipOptionalSVGSpacesOrDelimiter(span, 0, delimiter);
  span = span.subspan(position);
  return !span.empty();
}

// Scans `chars` from `position` until it finds a space or `delimiter`, and
// returns a span of the characters scanned. This is used for tokenizing lists
// of values in SVG attributes.
//
// Callers typically should do `position += span.size()` after calling this.
[[nodiscard]] base::span<const LChar> TokenUntilSvgSpaceOrDelimiter(
    const base::span<const LChar> chars,
    size_t position,
    char delimiter);
[[nodiscard]] base::span<const UChar> TokenUntilSvgSpaceOrDelimiter(
    const base::span<const UChar> chars,
    size_t position,
    char delimiter);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_PARSER_UTILITIES_H_
