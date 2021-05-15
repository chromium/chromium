// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/structured_headers.h"

#include <math.h>

#include <limits>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace structured_headers {
namespace {

// Helpers to make test cases clearer

Item Token(std::string value) {
  return Item(value, Item::kTokenType);
}

Item Integer(int64_t value) {
  return Item(value);
}

// Parameter with null value, only used in Structured Headers Draft 09
std::pair<std::string, Item> NullParam(std::string key) {
  return std::make_pair(key, Item());
}

std::pair<std::string, Item> BooleanParam(std::string key, bool value) {
  return std::make_pair(key, Item(value));
}

std::pair<std::string, Item> DoubleParam(std::string key, double value) {
  return std::make_pair(key, Item(value));
}

std::pair<std::string, Item> Param(std::string key, int64_t value) {
  return std::make_pair(key, Item(value));
}

std::pair<std::string, Item> Param(std::string key, std::string value) {
  return std::make_pair(key, Item(value));
}

std::pair<std::string, Item> ByteSequenceParam(std::string key,
                                               std::string value) {
  return std::make_pair(key, Item(value, Item::kByteSequenceType));
}

std::pair<std::string, Item> TokenParam(std::string key, std::string value) {
  return std::make_pair(key, Token(value));
}

// Test cases taken from https://github.com/httpwg/structured-header-tests can
// be found in structured_headers_generated_unittest.cc

const struct ItemTestCase {
  const char* name;
  const char* raw;
  const absl::optional<Item> expected;  // nullopt if parse error is expected.
  const char* canonical;  // nullptr if parse error is expected, or if canonical
                          // format is identical to raw.
} item_test_cases[] = {
    // Token
    {"bad token - item", "abc$@%!", absl::nullopt},
    {"leading whitespace", " foo", Token("foo"), "foo"},
    {"trailing whitespace", "foo ", Token("foo"), "foo"},
    {"leading asterisk", "*foo", Token("*foo")},
    // Number
    {"long integer", "999999999999999", Integer(999999999999999L)},
    {"long negative integer", "-999999999999999", Integer(-999999999999999L)},
    {"too long integer", "1000000000000000", absl::nullopt},
    {"negative too long integer", "-1000000000000000", absl::nullopt},
    {"integral decimal", "1.0", Item(1.0)},
    // String
    {"basic string", "\"foo\"", Item("foo")},
    {"non-ascii string", "\"f\xC3\xBC\xC3\xBC\"", absl::nullopt},
    // Additional tests
    {"valid quoting containing \\n", "\"\\\\n\"", Item("\\n")},
    {"valid quoting containing \\t", "\"\\\\t\"", Item("\\t")},
    {"valid quoting containing \\x", "\"\\\\x61\"", Item("\\x61")},
    {"c-style hex escape in string", "\"\\x61\"", absl::nullopt},
    {"valid quoting containing \\u", "\"\\\\u0061\"", Item("\\u0061")},
    {"c-style unicode escape in string", "\"\\u0061\"", absl::nullopt},
};

const ItemTestCase sh09_item_test_cases[] = {
    // Integer
    {"large integer", "9223372036854775807", Integer(9223372036854775807L)},
    {"large negative integer", "-9223372036854775807",
     Integer(-9223372036854775807L)},
    {"too large integer", "9223372036854775808", absl::nullopt},
    {"too large negative integer", "-9223372036854775808", absl::nullopt},
    // Byte Sequence
    {"basic binary", "*aGVsbG8=*", Item("hello", Item::kByteSequenceType)},
    {"empty binary", "**", Item("", Item::kByteSequenceType)},
    {"bad paddding", "*aGVsbG8*", Item("hello", Item::kByteSequenceType),
     "*aGVsbG8=*"},
    {"bad end delimiter", "*aGVsbG8=", absl::nullopt},
    {"extra whitespace", "*aGVsb G8=*", absl::nullopt},
    {"extra chars", "*aGVsbG!8=*", absl::nullopt},
    {"suffix chars", "*aGVsbG8=!*", absl::nullopt},
    {"non-zero pad bits", "*iZ==*", Item("\x89", Item::kByteSequenceType),
     "*iQ==*"},
    {"non-ASCII binary", "*/+Ah*", Item("\xFF\xE0!", Item::kByteSequenceType)},
    {"base64url binary", "*_-Ah*", absl::nullopt},
    {"token with leading asterisk", "*foo", absl::nullopt},
};

// For Structured Headers Draft 15
const struct ParameterizedItemTestCase {
  const char* name;
  const char* raw;
  const absl::optional<ParameterizedItem>
      expected;           // nullopt if parse error is expected.
  const char* canonical;  // nullptr if parse error is expected, or if canonical
                          // format is identical to raw.
} parameterized_item_test_cases[] = {
    {"single parameter item",
     "text/html;q=1.0",
     {{Token("text/html"), {DoubleParam("q", 1)}}}},
    {"missing parameter value item",
     "text/html;a;q=1.0",
     {{Token("text/html"), {BooleanParam("a", true), DoubleParam("q", 1)}}}},
    {"missing terminal parameter value item",
     "text/html;q=1.0;a",
     {{Token("text/html"), {DoubleParam("q", 1), BooleanParam("a", true)}}}},
    {"duplicate parameter keys with different value",
     "text/html;a=1;b=2;a=3.0",
     {{Token("text/html"), {DoubleParam("a", 3), Param("b", 2L)}}},
     "text/html;a=3.0;b=2"},
    {"multiple duplicate parameter keys at different position",
     "text/html;c=1;a=2;b;b=3.0;a",
     {{Token("text/html"),
       {Param("c", 1L), BooleanParam("a", true), DoubleParam("b", 3)}}},
     "text/html;c=1;a;b=3.0"},
    {"duplicate parameter keys with missing value",
     "text/html;a;a=1",
     {{Token("text/html"), {Param("a", 1L)}}},
     "text/html;a=1"},
    {"whitespace before = parameterised item", "text/html, text/plain;q =0.5",
     absl::nullopt},
    {"whitespace after = parameterised item", "text/html, text/plain;q= 0.5",
     absl::nullopt},
    {"whitespace before ; parameterised item", "text/html, text/plain ;q=0.5",
     absl::nullopt},
    {"whitespace after ; parameterised item",
     "text/plain; q=0.5",
     {{Token("text/plain"), {DoubleParam("q", 0.5)}}},
     "text/plain;q=0.5"},
    {"extra whitespace parameterised item",
     "text/plain;  q=0.5;  charset=utf-8",
     {{Token("text/plain"),
       {DoubleParam("q", 0.5), TokenParam("charset", "utf-8")}}},
     "text/plain;q=0.5;charset=utf-8"},
};

// For Structured Headers Draft 15
const struct ListTestCase {
  const char* name;
  const char* raw;
  const absl::optional<List> expected;  // nullopt if parse error is expected.
  const char* canonical;  // nullptr if parse error is expected, or if canonical
                          // format is identical to raw.
} list_test_cases[] = {
    // Lists of lists
    {"extra whitespace list of lists",
     "(1  42)",
     {{{{{Integer(1L), {}}, {Integer(42L), {}}}, {}}}},
     "(1 42)"},
    // Parameterized Lists
    {"basic parameterised list",
     "abc_123;a=1;b=2; cdef_456, ghi;q=\"9\";r=\"+w\"",
     {{{Token("abc_123"),
        {Param("a", 1), Param("b", 2), BooleanParam("cdef_456", true)}},
       {Token("ghi"), {Param("q", "9"), Param("r", "+w")}}}},
     "abc_123;a=1;b=2;cdef_456, ghi;q=\"9\";r=\"+w\""},
    // Parameterized inner lists
    {"parameterised basic list of lists",
     "(1;a=1.0 2), (42 43)",
     {{{{{Integer(1L), {DoubleParam("a", 1.0)}}, {Integer(2L), {}}}, {}},
       {{{Integer(42L), {}}, {Integer(43L), {}}}, {}}}}},
    {"parameters on inner members",
     "(1;a=1.0 2;b=c), (42;d=?0 43;e=:Zmdo:)",
     {{{{{Integer(1L), {DoubleParam("a", 1.0)}},
         {Integer(2L), {TokenParam("b", "c")}}},
        {}},
       {{{Integer(42L), {BooleanParam("d", false)}},
         {Integer(43L), {ByteSequenceParam("e", "fgh")}}},
        {}}}}},
    {"parameters on inner lists",
     "(1 2);a=1.0, (42 43);b=?0",
     {{{{{Integer(1L), {}}, {Integer(2L), {}}}, {DoubleParam("a", 1.0)}},
       {{{Integer(42L), {}}, {Integer(43L), {}}},
        {BooleanParam("b", false)}}}}},
    {"default true values for parameters on inner list members",
     "(1;a 2), (42 43;b)",
     {{{{{Integer(1L), {BooleanParam("a", true)}}, {Integer(2L), {}}}, {}},
       {{{Integer(42L), {}}, {Integer(43L), {BooleanParam("b", true)}}}, {}}}}},
    {"default true values for parameters on inner lists",
     "(1 2);a, (42 43);b",
     {{{{{Integer(1L), {}}, {Integer(2L), {}}}, {BooleanParam("a", true)}},
       {{{Integer(42L), {}}, {Integer(43L), {}}}, {BooleanParam("b", true)}}}}},
    {"extra whitespace before semicolon in parameters on inner list member",
     "(a;b ;c b)", absl::nullopt},
    {"extra whitespace between parameters on inner list member",
     "(a;b; c b)",
     {{{{{Token("a"), {BooleanParam("b", true), BooleanParam("c", true)}},
         {Token("b"), {}}},
        {}}}},
     "(a;b;c b)"},
    {"extra whitespace before semicolon in parameters on inner list",
     "(a b);c ;d, (e)", absl::nullopt},
    {"extra whitespace between parameters on inner list",
     "(a b);c; d, (e)",
     {{{{{Token("a"), {}}, {Token("b"), {}}},
        {BooleanParam("c", true), BooleanParam("d", true)}},
       {{{Token("e"), {}}}, {}}}},
     "(a b);c;d, (e)"},
};

// For Structured Headers Draft 15
const struct DictionaryTestCase {
  const char* name;
  const char* raw;
  const absl::optional<Dictionary>
      expected;           // nullopt if parse error is expected.
  const char* canonical;  // nullptr if parse error is expected, or if canonical
                          // format is identical to raw.
} dictionary_test_cases[] = {
    {"basic dictionary",
     "en=\"Applepie\", da=:aGVsbG8=:",
     {Dictionary{{{"en", {Item("Applepie"), {}}},
                  {"da", {Item("hello", Item::kByteSequenceType), {}}}}}}},
    {"tab separated dictionary", "a=1\t,\tb=2", absl::nullopt},
    {"missing value with params dictionary",
     "a=1, b;foo=9, c=3",
     {Dictionary{{{"a", {Integer(1L), {}}},
                  {"b", {Item(true), {Param("foo", 9)}}},
                  {"c", {Integer(3L), {}}}}}}},
    // Parameterised dictionary tests
    {"parameterised inner list member dict",
     "a=(\"1\";b=1;c=?0 \"2\");d=\"e\"",
     {Dictionary{{{"a",
                   {{{Item("1"), {Param("b", 1), BooleanParam("c", false)}},
                     {Item("2"), {}}},
                    {Param("d", "e")}}}}}}},
    {"explicit true value with parameter",
     "a=?1;b=1",
     {Dictionary{{{"a", {Item(true), {Param("b", 1)}}}}}},
     "a;b=1"},
    {"implicit true value with parameter",
     "a;b=1",
     {Dictionary{{{"a", {Item(true), {Param("b", 1)}}}}}}},
    {"implicit true value with implicitly-valued parameter",
     "a;b",
     {Dictionary{{{"a", {Item(true), {BooleanParam("b", true)}}}}}}},
};
}  // namespace

