// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/http/structured_header.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace http_structured_header {
namespace {

// Helpers to make test cases clearer

Item Token(std::string value) {
  return Item(value, Item::kTokenType);
}

std::pair<std::string, Item> Param(std::string key) {
  return std::make_pair(key, Item());
}

std::pair<std::string, Item> Param(std::string key, int64_t value) {
  return std::make_pair(key, Item(value));
}

std::pair<std::string, Item> Param(std::string key, std::string value) {
  return std::make_pair(key, Item(value));
}

// Most test cases are taken from
// https://github.com/httpwg/structured-header-tests.
const struct ItemTestCase {
  const char* name;
  const char* raw;
  const base::Optional<Item> expected;  // nullopt if parse error is expected.
  const char* canonical;  // nullptr if parse error is expected, or if canonical
                          // format is identical to raw.
} item_test_cases[] = {
    // Token
    {"basic token - item", "a_b-c.d3:f%00/*", Token("a_b-c.d3:f%00/*")},
    {"token with capitals - item", "fooBar", Token("fooBar")},
    {"token starting with capitals - item", "FooBar", Token("FooBar")},
    {"bad token - item", "abc$%!", base::nullopt},
    {"leading whitespace", " foo", Token("foo"), "foo"},
    {"trailing whitespace", "foo ", Token("foo"), "foo"},
    // Number
    {"basic integer", "42", Item(42)},
    {"zero integer", "0", Item(0)},
    {"comma", "2,3", base::nullopt},
    {"long integer", "999999999999999", Item(999999999999999L)},
    {"too long integer", "1000000000000000", base::nullopt},
    // Byte Sequence
    {"basic binary", "*aGVsbG8=*", Item("hello", Item::kByteSequenceType)},
    {"empty binary", "**", Item("", Item::kByteSequenceType)},
    {"bad paddding", "*aGVsbG8*", Item("hello", Item::kByteSequenceType),
     "*aGVsbG8=*"},
    {"bad end delimiter", "*aGVsbG8=", base::nullopt},
    {"extra whitespace", "*aGVsb G8=*", base::nullopt},
    {"extra chars", "*aGVsbG!8=*", base::nullopt},
    {"suffix chars", "*aGVsbG8=!*", base::nullopt},
    {"non-zero pad bits", "*iZ==*", Item("\x89", Item::kByteSequenceType),
     "*iQ==*"},
    {"non-ASCII binary", "*/+Ah*", Item("\xFF\xE0!", Item::kByteSequenceType)},
    {"base64url binary", "*_-Ah*", base::nullopt},
    // String
    {"basic string", "\"foo\"", Item("foo")},
    {"empty string", "\"\"", Item("")},
    {"long string",
     "\"foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
     "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
     "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
     "foo foo foo foo foo foo foo foo foo foo foo foo foo foo \"",
     Item("foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
          "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
          "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
          "foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo foo "
          "foo ")},
    {"whitespace string", "\"   \"", Item("   ")},
    {"non-ascii string", "\"f\xC3\xBC\xC3\xBC\"", base::nullopt},
    {"tab in string", "\"\t\"", base::nullopt},
    {"newline in string", "\" \n \"", base::nullopt},
    {"single quoted string", "'foo'", base::nullopt},
    {"unbalanced string", "\"foo", base::nullopt},
    {"string quoting", "\"foo \\\"bar\\\" \\\\ baz\"",
     Item("foo \"bar\" \\ baz")},
    {"bad string quoting", "\"foo \\,\"", base::nullopt},
    {"ending string quote", "\"foo \\\"", base::nullopt},
    {"abruptly ending string quote", "\"foo \\", base::nullopt},
    // Additional tests
    {"valid quoting containing \\n", "\"\\\\n\"", Item("\\n")},
    {"valid quoting containing \\t", "\"\\\\t\"", Item("\\t")},
    {"valid quoting containing \\x", "\"\\\\x61\"", Item("\\x61")},
    {"c-style hex escape in string", "\"\\x61\"", base::nullopt},
    {"valid quoting containing \\u", "\"\\\\u0061\"", Item("\\u0061")},
    {"c-style unicode escape in string", "\"\\u0061\"", base::nullopt},
};

// For Structured Headers Draft 13
const struct ListTestCase {
  const char* name;
  const char* raw;
  const base::Optional<List> expected;  // nullopt if parse error is expected.
  const char* canonical;  // nullptr if parse error is expected, or if canonical
                          // format is identical to raw.
} list_test_cases[] = {
    // Basic lists
    {"basic list", "1, 42", {{{Item(1), {}}, {Item(42), {}}}}},
    {"empty list", "", List()},
    {"single item list", "42", {{{Item(42), {}}}}},
    {"no whitespace list", "1,42", {{{Item(1), {}}, {Item(42), {}}}}, "1, 42"},
    {"trailing comma list", "1, 42,", base::nullopt},
    {"empty item list", "1,,42", base::nullopt},
    // Lists of lists
    {"basic list of lists",
     "(1 2), (42 43)",
     {{{{Item(1), Item(2)}, {}}, {{Item(42), Item(43)}, {}}}}},
    {"single item list of lists",
     "(42)",
     {{{std::vector<Item>{Item(42)}, {}}}}},
    {"empty item list of lists", "()", {{{std::vector<Item>(), {}}}}},
    {"empty middle item list of lists",
     "(1),(),(42)",
     {{{std::vector<Item>{Item(1)}, {}},
       {std::vector<Item>(), {}},
       {std::vector<Item>{Item(42)}, {}}}},
     "(1), (), (42)"},
    {"extra whitespace list of lists",
     "(1  42)",
     {{{{Item(1), Item(42)}, {}}}},
     "(1 42)"},
    {"no trailing parenthesis list of lists", "(1 42", base::nullopt},
    {"no trailing parenthesis middle list of lists", "(1 2, (42 43)",
     base::nullopt},
    // Parameterized Lists
    {"basic parameterised list",
     "abc_123;a=1;b=2; cdef_456, ghi;q=\"9\";r=\"w\"",
     {{{Token("abc_123"), {Param("a", 1), Param("b", 2), Param("cdef_456")}},
       {Token("ghi"), {Param("q", "9"), Param("r", "w")}}}},
     "abc_123;a=1;b=2;cdef_456, ghi;q=\"9\";r=\"w\""},
    {"single item parameterised list",
     "text/html;q=1",
     {{{Token("text/html"), {Param("q", 1)}}}}},
    {"no whitespace parameterised list",
     "text/html,text/plain;q=1",
     {{{Token("text/html"), {}}, {Token("text/plain"), {Param("q", 1)}}}},
     "text/html, text/plain;q=1"},
    {"whitespace before = parameterised list", "text/html, text/plain;q =1",
     base::nullopt},
    {"whitespace after = parameterised list", "text/html, text/plain;q= 1",
     base::nullopt},
    {"extra whitespace param-list",
     "text/html  ,  text/plain ;  q=1",
     {{{Token("text/html"), {}}, {Token("text/plain"), {Param("q", 1)}}}},
     "text/html, text/plain;q=1"},
    {"empty item parameterised list", "text/html,,text/plain;q=1",
     base::nullopt},
};

}  // namespace

