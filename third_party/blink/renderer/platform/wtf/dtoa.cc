/****************************************************************
 *
 * The author of this software is David M. Gay.
 *
 * Copyright (c) 1991, 2000, 2001 by Lucent Technologies.
 * Copyright (C) 2002, 2005, 2006, 2007, 2008, 2010, 2012 Apple Inc.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHOR NOR LUCENT MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 ***************************************************************/

/* Please send bug reports to David M. Gay (dmg at acm dot org,
 * with " at " changed at "@" and " dot " changed to ".").    */

/* On a machine with IEEE extended-precision registers, it is
 * necessary to specify double-precision (53-bit) rounding precision
 * before invoking strtod or dtoa.  If the machine uses (the equivalent
 * of) Intel 80x87 arithmetic, the call
 *    _control87(PC_53, MCW_PC);
 * does this with many compilers.  Whether this or another call is
 * appropriate depends on the compiler; for this to work, it may be
 * necessary to #include "float.h" or another system-dependent header
 * file.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/wtf/dtoa.h"

#include <string.h>

#include "base/numerics/safe_conversions.h"
#include "base/third_party/double_conversion/double-conversion/double-conversion.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace WTF {

namespace {

double ParseDoubleFromLongString(const UChar* string,
                                 size_t length,
                                 size_t& parsed_length) {
  wtf_size_t conversion_length = base::checked_cast<wtf_size_t>(length);
  auto conversion_buffer = std::make_unique<LChar[]>(conversion_length);
  for (wtf_size_t i = 0; i < conversion_length; ++i) {
    conversion_buffer[i] = IsASCII(string[i]) ? string[i] : 0;
  }
  return ParseDouble(conversion_buffer.get(), length, parsed_length);
}

const double_conversion::StringToDoubleConverter& GetDoubleConverter() {
  static double_conversion::StringToDoubleConverter converter(
      double_conversion::StringToDoubleConverter::ALLOW_LEADING_SPACES |
          double_conversion::StringToDoubleConverter::ALLOW_TRAILING_JUNK,
      0.0, 0, nullptr, nullptr);
  return converter;
}

}  // namespace

const char* NumberToString(double d, NumberToStringBuffer buffer) {
  double_conversion::StringBuilder builder(buffer, kNumberToStringBufferLength);
  const double_conversion::DoubleToStringConverter& converter =
      double_conversion::DoubleToStringConverter::EcmaScriptConverter();
  converter.ToShortest(d, &builder);
  return builder.Finalize();
}

static inline const char* FormatStringTruncatingTrailingZerosIfNeeded(
    NumberToStringBuffer buffer,
    double_conversion::StringBuilder& builder) {
  int length = builder.position();

  // If there is an exponent, stripping trailing zeros would be incorrect.
  // FIXME: Zeros should be stripped before the 'e'.
  if (memchr(buffer, 'e', length))
    return builder.Finalize();

  int decimal_point_position = 0;
  for (; decimal_point_position < length; ++decimal_point_position) {
    if (buffer[decimal_point_position] == '.')
      break;
  }

  if (decimal_point_position == length)
    return builder.Finalize();

  int truncated_length = length - 1;
  for (; truncated_length > decimal_point_position; --truncated_length) {
    if (buffer[truncated_length] != '0')
      break;
  }

  // No trailing zeros found to strip.
  if (truncated_length == length - 1)
    return builder.Finalize();

  // If we removed all trailing zeros, remove the decimal point as well.
  if (truncated_length == decimal_point_position) {
    DCHECK_GT(truncated_length, 0);
    --truncated_length;
  }

  // Truncate the StringBuilder, and return the final result.
  char* result = builder.Finalize();
  result[truncated_length + 1] = '\0';
  return result;
}

const char* NumberToFixedPrecisionString(double d,
                                         unsigned significant_figures,
                                         NumberToStringBuffer buffer) {
  // Mimic String::format("%.[precision]g", ...), but use dtoas rounding
  // facilities.
  // "g": Signed value printed in f or e format, whichever is more compact for
  // the given value and precision.
  // The e format is used only when the exponent of the value is less than -4 or
  // greater than or equal to the precision argument. Trailing zeros are
  // truncated, and the decimal point appears only if one or more digits follow
  // it.
  // "precision": The precision specifies the maximum number of significant
  // digits printed.
  double_conversion::StringBuilder builder(buffer, kNumberToStringBufferLength);
  const double_conversion::DoubleToStringConverter& converter =
      double_conversion::DoubleToStringConverter::EcmaScriptConverter();
  converter.ToPrecision(d, significant_figures, &builder);
  // FIXME: Trailing zeros should never be added in the first place. The
  // current implementation does not strip when there is an exponent, eg.
  // 1.50000e+10.
  return FormatStringTruncatingTrailingZerosIfNeeded(buffer, builder);
}

const char* NumberToFixedWidthString(double d,
                                     unsigned decimal_places,
                                     NumberToStringBuffer buffer) {
  // Mimic String::format("%.[precision]f", ...), but use dtoas rounding
  // facilities.
  // "f": Signed value having the form [ - ]dddd.dddd, where dddd is one or more
  // decimal digits.  The number of digits before the decimal point depends on
  // the magnitude of the number, and the number of digits after the decimal
  // point depends on the requested precision.
  // "precision": The precision value specifies the number of digits after the
  // decimal point.  If a decimal point appears, at least one digit appears
  // before it.  The value is rounded to the appropriate number of digits.
  double_conversion::StringBuilder builder(buffer, kNumberToStringBufferLength);
  const double_conversion::DoubleToStringConverter& converter =
      double_conversion::DoubleToStringConverter::EcmaScriptConverter();
  converter.ToFixed(d, decimal_places, &builder);
  return builder.Finalize();
}

double ParseDouble(const LChar* string, size_t length, size_t& parsed_length) {
  int int_parsed_length = 0;
  double d = GetDoubleConverter().StringToDouble(
      reinterpret_cast<const char*>(string), base::saturated_cast<int>(length),
      &int_parsed_length);
  parsed_length = int_parsed_length;
  return d;
}

double ParseDouble(const UChar* string, size_t length, size_t& parsed_length) {
  const size_t kConversionBufferSize = 64;
  if (length > kConversionBufferSize) {
    return ParseDoubleFromLongString(string, length, parsed_length);
  }
  LChar conversion_buffer[kConversionBufferSize];
  for (size_t i = 0; i < length; ++i) {
    conversion_buffer[i] =
        IsASCII(string[i]) ? static_cast<LChar>(string[i]) : 0;
  }
  return ParseDouble(conversion_buffer, length, parsed_length);
}

namespace internal {

void InitializeDoubleConverter() {
  // Force initialization of static DoubleToStringConverter converter variable
  // inside EcmaScriptConverter function while we are in single thread mode.
  double_conversion::DoubleToStringConverter::EcmaScriptConverter();

  GetDoubleConverter();
}

}  // namespace internal

}  // namespace WTF
