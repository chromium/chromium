// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/parse_number.h"

#include <limits>
#include <sstream>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// Returns a decimal string that is one larger than the maximum value that type
// T can represent.
template <typename T>
std::string CreateOverflowString() {
  const T value = std::numeric_limits<T>::max();
  std::string result = base::NumberToString(value);
  EXPECT_NE('9', result.back());
  result.back()++;
  return result;
}

// Returns a decimal string that is one less than the minimum value that
// (signed) type T can represent.
template <typename T>
std::string CreateUnderflowString() {
  EXPECT_TRUE(std::numeric_limits<T>::is_signed);
  const T value = std::numeric_limits<T>::min();
  std::string result = base::NumberToString(value);
  EXPECT_EQ('-', result.front());
  EXPECT_NE('9', result.back());
  result.back()++;
  return result;
}

// These are potentially valid inputs, along with whether they're non-negative
// or "strict" (minimal representations).
const struct {
  const char* input;
  int expected_output;
  bool is_non_negative;
  bool is_strict;
} kAnnotatedTests[] = {
    {"0", 0, /*is_non_negative=*/true, /*is_strict=*/true},
    {"10", 10, /*is_non_negative=*/true, /*is_strict=*/true},
    {"1234566", 1234566, /*is_non_negative=*/true, /*is_strict=*/true},
    {"00", 0, /*is_non_negative=*/true, /*is_strict=*/false},
    {"010", 10, /*is_non_negative=*/true, /*is_strict=*/false},
    {"0010", 10, /*is_non_negative=*/true, /*is_strict=*/false},
    {"-10", -10, /*is_non_negative=*/false, /*is_strict=*/true},
    {"-1234566", -1234566, /*is_non_negative=*/false, /*is_strict=*/true},
    {"-0", 0, /*is_non_negative=*/false, /*is_strict=*/false},
    {"-00", 0, /*is_non_negative=*/false, /*is_strict=*/false},
    {"-010", -10, /*is_non_negative=*/false, /*is_strict=*/false},
    {"-0000000000000000000000000000000000001234566", -1234566,
     /*is_non_negative=*/false, /*is_strict=*/false},
};

// These are invalid inputs that can not be parsed regardless of the format
// used (they are neither valid negative or non-negative values).
const char* kInvalidParseTests[] = {
    "",       "-",      "--",    "23-",  "134-34", "- ",   "    ",  "+42",
    " 123",   "123 ",   "123\n", "0xFF", "-0xFF",  "0x11", "-0x11", "x11",
    "-x11",   "F11",    "-F11",  "AF",   "-AF",    "0AF",  "0.0",   "13.",
    "13,000", "13.000", "13/5",  "Inf",  "NaN",    "null", "dog",
};

// This wrapper calls func() and expects the result to match |expected_output|.
template <typename OutputType, typename ParseFunc, typename ExpectationType>
void ExpectParseIntSuccess(ParseFunc func,
                           std::string_view input,
                           ParseIntFormat format,
                           ExpectationType expected_output) {
  // Try parsing without specifying an error output - expecting success.
  OutputType parsed_number1;
  EXPECT_TRUE(func(input, format, &parsed_number1, nullptr))
      << "Failed to parse: " << input;
  EXPECT_EQ(static_cast<OutputType>(expected_output), parsed_number1);

  // Try parsing with an error output - expecting success.
  ParseIntError kBogusError = static_cast<ParseIntError>(19);
  ParseIntError error = kBogusError;
  OutputType parsed_number2;
  EXPECT_TRUE(func(input, format, &parsed_number2, &error))
      << "Failed to parse: " << input;
  EXPECT_EQ(static_cast<OutputType>(expected_output), parsed_number2);
  // Check that the error output was not written to.
  EXPECT_EQ(kBogusError, error);
}

// This wrapper calls func() and expects the failure to match |expected_error|.
template <typename OutputType, typename ParseFunc>
void ExpectParseIntFailure(ParseFunc func,
                           std::string_view input,
                           ParseIntFormat format,
                           ParseIntError expected_error) {
  const OutputType kBogusOutput(23614);

  // Try parsing without specifying an error output - expecting failure.
  OutputType parsed_number1 = kBogusOutput;
  EXPECT_FALSE(func(input, format, &parsed_number1, nullptr))
      << "Succeded parsing: " << input;
  EXPECT_EQ(kBogusOutput, parsed_number1)
      << "Modified output when failed parsing";

  // Try parsing with an error output - expecting failure.
  OutputType parsed_number2 = kBogusOutput;
  ParseIntError error;
  EXPECT_FALSE(func(input, format, &parsed_number2, &error))
      << "Succeded parsing: " << input;
  EXPECT_EQ(kBogusOutput, parsed_number2)
      << "Modified output when failed parsing";
  EXPECT_EQ(expected_error, error);
}

