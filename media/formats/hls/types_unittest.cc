// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/types.h"

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include "base/location.h"
#include "media/base/media_serializers.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/test_util.h"
#include "media/formats/hls/variable_dictionary.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::hls {

TEST(HlsTypesTest, ParseDecimalInteger) {
  const auto error_test = [](std::string_view input,
                             const base::Location& from =
                                 base::Location::Current()) {
    auto result = types::ParseDecimalInteger(
        ResolvedSourceString::CreateForTesting(input));
    ASSERT_FALSE(result.has_value()) << from.ToString();
    auto error = std::move(result).error();
    EXPECT_EQ(error.code(), ParseStatusCode::kFailedToParseDecimalInteger)
        << from.ToString();
  };

  const auto ok_test =
      [](std::string_view input, types::DecimalInteger expected,
         const base::Location& from = base::Location::Current()) {
        auto result = types::ParseDecimalInteger(
            ResolvedSourceString::CreateForTesting(input));
        ASSERT_TRUE(result.has_value()) << from.ToString();
        auto value = std::move(result).value();
        EXPECT_EQ(value, expected) << from.ToString();
      };

  // Empty string is not allowed
  error_test("");

  // Decimal-integers may not be quoted
  error_test("'90132409'");
  error_test("\"12309234\"");

  // Decimal-integers may not be negative
  error_test("-81234");

  // Decimal-integers may not contain junk or leading/trailing spaces
  error_test("12.352334");
  error_test("  12352334");
  error_test("2352334   ");
  error_test("235.2334");
  error_test("+2352334");
  error_test("235x2334");

  // Decimal-integers may not exceed 20 characters
  error_test("000000000000000000001");

  // Test some valid inputs
  ok_test("00000000000000000001", 1);
  ok_test("0", 0);
  ok_test("1", 1);
  ok_test("2334509345", 2334509345);

  // Test max supported value
  ok_test("18446744073709551615", 18446744073709551615u);
  error_test("18446744073709551616");
}

TEST(HlsTypesTest, ParseDecimalFloatingPoint) {
  const auto error_test = [](std::string_view input,
                             const base::Location& from =
                                 base::Location::Current()) {
    auto result = types::ParseDecimalFloatingPoint(
        ResolvedSourceString::CreateForTesting(input));
    ASSERT_FALSE(result.has_value()) << from.ToString();
    auto error = std::move(result).error();
    EXPECT_EQ(error.code(), ParseStatusCode::kFailedToParseDecimalFloatingPoint)
        << from.ToString();
  };

  const auto ok_test =
      [](std::string_view input, types::DecimalFloatingPoint expected,
         const base::Location& from = base::Location::Current()) {
        auto result = types::ParseDecimalFloatingPoint(
            ResolvedSourceString::CreateForTesting(input));
        ASSERT_TRUE(result.has_value()) << from.ToString();
        auto value = std::move(result).value();
        EXPECT_DOUBLE_EQ(value, expected) << from.ToString();
      };

  // Empty string is not allowed
  error_test("");

  // Decimal-floating-point may not be quoted
  error_test("'901.32409'");
  error_test("\"123092.34\"");

  // Decimal-floating-point may not be negative */
  error_test("-812.34");

  // Decimal-floating-point may not contain junk or leading/trailing spaces
  error_test("+12352334");
  error_test("  123.45");
  error_test("123.45   ");
  error_test("235x2334");
  error_test("+2352334");

  // Test some valid inputs
  ok_test("0", 0.0);
  ok_test("00.00", 0.0);
  ok_test("42", 42.0);
  ok_test("42.0", 42.0);
  ok_test("42.", 42.0);
  ok_test("0.75", 0.75);
  ok_test(".75", 0.75);
  ok_test("12312309123.908908234", 12312309123.908908234);
  ok_test("0000000.000001", 0.000001);
}

TEST(HlsTypesTest, ParseSignedDecimalFloatingPoint) {
  const auto error_test = [](std::string_view input,
                             const base::Location& from =
                                 base::Location::Current()) {
    auto result = types::ParseSignedDecimalFloatingPoint(
        ResolvedSourceString::CreateForTesting(input));
    ASSERT_FALSE(result.has_value()) << from.ToString();
    auto error = std::move(result).error();
    EXPECT_EQ(error.code(),
              ParseStatusCode::kFailedToParseSignedDecimalFloatingPoint)
        << from.ToString();
  };

  const auto ok_test =
      [](std::string_view input, types::SignedDecimalFloatingPoint expected,
         const base::Location& from = base::Location::Current()) {
        auto result = types::ParseSignedDecimalFloatingPoint(
            ResolvedSourceString::CreateForTesting(input));
        ASSERT_TRUE(result.has_value()) << from.ToString();
        auto value = std::move(result).value();
        EXPECT_DOUBLE_EQ(value, expected) << from.ToString();
      };

  // Empty string is not allowed
  error_test("");

  // Signed-decimal-floating-point may not be quoted
  error_test("'901.32409'");
  error_test("\"123092.34\"");

  // Signed-decimal-floating-point may not contain junk or leading/trailing
  // spaces
  error_test("+12352334");
  error_test("  123.45");
  error_test("123.45   ");
  error_test("235x2334");
  error_test("+2352334");

  // Test some valid inputs
  ok_test("0", 0.0);
  ok_test("00.00", 0.0);
  ok_test("42", 42.0);
  ok_test("-42", -42.0);
  ok_test("42.0", 42.0);
  ok_test("75.", 75.0);
  ok_test("0.75", 0.75);
  ok_test("-0.75", -0.75);
  ok_test("-.75", -0.75);
  ok_test(".75", 0.75);
  ok_test("-75.", -75.0);
  ok_test("12312309123.908908234", 12312309123.908908234);
  ok_test("0000000.000001", 0.000001);
}

