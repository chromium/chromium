// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/literal_buffer.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

TEST(LiteralBufferTest, Empty) {
  LCharLiteralBuffer<16> buf;
  EXPECT_TRUE(buf.IsEmpty());
  EXPECT_EQ(0ul, buf.size());
}

TEST(LiteralBufferTest, AddAndClear) {
  LCharLiteralBuffer<16> buf;
  buf.AddChar('a');
  buf.AddChar('b');
  buf.AddChar('c');
  EXPECT_FALSE(buf.IsEmpty());
  EXPECT_EQ(3ul, buf.size());
  EXPECT_EQ(buf[0], 'a');
  EXPECT_EQ(buf[1], 'b');
  EXPECT_EQ(buf[2], 'c');

  buf.clear();
  EXPECT_TRUE(buf.IsEmpty());
  EXPECT_EQ(0ul, buf.size());
}

TEST(LiteralBufferTest, AppendLiteral) {
  LCharLiteralBuffer<16> lit;
  lit.AddChar('a');
  lit.AddChar('b');
  lit.AddChar('c');

  UCharLiteralBuffer<4> buf;
  buf.AddChar('d');
  buf.AddChar('e');
  buf.AddChar('f');

  buf.AppendLiteral(lit);

  EXPECT_EQ(6ul, buf.size());
  EXPECT_EQ(memcmp(buf.data(), u"defabc", buf.size()), 0);
}

TEST(LiteralBufferTest, Copy) {
  LCharLiteralBuffer<16> lit;
  lit.AddChar('a');
  lit.AddChar('b');
  lit.AddChar('c');

  LCharLiteralBuffer<2> buf;
  buf = lit;

  EXPECT_FALSE(buf.IsEmpty());
  EXPECT_EQ(3ul, buf.size());
  EXPECT_EQ(buf[0], 'a');
  EXPECT_EQ(buf[1], 'b');
  EXPECT_EQ(buf[2], 'c');

  EXPECT_NE(lit.data(), buf.data());
  EXPECT_EQ(lit.size(), buf.size());

  EXPECT_FALSE(lit.IsEmpty());
  EXPECT_EQ(lit[0], 'a');
  EXPECT_EQ(lit[1], 'b');
  EXPECT_EQ(lit[2], 'c');
}

TEST(LiteralBufferTest, Move) {
  LCharLiteralBuffer<2> lit;
  lit.AddChar('a');
  lit.AddChar('b');
  lit.AddChar('c');

  LCharLiteralBuffer<2> buf(std::move(lit));

  EXPECT_FALSE(buf.IsEmpty());
  EXPECT_EQ(3ul, buf.size());
  EXPECT_EQ(buf[0], 'a');
  EXPECT_EQ(buf[1], 'b');
  EXPECT_EQ(buf[2], 'c');
}

TEST(LiteralBufferTest, Is8BitAppend) {
  UCharLiteralBuffer<16> buf;
  EXPECT_TRUE(buf.Is8Bit());
  buf.AddChar('a');
  EXPECT_TRUE(buf.Is8Bit());
  buf.AddChar(U'\x01D6');
  EXPECT_FALSE(buf.Is8Bit());
  buf.clear();
  EXPECT_TRUE(buf.Is8Bit());
}

TEST(LiteralBufferTest, Is8BitMove) {
  UCharLiteralBuffer<16> buf;
  buf.AddChar(U'\x01D6');

  UCharLiteralBuffer<16> buf2(std::move(buf));
  EXPECT_FALSE(buf2.Is8Bit());
}

TEST(LiteralBufferTest, UCharAppendSpan) {
  UCharLiteralBuffer<16> buf;
  String string8("abc");
  buf.Append(string8);
  EXPECT_EQ(string8, buf.AsString());

  String string16 = u"\x01D6";
  ASSERT_FALSE(string16.Is8Bit());
  buf.clear();
  buf.Append(string16);
  EXPECT_EQ(string16, buf.AsString());
}

TEST(LiteralBufferTest, LCharAppendSpan) {
  LCharLiteralBuffer<16> buf;
  String string8("abc");
  buf.Append(string8.Span8());
  EXPECT_EQ(string8, buf.AsString());
}

TEST(LiteralBufferTest, AsString) {
  LCharLiteralBuffer<16> buf;
  buf.AddChar('x');
  const String as_string = buf.AsString();
  EXPECT_TRUE(as_string.Is8Bit());
  EXPECT_EQ("x", as_string);
}

TEST(LiteralBufferTest, AsStringIs8Bit) {
  LCharLiteralBuffer<2> lit;
  lit.AddChar('a');
  lit.AddChar('b');
  EXPECT_TRUE(lit.AsString().Is8Bit());
}

}  // anonymous namespace
}  // namespace blink
