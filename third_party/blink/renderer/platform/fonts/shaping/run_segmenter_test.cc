// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/run_segmenter.h"

#include <string>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/orientation_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct SegmenterTestRun {
  std::string text;
  UScriptCode script;
  OrientationIterator::RenderOrientation render_orientation;
  FontFallbackPriority font_fallback_priority;
};

struct SegmenterExpectedRun {
  wtf_size_t start;
  wtf_size_t limit;
  UScriptCode script;
  OrientationIterator::RenderOrientation render_orientation;
  FontFallbackPriority font_fallback_priority;

  SegmenterExpectedRun(
      wtf_size_t the_start,
      wtf_size_t the_limit,
      UScriptCode the_script,
      OrientationIterator::RenderOrientation the_render_orientation,
      FontFallbackPriority the_font_fallback_priority)
      : start(the_start),
        limit(the_limit),
        script(the_script),
        render_orientation(the_render_orientation),
        font_fallback_priority(the_font_fallback_priority) {}
};

class RunSegmenterTest : public testing::Test {
 protected:
  void CheckRuns(const Vector<SegmenterTestRun>& runs,
                 FontOrientation orientation) {
    StringBuilder text;
    text.Ensure16Bit();
    Vector<SegmenterExpectedRun> expect;
    for (auto& run : runs) {
      wtf_size_t length_before = text.length();
      text.Append(String::FromUtf8(run.text));
      expect.push_back(SegmenterExpectedRun(length_before, text.length(),
                                            run.script, run.render_orientation,
                                            run.font_fallback_priority));
    }
    RunSegmenter run_segmenter(text.Span16(), orientation);
    VerifyRuns(&run_segmenter, expect);
  }

  void CheckRunsMixed(const Vector<SegmenterTestRun>& runs) {
    CheckRuns(runs, FontOrientation::kVerticalMixed);
  }

  void CheckRunsHorizontal(const Vector<SegmenterTestRun>& runs) {
    CheckRuns(runs, FontOrientation::kHorizontal);
  }

  void VerifyRuns(RunSegmenter* run_segmenter,
                  const Vector<SegmenterExpectedRun>& expect) {
    RunSegmenter::RunSegmenterRange segmenter_range;
    wtf_size_t run_count = 0;
    while (run_segmenter->Consume(&segmenter_range)) {
      ASSERT_LT(run_count, expect.size());
      ASSERT_EQ(expect[run_count].start, segmenter_range.start);
      ASSERT_EQ(expect[run_count].limit, segmenter_range.end);
      ASSERT_EQ(expect[run_count].script, segmenter_range.script);
      ASSERT_EQ(expect[run_count].render_orientation,
                segmenter_range.render_orientation);
      ASSERT_EQ(expect[run_count].font_fallback_priority,
                segmenter_range.font_fallback_priority);
      ++run_count;
    }
    ASSERT_EQ(expect.size(), run_count);
  }
};

TEST_F(RunSegmenterTest, Empty) {
  String empty(g_empty_string16_bit);
  RunSegmenter::RunSegmenterRange segmenter_range = {
      0, 0, USCRIPT_INVALID_CODE, OrientationIterator::kOrientationKeep};
  RunSegmenter run_segmenter(empty.Span16(), FontOrientation::kVerticalMixed);
  DCHECK(!run_segmenter.Consume(&segmenter_range));
  ASSERT_EQ(segmenter_range.start, 0u);
  ASSERT_EQ(segmenter_range.end, 0u);
  ASSERT_EQ(segmenter_range.script, USCRIPT_INVALID_CODE);
  ASSERT_EQ(segmenter_range.render_orientation,
            OrientationIterator::kOrientationKeep);
  ASSERT_EQ(segmenter_range.font_fallback_priority,
            FontFallbackPriority::kText);
}