TEST(HlsTypesTest, AttributeListIterator) {
  using Items =
      std::initializer_list<std::pair<std::string_view, std::string_view>>;

  auto run_test = [](auto str, Items items, ParseStatusCode error,
                     const auto& from) {
    types::AttributeListIterator iter(SourceString::CreateForTesting(str));
    for (auto item : items) {
      auto result = iter.Next();
      ASSERT_TRUE(result.has_value()) << from.ToString();
      auto value = std::move(result).value();
      EXPECT_EQ(value.name.Str(), item.first) << from.ToString();
      EXPECT_EQ(value.value.Str(), item.second) << from.ToString();
    }

    // Afterwards, iterator should fail
    auto result = iter.Next();
    ASSERT_FALSE(result.has_value()) << from.ToString();
    EXPECT_EQ(std::move(result).error().code(), error) << from.ToString();
    result = iter.Next();
    ASSERT_FALSE(result.has_value()) << from.ToString();
    EXPECT_EQ(std::move(result).error().code(), error) << from.ToString();
  };

  // Checks for valid items, followed by an error
  auto error_test = [=](auto str, Items items,
                        const base::Location& from =
                            base::Location::Current()) {
    run_test(str, items, ParseStatusCode::kMalformedAttributeList, from);
  };

  // Checks for valid items, followed by EOF
  auto ok_test = [=](auto str, Items items,
                     const base::Location& from = base::Location::Current()) {
    run_test(str, items, ParseStatusCode::kReachedEOF, from);
  };

  ok_test("", {});
  ok_test(R"(HELLO=WORLD)", {{"HELLO", "WORLD"}});
  ok_test(R"(HELLO=WORLD,)", {{"HELLO", "WORLD"}});
  ok_test(R"(HELLO="WORLD")", {{"HELLO", "\"WORLD\""}});
  ok_test(R"(HE-LLO=foo,WORLD=2134)", {{"HE-LLO", "foo"}, {"WORLD", "2134"}});
  ok_test(R"(-HELLO-="",WORLD=.21-34,)",
          {{"-HELLO-", "\"\""}, {"WORLD", ".21-34"}});
  ok_test(R"(1HELLO=".zxc09,1,23%${}",2WORLD=3)",
          {{"1HELLO", "\".zxc09,1,23%${}\""}, {"2WORLD", "3"}});
  ok_test(R"(HELLO=" W O R L D ")", {{"HELLO", "\" W O R L D \""}});
  ok_test(R"(HELLO="x",WORLD="y")", {{"HELLO", "\"x\""}, {"WORLD", "\"y\""}});

  // Attribute names may not be empty
  error_test(R"(=BAR,HELLO=WORLD)", {});
  error_test(R"(  =BAR,HELLO=WORLD)", {});

  // Attribute values may not be empty
  error_test(R"(FOO=,HELLO=WORLD)", {});
  error_test(R"(FOO=BAR,HELLO=)", {{"FOO", "BAR"}});

  // Attribute names may not have lowercase letters
  error_test(R"(FOO=BAR,HeLLO=WORLD)", {{"FOO", "BAR"}});
  error_test(R"(FOO=BAR,hello=WORLD)", {{"FOO", "BAR"}});

  // Attribute names may not have other characters
  error_test(R"(FOO=BAR,HEL.LO=WORLD)", {{"FOO", "BAR"}});
  error_test(R"(FOO=BAR,HEL$LO=WORLD)", {{"FOO", "BAR"}});
  error_test(R"(FOO=BAR,HEL(LO=WORLD)", {{"FOO", "BAR"}});

  // Attribute names may have leading or trailing whitespace, but not interior
  // whitespace
  ok_test(" FOO\t =BAR,\tHELLO    =WORLD",
          {{"FOO", "BAR"}, {"HELLO", "WORLD"}});
  error_test("FOO=BAR,HE LLO=WORLD", {{"FOO", "BAR"}});

  // Attribute names must be followed by an equals sign
  error_test(R"(FOO=BAR,HELLOWORLD,)", {{"FOO", "BAR"}});

  // Attribute values may contain leading or trailing whitespace, but
  // it is not significant. Interior whitespace is not allowed in unquoted
  // attribute values.
  ok_test("FOO= BAR\t,HELLO= WORLD,", {{"FOO", "BAR"}, {"HELLO", "WORLD"}});
  ok_test("FOO=BAR,HELLO=WORLD \t,", {{"FOO", "BAR"}, {"HELLO", "WORLD"}});
  error_test("FOO=BAR,HELLO=WO RLD,", {{"FOO", "BAR"}});

  // Leading commas are not allowed
  error_test(R"(,FOO=BAR,HELLO=WORLD,)", {});

  // A single trailing comma is allowed, multiple are not
  ok_test("FOO=BAR,HELLO=WORLD, \t", {{"FOO", "BAR"}, {"HELLO", "WORLD"}});
  error_test("FOO=BAR,HELLO=WORLD, \t,", {{"FOO", "BAR"}, {"HELLO", "WORLD"}});

  // Single-quotes are allowed, though not treated as strings
  ok_test("FOO='hahaha'", {{"FOO", "'hahaha'"}});
  error_test("FOO='hah aha'", {});
  ok_test(R"(FOO="'hah aha'")", {{"FOO", "\"'hah aha'\""}});

  // Unmatched leading quote is not allowed, interior or trailing quotes are.
  error_test(R"(FOO=")", {});
  error_test(R"(FOO="BAR)", {});
  ok_test(R"(FOO= BAR"BAZ )", {{"FOO", "BAR\"BAZ"}});
  ok_test(R"(FOO=BAR")", {{"FOO", "BAR\""}});

  // Double-quote (even escaped) inside double-quotes is not allowed
  error_test(R"(FOO=""")", {});
  error_test(R"(FOO="\"")", {});

  // Empty quoted-string is allowed
  ok_test(R"(FOO="")", {{"FOO", "\"\""}});

  // Tabs inside quotes are allowed
  ok_test("FOO=\"\t\"", {{"FOO", "\"\t\""}});
}

