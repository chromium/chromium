// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/string_builder_stream.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(StringBuilderStreamTest, CharacterOverloads) {
  {
    StringBuilder builder;
    EXPECT_EQ("a ", StringView(builder << 'a' << ' '));
  }
  {
    StringBuilder builder;
    EXPECT_EQ("A", StringView(builder << static_cast<UChar>(65)));
  }
}

TEST(StringBuilderStreamTest, StringOverloads) {
  StringBuilder builder;
  EXPECT_EQ("abcdefghi", StringView(builder << "abc" << String("def")
                                            << std::string("ghi")));
}

TEST(StringBuilderStreamTest, NumberOverloads) {
  {
    StringBuilder builder;
    EXPECT_EQ("123", StringView(builder << 123));
  }
  {
    StringBuilder builder;
    EXPECT_EQ("45.6", StringView(builder << 45.6f));
  }
  {
    StringBuilder builder;
    EXPECT_EQ("-1", StringView(builder << -1));
  }
  {
    StringBuilder builder;
    EXPECT_EQ("65535", StringView(builder << static_cast<uint16_t>(65535)));
  }
  {
    // LChar (uint8_t) and UChar32 (int32_t) are treated as numbers.
    StringBuilder builder;
    EXPECT_EQ("65999", StringView(builder << static_cast<LChar>(65)
                                          << static_cast<UChar32>(999)));
  }
}

TEST(StringBuilderStreamTest, VectorOverload) {
  Vector<int> vector = {1, 2, 3};
  StringBuilder builder;
  // `int` is `UChar32`, so AppendNumber() is not used.
  // Maybe this is not an expected behavior.
  EXPECT_EQ(u"[\u0001, \u0002, \u0003]", StringView(builder << vector));

  Vector<char> char_vector = {'a', 'b', 'c'};
  Vector<UChar> uchar_vector = {0x3000, 0xFFFF};
  StringBuilder builder2;
  EXPECT_EQ(u"[a, b, c][\u3000, \uFFFF]",
            StringView(builder2 << char_vector << uchar_vector));
}

}  // namespace blink
