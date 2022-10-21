// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/code_point_iterator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace WTF {

TEST(CodePointIteratorTest, Ascii) {
  StringView str{"ascii"};
  StringBuilder builder;
  size_t iteration_count = 0;
  for (UChar32 code_point : str) {
    builder.Append(code_point);
    ++iteration_count;
  }
  EXPECT_EQ(String("ascii"), builder.ToString());
  EXPECT_EQ(5u, iteration_count);

  StringView empty{""};
  EXPECT_EQ(empty.begin(), empty.end());
}

TEST(CodePointIteratorTest, Bmp) {
  StringView str{u"\u30D0\u30CA\u30CA"};
  StringBuilder builder;
  size_t iteration_count = 0;
  for (UChar32 code_point : str) {
    builder.Append(code_point);
    ++iteration_count;
  }
  EXPECT_EQ(String(u"\u30D0\u30CA\u30CA"), builder.ToString());
  EXPECT_EQ(3u, iteration_count);
}

TEST(CodePointIteratorTest, Surrogates) {
  // U+00020BB7 and a unpaired lead surrogate.
  UChar input[3] = {0xD842, 0xDFB7, 0xD800};
  StringView str{input, 3};
  Vector<UChar32> results;
  size_t iteration_count = 0;
  for (UChar32 code_point : str) {
    results.push_back(code_point);
    ++iteration_count;
  }
  EXPECT_EQ(0x20BB7, results[0]);
  EXPECT_EQ(0xD800, results[1]);
  EXPECT_EQ(2u, iteration_count);
}

TEST(CodePointIteratorTest, Equality) {
  StringView str1{"foo"};
  EXPECT_EQ(str1.begin(), str1.begin());
  EXPECT_EQ(str1.end(), str1.end());
  EXPECT_FALSE(str1.begin() == str1.end());

  StringView str2{"bar"};
  EXPECT_NE(str1.begin(), str2.begin());
  EXPECT_NE(str1.end(), str2.end());
  EXPECT_FALSE(str1.end() != str1.end());
}

}  // namespace WTF