// Common tests for both ParseInt*() and ParseUint*()
//
// When testing ParseUint*() the |format| parameter is not applicable and
// should be passed as NON_NEGATIVE.
template <typename T, typename ParseFunc>
void TestParseIntUsingFormat(ParseFunc func, ParseIntFormat format) {
  bool is_format_non_negative = format == ParseIntFormat::NON_NEGATIVE ||
                                format == ParseIntFormat::STRICT_NON_NEGATIVE;
  bool is_format_strict = format == ParseIntFormat::STRICT_NON_NEGATIVE ||
                          format == ParseIntFormat::STRICT_OPTIONALLY_NEGATIVE;
  // Test annotated inputs, some of which may not be valid inputs when parsed
  // using `format`.
  for (const auto& test : kAnnotatedTests) {
    SCOPED_TRACE(test.input);
    if ((test.is_non_negative || !is_format_non_negative) &&
        (test.is_strict || !is_format_strict)) {
      ExpectParseIntSuccess<T>(func, test.input, format, test.expected_output);
    } else {
      ExpectParseIntFailure<T>(func, test.input, format,
                               ParseIntError::FAILED_PARSE);
    }
  }

  // Test invalid inputs (invalid regardless of parsing format)
  for (auto* input : kInvalidParseTests) {
    ExpectParseIntFailure<T>(func, input, format, ParseIntError::FAILED_PARSE);
  }

  // Test parsing the largest possible value for output type.
  {
    const T value = std::numeric_limits<T>::max();
    ExpectParseIntSuccess<T>(func, base::NumberToString(value), format, value);
  }

  // Test parsing a number one larger than the output type can accomodate
  // (overflow).
  ExpectParseIntFailure<T>(func, CreateOverflowString<T>(), format,
                           ParseIntError::FAILED_OVERFLOW);

  // Test parsing a number at least as large as the output allows AND contains
  // garbage at the end. This exercises an interesting internal quirk of
  // base::StringToInt*(), in that its result cannot distinguish this case
  // from overflow.
  ExpectParseIntFailure<T>(
      func, base::NumberToString(std::numeric_limits<T>::max()) + " ", format,
      ParseIntError::FAILED_PARSE);

  ExpectParseIntFailure<T>(func, CreateOverflowString<T>() + " ", format,
                           ParseIntError::FAILED_PARSE);

  // Test parsing the smallest possible value for output type. Don't do the
  // test for unsigned types since the smallest number 0 is tested elsewhere.
  if (std::numeric_limits<T>::is_signed) {
    const T value = std::numeric_limits<T>::min();
    std::string str_value = base::NumberToString(value);

    // The minimal value is necessarily negative, since this function is
    // testing only signed output types.
    if (is_format_non_negative) {
      ExpectParseIntFailure<T>(func, str_value, format,
                               ParseIntError::FAILED_PARSE);
    } else {
      ExpectParseIntSuccess<T>(func, str_value, format, value);
    }
  }

  // Test parsing a number one less than the output type can accomodate
  // (underflow).
  if (!is_format_non_negative) {
    ExpectParseIntFailure<T>(func, CreateUnderflowString<T>(), format,
                             ParseIntError::FAILED_UNDERFLOW);
  }

  // Test parsing a string that contains a valid number followed by a NUL
  // character.
  ExpectParseIntFailure<T>(func, std::string_view("123\0", 4), format,
                           ParseIntError::FAILED_PARSE);
}

// Common tests to run for each of the versions of ParseInt*().
//
// The `func` parameter should be a function pointer to the particular
// ParseInt*() function to test.
template <typename T, typename ParseFunc>
void TestParseInt(ParseFunc func) {
  // Test using each of the possible formats.
  ParseIntFormat kFormats[] = {ParseIntFormat::NON_NEGATIVE,
                               ParseIntFormat::OPTIONALLY_NEGATIVE,
                               ParseIntFormat::STRICT_NON_NEGATIVE,
                               ParseIntFormat::STRICT_OPTIONALLY_NEGATIVE};

  for (const auto& format : kFormats) {
    TestParseIntUsingFormat<T>(func, format);
  }
}

// Common tests to run for each of the versions of ParseUint*().
//
// The `func` parameter should be a function pointer to the particular
// ParseUint*() function to test.
template <typename T, typename ParseFunc>
void TestParseUint(ParseFunc func) {
  // Test using each of the possible formats.
  ParseIntFormat kFormats[] = {
      ParseIntFormat::NON_NEGATIVE,
      ParseIntFormat::STRICT_NON_NEGATIVE,
  };

  for (const auto& format : kFormats) {
    TestParseIntUsingFormat<T>(func, format);
  }
}

TEST(ParseNumberTest, ParseInt32) {
  TestParseInt<int32_t>(ParseInt32);
}

TEST(ParseNumberTest, ParseInt64) {
  TestParseInt<int64_t>(ParseInt64);
}

TEST(ParseNumberTest, ParseUint32) {
  TestParseUint<uint32_t>(ParseUint32);
}

TEST(ParseNumberTest, ParseUint64) {
  TestParseUint<uint64_t>(ParseUint64);
}

}  // namespace
}  // namespace net
