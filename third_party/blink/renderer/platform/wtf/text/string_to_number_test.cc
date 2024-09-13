// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"

#include <cstring>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

TEST(StringToNumberTest, CharactersToInt) {
#define EXPECT_VALID(string, options, expectedValue)                   \
  do {                                                                 \
    bool ok;                                                           \
    int value = CharactersToInt(String(string).Span8(), options, &ok); \
    EXPECT_TRUE(ok);                                                   \
    EXPECT_EQ(value, expectedValue);                                   \
  } while (false)

#define EXPECT_INVALID(string, options)                    \
  do {                                                     \
    bool ok;                                               \
    CharactersToInt(String(string).Span8(), options, &ok); \
    EXPECT_FALSE(ok);                                      \
  } while (false)

  constexpr auto kStrict = NumberParsingOptions::Strict();
  EXPECT_VALID("1", kStrict, 1);
  EXPECT_VALID("2", kStrict, 2);
  EXPECT_VALID("9", kStrict, 9);
  EXPECT_VALID("10", kStrict, 10);
  EXPECT_VALID("0", kStrict, 0);
  EXPECT_VALID("-0", kStrict, 0);
  EXPECT_VALID("-1", kStrict, -1);
  EXPECT_VALID("-2", kStrict, -2);
  EXPECT_VALID("-9", kStrict, -9);
  EXPECT_VALID("-10", kStrict, -10);
  EXPECT_VALID("+0", kStrict, 0);
  EXPECT_VALID("+1", kStrict, 1);
  EXPECT_INVALID("+1", NumberParsingOptions());
  EXPECT_VALID("+2", kStrict, 2);
  EXPECT_VALID("+9", kStrict, 9);
  EXPECT_VALID("+10", kStrict, 10);
  EXPECT_VALID("00", kStrict, 0);
  EXPECT_VALID("+00", kStrict, 0);
  EXPECT_VALID("-00", kStrict, 0);
  EXPECT_VALID("01", kStrict, 1);
  EXPECT_VALID("-01", kStrict, -1);
  EXPECT_VALID("00000000000000000000", kStrict, 0);
  EXPECT_VALID(" 3 ", kStrict, 3);
  EXPECT_INVALID(" 3 ", NumberParsingOptions());
  EXPECT_VALID(" 3 pt", NumberParsingOptions::Loose(), 3);
  EXPECT_INVALID(" 3 pt", kStrict);
  EXPECT_VALID("3px", NumberParsingOptions().SetAcceptTrailingGarbage(), 3);
  EXPECT_INVALID("a", kStrict);
  EXPECT_INVALID("1a", kStrict);
  EXPECT_INVALID("a1", kStrict);
  EXPECT_INVALID("-a", kStrict);
  EXPECT_INVALID("", kStrict);
  EXPECT_INVALID("-", kStrict);
  EXPECT_INVALID("--1", kStrict);
  EXPECT_INVALID("++1", kStrict);
  EXPECT_INVALID("+-1", kStrict);
  EXPECT_INVALID("-+1", kStrict);
  EXPECT_INVALID("0-", kStrict);
  EXPECT_INVALID("0+", kStrict);

  EXPECT_VALID("2147483647", kStrict, 2147483647);
  EXPECT_VALID("02147483647", kStrict, 2147483647);
  EXPECT_INVALID("2147483648", kStrict);
  EXPECT_INVALID("2147483649", kStrict);
  EXPECT_INVALID("2147483650", kStrict);
  EXPECT_INVALID("2147483700", kStrict);
  EXPECT_INVALID("2147484000", kStrict);
  EXPECT_INVALID("2200000000", kStrict);
  EXPECT_INVALID("3000000000", kStrict);
  EXPECT_INVALID("10000000000", kStrict);
  EXPECT_VALID("-2147483647", kStrict, -2147483647);
  EXPECT_VALID("-2147483648", kStrict, -2147483647 - 1);
  EXPECT_INVALID("-2147483649", kStrict);
  EXPECT_INVALID("-2147483650", kStrict);
  EXPECT_INVALID("-2147483700", kStrict);
  EXPECT_INVALID("-2147484000", kStrict);
  EXPECT_INVALID("-2200000000", kStrict);
  EXPECT_INVALID("-3000000000", kStrict);
  EXPECT_INVALID("-10000000000", kStrict);

#undef EXPECT_VALID
#undef EXPECT_INVALID
}

