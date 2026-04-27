// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(AsciiCTypeTest, AsciiCaseFoldTable) {
  LChar symbol = 0xff;
  while (symbol--) {
    EXPECT_EQ(ToAsciiLower<LChar>(symbol), kAsciiCaseFoldTable[symbol]);
  }
}

TEST(AsciiCTypeTest, IsAsciiSpaceWhatwg) {
  char c = 0xFF;
  do {
    bool expected_whitespace =
        c == 0x9 || c == 0xA || c == 0xC || c == 0xD || c == 0x20;
    EXPECT_EQ(IsAsciiSpaceWhatwg(c), expected_whitespace);
  } while (c--);
}

TEST(AaciiCTypeTest, IsAsciiHexDigit) {
  EXPECT_TRUE(IsAsciiHexDigit('0'));
  EXPECT_TRUE(IsAsciiHexDigit('9'));
  EXPECT_TRUE(IsAsciiHexDigit('a'));
  EXPECT_TRUE(IsAsciiHexDigit('f'));
  EXPECT_TRUE(IsAsciiHexDigit('A'));
  EXPECT_TRUE(IsAsciiHexDigit('F'));
  EXPECT_FALSE(IsAsciiHexDigit('g'));
  EXPECT_FALSE(IsAsciiHexDigit('G'));
  EXPECT_FALSE(IsAsciiHexDigit('/'));
  EXPECT_FALSE(IsAsciiHexDigit(':'));
  EXPECT_FALSE(IsAsciiHexDigit(0xFF));
}

TEST(AsciiCTypeTest, ToAsciiHexValue) {
  EXPECT_EQ(0, ToAsciiHexValue('0'));
  EXPECT_EQ(9, ToAsciiHexValue('9'));
  EXPECT_EQ(10, ToAsciiHexValue('a'));
  EXPECT_EQ(15, ToAsciiHexValue('f'));
  EXPECT_EQ(10, ToAsciiHexValue('A'));
  EXPECT_EQ(15, ToAsciiHexValue('F'));

  EXPECT_EQ(0x00, ToAsciiHexValue('0', '0'));
  EXPECT_EQ(0xFF, ToAsciiHexValue('F', 'F'));
  EXPECT_EQ(0xAB, ToAsciiHexValue('a', 'B'));
}

TEST(AsciiCTypeTest, NibbleToAsciiHexDigit) {
  EXPECT_EQ('0', LowerNibbleToAsciiHexDigit(0x00));
  EXPECT_EQ('F', LowerNibbleToAsciiHexDigit(0x0F));
  EXPECT_EQ('A', LowerNibbleToAsciiHexDigit(0x0A));

  EXPECT_EQ('0', UpperNibbleToAsciiHexDigit(0x00));
  EXPECT_EQ('F', UpperNibbleToAsciiHexDigit(0xF0));
  EXPECT_EQ('A', UpperNibbleToAsciiHexDigit(0xA0));
}

}  // namespace blink
