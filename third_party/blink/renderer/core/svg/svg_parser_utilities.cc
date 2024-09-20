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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/svg/svg_parser_utilities.h"

#include <limits>
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
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
static bool GenericParseNumber(const CharType*& cursor,
                               const CharType* end,
                               FloatType& number,
                               WhitespaceMode mode) {
  if (mode & kAllowLeadingWhitespace)
    SkipOptionalSVGSpaces(cursor, end);

  const CharType* ptr = cursor;
  // read the sign
  int sign = 1;
  if (ptr < end && *ptr == '+')
    ptr++;
  else if (ptr < end && *ptr == '-') {
    ptr++;
    sign = -1;
  }

  if (ptr == end || ((*ptr < '0' || *ptr > '9') && *ptr != '.'))
    // The first character of a number must be one of [0-9+-.]
    return false;

  // read the integer part, build right-to-left
  const CharType* digits_start = ptr;
  while (ptr < end && *ptr >= '0' && *ptr <= '9')
    ++ptr;  // Advance to first non-digit.

  FloatType integer = 0;
  if (ptr != digits_start) {
    const CharType* ptr_scan_int_part = ptr - 1;
    FloatType multiplier = 1;
    while (ptr_scan_int_part >= digits_start) {
      integer +=
          multiplier * static_cast<FloatType>(*(ptr_scan_int_part--) - '0');
      multiplier *= 10;
    }
    // Bail out early if this overflows.
    if (!IsValidRange(integer))
      return false;
  }

  FloatType decimal = 0;
  if (ptr < end && *ptr == '.') {  // read the decimals
    ptr++;

    // There must be a least one digit following the .
    if (ptr >= end || *ptr < '0' || *ptr > '9')
      return false;

    FloatType frac = 1;
    while (ptr < end && *ptr >= '0' && *ptr <= '9') {
      frac *= static_cast<FloatType>(0.1);
      decimal += (*(ptr++) - '0') * frac;
    }
  }

  // When we get here we should have consumed either a digit for the integer
  // part or a fractional part (with at least one digit after the '.'.)
  DCHECK_NE(digits_start, ptr);

  number = integer + decimal;
  number *= sign;

  // read the exponent part
  if (ptr + 1 < end && (*ptr == 'e' || *ptr == 'E') &&
      (ptr[1] != 'x' && ptr[1] != 'm')) {
    ptr++;

    // read the sign of the exponent
    bool exponent_is_negative = false;
    if (*ptr == '+')
      ptr++;
    else if (*ptr == '-') {
      ptr++;
      exponent_is_negative = true;
    }

    // There must be an exponent
    if (ptr >= end || *ptr < '0' || *ptr > '9')
      return false;

    FloatType exponent = 0;
    while (ptr < end && *ptr >= '0' && *ptr <= '9') {
      exponent *= static_cast<FloatType>(10);
      exponent += *ptr - '0';
      ptr++;
    }
    // TODO(fs): This is unnecessarily strict - the position of the decimal
    // point is not taken into account when limiting |exponent|.
    if (exponent_is_negative)
      exponent = -exponent;
    // Fail if the exponent is greater than the largest positive power
    // of ten (that would yield a representable float.)
    if (exponent > std::numeric_limits<FloatType>::max_exponent10)
      return false;
    // If the exponent is smaller than smallest negative power of 10 (that
    // would yield a representable float), then rely on the pow()+rounding to
    // produce a reasonable result (likely zero.)
    if (exponent)
      number *= static_cast<FloatType>(std::pow(10.0, exponent));
  }

  // Don't return Infinity() or NaN().
  if (!IsValidRange(number))
    return false;

  // A valid number has been parsed. Commit cursor.
  cursor = ptr;

  if (mode & kAllowTrailingWhitespace)
    SkipOptionalSVGSpacesOrDelimiter(cursor, end);

  return true;
}

bool ParseNumber(const LChar*& ptr,
                 const LChar* end,
                 float& number,
                 WhitespaceMode mode) {
  return GenericParseNumber(ptr, end, number, mode);
}

bool ParseNumber(const UChar*& ptr,
                 const UChar* end,
                 float& number,
                 WhitespaceMode mode) {
  return GenericParseNumber(ptr, end, number, mode);
}

bool ParseNumberOptionalNumber(const String& string, float& x, float& y) {
  if (string.empty())
    return false;

  return WTF::VisitCharacters(string, [&](auto chars) {
    const auto* ptr = chars.data();
    const auto* end = ptr + chars.size();
    if (!ParseNumber(ptr, end, x))
      return false;

    if (ptr == end)
      y = x;
    else if (!ParseNumber(ptr, end, y, kAllowLeadingAndTrailingWhitespace))
      return false;

    return ptr == end;
  });
}

}  // namespace blink