TEST(StructuredHeaderTest, ParseItem) {
  for (const auto& c : item_test_cases) {
    SCOPED_TRACE(c.name);
    base::Optional<Item> result = ParseItem(c.raw);
    EXPECT_EQ(result, c.expected);
  }
}

// In Structured Headers Draft 9, integers can be larger than 1e15. This
// behaviour is exposed in the parser for SH09-specific lists, so test it
// through that interface.
TEST(StructuredHeaderTest, SH09LargeInteger) {
  base::Optional<ListOfLists> result = ParseListOfLists("9223372036854775807");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result->size(), 1UL);
  EXPECT_EQ((*result)[0].size(), 1UL);
  EXPECT_EQ((*result)[0][0], Item(9223372036854775807L));

  result = ParseListOfLists("9223372036854775808");
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
       {{Item(1), Item(2)}, {Item(42), Item(43)}}},
      {"empty list of lists", "", {}},
      {"single item list of lists", "42", {{Item(42)}}},
      {"no whitespace list of lists", "1,42", {{Item(1)}, {Item(42)}}},
      {"no inner whitespace list of lists",
       "1;2, 42;43",
       {{Item(1), Item(2)}, {Item(42), Item(43)}}},
      {"extra whitespace list of lists", "1 , 42", {{Item(1)}, {Item(42)}}},
      {"extra inner whitespace list of lists",
       "1 ; 2,42 ; 43",
       {{Item(1), Item(2)}, {Item(42), Item(43)}}},
      {"trailing comma list of lists", "1;2, 42,", {}},
      {"trailing semicolon list of lists", "1;2, 42;43;", {}},
      {"leading comma list of lists", ",1;2, 42", {}},
      {"leading semicolon list of lists", ";1;2, 42;43", {}},
      {"empty item list of lists", "1,,42", {}},
      {"empty inner item list of lists", "1;;2,42", {}},
  };
  for (const auto& c : cases) {
    SCOPED_TRACE(c.name);
    base::Optional<ListOfLists> result = ParseListOfLists(c.raw);
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
            {Param("a", 1), Param("b", 2), Param("cdef_456")}},
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
    base::Optional<ParameterisedList> result = ParseParameterisedList(c.raw);
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

// For Structured Headers Draft 13
TEST(StructuredHeaderTest, ParseList) {
  for (const auto& c : list_test_cases) {
    SCOPED_TRACE(c.name);
    base::Optional<List> result = ParseList(c.raw);
    EXPECT_EQ(result, c.expected);
  }
}

// Serializer tests are all exclusively for Structured Headers Draft 13

TEST(StructuredHeaderTest, SerializeItem) {
  for (const auto& c : item_test_cases) {
    SCOPED_TRACE(c.name);
    if (c.expected) {
      base::Optional<std::string> result = SerializeItem(*c.expected);
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
      {"begins with asterisk", "*token"},
      {"begins with slash", "/token"},
  };
  for (const auto& bad_token : bad_tokens) {
    SCOPED_TRACE(bad_token.name);
    base::Optional<std::string> serialization =
        SerializeItem(Token(bad_token.value));
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
    base::Optional<std::string> serialization =
        SerializeItem(Item(bad_string.value));
    EXPECT_FALSE(serialization.has_value()) << *serialization;
  }
}

TEST(StructuredHeaderTest, UnserializableIntegers) {
  EXPECT_FALSE(SerializeItem(Item(1e15L)).has_value());
}

TEST(StructuredHeaderTest, SerializeList) {
  for (const auto& c : list_test_cases) {
    SCOPED_TRACE(c.name);
    if (c.expected) {
      base::Optional<std::string> result = SerializeList(*c.expected);
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
       {{std::vector<Item>{Token("\n")}, {}}}},
  };
  for (const auto& bad_list : bad_lists) {
    SCOPED_TRACE(bad_list.name);
    base::Optional<std::string> serialization = SerializeList(bad_list.value);
    EXPECT_FALSE(serialization.has_value()) << *serialization;
  }
}

}  // namespace http_structured_header
}  // namespace blink