TEST(StructuredHeaderTest, ParseBareItem) {
  for (const auto& c : item_test_cases) {
    SCOPED_TRACE(c.name);
    absl::optional<Item> result = ParseBareItem(c.raw);
    EXPECT_EQ(result, c.expected);
  }
}

// For Structured Headers Draft 15, these tests include parameters on Items.
TEST(StructuredHeaderTest, ParseItem) {
  for (const auto& c : parameterized_item_test_cases) {
    SCOPED_TRACE(c.name);
    absl::optional<ParameterizedItem> result = ParseItem(c.raw);
    EXPECT_EQ(result, c.expected);
  }
}

// Structured Headers Draft 9 parsing rules are different than Draft 15, and
// some strings which are considered invalid in SH15 should parse in SH09.
// The SH09 Item parser is not directly exposed, but can be used indirectly by
// calling the parser for SH09-specific lists.
TEST(StructuredHeaderTest, ParseSH09Item) {
  for (const auto& c : sh09_item_test_cases) {
    SCOPED_TRACE(c.name);
    absl::optional<ListOfLists> result = ParseListOfLists(c.raw);
    if (c.expected.has_value()) {
      EXPECT_TRUE(result.has_value());
      EXPECT_EQ(result->size(), 1UL);
      EXPECT_EQ((*result)[0].size(), 1UL);
      EXPECT_EQ((*result)[0][0], c.expected);
    } else {
      EXPECT_FALSE(result.has_value());
    }
  }
}

