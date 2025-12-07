// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ASCIICTypeTest, ASCIICaseFoldTable) {
  LChar symbol = 0xff;
  while (symbol--) {
    EXPECT_EQ(ToASCIILower<LChar>(symbol), kASCIICaseFoldTable[symbol]);
  }
}

TEST(ASCIICTypeTest, IsASCIISpaceWHATWG) {
  char c = 0xFF;
  do {
    bool expected_whitespace =
        c == 0x9 || c == 0xA || c == 0xC || c == 0xD || c == 0x20;
    EXPECT_EQ(IsASCIISpaceWHATWG(c), expected_whitespace);
  } while (c--);
}

}  // namespace blink
