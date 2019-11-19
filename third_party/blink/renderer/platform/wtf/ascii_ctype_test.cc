// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(ASCIICTypeTest, ASCIICaseFoldTable) {
  LChar symbol = 0xff;
  while (symbol--) {
    EXPECT_EQ(ToASCIILower<LChar>(symbol), kASCIICaseFoldTable[symbol]);
  }
}

}  // namespace WTF
