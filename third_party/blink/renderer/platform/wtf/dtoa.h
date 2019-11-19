/*
 *  Copyright (C) 2003, 2008, 2012 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_DTOA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_DTOA_H_

#include "base/numerics/safe_conversions.h"
#include "base/third_party/double_conversion/double-conversion/double-conversion.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

// Size = 80 for sizeof(DtoaBuffer) + some sign bits, decimal point, 'e',
// exponent digits.
const unsigned kNumberToStringBufferLength = 96;
typedef char NumberToStringBuffer[kNumberToStringBufferLength];

WTF_EXPORT const char* NumberToString(double, NumberToStringBuffer);
WTF_EXPORT const char* NumberToFixedPrecisionString(
    double,
    unsigned significant_figures,
    NumberToStringBuffer);
WTF_EXPORT const char* NumberToFixedWidthString(double,
                                                unsigned decimal_places,
                                                NumberToStringBuffer);

WTF_EXPORT double ParseDouble(const LChar* string,
                              size_t length,
                              size_t& parsed_length);
WTF_EXPORT double ParseDouble(const UChar* string,
                              size_t length,
                              size_t& parsed_length);

namespace internal {
double ParseDoubleFromLongString(const UChar* string,
                                 size_t length,
                                 size_t& parsed_length);
const double_conversion::StringToDoubleConverter& GetDoubleConverter();
}  // namespace internal

inline double ParseDouble(const LChar* string,
                          size_t length,
                          size_t& parsed_length) {
  int int_parsed_length = 0;
  double d = internal::GetDoubleConverter().StringToDouble(
      reinterpret_cast<const char*>(string), base::saturated_cast<int>(length),
      &int_parsed_length);
  parsed_length = int_parsed_length;
  return d;
}

inline double ParseDouble(const UChar* string,
                          size_t length,
                          size_t& parsed_length) {
  const size_t kConversionBufferSize = 64;
  if (length > kConversionBufferSize)
    return internal::ParseDoubleFromLongString(string, length, parsed_length);
  LChar conversion_buffer[kConversionBufferSize];
  for (size_t i = 0; i < length; ++i)
    conversion_buffer[i] =
        IsASCII(string[i]) ? static_cast<LChar>(string[i]) : 0;
  return ParseDouble(conversion_buffer, length, parsed_length);
}

}  // namespace WTF

using WTF::NumberToFixedPrecisionString;
using WTF::NumberToFixedWidthString;
using WTF::NumberToString;
using WTF::NumberToStringBuffer;
using WTF::ParseDouble;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_DTOA_H_
