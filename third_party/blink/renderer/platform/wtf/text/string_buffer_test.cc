// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(StringBufferTest, Initial) {
  StringBuffer<LChar> buf1;
  EXPECT_EQ(0u, buf1.length());
  EXPECT_FALSE(buf1.Characters());

  StringBuffer<LChar> buf2(0);
  EXPECT_EQ(0u, buf2.length());
  EXPECT_FALSE(buf2.Characters());

  StringBuffer<LChar> buf3(1);
  EXPECT_EQ(1u, buf3.length());
  EXPECT_TRUE(buf3.Characters());
}

TEST(StringBufferTest, shrink) {
  StringBuffer<LChar> buf(2);
  EXPECT_EQ(2u, buf.length());
  buf[0] = 'a';
  buf[1] = 'b';

  buf.Shrink(1);
  EXPECT_EQ(1u, buf.length());
  EXPECT_EQ('a', buf[0]);

  buf.Shrink(0);
  EXPECT_EQ(0u, buf.length());
}

}  // namespace WTF
