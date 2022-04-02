// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/types.h"
#include "base/location.h"
#include "media/formats/hls/source_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::hls {

namespace {

types::VariableName CreateVarName(base::StringPiece name) {
  return types::VariableName::Parse(SourceString::CreateForTesting(name))
      .value();
}

}  // namespace

TEST(HlsFormatParserTest, ParseDecimalIntegerTest) {
  auto const error_test = [](base::StringPiece input,
                             const base::Location& from =
                                 base::Location::Current()) {
    auto result =
        types::ParseDecimalInteger(SourceString::CreateForTesting(1, 1, input));
    ASSERT_TRUE(result.has_error()) << from.ToString();
    auto error = std::move(result).error();
    EXPECT_EQ(error.code(), ParseStatusCode::kFailedToParseDecimalInteger);
  };

  auto const ok_test = [](base::StringPiece input,
                          types::DecimalInteger expected,
                          const base::Location& from =
                              base::Location::Current()) {
    auto result =
        types::ParseDecimalInteger(SourceString::CreateForTesting(1, 1, input));
    ASSERT_TRUE(result.has_value()) << from.ToString();
    auto value = std::move(result).value();
    EXPECT_EQ(value, expected);
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

TEST(HlsFormatParserTest, ParseDecimalFloatingPointTest) {
  auto const error_test = [](base::StringPiece input,
                             const base::Location& from =
                                 base::Location::Current()) {
    auto result = types::ParseDecimalFloatingPoint(
        SourceString::CreateForTesting(1, 1, input));
    ASSERT_TRUE(result.has_error()) << from.ToString();
    auto error = std::move(result).error();
    EXPECT_EQ(error.code(),
              ParseStatusCode::kFailedToParseDecimalFloatingPoint);
  };

  auto const ok_test = [](base::StringPiece input,
                          types::DecimalFloatingPoint expected,
                          const base::Location& from =
                              base::Location::Current()) {
    auto result = types::ParseDecimalFloatingPoint(
        SourceString::CreateForTesting(1, 1, input));
    ASSERT_TRUE(result.has_value()) << from.ToString();
    auto value = std::move(result).value();
    EXPECT_DOUBLE_EQ(value, expected);
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

TEST(HlsFormatParserTest, ParseSignedDecimalFloatingPointTest) {
  auto const error_test = [](base::StringPiece input,
                             const base::Location& from =
                                 base::Location::Current()) {
    auto result = types::ParseSignedDecimalFloatingPoint(
        SourceString::CreateForTesting(1, 1, input));
    ASSERT_TRUE(result.has_error()) << from.ToString();
    auto error = std::move(result).error();
    EXPECT_EQ(error.code(),
              ParseStatusCode::kFailedToParseSignedDecimalFloatingPoint);
  };

  auto const ok_test = [](base::StringPiece input,
                          types::SignedDecimalFloatingPoint expected,
                          const base::Location& from =
                              base::Location::Current()) {
    auto result = types::ParseSignedDecimalFloatingPoint(
        SourceString::CreateForTesting(1, 1, input));
    ASSERT_TRUE(result.has_value()) << from.ToString();
    auto value = std::move(result).value();
    EXPECT_DOUBLE_EQ(value, expected);
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

TEST(HlsFormatParserTest, AttributeListIteratorTest) {
  using Items =
      std::initializer_list<std::pair<base::StringPiece, base::StringPiece>>;

  auto run_test = [](auto str, Items items, ParseStatusCode error,
                     const auto& from) {
    types::AttributeListIterator iter(SourceString::CreateForTesting(str));
    for (auto item : items) {
      auto result = iter.Next();
      ASSERT_TRUE(result.has_value()) << from.ToString();
      auto value = std::move(result).value();
      EXPECT_EQ(value.name.Str(), item.first);
      EXPECT_EQ(value.value.Str(), item.second);
    }

    // Afterwards, iterator should fail
    auto result = iter.Next();
    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(std::move(result).error().code(), error);
    result = iter.Next();
    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(std::move(result).error().code(), error);
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

  // Attribute names may not have leading, trailing, or interior whitespace
  error_test(R"(FOO=BAR, HELLO=WORLD)", {{"FOO", "BAR"}});
  error_test(R"(FOO=BAR,HELLO =WORLD)", {{"FOO", "BAR"}});
  error_test(R"(FOO=BAR,HE LLO=WORLD)", {{"FOO", "BAR"}});

  // Attribute names must be followed by an equals sign
  error_test(R"(FOO=BAR,HELLOWORLD,)", {{"FOO", "BAR"}});

  // Attribute values may not contain leading, interior, or trailing whitespace
  error_test(R"(FOO=BAR,HELLO= WORLD,)", {{"FOO", "BAR"}});
  error_test(R"(FOO=BAR,HELLO=WO RLD,)", {{"FOO", "BAR"}});
  error_test(R"(FOO=BAR,HELLO=WORLD ,)", {{"FOO", "BAR"}});

  // Leading commas are not allowed
  error_test(R"(,FOO=BAR,HELLO=WORLD,)", {});

  // A single trailing comma is allowed, multiple are not
  error_test(R"(FOO=BAR,HELLO=WORLD,,)", {{"FOO", "BAR"}, {"HELLO", "WORLD"}});

  // Single-quotes are not allowed unquoted
  error_test(R"(FOO='hahaha')", {});
  ok_test(R"(FOO="'hahaha'")", {{"FOO", "\"'hahaha'\""}});

  // Unmatched double-quote is not allowed
  error_test(R"(FOO=")", {});
  error_test(R"(FOO=BAR"BAZ)", {});
  error_test(R"(FOO=BAR")", {});

  // Double-quote (even escaped) inside double-quotes is not allowed
  error_test(R"(FOO=""")", {});
  error_test(R"(FOO="\"")", {});

  // Empty quoted-string is allowed
  ok_test(R"(FOO="")", {{"FOO", "\"\""}});

  // Tabs inside quotes are allowed
  ok_test("FOO=\"\t\"", {{"FOO", "\"\t\""}});

  // Linefeed or carriage return inside quotes are not allowed
  error_test("FOO=\"as\rdf\"", {});
  error_test("FOO=\"as\ndf\"", {});
}

TEST(HlsFormatParserTest, AttributeMapTest) {
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
    EXPECT_TRUE(result.has_error());
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
    EXPECT_TRUE(result.has_error());
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
    EXPECT_TRUE(result.has_error());
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
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(std::move(result).error().code(),
              ParseStatusCode::kAttributeListHasDuplicateNames);

    EXPECT_TRUE(storage[0].second.has_value());
    EXPECT_EQ(storage[0].second.value().Str(), "ear");
    EXPECT_FALSE(storage[1].second.has_value());
    EXPECT_TRUE(storage[2].second.has_value());
    EXPECT_EQ(storage[2].second.value().Str(), "eoo");

    // Calling again should result in the same error
    result = run_fill(storage, &iter);
    EXPECT_TRUE(result.has_error());
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
    auto iter = make_iter("FOO=foo,FAR=far   ,FAZ=faz,");

    auto result = run_fill(storage, &iter);
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(std::move(result).error().code(),
              ParseStatusCode::kMalformedAttributeList);

    EXPECT_FALSE(storage[0].second.has_value());
    EXPECT_FALSE(storage[1].second.has_value());
    EXPECT_TRUE(storage[2].second.has_value());
    EXPECT_EQ(storage[2].second.value().Str(), "foo");

    // Calling again should return same error
    result = run_fill(storage, &iter);
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(std::move(result).error().code(),
              ParseStatusCode::kMalformedAttributeList);

    EXPECT_FALSE(storage[0].second.has_value());
    EXPECT_FALSE(storage[1].second.has_value());
    EXPECT_TRUE(storage[2].second.has_value());
    EXPECT_EQ(storage[2].second.value().Str(), "foo");
  }
}

TEST(HlsFormatParserTest, ParseVariableNameTest) {
  auto const ok_test = [](base::StringPiece input,
                          const base::Location& from =
                              base::Location::Current()) {
    auto result =
        types::VariableName::Parse(SourceString::CreateForTesting(input));
    ASSERT_TRUE(result.has_value()) << from.ToString();
    EXPECT_EQ(std::move(result).value().GetName(), input);
  };

  auto const error_test = [](base::StringPiece input,
                             const base::Location& from =
                                 base::Location::Current()) {
    auto result =
        types::VariableName::Parse(SourceString::CreateForTesting(input));
    ASSERT_TRUE(result.has_error()) << from.ToString();
    EXPECT_EQ(std::move(result).error().code(),
              ParseStatusCode::kMalformedVariableName);
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

TEST(HlsFormatParserTest, ParseQuotedStringWithoutSubstitutionTest) {
  const auto ok_test = [](base::StringPiece in,
                          base::StringPiece expected_out) {
    auto in_str = SourceString::CreateForTesting(in);
    auto out = types::ParseQuotedStringWithoutSubstitution(in_str);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(std::move(out).value().Str(), expected_out);
  };

  const auto error_test = [](base::StringPiece in) {
    auto in_str = SourceString::CreateForTesting(in);
    auto out = types::ParseQuotedStringWithoutSubstitution(in_str);
    ASSERT_TRUE(out.has_error());
    EXPECT_EQ(std::move(out).error().code(),
              ParseStatusCode::kFailedToParseQuotedString);
  };

  // Test some basic examples
  ok_test("\"\"", "");
  ok_test("\" \"", " ");
  ok_test("\"Hello, world!\"", "Hello, world!");

  // Interior quotes are not checked by this function
  ok_test("\"Hello, \"World!\"\"", "Hello, \"World!\"");

  // Variables are not substituted by this function, and do not trigger an error
  ok_test("\"Hello, {$WORLD}\"", "Hello, {$WORLD}");

  // Single-quoted string is not allowed
  error_test("''");
  error_test("' '");
  error_test("'Hello, world!'");

  // Missing leading/trailing quote is not allowed
  error_test("\"");
  error_test("\" ");
  error_test(" \"");
  error_test("\"Hello, world!");
  error_test("Hello, world!\"");

  // Empty string is not allowed
  error_test("");
}

TEST(HlsFormatParserTest, ParseQuotedStringTest) {
  VariableDictionary dict;
  EXPECT_TRUE(dict.Insert(CreateVarName("FOO"), "bar"));
  EXPECT_TRUE(dict.Insert(CreateVarName("BAZ"), "\"foo\""));

  const auto ok_test = [&dict](base::StringPiece in,
                               base::StringPiece expected_out) {
    auto in_str = SourceString::CreateForTesting(in);
    VariableDictionary::SubstitutionBuffer sub_buffer;
    auto out = types::ParseQuotedString(in_str, dict, sub_buffer);
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(std::move(out).value(), expected_out);
  };

  const auto error_test = [&dict](base::StringPiece in,
                                  ParseStatusCode expected_error) {
    auto in_str = SourceString::CreateForTesting(in);
    VariableDictionary::SubstitutionBuffer sub_buffer;
    auto out = types::ParseQuotedString(in_str, dict, sub_buffer);
    ASSERT_TRUE(out.has_error());
    EXPECT_EQ(std::move(out).error().code(), expected_error);
  };

  // Test some basic examples
  ok_test("\"\"", "");
  ok_test("\" \"", " ");
  ok_test("\"Hello, world!\"", "Hello, world!");

  // Interior quotes are not checked by this function
  ok_test("\"Hello, \"World!\"\"", "Hello, \"World!\"");

  // Variables ARE substituted by this function
  ok_test("\"Hello, {$FOO}\"", "Hello, bar");
  ok_test("\"Hello, {$BAZ}\"", "Hello, \"foo\"");
  error_test("\"Hello, {$foo}\"", ParseStatusCode::kVariableUndefined);

  // Single-quoted string is not allowed
  error_test("''", ParseStatusCode::kFailedToParseQuotedString);
  error_test("' '", ParseStatusCode::kFailedToParseQuotedString);
  error_test("'Hello, world!'", ParseStatusCode::kFailedToParseQuotedString);

  // Missing leading/trailing quote is not allowed
  error_test("\"", ParseStatusCode::kFailedToParseQuotedString);
  error_test("\" ", ParseStatusCode::kFailedToParseQuotedString);
  error_test(" \"", ParseStatusCode::kFailedToParseQuotedString);
  error_test("\"Hello, world!", ParseStatusCode::kFailedToParseQuotedString);
  error_test("Hello, world!\"", ParseStatusCode::kFailedToParseQuotedString);

  // Empty string is not allowed
  error_test("", ParseStatusCode::kFailedToParseQuotedString);
}

}  // namespace media::hls
