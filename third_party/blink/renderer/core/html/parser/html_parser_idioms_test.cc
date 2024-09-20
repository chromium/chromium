// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

namespace {

TEST(HTMLParserIdiomsTest, ParseHTMLInteger) {
  test::TaskEnvironment task_environment;
  int value = 0;

  EXPECT_TRUE(ParseHTMLInteger("2147483646", value));
  EXPECT_EQ(2147483646, value);
  EXPECT_TRUE(ParseHTMLInteger("2147483647", value));
  EXPECT_EQ(2147483647, value);
  value = 12345;
  EXPECT_FALSE(ParseHTMLInteger("2147483648", value));
  EXPECT_EQ(12345, value);

  EXPECT_TRUE(ParseHTMLInteger("-2147483647", value));
  EXPECT_EQ(-2147483647, value);
  EXPECT_TRUE(ParseHTMLInteger("-2147483648", value));
  // The static_cast prevents a sign mismatch warning on Visual Studio, which
  // automatically promotes the subtraction result to unsigned long.
  EXPECT_EQ(static_cast<int>(0 - 2147483648), value);
  value = 12345;
  EXPECT_FALSE(ParseHTMLInteger("-2147483649", value));
  EXPECT_EQ(12345, value);
}

TEST(HTMLParserIdiomsTest, ParseHTMLNonNegativeInteger) {
  test::TaskEnvironment task_environment;
  unsigned value = 0;

  EXPECT_TRUE(ParseHTMLNonNegativeInteger("0", value));
  EXPECT_EQ(0U, value);

  EXPECT_TRUE(ParseHTMLNonNegativeInteger("+0", value));
  EXPECT_EQ(0U, value);

  EXPECT_TRUE(ParseHTMLNonNegativeInteger("-0", value));
  EXPECT_EQ(0U, value);

  EXPECT_TRUE(ParseHTMLNonNegativeInteger("2147483647", value));
  EXPECT_EQ(2147483647U, value);
  EXPECT_TRUE(ParseHTMLNonNegativeInteger("4294967295", value));
  EXPECT_EQ(4294967295U, value);

  EXPECT_TRUE(ParseHTMLNonNegativeInteger("0abc", value));
  EXPECT_EQ(0U, value);
  EXPECT_TRUE(ParseHTMLNonNegativeInteger(" 0", value));
  EXPECT_EQ(0U, value);

  value = 12345U;
  EXPECT_FALSE(ParseHTMLNonNegativeInteger("-1", value));
  EXPECT_EQ(12345U, value);
  EXPECT_FALSE(ParseHTMLNonNegativeInteger("abc", value));
  EXPECT_EQ(12345U, value);
  EXPECT_FALSE(ParseHTMLNonNegativeInteger("  ", value));
  EXPECT_EQ(12345U, value);
  EXPECT_FALSE(ParseHTMLNonNegativeInteger("-", value));
  EXPECT_EQ(12345U, value);
}

TEST(HTMLParserIdiomsTest, ParseHTMLListOfFloatingPointNumbers_null) {
  test::TaskEnvironment task_environment;
  Vector<double> numbers = ParseHTMLListOfFloatingPointNumbers(g_null_atom);
  EXPECT_EQ(0u, numbers.size());
}

struct SplitOnWhitespaceTestCase {
  const char* input;
  std::vector<const char*> expected;
};

class SplitOnWhitespaceTest
    : public testing::Test,
      public ::testing::WithParamInterface<SplitOnWhitespaceTestCase> {
 public:
  static const SplitOnWhitespaceTestCase test_cases[];
};

const SplitOnWhitespaceTestCase SplitOnWhitespaceTest::test_cases[] = {
    {"", {}},
    {" ", {}},
    {"  ", {}},
    {" \t ", {}},
    {" \t\t ", {}},
    {"\r\n\r\n", {}},
    {"a", {"a"}},
    {"abc", {"abc"}},
    {"  a  ", {"a"}},
    {" abc", {"abc"}},
    {"  abc", {"abc"}},
    {"\tabc", {"abc"}},
    {"\t abc", {"abc"}},
    {"abc\n", {"abc"}},
    {"abc \r\n", {"abc"}},
    {" \tabc\n", {"abc"}},
    {"abc\v", {"abc\v"}},
    {"abc def", {"abc", "def"}},
    {"abc  def", {"abc", "def"}},
    {"abc\ndef", {"abc", "def"}},
    {"\tabc\ndef\t", {"abc", "def"}},
    {"  abc\ndef ghi", {"abc", "def", "ghi"}},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SplitOnWhitespaceTest,
    ::testing::ValuesIn(SplitOnWhitespaceTest::test_cases));

TEST_P(SplitOnWhitespaceTest, SplitOnASCIIWhitespace) {
  const SplitOnWhitespaceTestCase test_case = GetParam();
  Vector<String> output = SplitOnASCIIWhitespace(test_case.input);
  EXPECT_EQ(output.size(), test_case.expected.size());
  for (wtf_size_t i = 0; i < output.size(); ++i) {
    EXPECT_EQ(output[i], test_case.expected[i]);
  }
}

TEST_P(SplitOnWhitespaceTest, UTF16SplitOnASCIIWhitespace) {
  const SplitOnWhitespaceTestCase test_case = GetParam();
  String input8 = test_case.input;
  String input16 = String::Make16BitFrom8BitSource(input8.Span8());
  Vector<String> output = SplitOnASCIIWhitespace(input16);
  EXPECT_EQ(output.size(), test_case.expected.size());
  for (wtf_size_t i = 0; i < output.size(); ++i) {
    String output8 = test_case.expected[i];
    String output16 = String::Make16BitFrom8BitSource(output8.Span8());
    EXPECT_EQ(output[i], output16);
  }
}

}  // namespace

}  // namespace blink
