// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static CSSParserToken IdentToken(const String& string) {
  return CSSParserToken(kIdentToken, string);
}
static CSSParserToken DimensionToken(double value, const String& unit) {
  CSSParserToken token(kNumberToken, value, kNumberValueType, kNoSign);
  token.ConvertToDimensionWithUnit(unit);
  return token;
}

TEST(CSSParserTokenTest, IdentTokenEquality) {
  String foo8_bit("foo");
  String bar8_bit("bar");
  String foo16_bit = String::Make16BitFrom8BitSource(foo8_bit.Characters8(),
                                                     foo8_bit.length());

  EXPECT_EQ(IdentToken(foo8_bit), IdentToken(foo16_bit));
  EXPECT_EQ(IdentToken(foo16_bit), IdentToken(foo8_bit));
  EXPECT_EQ(IdentToken(foo16_bit), IdentToken(foo16_bit));
  EXPECT_NE(IdentToken(bar8_bit), IdentToken(foo8_bit));
  EXPECT_NE(IdentToken(bar8_bit), IdentToken(foo16_bit));
}

TEST(CSSParserTokenTest, DimensionTokenEquality) {
  String em8_bit("em");
  String rem8_bit("rem");
  String em16_bit =
      String::Make16BitFrom8BitSource(em8_bit.Characters8(), em8_bit.length());

  EXPECT_EQ(DimensionToken(1, em8_bit), DimensionToken(1, em16_bit));
  EXPECT_EQ(DimensionToken(1, em8_bit), DimensionToken(1, em8_bit));
  EXPECT_NE(DimensionToken(1, em8_bit), DimensionToken(1, rem8_bit));
  EXPECT_NE(DimensionToken(2, em8_bit), DimensionToken(1, em16_bit));
}

}  // namespace blink
