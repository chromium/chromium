// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/literal_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(LiteralBufferTest, Empty) {
  LiteralBuffer<LChar, 16> buf;
  EXPECT_TRUE(buf.IsEmpty());
  EXPECT_EQ(0ul, buf.size());
}

TEST(LiteralBufferTest, AddAndClear) {
  LiteralBuffer<LChar, 16> buf;
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
  LiteralBuffer<LChar, 16> lit;
  lit.AddChar('a');
  lit.AddChar('b');
  lit.AddChar('c');

  LiteralBuffer<UChar, 4> buf;
  buf.AddChar('d');
  buf.AddChar('e');
  buf.AddChar('f');

  buf.AppendLiteral(lit);

  EXPECT_EQ(6ul, buf.size());
  EXPECT_EQ(memcmp(buf.data(), u"defabc", buf.size()), 0);
}

TEST(LiteralBufferTest, Copy) {
  LiteralBuffer<LChar, 16> lit;
  lit.AddChar('a');
  lit.AddChar('b');
  lit.AddChar('c');

  LiteralBuffer<LChar, 2> buf;
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
  LiteralBuffer<LChar, 2> lit;
  lit.AddChar('a');
  lit.AddChar('b');
  lit.AddChar('c');

  LiteralBuffer<LChar, 2> buf(std::move(lit));

  EXPECT_FALSE(buf.IsEmpty());
  EXPECT_EQ(3ul, buf.size());
  EXPECT_EQ(buf[0], 'a');
  EXPECT_EQ(buf[1], 'b');
  EXPECT_EQ(buf[2], 'c');

  EXPECT_TRUE(lit.IsEmpty());
  EXPECT_EQ(0ul, lit.size());
}

}  // anonymous namespace

}  // namespace blink