TEST(HlsTypesTest, AttributeMap) {
  auto make_iter = [](auto str) {
    return types::AttributeListIterator(SourceString::CreateForTesting(str));
  };

  auto run_fill = [](auto& storage, auto* iter) {
    types::AttributeMap map(storage);
    return map.Fill(iter);
  };

  // Test that AttributeMap fills all entries
  {
    auto storage = types::AttributeMap::MakeStorage("BAR", "BAZ", "FOO");
    auto iter = make_iter("FOO=foo,BAR=bar,BAZ=baz");

    auto result = run_fill(storage, &iter);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(std::move(result).error().code(), ParseStatusCode::kReachedEOF);

    EXPECT_TRUE(storage[0].second.has_value());
    EXPECT_EQ(storage[0].second.value().Str(), "bar");
    EXPECT_TRUE(storage[1].second.has_value());
    EXPECT_EQ(storage[1].second.value().Str(), "baz");
    EXPECT_TRUE(storage[2].second.has_value());
    EXPECT_EQ(storage[2].second.value().Str(), "foo");
  }

  // Test that AttributeMap doesn't touch missing entries
  {
    auto storage = types::AttributeMap::MakeStorage("CAR", "CAZ", "COO", "GOO");
    auto iter = make_iter("COO=coo,CAR=car,CAZ=caz");

    auto result = run_fill(storage, &iter);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(std::move(result).error().code(), ParseStatusCode::kReachedEOF);

    EXPECT_TRUE(storage[0].second.has_value());
    EXPECT_EQ(storage[0].second.value().Str(), "car");
    EXPECT_TRUE(storage[1].second.has_value());
    EXPECT_EQ(storage[1].second.value().Str(), "caz");
    EXPECT_TRUE(storage[2].second.has_value());
    EXPECT_EQ(storage[2].second.value().Str(), "coo");
    EXPECT_FALSE(storage[3].second.has_value());
  }

  // Test that attribute map returns unexpected entries
  {
    auto storage = types::AttributeMap::MakeStorage("DAR", "DAZ", "DOO");
    auto iter = make_iter("DOO=doo,GOO=goo,DAR=dar,DAZ=daz,");

    auto result = run_fill(storage, &iter);
    EXPECT_TRUE(result.has_value());
    auto item = std::move(result).value();
    EXPECT_EQ(item.name.Str(), "GOO");
    EXPECT_EQ(item.value.Str(), "goo");

    EXPECT_FALSE(storage[0].second.has_value());
    EXPECT_FALSE(storage[1].second.has_value());
    EXPECT_TRUE(storage[2].second.has_value());
    EXPECT_EQ(storage[2].second.value().Str(), "doo");

    result = run_fill(storage, &iter);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(std::move(result).error().code(), ParseStatusCode::kReachedEOF);

    EXPECT_TRUE(storage[0].second.has_value());
    EXPECT_EQ(storage[0].second.value().Str(), "dar");
    EXPECT_TRUE(storage[1].second.has_value());
    EXPECT_EQ(storage[1].second.value().Str(), "daz");
    EXPECT_TRUE(storage[2].second.has_value());
    EXPECT_EQ(storage[2].second.value().Str(), "doo");
  }

  // Test that the attribute map handles duplicate entries
  {
    auto storage = types::AttributeMap::MakeStorage("EAR", "EAZ", "EOO");
    auto iter = make_iter("EOO=eoo,EAR=ear,EOO=eoo2,EAZ=eaz,");

    auto result = run_fill(storage, &iter);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(std::move(result).error().code(),
              ParseStatusCode::kAttributeListHasDuplicateNames);

    EXPECT_TRUE(storage[0].second.has_value());
    EXPECT_EQ(storage[0].second.value().Str(), "ear");
    EXPECT_FALSE(storage[1].second.has_value());
    EXPECT_TRUE(storage[2].second.has_value());
    EXPECT_EQ(storage[2].second.value().Str(), "eoo");

    // Calling again should result in the same error
    result = run_fill(storage, &iter);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(std::move(result).error().code(),
              ParseStatusCode::kAttributeListHasDuplicateNames);

    EXPECT_TRUE(storage[0].second.has_value());
    EXPECT_EQ(storage[0].second.value().Str(), "ear");
    EXPECT_FALSE(storage[1].second.has_value());
    EXPECT_TRUE(storage[2].second.has_value());
    EXPECT_EQ(storage[2].second.value().Str(), "eoo");
  }

  // Test that the attribute map forwards errors to the caller
  {
    auto storage = types::AttributeMap::MakeStorage("FAR", "FAZ", "FOO");
    auto iter = make_iter("FOO=foo,FAR=\"far,FAZ=faz,");

    auto result = run_fill(storage, &iter);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(std::move(result).error().code(),
              ParseStatusCode::kMalformedAttributeList);

    EXPECT_FALSE(storage[0].second.has_value());
    EXPECT_FALSE(storage[1].second.has_value());
    EXPECT_TRUE(storage[2].second.has_value());
    EXPECT_EQ(storage[2].second.value().Str(), "foo");

    // Calling again should return same error
    result = run_fill(storage, &iter);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(std::move(result).error().code(),
              ParseStatusCode::kMalformedAttributeList);

    EXPECT_FALSE(storage[0].second.has_value());
    EXPECT_FALSE(storage[1].second.has_value());
    EXPECT_TRUE(storage[2].second.has_value());
    EXPECT_EQ(storage[2].second.value().Str(), "foo");
  }
}

