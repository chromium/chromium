// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/bidi_paragraph.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/text/character.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using testing::ElementsAre;

TEST(BidiParagraph, SetParagraphHeuristicLtr) {
  String text(u"abc");
  BidiParagraph bidi;
  bidi.SetParagraph(text, std::nullopt);
  EXPECT_EQ(bidi.BaseDirection(), TextDirection::kLtr);
}

TEST(BidiParagraph, SetParagraphHeuristicRtl) {
  String text(u"\u05D0\u05D1\u05D2");
  BidiParagraph bidi;
  bidi.SetParagraph(text, std::nullopt);
  EXPECT_EQ(bidi.BaseDirection(), TextDirection::kRtl);
}

TEST(BidiParagraph, GetLogicalRuns) {
  String text(u"\u05D0\u05D1\u05D2 abc \u05D3\u05D4\u05D5");
  BidiParagraph bidi;
  bidi.SetParagraph(text, TextDirection::kRtl);
  BidiParagraph::Runs runs;
  bidi.GetLogicalRuns(text, &runs);
  EXPECT_THAT(runs, ElementsAre(BidiParagraph::Run(0, 4, 1),
                                BidiParagraph::Run(4, 7, 2),
                                BidiParagraph::Run(7, 11, 1)));
}

static struct BaseDirectionData {
  const UChar* text;
  std::optional<TextDirection> direction_line_feed;
  std::optional<TextDirection> direction_no_stop;
} base_direction_data[] = {
    {u"A", TextDirection::kLtr},
    {u"\u05D0", TextDirection::kRtl},
    // "!" is a neutral character in the ASCII range.
    {u"!", std::nullopt},
    // Surrogate pair, Avestan is RTL. crbug.com/488904.
    {u"\U00010B15", TextDirection::kRtl},
    // Surrogate pair, Emoji is neutral. crbug.com/559932.
    {u"\U0001F62D", std::nullopt},
    // Leading neutral characters should be ignored.
    {u"!A", TextDirection::kLtr},
    {u"!A\u05D0", TextDirection::kLtr},
    {u"!\u05D0Z", TextDirection::kRtl},
    // Strong characters after a segment break should be ignored.
    {u"!\nA", std::nullopt, TextDirection::kLtr},
    {u"!\nA\u05D0", std::nullopt, TextDirection::kLtr},
    {u"!\n\u05D0Z", std::nullopt, TextDirection::kRtl}};
class BaseDirectionTest : public testing::TestWithParam<BaseDirectionData> {};
INSTANTIATE_TEST_SUITE_P(BidiParagraph,
                         BaseDirectionTest,
                         testing::ValuesIn(base_direction_data));

TEST_P(BaseDirectionTest, Data) {
  const BaseDirectionData& test = GetParam();
  String text(test.text);

  // Test when the search stops at Line Feed.
  EXPECT_EQ(BidiParagraph::BaseDirectionForString(text, Character::IsLineFeed),
            test.direction_line_feed)
      << text;

  // Test without stop characters.
  EXPECT_EQ(BidiParagraph::BaseDirectionForString(text),
            test.direction_no_stop ? test.direction_no_stop
                                   : test.direction_line_feed)
      << text;

  // Test the 8 bits code path if all characters are 8 bits.
  if (text.IsAllSpecialCharacters<[](UChar ch) { return ch <= 0x00FF; }>()) {
    String text8 = String::Make8BitFrom16BitSource(text.Span16());

    // Test when the search stops at Line Feed.
    EXPECT_EQ(
        BidiParagraph::BaseDirectionForString(text8, Character::IsLineFeed),
        test.direction_line_feed)
        << text;

    // Test without stop characters.
    EXPECT_EQ(BidiParagraph::BaseDirectionForString(text8),
              test.direction_no_stop ? test.direction_no_stop
                                     : test.direction_line_feed)
        << text;
  }
}

}  // namespace blink