// In Structured Headers Draft 9, floats can have more than three fractional
// digits, and can be larger than 1e12. This behaviour is exposed in the parser
// for SH09-specific lists, so test it through that interface.
TEST(StructuredHeaderTest, SH09HighPrecisionFloats) {
  // These values are exactly representable in binary floating point, so no
  // accuracy issues are expected in this test.
  absl::optional<ListOfLists> result =
      ParseListOfLists("1.03125;-1.03125;12345678901234.5;-12345678901234.5");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result,
            (ListOfLists{{Item(1.03125), Item(-1.03125), Item(12345678901234.5),
                          Item(-12345678901234.5)}}));

  result = ParseListOfLists("123456789012345.0");
  EXPECT_FALSE(result.has_value());

  result = ParseListOfLists("-123456789012345.0");
  EXPECT_FALSE(result.has_value());
}

// For Structured Headers Draft 9
TEST(StructuredHeaderTest, ParseListOfLists) {
  static const struct TestCase {
    const char* name;
    const char* raw;
    ListOfLists expected;  // empty if parse error is expected
  } cases[] = {
      {"basic list of lists",
       "1;2, 42;43",
       {{Integer(1L), Integer(2L)}, {Integer(42L), Integer(43L)}}},
      {"empty list of lists", "", {}},
      {"single item list of lists", "42", {{Integer(42L)}}},
      {"no whitespace list of lists", "1,42", {{Integer(1L)}, {Integer(42L)}}},
      {"no inner whitespace list of lists",
       "1;2, 42;43",
       {{Integer(1L), Integer(2L)}, {Integer(42L), Integer(43L)}}},
      {"extra whitespace list of lists",
       "1 , 42",
       {{Integer(1L)}, {Integer(42L)}}},
      {"extra inner whitespace list of lists",
       "1 ; 2,42 ; 43",
       {{Integer(1L), Integer(2L)}, {Integer(42L), Integer(43L)}}},
      {"trailing comma list of lists", "1;2, 42,", {}},
      {"trailing semicolon list of lists", "1;2, 42;43;", {}},
      {"leading comma list of lists", ",1;2, 42", {}},
      {"leading semicolon list of lists", ";1;2, 42;43", {}},
      {"empty item list of lists", "1,,42", {}},
      {"empty inner item list of lists", "1;;2,42", {}},
  };
  for (const auto& c : cases) {
    SCOPED_TRACE(c.name);
    absl::optional<ListOfLists> result = ParseListOfLists(c.raw);
    if (!c.expected.empty()) {
      EXPECT_TRUE(result.has_value());
      EXPECT_EQ(*result, c.expected);
    } else {
      EXPECT_FALSE(result.has_value());
    }
  }
}

