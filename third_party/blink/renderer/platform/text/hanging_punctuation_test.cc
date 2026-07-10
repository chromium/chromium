// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/hanging_punctuation.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(HangingPunctuationTest, IsHangingPunctuation) {
  // kFirst: opening bracket, quote, or ideographic space (Ps, Pf, Pi + ASCII
  // quotes + U+3000)
  EXPECT_TRUE(IsHangingPunctuation(0x0028, HangingPunctuation::kFirst));  // (
  EXPECT_TRUE(IsHangingPunctuation(0x005B, HangingPunctuation::kFirst));  // [
  EXPECT_TRUE(IsHangingPunctuation(0x007B, HangingPunctuation::kFirst));  // {
  EXPECT_TRUE(IsHangingPunctuation(0x2018, HangingPunctuation::kFirst));  // ‘
  EXPECT_TRUE(IsHangingPunctuation(0x201C, HangingPunctuation::kFirst));  // “
  EXPECT_TRUE(IsHangingPunctuation(0x0027, HangingPunctuation::kFirst));  // '
  EXPECT_TRUE(IsHangingPunctuation(0x0022, HangingPunctuation::kFirst));  // "
  EXPECT_TRUE(IsHangingPunctuation(
      0x3000, HangingPunctuation::kFirst));  // IDEOGRAPHIC SPACE
  EXPECT_FALSE(IsHangingPunctuation('A', HangingPunctuation::kFirst));
  EXPECT_FALSE(IsHangingPunctuation('.', HangingPunctuation::kFirst));
  EXPECT_FALSE(IsHangingPunctuation(0x0029, HangingPunctuation::kFirst));  // )

  // kLast: closing bracket or quote (Pe, Pf, Pi + ASCII quotes)
  EXPECT_TRUE(IsHangingPunctuation(0x0029, HangingPunctuation::kLast));  // )
  EXPECT_TRUE(IsHangingPunctuation(0x005D, HangingPunctuation::kLast));  // ]
  EXPECT_TRUE(IsHangingPunctuation(0x007D, HangingPunctuation::kLast));  // }
  EXPECT_TRUE(IsHangingPunctuation(0x2019, HangingPunctuation::kLast));  // ’
  EXPECT_TRUE(IsHangingPunctuation(0x201D, HangingPunctuation::kLast));  // ”
  EXPECT_TRUE(IsHangingPunctuation(0x0027, HangingPunctuation::kLast));  // '
  EXPECT_TRUE(IsHangingPunctuation(0x0022, HangingPunctuation::kLast));  // "
  EXPECT_FALSE(IsHangingPunctuation(
      0x3000, HangingPunctuation::kLast));  // IDEOGRAPHIC SPACE
  EXPECT_FALSE(IsHangingPunctuation('A', HangingPunctuation::kLast));
  EXPECT_FALSE(IsHangingPunctuation('.', HangingPunctuation::kLast));
  EXPECT_FALSE(IsHangingPunctuation(0x0028, HangingPunctuation::kLast));  // (

  // kAllowEnd: specific stops and commas
  EXPECT_TRUE(
      IsHangingPunctuation(0x002C, HangingPunctuation::kAllowEnd));  // ,
  EXPECT_TRUE(
      IsHangingPunctuation(0x002E, HangingPunctuation::kAllowEnd));  // .
  EXPECT_TRUE(
      IsHangingPunctuation(0x060C, HangingPunctuation::kAllowEnd));  // ،
  EXPECT_TRUE(
      IsHangingPunctuation(0x06D4, HangingPunctuation::kAllowEnd));  // ۔
  EXPECT_TRUE(
      IsHangingPunctuation(0x3001, HangingPunctuation::kAllowEnd));  // 、
  EXPECT_TRUE(
      IsHangingPunctuation(0x3002, HangingPunctuation::kAllowEnd));  // 。
  EXPECT_TRUE(
      IsHangingPunctuation(0xFF0C, HangingPunctuation::kAllowEnd));  // ，
  EXPECT_TRUE(
      IsHangingPunctuation(0xFF0E, HangingPunctuation::kAllowEnd));  // ．
  EXPECT_TRUE(
      IsHangingPunctuation(0xFE50, HangingPunctuation::kAllowEnd));  // ﹐
  EXPECT_TRUE(
      IsHangingPunctuation(0xFE51, HangingPunctuation::kAllowEnd));  // ﹑
  EXPECT_TRUE(
      IsHangingPunctuation(0xFE52, HangingPunctuation::kAllowEnd));  // ﹒
  EXPECT_TRUE(
      IsHangingPunctuation(0xFF61, HangingPunctuation::kAllowEnd));  // ｡
  EXPECT_TRUE(
      IsHangingPunctuation(0xFF64, HangingPunctuation::kAllowEnd));  // ､
  // Non-hanging punctuation
  EXPECT_FALSE(IsHangingPunctuation('A', HangingPunctuation::kAllowEnd));
  EXPECT_FALSE(IsHangingPunctuation('(', HangingPunctuation::kAllowEnd));
  EXPECT_FALSE(
      IsHangingPunctuation(0x2010, HangingPunctuation::kAllowEnd));  // hyphen
}

}  // namespace blink
