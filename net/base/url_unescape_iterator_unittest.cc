// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/url_unescape_iterator.h"

#include <iterator>
#include <limits>
#include <ranges>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace net {

namespace {

static_assert(std::forward_iterator<UrlUnescapeIterator>);

// A single test case. Does not own the referenced strings.
struct Case {
  std::string_view input;
  std::string_view expected_output;
  std::string_view description;
};

// A test case that is constructed at runtime. Can be converted to a Case.
struct OwningCase {
  std::string input;
  std::string expected_output;
  std::string description;

  OwningCase(std::string_view in,
             std::string_view expected,
             std::string_view desc)
      : input(in), expected_output(expected), description(desc) {}

  explicit operator Case() const {
    return Case(input, expected_output, description);
  }
};

// Convenience function used in most tests.
std::string UnescapeToString(std::string_view in) {
  auto as_range = MakeUrlUnescapeRange(in);
  static_assert(std::ranges::forward_range<decltype(as_range)>);
  return std::string(std::ranges::begin(as_range), std::ranges::end(as_range));
}

// Test a contiguous range of cases.
void TestCases(base::span<const Case> cases) {
  for (const auto [input, expected_output, description] : cases) {
    EXPECT_EQ(UnescapeToString(input), expected_output) << description;
  }
}

// Same as above, but for OwningCase.
void TestCases(base::span<const OwningCase> cases) {
  auto unowned =
      base::ToVector(cases, [](const OwningCase& c) { return Case(c); });
  TestCases(unowned);
}

// Converts the test cases in `cases` to percent-encoded form by escaping all
// non-ASCII characters as %xx, then runs them.
void EncodeThenTestCases(base::span<const Case> cases) {
  auto encoded = base::ToVector(cases, [](const Case& in) {
    auto [input, expected, description] = in;
    auto escaped = base::EscapeNonASCII(input);
    return OwningCase(escaped, expected, description);
  });
  TestCases(encoded);
}

TEST(UrlUnescapeIteratorTest, DefaultConstructor) {
  constexpr UrlUnescapeIterator a;
  constexpr UrlUnescapeIterator b;
  EXPECT_EQ(a, b);
  static_assert(a == b);
}

TEST(UrlUnescapeIteratorTest, CopyAndAssignAndEquality) {
  auto [a, b] = MakeUrlUnescapeRange("walk");
  EXPECT_NE(a, b);
  b = a;
  EXPECT_EQ(a, b);
  const UrlUnescapeIterator c = a;
  EXPECT_EQ(a, c);
  const UrlUnescapeIterator d = c;
  EXPECT_EQ(c, d);
  b = d;
  EXPECT_EQ(b, d);
}

TEST(UrlUnescapeIteratorTest, PostIncrement) {
  auto [it, end] = MakeUrlUnescapeRange("a");
  const UrlUnescapeIterator old_it = it;
  EXPECT_EQ(old_it, it++);
  EXPECT_NE(old_it, it);
  EXPECT_EQ(it, end);
}

TEST(UrlUnescapeIteratorTest, GoodAscii) {
  static constexpr std::string_view kNul("\0", 1u);
  static constexpr Case cases[] = {
      {"", "", "empty"},
      {"a", "a", "one letter"},
      {"word", "word", "multiple letters"},
      {"two words", "two words", "space"},
      {"two+words", "two words", "plus"},
      {"two%20words", "two words", "escaped space"},
      {"%2b", "+", "escaped plus"},
      {"%2B", "+", "escaped plus, uppercase hex"},
      {"++", "  ", "double plus"},
      {"+%20+", "   ", "plus, escaped space, plus"},
      {"%61b", "ab", "escaped start"},
      {"a%62", "ab", "escaped end"},
      {"%00", kNul, "escaped nul byte"},
      {"line%0a", "line\x0a", "escaped newline"},
      {"l%7D", "l\x7d", "escaped del control code"},
  };
  TestCases(cases);
}

TEST(UrlUnescapeIteratorTest, BadPercentEncoding) {
  static constexpr Case cases[] = {
      {"%", "%", "percent at end of string"},
      {"%2", "%2", "not followed by two characters"},
      {"%g1", "%g1", "first character not hex"},
      {"%1 ", "%1 ", "second character not hex"},
      {"%+20", "% 20", "first character is plus"},
      {"% 20", "% 20", "first character is space"},
      {"%1%20", "%1 ", "second character is percent"},
      {"%%34%31", "%41", "no double expansion"},
  };
  TestCases(cases);
}

static constexpr Case kGoodUtf8[] = {
    {"\xc2\xa5", "\xc2\xa5", "two bytes"},
    {"\xef\xbf\xa5", "\xef\xbf\xa5", "three bytes"},
    {"\xf0\x9f\x86\x91", "\xf0\x9f\x86\x91", "four bytes"},
    {"\xef\xb7\x90", "\xef\xb7\x90", "non-character"},
};

TEST(UrlUnescapeIteratorTest, GoodUtf8) {
  TestCases(kGoodUtf8);
}

TEST(UrlUnescapeIteratorTest, GoodUtf8Encoded) {
  EncodeThenTestCases(kGoodUtf8);
}

// Verifies that mixing encoded and unencoded bytes in a single character
// works.
TEST(UrlUnescapeIteratorTest, GoodUtf8MixedEncoded) {
  std::vector<OwningCase> encoded;
  // Not the correct size, just an estimate to reduce resizes.
  encoded.reserve(std::size(kGoodUtf8) * 2);
  for (const auto [input, expected, description] : kGoodUtf8) {
    for (int byte_to_encode = 0; byte_to_encode < input.size();
         ++byte_to_encode) {
      const std::string encoded_byte =
          base::EscapeNonASCII(input.substr(byte_to_encode, 1));
      const std::string encoded_input =
          base::StrCat({input.substr(0, byte_to_encode), encoded_byte,
                        input.substr(byte_to_encode + 1)});
      encoded.emplace_back(encoded_input, expected,
                           base::StringPrintf("%s, encoded byte %zu",
                                              description, byte_to_encode));
    }
  }

  TestCases(encoded);
}

#define REPLACEMENT_CHAR "\xef\xbf\xbd"

constexpr char kReplacementChar[] = REPLACEMENT_CHAR;
constexpr char kReplacementCharx2[] = REPLACEMENT_CHAR REPLACEMENT_CHAR;
constexpr char kReplacementCharx3[] =
    REPLACEMENT_CHAR REPLACEMENT_CHAR REPLACEMENT_CHAR;
constexpr char kReplacementCharx4[] =
    REPLACEMENT_CHAR REPLACEMENT_CHAR REPLACEMENT_CHAR REPLACEMENT_CHAR;
constexpr char kReplacementCharx5[] = REPLACEMENT_CHAR REPLACEMENT_CHAR
    REPLACEMENT_CHAR REPLACEMENT_CHAR REPLACEMENT_CHAR;
constexpr char kReplacementCharx6[] = REPLACEMENT_CHAR REPLACEMENT_CHAR
    REPLACEMENT_CHAR REPLACEMENT_CHAR REPLACEMENT_CHAR REPLACEMENT_CHAR;

std::string ReplacementCharNTimes(size_t n) {
  const std::vector<std::string_view> to_concat(n, kReplacementChar);
  return base::StrCat(to_concat);
}

TEST(UrlUnescapeIteratorTest, TruncatedUtf8) {
  std::vector<OwningCase> truncated;
  truncated.reserve(std::size(kGoodUtf8) * 4);
  for (const auto [input, expected, description] : kGoodUtf8) {
    for (int truncate_pos = 1; truncate_pos < input.size(); ++truncate_pos) {
      const std::string truncated_input(input.substr(0, truncate_pos));
      // We expect one replacement character per UTF-8 start byte, regardless
      // of length.
      truncated.emplace_back(truncated_input, kReplacementChar,
                             base::StringPrintf("%s, truncated to length %zu",
                                                description, truncate_pos));
      truncated.emplace_back(
          base::EscapeNonASCII(truncated_input), kReplacementChar,
          base::StringPrintf("%s, truncated to length %zu, encoded",
                             description, truncate_pos));
    }
  }
  TestCases(truncated);
}

TEST(UrlUnescapeIteratorTest, CorruptedUtf8) {
  std::vector<OwningCase> corrupted;
  corrupted.reserve(std::size(kGoodUtf8) * 4);
  for (const auto [input, expected, description] : kGoodUtf8) {
    for (int corrupt_byte = 0; corrupt_byte < input.size(); ++corrupt_byte) {
      const std::string corrupted_input =
          base::StrCat({"-", input.substr(0, corrupt_byte), "X",
                        input.substr(corrupt_byte + 1), "-"});
      // A valid initial sequence will be replaced with a single replacement
      // character. Unexpected continuation bytes will be replaced with one
      // replacement character each.
      const std::string expected_output = base::StrCat(
          {"-", corrupt_byte > 0 ? kReplacementChar : "", "X",
           ReplacementCharNTimes(input.size() - corrupt_byte - 1), "-"});
      corrupted.emplace_back(corrupted_input, expected_output,
                             base::StringPrintf("%s, with byte %zu corrupted",
                                                description, corrupt_byte));
      corrupted.emplace_back(
          base::EscapeNonASCII(corrupted_input), expected_output,
          base::StringPrintf("%s, with byte %zu corrupted, encoded",
                             description, corrupt_byte));
    }
  }
  TestCases(corrupted);
}

constexpr Case kBadUtf8[] = {
    {"\xC0\x80", kReplacementCharx2,
     "Overlong encoding of U+0000 (null). 0xC0 is never a valid start."},
    {"\xC1\xBF", kReplacementCharx2,
     "Overlong encoding of U+007F. 0xC1 is never a valid start."},
    {"\xE0\x80\x80", kReplacementCharx3,
     "Overlong encoding of U+0000 (null) as 3 bytes."},
    {"\xE0\x9F\xBF", kReplacementCharx3,
     "Overlong encoding of U+07FF as 3 bytes (should be 2)."},
    {"\xF0\x80\x80\x80", kReplacementCharx4,
     "Overlong encoding of U+0000 (null) as 4 bytes."},
    {"\xF0\x8F\xBF\xBF", kReplacementCharx4,
     "Overlong encoding of U+FFFF as 4 bytes (should be 3)."},
    {"\xED\xA0\x80", kReplacementCharx3,
     "Invalid surrogate half U+D800 (start of surrogate range)"},
    {"\xED\xBF\xBF", kReplacementCharx3,
     "Invalid surrogate half U+DFFF (end of surrogate range)"},
    {"\xED\xA0\x81\xED\xB0\x80", kReplacementCharx6,
     "Incorrectly encoded surrogate pair"},
    {"\xF4\x90\x80\x80", kReplacementCharx4,
     "Invalid code point U+110000 (beyond Unicode max U+10FFFF)"},
    {"\xF5\x80\x80\x80", kReplacementCharx4,
     "Invalid start byte 0xF5 (would encode > U+10FFFF)"},
    {"\xF8\x80\x80\x80\x80", kReplacementCharx5,
     "Invalid start byte 0xF8 (formerly 5-byte sequence)"},
    {"\xFC\x80\x80\x80\x80\x80", kReplacementCharx6,
     "Invalid start byte 0xFC (formerly 6-byte sequence)"},
    {"\xFE", kReplacementChar, "Invalid byte 0xFE (never used)"},
    {"\xFF", kReplacementChar, "Invalid byte 0xFF (never used)"},
    {"\xc2\xa5\xc1\xc2\xa5", "\xc2\xa5" REPLACEMENT_CHAR "\xc2\xa5",
     "Valid followed by invalid followed by valid"},
    {"\xE2\xE2", kReplacementCharx2, "Overshort with error"},
};

TEST(UrlUnescapeIteratorTest, OtherBadUtf8) {
  TestCases(kBadUtf8);
}

TEST(UrlUnescapeIteratorTest, OtherBadUtf8Encoded) {
  EncodeThenTestCases(kBadUtf8);
}

void SameOutputAsUnescapePercentEncodedUrl(std::string_view input) {
  EXPECT_EQ(UnescapeToString(input), UnescapePercentEncodedUrl(input));
}

// Exhaustively test the output is the same as UnescapePercentEncodedUrl() for
// all single-byte inputs.
TEST(UrlUnescapeIteratorTest, OneByteSameAsUnescapePercentEncodedUrl) {
  // `i` is int to avoid problems with overflowing.
  for (int i = std::numeric_limits<char>::min();
       i <= std::numeric_limits<char>::max(); ++i) {
    const char c = static_cast<char>(i);
    SameOutputAsUnescapePercentEncodedUrl(std::string_view(&c, 1u));
  }
}

// Same thing, but %-encoded.
TEST(UrlUnescapeIteratorTest, OneByteSameAsUnescapePercentEncodedUrlEncoded) {
  for (int i = 0; i <= 0xFF; ++i) {
    const std::string input = base::StringPrintf("%%%02x", i);
    SameOutputAsUnescapePercentEncodedUrl(input);
  }
}

FUZZ_TEST(UrlUnescapeIteratorTest, SameOutputAsUnescapePercentEncodedUrl);

TEST(UrlUnescapeIteratorTest, TrivialSelfEquals) {
  auto expect_self_equals = [](base::span<const Case> cases) {
    for (const auto [input, _, description] : cases) {
      EXPECT_TRUE(EqualsAfterUrlDecoding(input, input)) << description;
    }
  };
  for (const char* input : {"", "a", "word", " ", "+", "%", "%2", "%20"}) {
    EXPECT_TRUE(EqualsAfterUrlDecoding(input, input)) << input;
  }
  expect_self_equals(kGoodUtf8);
  expect_self_equals(kBadUtf8);
}

TEST(UrlUnescapeIteratorTest, EqualsAfterEscaping) {
  auto expect_equals_after_escaping = [](base::span<const Case> cases) {
    for (const auto [input, _, description] : cases) {
      EXPECT_TRUE(
          EqualsAfterUrlDecoding(input, base::EscapeAllExceptUnreserved(input)))
          << description;
      EXPECT_TRUE(
          EqualsAfterUrlDecoding(base::EscapeAllExceptUnreserved(input), input))
          << description << ", backwards";
    }
  };
  expect_equals_after_escaping(kGoodUtf8);
  expect_equals_after_escaping(kBadUtf8);
}

struct StringPair {
  std::string_view a;
  std::string_view b;
};

TEST(UrlUnescapeIteratorTest, InterestinglyEqual) {
  static constexpr StringPair cases[] = {
      {" ", "+"},        {"+", "%20"},         {"%", "%25"},
      {"%2a", "%2A"},    {"%c2%A5", "%C2%a5"}, {"%c2\xa5", "\xc2%a5"},
      {"%c0", "%c1"},     // both become replacement character
      {"%c2", "%ef%bf"},  // both are truncated UTF-8 codepoints
  };
  for (const auto [a, b] : cases) {
    EXPECT_TRUE(EqualsAfterUrlDecoding(a, b))
        << "(\"" << a << "\", \"" << b << "\")";
  }
}

TEST(UrlUnescapeIteratorTest, Unequal) {
  static constexpr StringPair cases[] = {
      {"", "%00"},          {"abc", "ABC"}, {"\xc2\xa5", "\xc2\xa6"},
      {"%c2%a5", "%c2%a6"}, {"%a", "%A"},   {"%2g", "%2G"},
      {"%00a", "%00A"},
  };
  for (const auto [a, b] : cases) {
    EXPECT_FALSE(EqualsAfterUrlDecoding(a, b))
        << "(\"" << a << "\", \"" << b << "\")";
  }
}

#undef REPLACEMENT_CHAR

}  // namespace

}  // namespace net