TEST(StringToNumberTest, CharactersToUInt) {
#define EXPECT_VALID(string, options, expectedValue)                         \
  do {                                                                       \
    bool ok;                                                                 \
    unsigned value = CharactersToUInt(String(string).Span8(), options, &ok); \
    EXPECT_TRUE(ok);                                                         \
    EXPECT_EQ(value, expectedValue);                                         \
  } while (false)

#define EXPECT_INVALID(string, options)                     \
  do {                                                      \
    bool ok;                                                \
    CharactersToUInt(String(string).Span8(), options, &ok); \
    EXPECT_FALSE(ok);                                       \
  } while (false)

  constexpr auto kStrict = NumberParsingOptions::Strict();
  constexpr auto kAcceptMinusZeroForUnsigned =
      NumberParsingOptions().SetAcceptMinusZeroForUnsigned();
  EXPECT_VALID("1", kStrict, 1u);
  EXPECT_VALID("2", kStrict, 2u);
  EXPECT_VALID("9", kStrict, 9u);
  EXPECT_VALID("10", kStrict, 10u);
  EXPECT_VALID("0", kStrict, 0u);
  EXPECT_VALID("+0", kStrict, 0u);
  EXPECT_VALID("+1", kStrict, 1u);
  EXPECT_VALID("+2", kStrict, 2u);
  EXPECT_VALID("+9", kStrict, 9u);
  EXPECT_VALID("+10", kStrict, 10u);
  EXPECT_INVALID("+10", NumberParsingOptions());
  EXPECT_VALID("00", kStrict, 0u);
  EXPECT_VALID("+00", kStrict, 0u);
  EXPECT_VALID("01", kStrict, 1u);
  EXPECT_VALID("00000000000000000000", kStrict, 0u);
  EXPECT_INVALID("a", kStrict);
  EXPECT_INVALID("1a", kStrict);
  EXPECT_INVALID("a1", kStrict);
  EXPECT_INVALID("-a", kStrict);
  EXPECT_INVALID("", kStrict);
  EXPECT_INVALID("-", kStrict);
  EXPECT_INVALID("-0", kStrict);
  EXPECT_VALID("-0", kAcceptMinusZeroForUnsigned, 0u);
  EXPECT_INVALID("-1", kStrict);
  EXPECT_INVALID("-1", kAcceptMinusZeroForUnsigned);
  EXPECT_INVALID("-2", kStrict);
  EXPECT_INVALID("-9", kStrict);
  EXPECT_INVALID("-10", kStrict);
  EXPECT_INVALID("-00", kStrict);
  EXPECT_VALID("-00", kAcceptMinusZeroForUnsigned, 0u);
  EXPECT_INVALID("-01", kStrict);
  EXPECT_INVALID("--1", kStrict);
  EXPECT_INVALID("++1", kStrict);
  EXPECT_INVALID("+-1", kStrict);
  EXPECT_INVALID("-+1", kStrict);
  EXPECT_INVALID("0-", kStrict);
  EXPECT_INVALID("0+", kStrict);

  EXPECT_VALID("2147483647", kStrict, 2147483647u);
  EXPECT_VALID("02147483647", kStrict, 2147483647u);
  EXPECT_VALID("2147483648", kStrict, 2147483648u);
  EXPECT_VALID("4294967295", kStrict, 4294967295u);
  EXPECT_VALID("0004294967295", kStrict, 4294967295u);
  EXPECT_INVALID("4294967296", kStrict);
  EXPECT_INVALID("4294967300", kStrict);
  EXPECT_INVALID("4300000000", kStrict);
  EXPECT_INVALID("5000000000", kStrict);
  EXPECT_INVALID("10000000000", kStrict);
  EXPECT_INVALID("-2147483647", kStrict);
  EXPECT_INVALID("-2147483648", kStrict);
  EXPECT_INVALID("-2147483649", kStrict);
  EXPECT_INVALID("-2147483650", kStrict);
  EXPECT_INVALID("-2147483700", kStrict);
  EXPECT_INVALID("-2147484000", kStrict);
  EXPECT_INVALID("-2200000000", kStrict);
  EXPECT_INVALID("-3000000000", kStrict);
  EXPECT_INVALID("-10000000000", kStrict);

#undef EXPECT_VALID
#undef EXPECT_INVALID
}

