// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/literal_buffer.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

TEST(LiteralBufferTest, Empty) {
  test::TaskEnvironment task_environment;
  LCharLiteralBuffer<16> buf;
  EXPECT_TRUE(buf.IsEmpty());
  EXPECT_EQ(0ul, buf.size());
}

TEST(LiteralBufferTest, AddAndClear) {
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
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
  test::TaskEnvironment task_environment;
  UCharLiteralBuffer<16> buf;
  buf.AddChar(U'\x01D6');

  UCharLiteralBuffer<16> buf2(std::move(buf));
  EXPECT_FALSE(buf2.Is8Bit());
}

TEST(LiteralBufferTest, AsString) {
  test::TaskEnvironment task_environment;
  LCharLiteralBuffer<16> buf;
  buf.AddChar('x');
  const String as_string = buf.AsString();
  EXPECT_TRUE(as_string.Is8Bit());
  EXPECT_EQ("x", as_string);
}

TEST(LiteralBufferTest, AsStringIs8Bit) {
  test::TaskEnvironment task_environment;
  LCharLiteralBuffer<2> lit;
  lit.AddChar('a');
  lit.AddChar('b');
  EXPECT_TRUE(lit.AsString().Is8Bit());
}

}  // anonymous namespace
}  // namespace blink