TEST(HlsTypesTest, ParseVariableName) {
  const auto ok_test = [](std::string_view input,
                          const base::Location& from =
                              base::Location::Current()) {
    auto result =
        types::VariableName::Parse(SourceString::CreateForTesting(input));
    ASSERT_TRUE(result.has_value()) << from.ToString();
    EXPECT_EQ(std::move(result).value().GetName(), input) << from.ToString();
  };

  const auto error_test = [](std::string_view input,
                             const base::Location& from =
                                 base::Location::Current()) {
    auto result =
        types::VariableName::Parse(SourceString::CreateForTesting(input));
    ASSERT_FALSE(result.has_value()) << from.ToString();
    EXPECT_EQ(std::move(result).error().code(),
              ParseStatusCode::kMalformedVariableName)
        << from.ToString();
  };

  // Variable names may not be empty
  error_test("");

  // Variable names may not have whitespace
  error_test(" Hello");
  error_test("He llo");
  error_test("Hello ");

  // Variable names may not have characters outside the allowed set
  error_test("He*llo");
  error_test("!Hello");
  error_test("He$o");
  error_test("He=o");

  // Test some valid variable names
  ok_test("Hello");
  ok_test("HE-LLO");
  ok_test("__H3LL0__");
  ok_test("12345");
  ok_test("-1_2-3_4-5_");
  ok_test("______-___-__---");
}

TEST(HlsTypesTest, ParseQuotedStringWithoutSubstitution) {
  const auto ok_test = [](std::string_view in, bool allow_empty,
                          std::string_view expected_out,
                          const base::Location& from =
                              base::Location::Current()) {
    auto in_str = SourceString::CreateForTesting(in);
    auto out = types::ParseQuotedStringWithoutSubstitution(in_str, allow_empty);
    ASSERT_TRUE(out.has_value()) << from.ToString();
    EXPECT_EQ(std::move(out).value().Str(), expected_out) << from.ToString();
  };

  const auto error_test = [](std::string_view in, bool allow_empty,
                             const base::Location& from =
                                 base::Location::Current()) {
    auto in_str = SourceString::CreateForTesting(in);
    auto out = types::ParseQuotedStringWithoutSubstitution(in_str, allow_empty);
    ASSERT_FALSE(out.has_value()) << from.ToString();
    EXPECT_EQ(std::move(out).error().code(),
              ParseStatusCode::kFailedToParseQuotedString)
        << from.ToString();
  };

  // Test some basic examples
  ok_test("\"a\"", false, "a");
  ok_test("\" \"", false, " ");
  ok_test("\"Hello, world!\"", false, "Hello, world!");

  // Empty output string is not allowed by default
  error_test("\"\"", false);
  ok_test("\"\"", true, "");

  // Interior quotes are not checked by this function
  ok_test("\"Hello, \"World!\"\"", false, "Hello, \"World!\"");

  // Variables are not substituted by this function, and do not trigger an error
  ok_test("\"Hello, {$WORLD}\"", false, "Hello, {$WORLD}");

  // Single-quoted string is not allowed
  error_test("''", false);
  error_test("' '", false);
  error_test("'Hello, world!'", false);

  // Missing leading/trailing quote is not allowed
  error_test("\"", false);
  error_test("\" ", false);
  error_test(" \"", false);
  error_test("\"Hello, world!", false);
  error_test("Hello, world!\"", false);

  // Empty input string is not allowed
  error_test("", false);
}