TEST_F(RunSegmenterTest, LatinPunctuationSideways) {
  CheckRunsMixed({{"Abc.;?Xyz", USCRIPT_LATIN,
                   OrientationIterator::kOrientationRotateSideways,
                   FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, OneSpace) {
  CheckRunsMixed(
      {{" ", USCRIPT_COMMON, OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, ArabicHangul) {
  CheckRunsMixed(
      {{"نص", USCRIPT_ARABIC, OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText},
       {"키스의", USCRIPT_HANGUL, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, JapaneseHindiEmojiMix) {
  CheckRunsMixed(
      {{"百家姓", USCRIPT_HAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"ऋषियों", USCRIPT_DEVANAGARI,
        OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText},
       {"🌱🌲🌳🌴", USCRIPT_DEVANAGARI, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kEmojiEmoji},
       {"百家姓", USCRIPT_HAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"🌱🌲", USCRIPT_HAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(RunSegmenterTest, CombiningCircle) {
#if U_ICU_VERSION_MAJOR_NUM >= 76
  const UScriptCode script = USCRIPT_LATIN;
#else
  const UScriptCode script = USCRIPT_COMMON;
#endif
  CheckRunsHorizontal({{"◌́◌̀◌̈◌̂◌̄◌̊", script, OrientationIterator::kOrientationKeep,
                        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, HangulSpace) {
  CheckRunsMixed(
      {{"키스의", USCRIPT_HANGUL, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {" ", USCRIPT_HANGUL, OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText},
       {"고유조건은", USCRIPT_HANGUL, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, TechnicalCommonUpright) {
  CheckRunsMixed({{"⌀⌁⌂", USCRIPT_COMMON, OrientationIterator::kOrientationKeep,
                   FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, PunctuationCommonSideways) {
  CheckRunsMixed(
      {{".…¡", USCRIPT_COMMON, OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, JapanesePunctuationMixedInside) {
  CheckRunsMixed(
      {{"いろはに", USCRIPT_HIRAGANA, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {".…¡", USCRIPT_HIRAGANA,
        OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText},
       {"ほへと", USCRIPT_HIRAGANA, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, JapanesePunctuationMixedInsideHorizontal) {
  CheckRunsHorizontal(
      {{"いろはに.…¡ほへと", USCRIPT_HIRAGANA,
        OrientationIterator::kOrientationKeep, FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, PunctuationDevanagariCombining) {
  CheckRunsHorizontal(
      {{"क+े", USCRIPT_DEVANAGARI, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, EmojiZWJSequences) {
  CheckRunsHorizontal(
      {{"👩‍👩‍👧‍👦👩‍❤️‍💋‍👨", USCRIPT_LATIN,
        OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kEmojiEmoji},
       {"abcd", USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"👩‍👩", USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kEmojiEmoji},
       {"\U0000200D‍efg", USCRIPT_LATIN,
        OrientationIterator::kOrientationKeep, FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, JapaneseLetterlikeEnd) {
  CheckRunsMixed(
      {{"いろは", USCRIPT_HIRAGANA, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"ℐℒℐℒℐℒℐℒℐℒℐℒℐℒ", USCRIPT_HIRAGANA,
        OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, JapaneseCase) {
  CheckRunsMixed(
      {{"いろは", USCRIPT_HIRAGANA, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"aaAA", USCRIPT_LATIN, OrientationIterator::kOrientationRotateSideways,
        FontFallbackPriority::kText},
       {"いろは", USCRIPT_HIRAGANA, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, DingbatsMiscSymbolsModifier) {
  CheckRunsHorizontal({{"⛹🏻✍🏻✊🏼", USCRIPT_COMMON,
                        OrientationIterator::kOrientationKeep,
                        FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(RunSegmenterTest, ArmenianCyrillicCase) {
  CheckRunsHorizontal(
      {{"աբգ", USCRIPT_ARMENIAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"αβγ", USCRIPT_GREEK, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"ԱԲԳ", USCRIPT_ARMENIAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, EmojiSubdivisionFlags) {
  CheckRunsHorizontal(
      {{"🏴󠁧󠁢󠁷󠁬󠁳󠁿🏴󠁧󠁢󠁳󠁣󠁴󠁿🏴󠁧󠁢"
        "󠁥󠁮󠁧󠁿",
        USCRIPT_COMMON, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kEmojiEmoji}});
}

TEST_F(RunSegmenterTest, NonEmojiPresentationSymbols) {
  CheckRunsHorizontal(
      {{"\U00002626\U0000262a\U00002638\U0000271d\U00002721\U00002627"
        "\U00002628\U00002629\U0000262b\U0000262c\U00002670"
        "\U00002671\U0000271f\U00002720",
        USCRIPT_COMMON, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, CJKBracketsAfterLatinLetter) {
  CheckRunsHorizontal(
      {{"A", USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"\u300C"   // CJK LEFT CORNER BRACKET
        "\u56FD"   // CJK UNIFIED IDEOGRAPH
        "\u300D",  // CJK RIGHT CORNER BRACKET
        USCRIPT_HAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, CJKBracketsAfterLatinParenthesis) {
  CheckRunsHorizontal(
      {{"A(", USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"\u300C"   // CJK LEFT CORNER BRACKET
        "\u56FD"   // CJK UNIFIED IDEOGRAPH
        "\u300D",  // CJK RIGHT CORNER BRACKET
        USCRIPT_HAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {")", USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, CJKBracketsWithLatinParenthesisInside) {
  CheckRunsHorizontal(
      {{"A", USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"\u300C"  // CJK LEFT CORNER BRACKET
        "\u56FD"  // CJK UNIFIED IDEOGRAPH
        "(",
        USCRIPT_HAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"A", USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {")"
        "\u300D",  // CJK RIGHT CORNER BRACKET
        USCRIPT_HAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

TEST_F(RunSegmenterTest, CJKBracketsAfterUnmatchingLatinParenthesis) {
  CheckRunsHorizontal(
      {{"A((", USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {"\u300C"   // CJK LEFT CORNER BRACKET
        "\u56FD"   // CJK UNIFIED IDEOGRAPH
        "\u300D",  // CJK RIGHT CORNER BRACKET
        USCRIPT_HAN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText},
       {")", USCRIPT_LATIN, OrientationIterator::kOrientationKeep,
        FontFallbackPriority::kText}});
}

}  // namespace blink
