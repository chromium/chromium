// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "third_party/blink/renderer/platform/text/text_boundaries.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class TextBoundariesTest : public testing::Test {};

namespace {

std::pair<String, int> ParsePositionMarker(const std::string input8) {
  String input16 = String::FromUTF8(input8.data(), input8.size());
  input16.Ensure16Bit();
  const size_t position = input16.find('|');
  DCHECK(position != kNotFound) << input8 << " should have position marker(|).";
  String output = input16.Left(position) + input16.Substring(position + 1);
  output.Ensure16Bit();
  return {output, position};
}

std::string MakeResultText(const String& text, int start, int end) {
  StringBuilder builder;
  if (start < 0 && end < 0) {
    builder.Append(text);
  } else if (start < 0) {
    builder.Append(text.Left(end));
    builder.Append('^');
    builder.Append(text.Substring(end));
  } else if (end < 0) {
    builder.Append(text.Left(start));
    builder.Append('|');
    builder.Append(text.Substring(start));
  } else {
    builder.Append(text.Left(start));
    builder.Append('^');
    builder.Append(text.Substring(start, end - start));
    builder.Append('|');
  }
  builder.Append(text.Substring(end));
  return builder.ToString().Utf8();
}

// Returns word boundray with start(^) and end(|) markes from text with
// position(|) marker.
std::string TryFindWordBackward(const std::string input8) {
  std::pair<String, int> string_and_offset = ParsePositionMarker(input8);
  const String text16 = string_and_offset.first;
  const int position = string_and_offset.second;
  std::pair<int, int> start_and_end =
      FindWordBackward(text16.Characters16(), text16.length(), position);
  return MakeResultText(text16, start_and_end.first, start_and_end.second);
}

// Returns word boundray with start(^) and end(|) markes from text with
// position(|) marker.
std::string TryFindWordForward(const std::string input8) {
  std::pair<String, int> string_and_offset = ParsePositionMarker(input8);
  const String text16 = string_and_offset.first;
  const int position = string_and_offset.second;
  std::pair<int, int> start_and_end =
      FindWordForward(text16.Characters16(), text16.length(), position);
  return MakeResultText(text16, start_and_end.first, start_and_end.second);
}

}  // namespace

TEST_F(TextBoundariesTest, BackwardBasic) {
  EXPECT_EQ("^|abc def", TryFindWordBackward("|abc def"));
  EXPECT_EQ("^abc| def", TryFindWordBackward("a|bc def"));
  EXPECT_EQ("^abc| def", TryFindWordBackward("ab|c def"));
  EXPECT_EQ("^abc| def", TryFindWordBackward("abc| def"));
  EXPECT_EQ("abc^ |def", TryFindWordBackward("abc |def"));
  EXPECT_EQ("abc ^def|", TryFindWordBackward("abc d|ef"));
  EXPECT_EQ("abc ^def|", TryFindWordBackward("abc de|f"));
  EXPECT_EQ("abc ^def|", TryFindWordBackward("abc def|"));
}

TEST_F(TextBoundariesTest, ForwardBasic) {
  EXPECT_EQ("^abc| def", TryFindWordForward("|abc def"));
  EXPECT_EQ("^abc| def", TryFindWordForward("a|bc def"));
  EXPECT_EQ("^abc| def", TryFindWordForward("ab|c def"));
  EXPECT_EQ("abc^ |def", TryFindWordForward("abc| def"));
  EXPECT_EQ("abc ^def|", TryFindWordForward("abc |def"));
  EXPECT_EQ("abc ^def|", TryFindWordForward("abc d|ef"));
  EXPECT_EQ("abc ^def|", TryFindWordForward("abc de|f"));
  EXPECT_EQ("abc def^|", TryFindWordForward("abc def|"));
}

TEST_F(TextBoundariesTest, ForwardBiDi) {
  EXPECT_EQ(u8"^\u0620\u0621\u0622| \u0623\u0624\u0625",
            TryFindWordForward(u8"|\u0620\u0621\u0622 \u0623\u0624\u0625"));
  EXPECT_EQ(u8"^\u0620\u0621\u0622| \u0623\u0624\u0625",
            TryFindWordForward(u8"\u0620|\u0621\u0622 \u0623\u0624\u0625"));
  EXPECT_EQ(u8"^\u0620\u0621\u0622| \u0623\u0624\u0625",
            TryFindWordForward(u8"\u0620\u0621|\u0622 \u0623\u0624\u0625"));
  EXPECT_EQ(u8"\u0620\u0621\u0622^ |\u0623\u0624\u0625",
            TryFindWordForward(u8"\u0620\u0621\u0622| \u0623\u0624\u0625"));
  EXPECT_EQ(u8"\u0620\u0621\u0622 ^\u0623\u0624\u0625|",
            TryFindWordForward(u8"\u0620\u0621\u0622 |\u0623\u0624\u0625"));
  EXPECT_EQ(u8"\u0620\u0621\u0622 \u0623\u0624\u0625^|",
            TryFindWordForward(u8"\u0620\u0621\u0622 \u0623\u0624\u0625|"));
}