TEST(HlsTypesTest, ParseQuotedString) {
  VariableDictionary dict;
  EXPECT_TRUE(dict.Insert(CreateVarName("FOO"), "bar"));
  EXPECT_TRUE(dict.Insert(CreateVarName("BAZ"), "foo"));
  EXPECT_TRUE(dict.Insert(CreateVarName("EMPTY"), ""));

  const auto ok_test = [&dict](std::string_view in, bool allow_empty,
                               std::string_view expected_out,
                               const base::Location& from =
                                   base::Location::Current()) {
    auto in_str = SourceString::CreateForTesting(in);
    VariableDictionary::SubstitutionBuffer sub_buffer;
    auto out = types::ParseQuotedString(in_str, dict, sub_buffer, allow_empty);
    ASSERT_TRUE(out.has_value()) << from.ToString();
    EXPECT_EQ(std::move(out).value().Str(), expected_out) << from.ToString();
  };

  const auto error_test = [&dict](std::string_view in, bool allow_empty,
                                  ParseStatusCode expected_error,
                                  const base::Location& from =
                                      base::Location::Current()) {
    auto in_str = SourceString::CreateForTesting(in);
    VariableDictionary::SubstitutionBuffer sub_buffer;
    auto out = types::ParseQuotedString(in_str, dict, sub_buffer, allow_empty);
    ASSERT_FALSE(out.has_value()) << from.ToString();
    EXPECT_EQ(std::move(out).error().code(), expected_error) << from.ToString();
  };

  // Test some basic examples
  ok_test("\"a\"", false, "a");
  ok_test("\" \"", false, " ");
  ok_test("\"Hello, world!\"", false, "Hello, world!");

  // Empty output string is not allowed by default (before or after
  // variable substitution)
  error_test("\"\"", false, ParseStatusCode::kFailedToParseQuotedString);
  ok_test("\"\"", true, "");
  error_test("\"{$EMPTY}\"", false,
             ParseStatusCode::kFailedToParseQuotedString);
  ok_test("\"{$EMPTY}\"", true, "");

  // Interior quotes are not checked by this function
  ok_test("\"Hello, \"World!\"\"", false, "Hello, \"World!\"");

  // Variables are substituted by this function
  ok_test("\"Hello, {$FOO}\"", false, "Hello, bar");
  ok_test("\"Hello, {$BAZ}\"", false, "Hello, foo");
  error_test("\"Hello, {$foo}\"", false, ParseStatusCode::kVariableUndefined);

  // Single-quoted string is not allowed
  error_test("''", false, ParseStatusCode::kFailedToParseQuotedString);
  error_test("' '", false, ParseStatusCode::kFailedToParseQuotedString);
  error_test("'Hello, world!'", false,
             ParseStatusCode::kFailedToParseQuotedString);

  // Missing leading/trailing quote is not allowed
  error_test("\"", false, ParseStatusCode::kFailedToParseQuotedString);
  error_test("\" ", false, ParseStatusCode::kFailedToParseQuotedString);
  error_test(" \"", false, ParseStatusCode::kFailedToParseQuotedString);
  error_test("\"Hello, world!", false,
             ParseStatusCode::kFailedToParseQuotedString);
  error_test("Hello, world!\"", false,
             ParseStatusCode::kFailedToParseQuotedString);

  // Empty input string is not allowed
  error_test("", false, ParseStatusCode::kFailedToParseQuotedString);
}

TEST(HlsTypesTest, ParseDecimalResolution) {
  const auto error_test = [](std::string_view input,
                             const base::Location& from =
                                 base::Location::Current()) {
    auto result = types::DecimalResolution::Parse(
        ResolvedSourceString::CreateForTesting(input));
    ASSERT_FALSE(result.has_value()) << from.ToString();
    auto error = std::move(result).error();
    EXPECT_EQ(error.code(), ParseStatusCode::kFailedToParseDecimalResolution)
        << from.ToString();
  };

  const auto ok_test =
      [](std::string_view input, types::DecimalResolution expected,
         const base::Location& from = base::Location::Current()) {
        auto result = types::DecimalResolution::Parse(
            ResolvedSourceString::CreateForTesting(input));
        ASSERT_TRUE(result.has_value()) << from.ToString();
        auto value = std::move(result).value();
        EXPECT_EQ(value.width, expected.width) << from.ToString();
        EXPECT_EQ(value.height, expected.height) << from.ToString();
      };

  // Empty string is not allowed
  error_test("");

  // Decimal-resolution must have a single lower-case 'x' between two
  // DecimalIntegers
  error_test("123");
  error_test("123X456");
  error_test("123*456");
  error_test("123x");
  error_test("x456");
  error_test("123x456x");
  error_test("x123x456");
  error_test("x123x456x");
  error_test("0X123");

  // Decimal-resolutions may not be quoted
  error_test("'123x456'");
  error_test("\"123x456\"");

  // Decimal-resolutions may not be negative
  error_test("-123x456");
  error_test("123x-456");
  error_test("-123x-456");
  error_test("-0x456");

  // Decimal-integers may not contain junk or leading/trailing spaces
  error_test("12.3x456");
  error_test("  123x456");
  error_test("123 x456");
  error_test("123x456 ");
  error_test("123x 456");

  // Decimal-integers may not exceed 20 characters
  error_test("000000000000000000001x456");
  error_test("123x000000000000000000001");

  // Test some valid inputs
  ok_test("00000000000000000001x456",
          types::DecimalResolution{.width = 1, .height = 456});
  ok_test("0x0", types::DecimalResolution{.width = 0, .height = 0});
  ok_test("1x1", types::DecimalResolution{.width = 1, .height = 1});
  ok_test("123x456", types::DecimalResolution{.width = 123, .height = 456});
  ok_test("123x0", types::DecimalResolution{.width = 123, .height = 0});
  ok_test("0x123", types::DecimalResolution{.width = 0, .height = 123});

  // Test max supported value
  ok_test("18446744073709551615x18446744073709551615",
          types::DecimalResolution{.width = 18446744073709551615u,
                                   .height = 18446744073709551615u});
  error_test("18446744073709551616x18446744073709551616");
}