// For Structured Headers Draft 9
TEST(StructuredHeaderTest, ParseParameterisedList) {
  static const struct TestCase {
    const char* name;
    const char* raw;
    ParameterisedList expected;  // empty if parse error is expected
  } cases[] = {
      {"basic param-list",
       "abc_123;a=1;b=2; cdef_456, ghi;q=\"9\";r=\"w\"",
       {
           {Token("abc_123"),
            {Param("a", 1), Param("b", 2), NullParam("cdef_456")}},
           {Token("ghi"), {Param("q", "9"), Param("r", "w")}},
       }},
      {"empty param-list", "", {}},
      {"single item param-list",
       "text/html;q=1",
       {{Token("text/html"), {Param("q", 1)}}}},
      {"empty param-list", "", {}},
      {"no whitespace param-list",
       "text/html,text/plain;q=1",
       {{Token("text/html"), {}}, {Token("text/plain"), {Param("q", 1)}}}},
      {"whitespace before = param-list", "text/html, text/plain;q =1", {}},
      {"whitespace after = param-list", "text/html, text/plain;q= 1", {}},
      {"extra whitespace param-list",
       "text/html  ,  text/plain ;  q=1",
       {{Token("text/html"), {}}, {Token("text/plain"), {Param("q", 1)}}}},
      {"duplicate key", "abc;a=1;b=2;a=1", {}},
      {"numeric key", "abc;a=1;1b=2;c=1", {}},
      {"uppercase key", "abc;a=1;B=2;c=1", {}},
      {"bad key", "abc;a=1;b!=2;c=1", {}},
      {"another bad key", "abc;a=1;b==2;c=1", {}},
      {"empty key name", "abc;a=1;=2;c=1", {}},
      {"empty parameter", "abc;a=1;;c=1", {}},
      {"empty list item", "abc;a=1,,def;b=1", {}},
      {"extra semicolon", "abc;a=1;b=1;", {}},
      {"extra comma", "abc;a=1,def;b=1,", {}},
      {"leading semicolon", ";abc;a=1", {}},
      {"leading comma", ",abc;a=1", {}},
  };
  for (const auto& c : cases) {
    SCOPED_TRACE(c.name);
    absl::optional<ParameterisedList> result = ParseParameterisedList(c.raw);
    if (c.expected.empty()) {
      EXPECT_FALSE(result.has_value());
      continue;
    }
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), c.expected.size());
    if (result->size() == c.expected.size()) {
      for (size_t i = 0; i < c.expected.size(); ++i)
        EXPECT_EQ((*result)[i], c.expected[i]);
    }
  }
}

