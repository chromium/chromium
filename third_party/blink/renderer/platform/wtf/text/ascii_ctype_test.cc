// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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