TEST(HlsTypesTest, DecimalResolutionSzudzik) {
  std::set<types::DecimalInteger> values;
  for (types::DecimalInteger x = 0; x < 10; x++) {
    for (types::DecimalInteger y = 0; y < 10; y++) {
      types::DecimalResolution res{x, y};
      values.insert(res.Szudzik());
    }
  }
  ASSERT_EQ(values.size(), 100u);
}

TEST(HlsTypesTest, ParseByteRangeExpression) {
  const auto error_test = [](std::string_view input,
                             const base::Location& from =
                                 base::Location::Current()) {
    auto result = types::parsing::ByteRangeExpression::Parse(
        ResolvedSourceString::CreateForTesting(input));
    ASSERT_FALSE(result.has_value());
    auto error = std::move(result).error();
    EXPECT_EQ(error.code(), ParseStatusCode::kFailedToParseByteRange)
        << from.ToString();
  };
  const auto ok_test =
      [](std::string_view input, types::parsing::ByteRangeExpression expected,
         const base::Location& from = base::Location::Current()) {
        auto result = types::parsing::ByteRangeExpression::Parse(
            ResolvedSourceString::CreateForTesting(input));
        ASSERT_TRUE(result.has_value());
        auto value = std::move(result).value();
        EXPECT_EQ(value.length, expected.length);
        EXPECT_EQ(value.offset, expected.offset);
      };

  // Empty string is not allowed
  error_test("");

  // Length must be a valid DecimalInteger
  error_test("-1");
  error_test(" 1");
  error_test("1 ");
  error_test(" 1 ");
  error_test("1.2");
  error_test("one");
  error_test("{$length}");
  error_test("@34");

  // Offset must be a valid DecimalInteger
  error_test("12@");
  error_test("12@-3");
  error_test("12@ 3");
  error_test("12@3 ");
  error_test("12@ 3 ");
  error_test("12@3.4");
  error_test("12@three");
  error_test("12@{$offset}");
  error_test("12@34@");

  // ByteRange may not be quoted
  error_test("'12@34'");
  error_test("\"12@34\"");

  // Test some valid inputs
  ok_test("0", types::parsing::ByteRangeExpression{.length = 0,
                                                   .offset = std::nullopt});
  ok_test("12", types::parsing::ByteRangeExpression{.length = 12,
                                                    .offset = std::nullopt});
  ok_test("12@0",
          types::parsing::ByteRangeExpression{.length = 12, .offset = 0});
  ok_test("12@34",
          types::parsing::ByteRangeExpression{.length = 12, .offset = 34});
  ok_test("0@34",
          types::parsing::ByteRangeExpression{.length = 0, .offset = 34});
  ok_test("0@0", types::parsing::ByteRangeExpression{.length = 0, .offset = 0});

  // Test max supported values. These are valid ByteRangeExpressions, but not
  // necessarily valid ByteRanges.
  ok_test("18446744073709551615@0",
          types::parsing::ByteRangeExpression{.length = 18446744073709551615u,
                                              .offset = 0});
  error_test("18446744073709551616@0");
  ok_test("0@18446744073709551615",
          types::parsing::ByteRangeExpression{.length = 0,
                                              .offset = 18446744073709551615u});
  error_test("0@18446744073709551616");
  ok_test("18446744073709551615@18446744073709551615",
          types::parsing::ByteRangeExpression{.length = 18446744073709551615u,
                                              .offset = 18446744073709551615u});
  error_test("18446744073709551616@18446744073709551615");
  error_test("18446744073709551615@18446744073709551616");
  error_test("18446744073709551616@18446744073709551616");
}

TEST(HlsTypesTest, ValidateByteRange) {
  // Any non-empty range where `ByteRange::GetEnd()` doesn't overflow
  // `DecimalInteger` is valid.
  constexpr auto ok_test =
      [](types::DecimalInteger length, types::DecimalInteger offset,
         const base::Location& from = base::Location::Current()) {
        const auto result = types::ByteRange::Validate(length, offset);
        EXPECT_TRUE(result.has_value()) << from.ToString();
      };
  constexpr auto error_test =
      [](types::DecimalInteger length, types::DecimalInteger offset,
         const base::Location& from = base::Location::Current()) {
        const auto result = types::ByteRange::Validate(length, offset);
        EXPECT_FALSE(result.has_value()) << from.ToString();
      };

  ok_test(1, 1);
  ok_test(1, 0);

  // Empty range is not allowed
  error_test(0, 0);
  error_test(0, 1);
  error_test(0, 18446744073709551615u);

  // Overflowing range is not allowed
  ok_test(18446744073709551615u, 0);
  error_test(18446744073709551615u, 1);
  error_test(1, 18446744073709551615u);
  error_test(18446744073709551615u, 18446744073709551615u);
  error_test(9223372036854775808u, 9223372036854775808u);
  ok_test(9223372036854775808u, 9223372036854775807u);
  ok_test(9223372036854775807u, 9223372036854775808u);
}

