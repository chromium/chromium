/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009, 2013 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"

#include <limits>

#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"

namespace blink {

template <typename FloatType>
static inline bool IsValidRange(const FloatType x) {
  static const FloatType kMax = std::numeric_limits<FloatType>::max();
  return x >= -kMax && x <= kMax;
}

// We use this generic parseNumber function to allow the Path parsing code to
// work at a higher precision internally, without any unnecessary runtime cost
// or code complexity.
template <typename CharType, typename FloatType>
static bool GenericParseNumber(base::span<const CharType>& result_span,
                               FloatType& number,
                               WhitespaceMode mode) {
  if (mode & kAllowLeadingWhitespace) {
    SkipOptionalSVGSpaces(result_span);
  }

  base::span<const CharType> span = result_span;
  // read the sign
  int sign = 1;
  if (!span.empty() && span[0] == '+') {
    span = span.template subspan<1u>();
  } else if (!span.empty() && span[0] == '-') {
    span = span.template subspan<1u>();
    sign = -1;
  }

  if (span.empty() || (!IsASCIIDigit(span[0]) && span[0] != '.')) {
    // The first character of a number must be one of [0-9+-.]
    return false;
  }

  // read the integer part, build right-to-left
  size_t digits_count = 0;
  while (digits_count < span.size() && IsASCIIDigit(span[digits_count])) {
    ++digits_count;  // Advance to first non-digit.
  }

  FloatType integer = 0;
  if (digits_count != 0) {
    FloatType multiplier = 1;
    size_t index = digits_count;
    while (index > 0) {
      integer += multiplier * static_cast<FloatType>(span[index - 1] - '0');
      multiplier *= 10;
      --index;
    }
    // Bail out early if this overflows.
    if (!IsValidRange(integer)) {
      return false;
    }

    span = span.subspan(digits_count);
  }

  FloatType decimal = 0;
  if (!span.empty() && span[0] == '.') {  // read the decimals
    span = span.template subspan<1u>();

    // There must be a least one digit following the .
    if (span.empty() || !IsASCIIDigit(span[0])) {
      return false;
    }

    FloatType frac = 1;
    size_t decimal_count = 0;
    while (decimal_count < span.size() && IsASCIIDigit(span[decimal_count])) {
      frac *= static_cast<FloatType>(0.1);
      decimal += (span[decimal_count] - '0') * frac;
      ++decimal_count;
    }
    span = span.subspan(decimal_count);
    digits_count += decimal_count;
  }

  // When we get here we should have consumed either a digit for the integer
  // part or a fractional part (with at least one digit after the '.'.)
  DCHECK_NE(digits_count, 0u);

  number = integer + decimal;
  number *= sign;

  // read the exponent part
  if (span.size() > 1 && (span[0] == 'e' || span[0] == 'E') &&
      (span[1] != 'x' && span[1] != 'm')) {
    span = span.template subspan<1u>();

    // read the sign of the exponent
    bool exponent_is_negative = false;
    if (span[0] == '+') {
      span = span.template subspan<1u>();
    } else if (span[0] == '-') {
      span = span.template subspan<1u>();
      exponent_is_negative = true;
    }

    // There must be an exponent
    if (span.empty() || !IsASCIIDigit(span[0])) {
      return false;
    }

    FloatType exponent = 0;
    size_t exponent_count = 0;
    while (exponent_count < span.size() && IsASCIIDigit(span[exponent_count])) {
      exponent *= static_cast<FloatType>(10);
      exponent += span[exponent_count] - '0';
      ++exponent_count;
    }

    // TODO(fs): This is unnecessarily strict - the position of the decimal
    // point is not taken into account when limiting |exponent|.
    if (exponent_is_negative) {
      exponent = -exponent;
    }
    // Fail if the exponent is greater than the largest positive power
    // of ten (that would yield a representable float.)
    if (exponent > std::numeric_limits<FloatType>::max_exponent10) {
      return false;
    }
    // If the exponent is smaller than smallest negative power of 10 (that
    // would yield a representable float), then rely on the pow()+rounding to
    // produce a reasonable result (likely zero.)
    if (exponent) {
      number *= static_cast<FloatType>(std::pow(10.0, exponent));
    }

    span = span.subspan(exponent_count);
  }

  // Don't return Infinity() or NaN().
  if (!IsValidRange(number)) {
    return false;
  }

  // A valid number has been parsed. Commit cursor.
  result_span = span;

  if (mode & kAllowTrailingWhitespace) {
    SkipOptionalSVGSpacesOrDelimiter(result_span);
  }

  return true;
}

bool ParseNumber(base::span<const LChar>& span,
                 float& number,
                 WhitespaceMode mode) {
  return GenericParseNumber(span, number, mode);
}

bool ParseNumber(base::span<const UChar>& span,
                 float& number,
                 WhitespaceMode mode) {
  return GenericParseNumber(span, number, mode);
}

bool ParseNumberOptionalNumber(const String& string, float& x, float& y) {
  if (string.empty()) {
    return false;
  }

  return VisitCharacters(string, [&](auto chars) {
    if (!ParseNumber(chars, x)) {
      return false;
    }

    if (chars.empty()) {
      y = x;
    } else if (!ParseNumber(chars, y, kAllowLeadingAndTrailingWhitespace)) {
      return false;
    }

    return chars.empty();
  });
}

template <typename CharType>
base::span<const CharType> TokenUntilSvgSpaceOrDelimiter(
    const base::span<const CharType> chars,
    size_t position,
    char delimiter) {
  size_t start = position;
  while (position < chars.size() && chars[position] != delimiter &&
         !IsHTMLSpace<CharType>(chars[position])) {
    ++position;
  }
  return chars.subspan(start, position - start);
}

base::span<const LChar> TokenUntilSvgSpaceOrDelimiter(
    const base::span<const LChar> chars,
    size_t position,
    char delimiter) {
  return TokenUntilSvgSpaceOrDelimiter<LChar>(chars, position, delimiter);
}

base::span<const UChar> TokenUntilSvgSpaceOrDelimiter(
    const base::span<const UChar> chars,
    size_t position,
    char delimiter) {
  return TokenUntilSvgSpaceOrDelimiter<UChar>(chars, position, delimiter);
}

}  // namespace blink
