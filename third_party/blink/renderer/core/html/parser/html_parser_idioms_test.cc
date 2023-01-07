// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(HTMLParserIdiomsTest, ParseHTMLInteger) {
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
  Vector<double> numbers = ParseHTMLListOfFloatingPointNumbers(g_null_atom);
  EXPECT_EQ(0u, numbers.size());
}

}  // namespace

}  // namespace blink