// For Structured Headers Draft 15
TEST(StructuredHeaderTest, ParseList) {
  for (const auto& c : list_test_cases) {
    SCOPED_TRACE(c.name);
    absl::optional<List> result = ParseList(c.raw);
    EXPECT_EQ(result, c.expected);
  }
}

// For Structured Headers Draft 15
TEST(StructuredHeaderTest, ParseDictionary) {
  for (const auto& c : dictionary_test_cases) {
    SCOPED_TRACE(c.name);
    absl::optional<Dictionary> result = ParseDictionary(c.raw);
    EXPECT_EQ(result, c.expected);
  }
}

// Serializer tests are all exclusively for Structured Headers Draft 15

TEST(StructuredHeaderTest, SerializeItem) {
  for (const auto& c : item_test_cases) {
    SCOPED_TRACE(c.name);
    if (c.expected) {
      absl::optional<std::string> result = SerializeItem(*c.expected);
      EXPECT_TRUE(result.has_value());
      EXPECT_EQ(result.value(), std::string(c.canonical ? c.canonical : c.raw));
    }
  }
}

TEST(StructuredHeaderTest, SerializeParameterizedItem) {
  for (const auto& c : parameterized_item_test_cases) {
    SCOPED_TRACE(c.name);
    if (c.expected) {
      absl::optional<std::string> result = SerializeItem(*c.expected);
      EXPECT_TRUE(result.has_value());
      EXPECT_EQ(result.value(), std::string(c.canonical ? c.canonical : c.raw));
    }
  }
}