TEST(StringToNumberTest, HexCharactersToUInt) {
#define EXPECT_VALID(string, expectedValue)                                    \
  do {                                                                         \
    bool ok;                                                                   \
    unsigned value = HexCharactersToUInt(String(string).Span8(),               \
                                         NumberParsingOptions::Strict(), &ok); \
    EXPECT_TRUE(ok);                                                           \
    EXPECT_EQ(value, expectedValue);                                           \
  } while (false)

#define EXPECT_INVALID(string)                                \
  do {                                                        \
    bool ok;                                                  \
    HexCharactersToUInt(String(string).Span8(),               \
                        NumberParsingOptions::Strict(), &ok); \
    EXPECT_FALSE(ok);                                         \
  } while (false)

  EXPECT_VALID("1", 1u);
  EXPECT_VALID("a", 0xAu);
  EXPECT_VALID("A", 0xAu);
  EXPECT_VALID("+a", 0xAu);
  EXPECT_VALID("+A", 0xAu);
  EXPECT_INVALID("-a");
  EXPECT_INVALID("-A");

  EXPECT_VALID("7fffffff", 0x7FFFFFFFu);
  EXPECT_VALID("80000000", 0x80000000u);
  EXPECT_VALID("fffffff0", 0xFFFFFFF0u);
  EXPECT_VALID("ffffffff", 0xFFFFFFFFu);
  EXPECT_VALID("00ffffffff", 0xFFFFFFFFu);
  EXPECT_INVALID("100000000");
  EXPECT_INVALID("7fffffff0");
  EXPECT_INVALID("-7fffffff");
  EXPECT_INVALID("-80000000");
  EXPECT_INVALID("-80000001");
  EXPECT_INVALID("-8000000a");
  EXPECT_INVALID("-8000000f");
  EXPECT_INVALID("-80000010");
  EXPECT_INVALID("-90000000");
  EXPECT_INVALID("-f0000000");
  EXPECT_INVALID("-fffffff0");
  EXPECT_INVALID("-ffffffff");

#undef EXPECT_VALID
#undef EXPECT_INVALID
}

NumberParsingResult ParseUInt(const String str, unsigned* value) {
  NumberParsingResult result;
  *value =
      CharactersToUInt(str.Span8(), NumberParsingOptions::Strict(), &result);
  return result;
}

TEST(StringToNumberTest, NumberParsingState) {
  unsigned value;
  EXPECT_EQ(NumberParsingResult::kOverflowMax,
            ParseUInt("10000000000", &value));
  EXPECT_EQ(NumberParsingResult::kError, ParseUInt("10000000000abc", &value));
  EXPECT_EQ(NumberParsingResult::kError, ParseUInt("-10000000000", &value));
  EXPECT_EQ(NumberParsingResult::kError, ParseUInt("-0", &value));
  EXPECT_EQ(NumberParsingResult::kSuccess, ParseUInt("10", &value));
}

void ParseDouble(const String& str, double expected_value) {
  bool ok;
  double value = CharactersToDouble(str.Span8(), &ok);
  EXPECT_TRUE(ok) << "\"" << str << "\"";
  EXPECT_EQ(expected_value, value);
}

void FailToParseDouble(const String& str) {
  bool ok;
  CharactersToDouble(str.Span8(), &ok);
  EXPECT_FALSE(ok) << "\"" << str << "\"";
}

TEST(StringToNumberTest, CharactersToDouble) {
  FailToParseDouble("");
  ParseDouble("0", 0.0);
  ParseDouble("-0", 0.0);
  ParseDouble("1.5", 1.5);
  ParseDouble("+1.5", 1.5);
  FailToParseDouble("+");
  FailToParseDouble("-");
  ParseDouble(".5", 0.5);
  ParseDouble("1.", 1);
  FailToParseDouble(".");
  ParseDouble("1e-100", 1e-100);
  ParseDouble("1e100", 1e+100);
  ParseDouble("    1.5", 1.5);
  FailToParseDouble("1.5   ");
  FailToParseDouble("1.5px");
  FailToParseDouble("NaN");
  FailToParseDouble("nan");
  FailToParseDouble("Infinity");
  FailToParseDouble("infinity");
  FailToParseDouble("Inf");
  FailToParseDouble("inf");
  ParseDouble("1e+4000", std::numeric_limits<double>::infinity());
  ParseDouble("-1e+4000", -std::numeric_limits<double>::infinity());
  ParseDouble("1e-4000", 0);
  FailToParseDouble("1e");
  FailToParseDouble("1e-");
  FailToParseDouble("1e+");
  FailToParseDouble("1e3.");
  FailToParseDouble("1e3.5");
  FailToParseDouble("1e.3");
}

