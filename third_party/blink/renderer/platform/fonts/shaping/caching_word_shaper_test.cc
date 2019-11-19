// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/caching_word_shaper.h"

#include <memory>

#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/caching_word_shape_iterator.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_test_info.h"

namespace blink {

class CachingWordShaperTest : public testing::Test {
 protected:
  void SetUp() override {
    font_description.SetComputedSize(12.0);
    font_description.SetLocale(LayoutLocale::Get("en"));
    ASSERT_EQ(USCRIPT_LATIN, font_description.GetScript());
    font_description.SetGenericFamily(FontDescription::kStandardFamily);

    font = Font(font_description);
    font.Update(nullptr);
    ASSERT_TRUE(font.CanShapeWordByWord());
    fallback_fonts = nullptr;
    cache = std::make_unique<ShapeCache>();
  }

  base::test::TaskEnvironment task_environment_;
  FontCachePurgePreventer font_cache_purge_preventer;
  FontDescription font_description;
  Font font;
  std::unique_ptr<ShapeCache> cache;
  HashSet<const SimpleFontData*>* fallback_fonts;
  unsigned start_index = 0;
  unsigned num_glyphs = 0;
  hb_script_t script = HB_SCRIPT_INVALID;
};

static inline const ShapeResultTestInfo* TestInfo(
    scoped_refptr<const ShapeResult>& result) {
  return static_cast<const ShapeResultTestInfo*>(result.get());
}

TEST_F(CachingWordShaperTest, LatinLeftToRightByWord) {
  TextRun text_run(reinterpret_cast<const LChar*>("ABC DEF."), 8);

  scoped_refptr<const ShapeResult> result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);
  ASSERT_TRUE(iterator.Next(&result));
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, start_index);
  EXPECT_EQ(3u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);

  ASSERT_TRUE(iterator.Next(&result));
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, start_index);
  EXPECT_EQ(1u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_COMMON, script);

  ASSERT_TRUE(iterator.Next(&result));
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, start_index);
  EXPECT_EQ(4u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);

  ASSERT_FALSE(iterator.Next(&result));
}

TEST_F(CachingWordShaperTest, CommonAccentLeftToRightByWord) {
  const UChar kStr[] = {0x2F, 0x301, 0x2E, 0x20, 0x2E, 0x0};
  TextRun text_run(kStr, 5);

  unsigned offset = 0;
  scoped_refptr<const ShapeResult> result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);
  ASSERT_TRUE(iterator.Next(&result));
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, offset + start_index);
  EXPECT_EQ(3u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_COMMON, script);
  offset += result->NumCharacters();

  ASSERT_TRUE(iterator.Next(&result));
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(3u, offset + start_index);
  EXPECT_EQ(1u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_COMMON, script);
  offset += result->NumCharacters();

  ASSERT_TRUE(iterator.Next(&result));
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(4u, offset + start_index);
  EXPECT_EQ(1u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_COMMON, script);
  offset += result->NumCharacters();

  ASSERT_EQ(5u, offset);
  ASSERT_FALSE(iterator.Next(&result));
}

