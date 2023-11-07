// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/parse_number.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace net {

namespace {

// The string to number conversion functions in //base include the type in the
// name (like StringToInt64()). The following wrapper methods create a
// consistent interface to StringToXXX() that calls the appropriate //base
// version. This simplifies writing generic code with a template.

bool StringToNumber(std::string_view input, int32_t* output) {
  // This assumes ints are 32-bits (will fail compile if that ever changes).
  return base::StringToInt(input, output);
}

bool StringToNumber(std::string_view input, uint32_t* output) {
  // This assumes ints are 32-bits (will fail compile if that ever changes).
  return base::StringToUint(input, output);
}

bool StringToNumber(std::string_view input, int64_t* output) {
  return base::StringToInt64(input, output);
}

bool StringToNumber(std::string_view input, uint64_t* output) {
  return base::StringToUint64(input, output);
}

bool SetError(ParseIntError error, ParseIntError* optional_error) {
  if (optional_error)
    *optional_error = error;
  return false;
}

template <typename T>
bool ParseIntHelper(std::string_view input,
                    ParseIntFormat format,
                    T* output,
                    ParseIntError* optional_error) {
  // Check that the input matches the format before calling StringToNumber().
  // Numbers must start with either a digit or a negative sign.
  if (input.empty())
    return SetError(ParseIntError::FAILED_PARSE, optional_error);

  bool is_non_negative = (format == ParseIntFormat::NON_NEGATIVE ||
                          format == ParseIntFormat::STRICT_NON_NEGATIVE);
  bool is_strict = (format == ParseIntFormat::STRICT_NON_NEGATIVE ||
                    format == ParseIntFormat::STRICT_OPTIONALLY_NEGATIVE);

  bool starts_with_negative = input[0] == '-';
  bool starts_with_digit = base::IsAsciiDigit(input[0]);

  if (!starts_with_digit) {
    // The length() < 2 check catches "-". It's needed here to prevent reading
    // beyond the end of the array on line 70.
    if (is_non_negative || !starts_with_negative || input.length() < 2) {
      return SetError(ParseIntError::FAILED_PARSE, optional_error);
    }
    // If the first digit after the negative is a 0, then either the number is
    // -0 or it has an unnecessary leading 0. Either way, it violates the
    // requirements of being "strict", so fail if strict.
    if (is_strict && input[1] == '0') {
      return SetError(ParseIntError::FAILED_PARSE, optional_error);
    }
  } else {
    // Fail if the first character is a zero and the string has more than 1
    // digit.
    if (is_strict && input[0] == '0' && input.length() > 1) {
      return SetError(ParseIntError::FAILED_PARSE, optional_error);
    }
  }

  // Dispatch to the appropriate flavor of base::StringToXXX() by calling one of
  // the type-specific overloads.
  T result;
  if (StringToNumber(input, &result)) {
    *output = result;
    return true;
  }

  // Optimization: If the error is not going to be inspected, don't bother
  // calculating it.
  if (!optional_error)
    return false;

  // Set an error that distinguishes between parsing/underflow/overflow errors.
  //
  // Note that the output set by base::StringToXXX() on failure cannot be used
  // as it has ambiguity with parse errors.

  // Strip any leading negative sign off the number.
  std::string_view numeric_portion =
      starts_with_negative ? input.substr(1) : input;

  // Test if |numeric_portion| is a valid non-negative integer.
  if (!numeric_portion.empty() &&
      numeric_portion.find_first_not_of("0123456789") == std::string::npos) {
    // If it was, the failure must have been due to underflow/overflow.
    return SetError(starts_with_negative ? ParseIntError::FAILED_UNDERFLOW
                                         : ParseIntError::FAILED_OVERFLOW,
                    optional_error);
  }

  // Otherwise it was a mundane parsing error.
  return SetError(ParseIntError::FAILED_PARSE, optional_error);
}

}  // namespace

bool ParseInt32(std::string_view input,
                ParseIntFormat format,
                int32_t* output,
                ParseIntError* optional_error) {
  return ParseIntHelper(input, format, output, optional_error);
}

bool ParseInt64(std::string_view input,
                ParseIntFormat format,
                int64_t* output,
                ParseIntError* optional_error) {
  return ParseIntHelper(input, format, output, optional_error);
}

bool ParseUint32(std::string_view input,
                 ParseIntFormat format,
                 uint32_t* output,
                 ParseIntError* optional_error) {
  CHECK(format == ParseIntFormat::NON_NEGATIVE ||
        format == ParseIntFormat::STRICT_NON_NEGATIVE);
  return ParseIntHelper(input, format, output, optional_error);
}

bool ParseUint64(std::string_view input,
                 ParseIntFormat format,
                 uint64_t* output,
                 ParseIntError* optional_error) {
  CHECK(format == ParseIntFormat::NON_NEGATIVE ||
        format == ParseIntFormat::STRICT_NON_NEGATIVE);
  return ParseIntHelper(input, format, output, optional_error);
}

}  // namespace net