size_t ParseDouble(const String& str) {
  size_t parsed;
  CharactersToDouble(str.Span8(), parsed);
  return parsed;
}

TEST(StringToNumberTest, CharactersToDoubleParsedLength) {
  EXPECT_EQ(0u, ParseDouble(""));
  EXPECT_EQ(0u, ParseDouble("  "));
  EXPECT_EQ(0u, ParseDouble("+"));
  EXPECT_EQ(0u, ParseDouble("-"));
  EXPECT_EQ(0u, ParseDouble("."));
  EXPECT_EQ(0u, ParseDouble("  "));
  EXPECT_EQ(4u, ParseDouble(" 123"));
  EXPECT_EQ(4u, ParseDouble(" 123 "));
  EXPECT_EQ(4u, ParseDouble(" 123px"));
  EXPECT_EQ(5u, ParseDouble("1.234"));
  EXPECT_EQ(5u, ParseDouble("1.234e"));
  EXPECT_EQ(7u, ParseDouble("1.234e1"));
}

void ParseFloat(const String& str, float expected_value) {
  bool ok;
  float value = CharactersToFloat(str.Span8(), &ok);
  EXPECT_TRUE(ok) << "\"" << str << "\"";
  EXPECT_EQ(expected_value, value);
}

void FailToParseFloat(const String& str) {
  bool ok;
  CharactersToFloat(str.Span8(), &ok);
  EXPECT_FALSE(ok) << "\"" << str << "\"";
}

TEST(StringToNumberTest, CharactersToFloat) {
  FailToParseFloat("");
  ParseFloat("0", 0.0f);
  ParseFloat("-0", 0.0f);
  ParseFloat("1.5", 1.5f);
  ParseFloat("+1.5", 1.5f);
  FailToParseFloat("+");
  FailToParseFloat("-");
  ParseFloat(".5", 0.5f);
  ParseFloat("1.", 1.0f);
  FailToParseFloat(".");
  ParseFloat("1e-40", 1e-40f);
  ParseFloat("1e30", 1e+30f);
  ParseFloat("    1.5", 1.5f);
  FailToParseFloat("1.5   ");
  FailToParseFloat("1.5px");
  FailToParseFloat("NaN");
  FailToParseFloat("nan");
  FailToParseFloat("Infinity");
  FailToParseFloat("infinity");
  FailToParseFloat("Inf");
  FailToParseFloat("inf");
  ParseFloat("1e+4000", std::numeric_limits<float>::infinity());
  ParseFloat("-1e+4000", -std::numeric_limits<float>::infinity());
  ParseFloat("1e+100", std::numeric_limits<float>::infinity());
  ParseFloat("-1e+100", -std::numeric_limits<float>::infinity());
  ParseFloat("1e-4000", 0);
  FailToParseFloat("1e");
  FailToParseFloat("1e-");
  FailToParseFloat("1e+");
  FailToParseFloat("1e3.");
  FailToParseFloat("1e3.5");
  FailToParseFloat("1e.3");
}

size_t ParseFloat(const String& str) {
  size_t parsed;
  CharactersToFloat(str.Span8(), parsed);
  return parsed;
}

TEST(StringToNumberTest, CharactersToFloatParsedLength) {
  EXPECT_EQ(0u, ParseFloat(""));
  EXPECT_EQ(0u, ParseFloat("  "));
  EXPECT_EQ(0u, ParseFloat("+"));
  EXPECT_EQ(0u, ParseFloat("-"));
  EXPECT_EQ(0u, ParseFloat("."));
  EXPECT_EQ(0u, ParseFloat("  "));
  EXPECT_EQ(4u, ParseFloat(" 123"));
  EXPECT_EQ(4u, ParseFloat(" 123 "));
  EXPECT_EQ(4u, ParseFloat(" 123px"));
  EXPECT_EQ(5u, ParseFloat("1.234"));
  EXPECT_EQ(5u, ParseFloat("1.234e"));
  EXPECT_EQ(7u, ParseFloat("1.234e1"));
}

}  // namespace WTF