TEST_F(CachingWordShaperTest, SegmentCJKByCharacter) {
  const UChar kStr[] = {0x56FD, 0x56FD,  // CJK Unified Ideograph
                        'a',    'b',
                        0x56FD,  // CJK Unified Ideograph
                        'x',    'y',    'z',
                        0x3042,  // HIRAGANA LETTER A
                        0x56FD,  // CJK Unified Ideograph
                        0x0};
  TextRun text_run(kStr, 10);

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());
  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(2u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(3u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());
  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentCJKAndCommon) {
  const UChar kStr[] = {'a',    'b',
                        0xFF08,  // FULLWIDTH LEFT PARENTHESIS (script=common)
                        0x56FD,  // CJK Unified Ideograph
                        0x56FD,  // CJK Unified Ideograph
                        0x56FD,  // CJK Unified Ideograph
                        0x3002,  // IDEOGRAPHIC FULL STOP (script=common)
                        0x0};
  TextRun text_run(kStr, 7);

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(2u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(2u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(2u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentCJKAndInherit) {
  const UChar kStr[] = {
      0x304B,  // HIRAGANA LETTER KA
      0x304B,  // HIRAGANA LETTER KA
      0x3009,  // COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
      0x304B,  // HIRAGANA LETTER KA
      0x0};
  TextRun text_run(kStr, 4);

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(2u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentCJKAndNonCJKCommon) {
  const UChar kStr[] = {0x56FD,  // CJK Unified Ideograph
                        ' ', 0x0};
  TextRun text_run(kStr, 2);

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentEmojiZWJCommon) {
  // A family followed by a couple with heart emoji sequence,
  // the latter including a variation selector.
  const UChar kStr[] = {0xD83D, 0xDC68, 0x200D, 0xD83D, 0xDC69, 0x200D,
                        0xD83D, 0xDC67, 0x200D, 0xD83D, 0xDC66, 0xD83D,
                        0xDC69, 0x200D, 0x2764, 0xFE0F, 0x200D, 0xD83D,
                        0xDC8B, 0x200D, 0xD83D, 0xDC68, 0x0};
  TextRun text_run(kStr, 22);

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(22u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentEmojiPilotJudgeSequence) {
  // A family followed by a couple with heart emoji sequence,
  // the latter including a variation selector.
  const UChar kStr[] = {0xD83D, 0xDC68, 0xD83C, 0xDFFB, 0x200D, 0x2696, 0xFE0F,
                        0xD83D, 0xDC68, 0xD83C, 0xDFFB, 0x200D, 0x2708, 0xFE0F};
  TextRun text_run(kStr, base::size(kStr));

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(base::size(kStr), word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentEmojiHeartZWJSequence) {
  // A ZWJ, followed by two family ZWJ Sequences.
  const UChar kStr[] = {0xD83D, 0xDC69, 0x200D, 0x2764, 0xFE0F, 0x200D,
                        0xD83D, 0xDC8B, 0x200D, 0xD83D, 0xDC68, 0x0};
  TextRun text_run(kStr, 11);

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(11u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentEmojiSignsOfHornsModifier) {
  // A Sign of the Horns emoji, followed by a fitzpatrick modifer
  const UChar kStr[] = {0xD83E, 0xDD18, 0xD83C, 0xDFFB, 0x0};
  TextRun text_run(kStr, 4);

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(4u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentEmojiExtraZWJPrefix) {
  // A ZWJ, followed by a family and a heart-kiss sequence.
  const UChar kStr[] = {0x200D, 0xD83D, 0xDC68, 0x200D, 0xD83D, 0xDC69,
                        0x200D, 0xD83D, 0xDC67, 0x200D, 0xD83D, 0xDC66,
                        0xD83D, 0xDC69, 0x200D, 0x2764, 0xFE0F, 0x200D,
                        0xD83D, 0xDC8B, 0x200D, 0xD83D, 0xDC68, 0x0};
  TextRun text_run(kStr, 23);

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(22u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentEmojiSubdivisionFlags) {
  // Subdivision flags for Wales, Scotland, England.
  const UChar kStr[] = {0xD83C, 0xDFF4, 0xDB40, 0xDC67, 0xDB40, 0xDC62, 0xDB40,
                        0xDC77, 0xDB40, 0xDC6C, 0xDB40, 0xDC73, 0xDB40, 0xDC7F,
                        0xD83C, 0xDFF4, 0xDB40, 0xDC67, 0xDB40, 0xDC62, 0xDB40,
                        0xDC73, 0xDB40, 0xDC63, 0xDB40, 0xDC74, 0xDB40, 0xDC7F,
                        0xD83C, 0xDFF4, 0xDB40, 0xDC67, 0xDB40, 0xDC62, 0xDB40,
                        0xDC65, 0xDB40, 0xDC6E, 0xDB40, 0xDC67, 0xDB40, 0xDC7F};
  TextRun text_run(kStr, base::size(kStr));

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(42u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentCJKCommon) {
  const UChar kStr[] = {0xFF08,  // FULLWIDTH LEFT PARENTHESIS (script=common)
                        0xFF08,  // FULLWIDTH LEFT PARENTHESIS (script=common)
                        0xFF08,  // FULLWIDTH LEFT PARENTHESIS (script=common)
                        0x0};
  TextRun text_run(kStr, 3);

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(3u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentCJKCommonAndNonCJK) {
  const UChar kStr[] = {0xFF08,  // FULLWIDTH LEFT PARENTHESIS (script=common)
                        'a', 'b', 0x0};
  TextRun text_run(kStr, 3);

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(2u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentCJKSmallFormVariants) {
  const UChar kStr[] = {0x5916,  // CJK UNIFIED IDEOGRPAH
                        0xFE50,  // SMALL COMMA
                        0x0};
  TextRun text_run(kStr, 2);

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(2u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentHangulToneMark) {
  const UChar kStr[] = {0xC740,  // HANGUL SYLLABLE EUN
                        0x302E,  // HANGUL SINGLE DOT TONE MARK
                        0x0};
  TextRun text_run(kStr, 2);

  scoped_refptr<const ShapeResult> word_result;
  CachingWordShapeIterator iterator(cache.get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(2u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, TextOrientationFallbackShouldNotInFallbackList) {
  const UChar kStr[] = {
      'A',  // code point for verticalRightOrientationFontData()
      // Ideally we'd like to test uprightOrientationFontData() too
      // using code point such as U+3042, but it'd fallback to system
      // fonts as the glyph is missing.
      0x0};
  TextRun text_run(kStr, 1);

  font_description.SetOrientation(FontOrientation::kVerticalMixed);
  Font vertical_mixed_font = Font(font_description);
  vertical_mixed_font.Update(nullptr);
  ASSERT_TRUE(vertical_mixed_font.CanShapeWordByWord());

  CachingWordShaper shaper(vertical_mixed_font);
  FloatRect glyph_bounds;
  HashSet<const SimpleFontData*> fallback_fonts;
  ASSERT_GT(shaper.Width(text_run, &fallback_fonts, &glyph_bounds), 0);
  EXPECT_EQ(0u, fallback_fonts.size());
}

TEST_F(CachingWordShaperTest, GlyphBoundsWithSpaces) {
  CachingWordShaper shaper(font);

  TextRun periods(reinterpret_cast<const LChar*>(".........."), 10);
  FloatRect periods_glyph_bounds;
  float periods_width = shaper.Width(periods, nullptr, &periods_glyph_bounds);

  TextRun periods_and_spaces(
      reinterpret_cast<const LChar*>(". . . . . . . . . ."), 19);
  FloatRect periods_and_spaces_glyph_bounds;
  float periods_and_spaces_width = shaper.Width(
      periods_and_spaces, nullptr, &periods_and_spaces_glyph_bounds);

  // The total width of periods and spaces should be longer than the width of
  // periods alone.
  ASSERT_GT(periods_and_spaces_width, periods_width);

  // The glyph bounds of periods and spaces should be longer than the glyph
  // bounds of periods alone.
  ASSERT_GT(periods_and_spaces_glyph_bounds.Width(),
            periods_glyph_bounds.Width());
}

}  // namespace blink