TEST(StructuredHeaderTest, UnserializableItems) {
  // Test that items with unknown type are not serialized.
  EXPECT_FALSE(SerializeItem(Item()).has_value());
}

TEST(StructuredHeaderTest, UnserializableTokens) {
  static const struct UnserializableString {
    const char* name;
    const char* value;
  } bad_tokens[] = {
      {"empty token", ""},
      {"contains high ascii", "a\xff"},
      {"contains nonprintable character", "a\x7f"},
      {"contains C0", "a\x01"},
      {"UTF-8 encoded", "a\xc3\xa9"},
      {"contains TAB", "a\t"},
      {"contains LF", "a\n"},
      {"contains CR", "a\r"},
      {"contains SP", "a "},
      {"begins with digit", "9token"},
      {"begins with hyphen", "-token"},
      {"begins with LF", "\ntoken"},
      {"begins with SP", " token"},
      {"begins with colon", ":token"},
      {"begins with percent", "%token"},
      {"begins with period", ".token"},
      {"begins with slash", "/token"},
  };
  for (const auto& bad_token : bad_tokens) {
    SCOPED_TRACE(bad_token.name);
    absl::optional<std::string> serialization =
        SerializeItem(Token(bad_token.value));
    EXPECT_FALSE(serialization.has_value()) << *serialization;
  }
}

TEST(StructuredHeaderTest, UnserializableKeys) {
  static const struct UnserializableString {
    const char* name;
    const char* value;
  } bad_keys[] = {
      {"empty key", ""},
      {"contains high ascii", "a\xff"},
      {"contains nonprintable character", "a\x7f"},
      {"contains C0", "a\x01"},
      {"UTF-8 encoded", "a\xc3\xa9"},
      {"contains TAB", "a\t"},
      {"contains LF", "a\n"},
      {"contains CR", "a\r"},
      {"contains SP", "a "},
      {"begins with uppercase", "Atoken"},
      {"begins with digit", "9token"},
      {"begins with hyphen", "-token"},
      {"begins with LF", "\ntoken"},
      {"begins with SP", " token"},
      {"begins with colon", ":token"},
      {"begins with percent", "%token"},
      {"begins with period", ".token"},
      {"begins with slash", "/token"},
  };
  for (const auto& bad_key : bad_keys) {
    SCOPED_TRACE(bad_key.name);
    absl::optional<std::string> serialization =
        SerializeItem(ParameterizedItem("a", {{bad_key.value, "a"}}));
    EXPECT_FALSE(serialization.has_value()) << *serialization;
  }
}

TEST(StructuredHeaderTest, UnserializableStrings) {
  static const struct UnserializableString {
    const char* name;
    const char* value;
  } bad_strings[] = {
      {"contains high ascii", "a\xff"},
      {"contains nonprintable character", "a\x7f"},
      {"UTF-8 encoded", "a\xc3\xa9"},
      {"contains TAB", "a\t"},
      {"contains LF", "a\n"},
      {"contains CR", "a\r"},
      {"contains C0", "a\x01"},
  };
  for (const auto& bad_string : bad_strings) {
    SCOPED_TRACE(bad_string.name);
    absl::optional<std::string> serialization =
        SerializeItem(Item(bad_string.value));
    EXPECT_FALSE(serialization.has_value()) << *serialization;
  }
}

TEST(StructuredHeaderTest, UnserializableIntegers) {
  EXPECT_FALSE(SerializeItem(Integer(1e15L)).has_value());
  EXPECT_FALSE(SerializeItem(Integer(-1e15L)).has_value());
}

TEST(StructuredHeaderTest, UnserializableDecimals) {
  for (double value :
       {std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), 1e12, 1e12 - 0.0001,
        1e12 - 0.0005, -1e12, -1e12 + 0.0001, -1e12 + 0.0005}) {
    auto x = SerializeItem(Item(value));
    EXPECT_FALSE(SerializeItem(Item(value)).has_value());
  }
}