TEST(HlsTypesTest, ParseStableId) {
  constexpr auto ok_test = [](std::string_view x,
                              const base::Location& from =
                                  base::Location::Current()) {
    auto result =
        types::StableId::Parse(ResolvedSourceString::CreateForTesting(x));
    ASSERT_TRUE(result.has_value()) << from.ToString();
    auto value = std::move(result).value();
    EXPECT_EQ(value.Str(), x);
  };
  constexpr auto error_test = [](std::string_view x,
                                 const base::Location& from =
                                     base::Location::Current()) {
    auto result =
        types::StableId::Parse(ResolvedSourceString::CreateForTesting(x));
    ASSERT_FALSE(result.has_value()) << from.ToString();
    EXPECT_EQ(std::move(result).error().code(),
              ParseStatusCode::kFailedToParseStableId)
        << from.ToString();
  };

  // StableId may not be empty
  error_test("");

  // StableId may not contain whitespace
  error_test("hello world");
  error_test(" world");
  error_test("world ");
  error_test("hello\tworld");

  // StableId may not contain certain characters
  error_test("hello&world");
  error_test("hello*world");
  error_test("hello,world");

  ok_test("hello_world");
  ok_test("HELLO_WORLD");
  ok_test("HELLO_WORLD123");
  ok_test("123HELLO/WORLD");
  ok_test("HELLO=WORLD");
  ok_test("H3Llo.World");
  ok_test("-/HELLO+World");
}

TEST(HlsTypesTest, ParseInstreamId) {
  constexpr auto ok_test =
      [](std::string_view x, types::InstreamId::Type type, uint8_t number,
         const base::Location& from = base::Location::Current()) {
        auto result =
            types::InstreamId::Parse(ResolvedSourceString::CreateForTesting(x));
        ASSERT_TRUE(result.has_value()) << from.ToString();
        auto value = std::move(result).value();
        EXPECT_EQ(value.GetType(), type) << from.ToString();
        EXPECT_EQ(value.GetNumber(), number) << from.ToString();
      };
  constexpr auto error_test = [](std::string_view x,
                                 const base::Location& from =
                                     base::Location::Current()) {
    auto result =
        types::InstreamId::Parse(ResolvedSourceString::CreateForTesting(x));
    ASSERT_FALSE(result.has_value()) << from.ToString();
    EXPECT_EQ(std::move(result).error().code(),
              ParseStatusCode::kFailedToParseInstreamId)
        << from.ToString();
  };

  // InstreamId may not be empty
  error_test("");

  // InstreamId is case-sensitive
  error_test("cc1");
  error_test("Cc1");
  error_test("cC1");
  error_test("service1");
  error_test("Service1");

  // InstreamId may not contain spaces
  error_test(" CC1");
  error_test("CC1 ");
  error_test("CC 1");
  error_test(" SERVICE1");
  error_test("SERVICE1 ");
  error_test("SERVICE 1");

  // Min/Max allowed value depends on type
  error_test("CC0");
  error_test("CC-1");
  error_test("CC1.5");
  error_test("CC5");
  error_test("SERVICE0");
  error_test("SERVICE-1");
  error_test("SERVICE1.5");
  error_test("SERVICE64");

  // Test some valid inputs
  ok_test("CC1", types::InstreamId::Type::kCc, 1);
  ok_test("CC4", types::InstreamId::Type::kCc, 4);
  ok_test("SERVICE1", types::InstreamId::Type::kService, 1);
  ok_test("SERVICE63", types::InstreamId::Type::kService, 63);
}

