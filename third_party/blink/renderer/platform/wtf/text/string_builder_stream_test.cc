// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/string_builder_stream.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

void ExpectBuilderContent(const StringView& expected,
                          const StringBuilder& builder) {
  // Not using builder.toString() because it changes internal state of builder.
  if (builder.Is8Bit()) {
    EXPECT_EQ(expected, String(builder.Span8()));
  } else {
    EXPECT_EQ(expected, String(builder.Span16()));
  }
}

}  // namespace

TEST(StringBuilderStreamTest, CharacterOverloads) {
  {
    StringBuilder builder;
    ExpectBuilderContent("a ", builder << 'a' << ' ');
  }
  {
    StringBuilder builder;
    ExpectBuilderContent("A", builder << static_cast<UChar>(65));
  }
}

TEST(StringBuilderStreamTest, StringOverloads) {
  StringBuilder builder;
  ExpectBuilderContent("abcdefghi",
                       builder << "abc" << String("def") << std::string("ghi"));
}

TEST(StringBuilderStreamTest, NumberOverloads) {
  {
    StringBuilder builder;
    ExpectBuilderContent("123", builder << 123);
  }
  {
    StringBuilder builder;
    ExpectBuilderContent("45.6", builder << 45.6f);
  }
  {
    StringBuilder builder;
    ExpectBuilderContent("-1", builder << -1);
  }
  {
    StringBuilder builder;
    ExpectBuilderContent("65535", builder << static_cast<uint16_t>(65535));
  }
  {
    // LChar (uint8_t) and UChar32 (int32_t) are treated as numbers.
    StringBuilder builder;
    ExpectBuilderContent("65999", builder << static_cast<LChar>(65)
                                          << static_cast<UChar32>(999));
  }
}

TEST(StringBuilderStreamTest, VectorOverload) {
  Vector<int> vector = {1, 2, 3};
  StringBuilder builder;
  // `int` is `UChar32`, so AppendNumber() is not used.
  // Maybe this is not an expected behavior.
  ExpectBuilderContent(u"[\u0001, \u0002, \u0003]", builder << vector);

  Vector<char> char_vector = {'a', 'b', 'c'};
  Vector<UChar> uchar_vector = {0x3000, 0xFFFF};
  StringBuilder builder2;
  ExpectBuilderContent(u"[a, b, c][\u3000, \uFFFF]",
                       builder2 << char_vector << uchar_vector);
}

}  // namespace blink