// These values cannot be directly parsed from headers, but are valid doubles
// which can be serialized as sh-floats (though rounding is expected.)
TEST(StructuredHeaderTest, SerializeUnparseableDecimals) {
  struct UnparseableDecimal {
    const char* name;
    double value;
    const char* canonical;
  } float_test_cases[] = {
      {"negative 0", -0.0, "0.0"},
      {"0.0001", 0.0001, "0.0"},
      {"0.0000001", 0.0000001, "0.0"},
      {"1.0001", 1.0001, "1.0"},
      {"1.0009", 1.0009, "1.001"},
      {"round positive odd decimal", 0.0015, "0.002"},
      {"round positive even decimal", 0.0025, "0.002"},
      {"round negative odd decimal", -0.0015, "-0.002"},
      {"round negative even decimal", -0.0025, "-0.002"},
      {"round decimal up to integer part", 9.9995, "10.0"},
      {"subnormal numbers", std::numeric_limits<double>::denorm_min(), "0.0"},
      {"round up to 10 digits", 1e9 - 0.0000001, "1000000000.0"},
      {"round up to 11 digits", 1e10 - 0.000001, "10000000000.0"},
      {"round up to 12 digits", 1e11 - 0.00001, "100000000000.0"},
      {"largest serializable float", nextafter(1e12 - 0.0005, 0),
       "999999999999.999"},
      {"largest serializable negative float", -nextafter(1e12 - 0.0005, 0),
       "-999999999999.999"},
      // This will fail if we simply truncate the fractional portion.
      {"float rounds up to next int", 3.9999999, "4.0"},
      // This will fail if we first round to >3 digits, and then round again to
      // 3 digits.
      {"don't double round", 3.99949, "3.999"},
      // This will fail if we first round to 3 digits, and then round again to
      // max_avail_digits.
      {"don't double round", 123456789.99949, "123456789.999"},
  };
  for (const auto& test_case : float_test_cases) {
    SCOPED_TRACE(test_case.name);
    absl::optional<std::string> serialization =
        SerializeItem(Item(test_case.value));
    EXPECT_TRUE(serialization.has_value());
    EXPECT_EQ(*serialization, test_case.canonical);
  }
}

TEST(StructuredHeaderTest, SerializeList) {
  for (const auto& c : list_test_cases) {
    SCOPED_TRACE(c.name);
    if (c.expected) {
      absl::optional<std::string> result = SerializeList(*c.expected);
      EXPECT_TRUE(result.has_value());
      EXPECT_EQ(result.value(), std::string(c.canonical ? c.canonical : c.raw));
    }
  }
}

TEST(StructuredHeaderTest, UnserializableLists) {
  static const struct UnserializableList {
    const char* name;
    const List value;
  } bad_lists[] = {
      {"Null item as member", {{Item(), {}}}},
      {"Unserializable item as member", {{Token("\n"), {}}}},
      {"Key is empty", {{Token("abc"), {Param("", 1)}}}},
      {"Key contains whitespace", {{Token("abc"), {Param("a\n", 1)}}}},
      {"Key contains UTF8", {{Token("abc"), {Param("a\xc3\xa9", 1)}}}},
      {"Key contains unprintable characters",
       {{Token("abc"), {Param("a\x7f", 1)}}}},
      {"Key contains disallowed characters",
       {{Token("abc"), {Param("a:", 1)}}}},
      {"Param value is unserializable", {{Token("abc"), {{"a", Token("\n")}}}}},
      {"Inner list contains unserializable item",
       {{std::vector<ParameterizedItem>{{Token("\n"), {}}}, {}}}},
  };
  for (const auto& bad_list : bad_lists) {
    SCOPED_TRACE(bad_list.name);
    absl::optional<std::string> serialization = SerializeList(bad_list.value);
    EXPECT_FALSE(serialization.has_value()) << *serialization;
  }
}

TEST(StructuredHeaderTest, SerializeDictionary) {
  for (const auto& c : dictionary_test_cases) {
    SCOPED_TRACE(c.name);
    if (c.expected) {
      absl::optional<std::string> result = SerializeDictionary(*c.expected);
      EXPECT_TRUE(result.has_value());
      EXPECT_EQ(result.value(), std::string(c.canonical ? c.canonical : c.raw));
    }
  }
}