TEST(HlsTypesTest, ParseAudioChannels) {
  constexpr auto ok_test =
      [](std::string_view str, types::DecimalInteger max_channels,
         const std::initializer_list<std::string>& audio_coding_identifiers,
         const base::Location& from = base::Location::Current()) {
        auto result = types::AudioChannels::Parse(
            ResolvedSourceString::CreateForTesting(str));
        ASSERT_TRUE(result.has_value()) << from.ToString();
        const auto value = std::move(result).value();
        EXPECT_EQ(value.GetMaxChannels(), max_channels) << from.ToString();
        EXPECT_TRUE(base::ranges::equal(value.GetAudioCodingIdentifiers(),
                                        audio_coding_identifiers))
            << from.ToString();
      };
  constexpr auto error_test = [](std::string_view str,
                                 const base::Location& from =
                                     base::Location::Current()) {
    auto result = types::AudioChannels::Parse(
        ResolvedSourceString::CreateForTesting(str));
    ASSERT_FALSE(result.has_value()) << from.ToString();
    EXPECT_EQ(std::move(result).error().code(),
              ParseStatusCode::kFailedToParseAudioChannels)
        << from.ToString();
  };

  // First parameter must be a valid DecimalInteger
  error_test("");
  error_test("/");
  error_test("-1");
  error_test("-1/");
  error_test("/1");
  error_test("/FOO");
  error_test("1.5");
  ok_test("1", 1, {});
  ok_test("0", 0, {});
  ok_test("2/", 2, {});
  ok_test("99/", 99, {});

  // Second parameter must be a valid list of audio coding identifiers
  error_test("2/foo");
  error_test("2/+");
  error_test("2/,");
  error_test("2/FOO,,");
  ok_test("2/FOO", 2, {"FOO"});
  ok_test("2/FOO,", 2, {"FOO"});
  ok_test("2/FOO,BAR", 2, {"FOO", "BAR"});
  ok_test("2/FOO-BAR,8AZ", 2, {"FOO-BAR", "8AZ"});
  ok_test("2/-", 2, {"-"});

  // Additional parameters are ignored
  ok_test("2//19090zz**-0/", 2, {});
  ok_test("2/FOO/19090zz**-0", 2, {"FOO"});
}

namespace {

template <size_t bits>
void HexErrorTest(std::string_view str,
                  bool extrapolate_leading_zeros = false,
                  bool has_prefix = true,
                  const base::Location& from = base::Location::Current()) {
  auto result = types::parsing::HexRepr<bits>::Parse(
      ResolvedSourceString::CreateForTesting(str), extrapolate_leading_zeros,
      has_prefix);
  ASSERT_FALSE(result.has_value());
  ASSERT_EQ(std::move(result).error().code(),
            ParseStatusCode::kFailedToParseHexadecimalString)
      << from.ToString();
}

template <size_t bits>
types::parsing::HexRepr<bits>::Container HexParseOk(
    std::string_view str,
    bool extrapolate_leading_zeros = false,
    bool has_prefix = true,
    const base::Location& from = base::Location::Current()) {
  auto result = types::parsing::HexRepr<bits>::Parse(
      ResolvedSourceString::CreateForTesting(str), extrapolate_leading_zeros,
      has_prefix);
  EXPECT_TRUE(result.has_value()) << from.ToString();
  CHECK(result.has_value());
  return std::move(result).value();
}

}  // namespace

TEST(HlsTypesTest, HexInvalidChars) {
  HexErrorTest<8>("q");
  HexErrorTest<8>("");
  HexErrorTest<8>("x");
  HexErrorTest<8>("~");
  HexErrorTest<8>("Á");
}

TEST(HlsTypesTest, HexPrefixFlag) {
  HexErrorTest<8>("ff");
  ASSERT_EQ(HexParseOk<8>("12", false, false), std::make_tuple<uint8_t>(0x12));
}

TEST(HlsTypesTest, HexCapsLowerCase) {
  ASSERT_EQ(HexParseOk<8>("0x1f"), std::make_tuple<uint8_t>(0x1f));
  ASSERT_EQ(HexParseOk<8>("0x1F"), std::make_tuple<uint8_t>(0x1f));
}

TEST(HlsTypesTest, HexExtrapolateZero) {
  HexErrorTest<8>("0xf");
  ASSERT_EQ(HexParseOk<8>("0xF", true), std::make_tuple<uint8_t>(0x0f));

  // extrapolate leading zeros and no prefix:
  ASSERT_EQ(HexParseOk<8>("F", true, false), std::make_tuple<uint8_t>(0x0f));
}

TEST(HlsTypesTest, HexTooLong) {
  HexErrorTest<8>("0x123");
}

TEST(HlsTypesTest, Hex16) {
  // different sizes (use assignment so == operator doesn't try tuple tricks)
  std::tuple<uint16_t> value16 = HexParseOk<16>("0x1234");
  ASSERT_EQ(value16, std::make_tuple<uint16_t>(0x1234));
}

TEST(HlsTypesTest, Hex32) {
  // different sizes (use assignment so == operator doesn't try tuple tricks)
  std::tuple<uint32_t> value32 = HexParseOk<32>("0x12345678");
  ASSERT_EQ(value32, std::make_tuple<uint32_t>(0x12345678));
}

TEST(HlsTypesTest, Hex64) {
  // different sizes (use assignment so == operator doesn't try tuple tricks)
  std::tuple<uint64_t> value64 = HexParseOk<64>("0x1234567812345678");
  ASSERT_EQ(value64, std::make_tuple<uint64_t>(0x1234567812345678));
}

TEST(HlsTypesTest, HexUnpack) {
  std::tuple<uint8_t, uint8_t, uint8_t> value24 = HexParseOk<24>("0x123456");
  std::tuple<uint8_t, uint8_t, uint8_t> expect24 =
      std::make_tuple(0x12, 0x34, 0x56);
  ASSERT_EQ(value24, expect24);

  std::tuple<uint16_t, uint16_t, uint16_t> value48 =
      HexParseOk<48>("0x123456", true);
  std::tuple<uint16_t, uint16_t, uint16_t> expect48 =
      std::make_tuple<uint16_t, uint16_t, uint16_t>(0x0000, 0x0012, 0x3456);
  ASSERT_EQ(value48, expect48);
}

}  // namespace media::hls