TEST_F(TextBoundariesTest, ForwardBiDiMixed) {
  EXPECT_EQ(u8"^abc\u0620\u0621\u0622|",
            TryFindWordForward(u8"|abc\u0620\u0621\u0622"));
  EXPECT_EQ(u8"^abc\u0620\u0621\u0622|",
            TryFindWordForward(u8"ab|c\u0620\u0621\u0622"));
  EXPECT_EQ(u8"^abc\u0620\u0621\u0622|",
            TryFindWordForward(u8"abc|\u0620\u0621\u0622"))
      << "At L1/L2 boundary";
  EXPECT_EQ(u8"^abc\u0620\u0621\u0622|",
            TryFindWordForward(u8"abc\u0620|\u0621\u0622"));

  EXPECT_EQ(u8"^\u0620\u0621\u0622xyz|",
            TryFindWordForward(u8"|\u0620\u0621\u0622xyz"));
  EXPECT_EQ(u8"^\u0620\u0621\u0622xyz|",
            TryFindWordForward(u8"\u0620|\u0621\u0622xyz"));
  EXPECT_EQ(u8"^\u0620\u0621\u0622xyz|",
            TryFindWordForward(u8"\u0620\u0621\u0622|xyz"))
      << "At L2/L1 boundary";
  EXPECT_EQ(u8"^\u0620\u0621\u0622xyz|",
            TryFindWordForward(u8"\u0620\u0621\u0622xy|z"));
}

TEST_F(TextBoundariesTest, ForwardOne) {
  EXPECT_EQ("^a|", TryFindWordForward("|a"));
  EXPECT_EQ("a^|", TryFindWordForward("a|")) << "No word after |";
}

TEST_F(TextBoundariesTest, ForwardParenthesis) {
  EXPECT_EQ("^(|abc)", TryFindWordForward("|(abc)"));
  EXPECT_EQ("(^abc|)", TryFindWordForward("(|abc)"));
  EXPECT_EQ("(^abc|)", TryFindWordForward("(a|bc)"));
  EXPECT_EQ("(^abc|)", TryFindWordForward("(ab|c)"));
  EXPECT_EQ("(abc)^|", TryFindWordForward("(abc)|")) << "No word after |";
}

TEST_F(TextBoundariesTest, ForwardPunctuations) {
  EXPECT_EQ("^abc|,,", TryFindWordForward("|abc,,"));
  EXPECT_EQ("abc^,|,", TryFindWordForward("abc|,,"));
}

// A sequence of whitespaces is treated as a single word.
// See http://unicode.org/reports/tr29/#WB3d .
TEST_F(TextBoundariesTest, ForwardWhitespaces) {
  EXPECT_EQ("^  |abc  def  ", TryFindWordForward("|  abc  def  "));
  EXPECT_EQ("^  |abc  def  ", TryFindWordForward(" | abc  def  "));
  EXPECT_EQ("  ^abc|  def  ", TryFindWordForward("  |abc  def  "));
  EXPECT_EQ("  ^abc|  def  ", TryFindWordForward("  a|bc  def  "));
  EXPECT_EQ("  ^abc|  def  ", TryFindWordForward("  ab|c  def  "));
  EXPECT_EQ("  abc^  |def  ", TryFindWordForward("  abc|  def  "));
  EXPECT_EQ("  abc^  |def  ", TryFindWordForward("  abc | def  "));
  EXPECT_EQ("  abc  ^def|  ", TryFindWordForward("  abc  |def  "));
  EXPECT_EQ("  abc  ^def|  ", TryFindWordForward("  abc  d|ef  "));
  EXPECT_EQ("  abc  ^def|  ", TryFindWordForward("  abc  de|f  "));
  EXPECT_EQ("  abc  def^  |", TryFindWordForward("  abc  def|  "));
  EXPECT_EQ("  abc  def^  |", TryFindWordForward("  abc  def | "));
  EXPECT_EQ("  abc  def  ^|", TryFindWordForward("  abc  def  |"))
      << "No word after |";
}

TEST_F(TextBoundariesTest, ForwardZero) {
  EXPECT_EQ("^|", TryFindWordForward("|"));
}

}  // namespace blink