TEST(StructuredHeaderTest, DictionaryConstructors) {
  const std::string key0 = "key0";
  const std::string key1 = "key1";
  const ParameterizedMember member0{Item("Applepie"), {}};
  const ParameterizedMember member1{Item("hello", Item::kByteSequenceType), {}};

  Dictionary dict;
  EXPECT_TRUE(dict.empty());
  EXPECT_EQ(0U, dict.size());
  dict[key0] = member0;
  EXPECT_FALSE(dict.empty());
  EXPECT_EQ(1U, dict.size());

  const Dictionary dict_copy = dict;
  EXPECT_FALSE(dict_copy.empty());
  EXPECT_EQ(1U, dict_copy.size());
  EXPECT_EQ(dict, dict_copy);

  const Dictionary dict_init{{{key0, member0}, {key1, member1}}};
  EXPECT_FALSE(dict_init.empty());
  EXPECT_EQ(2U, dict_init.size());
  EXPECT_EQ(member0, dict_init.at(key0));
  EXPECT_EQ(member1, dict_init.at(key1));
}

TEST(StructuredHeaderTest, DictionaryAccessors) {
  const std::string key0 = "key0";
  const std::string key1 = "key1";

  const ParameterizedMember nonempty_member0{Item("Applepie"), {}};
  const ParameterizedMember nonempty_member1{
      Item("hello", Item::kByteSequenceType), {}};
  const ParameterizedMember empty_member;

  Dictionary dict{{{key0, nonempty_member0}}};
  EXPECT_TRUE(dict.contains(key0));
  EXPECT_EQ(nonempty_member0, dict[key0]);
  EXPECT_EQ(&dict[key0], &dict.at(key0));
  EXPECT_EQ(&dict[key0], &dict[0]);
  EXPECT_EQ(&dict[key0], &dict.at(0));

  // Even if the key does not yet exist in |dict|, operator[]() should
  // automatically create an empty entry.
  ASSERT_FALSE(dict.contains(key1));
  ParameterizedMember& member1 = dict[key1];
  EXPECT_TRUE(dict.contains(key1));
  EXPECT_EQ(empty_member, member1);
  EXPECT_EQ(&member1, &dict[key1]);
  EXPECT_EQ(&member1, &dict.at(key1));
  EXPECT_EQ(&member1, &dict[1]);
  EXPECT_EQ(&member1, &dict.at(1));

  member1 = nonempty_member1;
  EXPECT_EQ(nonempty_member1, dict[key1]);
  EXPECT_EQ(&dict[key1], &dict.at(key1));
  EXPECT_EQ(&dict[key1], &dict[1]);
  EXPECT_EQ(&dict[key1], &dict.at(1));

  // at(StringPiece) and indexed accessors have const overloads.
  const Dictionary& dict_ref = dict;
  EXPECT_EQ(&member1, &dict_ref.at(key1));
  EXPECT_EQ(&member1, &dict_ref[1]);
  EXPECT_EQ(&member1, &dict_ref.at(1));
}

TEST(StructuredHeaderTest, UnserializableDictionary) {
  static const struct UnserializableDictionary {
    const char* name;
    const Dictionary value;
  } bad_dictionaries[] = {
      {"Unserializable dict key", Dictionary{{{"ABC", {Token("abc"), {}}}}}},
      {"Dictionary item is unserializable",
       Dictionary{{{"abc", {Token("abc="), {}}}}}},
      {"Param value is unserializable",
       Dictionary{{{"abc", {Token("abc"), {{"a", Token("\n")}}}}}}},
      {"Dictionary inner-list contains unserializable item",
       Dictionary{
           {{"abc",
             {std::vector<ParameterizedItem>{{Token("abc="), {}}}, {}}}}}},
  };
  for (const auto& bad_dictionary : bad_dictionaries) {
    SCOPED_TRACE(bad_dictionary.name);
    absl::optional<std::string> serialization =
        SerializeDictionary(bad_dictionary.value);
    EXPECT_FALSE(serialization.has_value()) << *serialization;
  }
}

}  // namespace structured_headers
}  // namespace net
