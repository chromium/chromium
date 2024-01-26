// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/caching_word_shaper.h"

#include <memory>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/shaping/caching_word_shape_iterator.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_test_info.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"

namespace blink {

class CachingWordShaperTest : public FontTestBase {
 protected:
  void SetUp() override {
    font_description.SetComputedSize(12.0);
    font_description.SetLocale(LayoutLocale::Get(AtomicString("en")));
    ASSERT_EQ(USCRIPT_LATIN, font_description.GetScript());
    font_description.SetGenericFamily(FontDescription::kStandardFamily);

    cache = MakeGarbageCollected<ShapeCache>();
  }

  FontCachePurgePreventer font_cache_purge_preventer;
  FontDescription font_description;
  Persistent<ShapeCache> cache;
  unsigned start_index = 0;
  unsigned num_glyphs = 0;
  hb_script_t script = HB_SCRIPT_INVALID;
};

static inline const ShapeResultTestInfo* TestInfo(const ShapeResult* result) {
  return static_cast<const ShapeResultTestInfo*>(result);
}

TEST_F(CachingWordShaperTest, LatinLeftToRightByWord) {
  Font font(font_description);

  TextRun text_run(reinterpret_cast<const LChar*>("ABC DEF."), 8);

  const ShapeResult* result = nullptr;
  CachingWordShapeIterator iterator(cache.Get(), text_run, &font);
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
  Font font(font_description);

  const UChar kStr[] = {0x2F, 0x301, 0x2E, 0x20, 0x2E, 0x0};
  TextRun text_run(kStr, 5);

  unsigned offset = 0;
  const ShapeResult* result = nullptr;
  CachingWordShapeIterator iterator(cache.Get(), text_run, &font);
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
  Font font(font_description);

  const UChar kStr[] = {0x56FD, 0x56FD,  // CJK Unified Ideograph
                        'a',    'b',
                        0x56FD,  // CJK Unified Ideograph
                        'x',    'y',    'z',
                        0x3042,  // HIRAGANA LETTER A
                        0x56FD,  // CJK Unified Ideograph
                        0x0};
  TextRun text_run(kStr, 10);

  const ShapeResult* word_result = nullptr;
  CachingWordShapeIterator iterator(cache.Get(), text_run, &font);

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
  Font font(font_description);

  const UChar kStr[] = {'a',    'b',
                        0xFF08,  // FULLWIDTH LEFT PARENTHESIS (script=common)
                        0x56FD,  // CJK Unified Ideograph
                        0x56FD,  // CJK Unified Ideograph
                        0x56FD,  // CJK Unified Ideograph
                        0x3002,  // IDEOGRAPHIC FULL STOP (script=common)
                        0x0};
  TextRun text_run(kStr, 7);

  const ShapeResult* word_result = nullptr;
  CachingWordShapeIterator iterator(cache.Get(), text_run, &font);

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
  Font font(font_description);

  const UChar kStr[] = {
      0x304B,  // HIRAGANA LETTER KA
      0x304B,  // HIRAGANA LETTER KA
      0x3009,  // COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
      0x304B,  // HIRAGANA LETTER KA
      0x0};
  TextRun text_run(kStr, 4);

  const ShapeResult* word_result = nullptr;
  CachingWordShapeIterator iterator(cache.Get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(2u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentCJKAndNonCJKCommon) {
  Font font(font_description);

  const UChar kStr[] = {0x56FD,  // CJK Unified Ideograph
                        ' ', 0x0};
  TextRun text_run(kStr, 2);

  const ShapeResult* word_result = nullptr;
  CachingWordShapeIterator iterator(cache.Get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentEmojiSequences) {
  Font font(font_description);

  std::vector<std::string> test_strings = {
      // A family followed by a couple with heart emoji sequence,
      // the latter including a variation selector.
      "\U0001f468\u200D\U0001f469\u200D\U0001f467\u200D\U0001f466\U0001f469"
      "\u200D\u2764\uFE0F\u200D\U0001f48b\u200D\U0001f468",
      // Pirate flag
      "\U0001F3F4\u200D\u2620\uFE0F",
      // Pilot, judge sequence
      "\U0001f468\U0001f3fb\u200D\u2696\uFE0F\U0001f468\U0001f3fb\u200D\u2708"
      "\uFE0F",
      // Woman, Kiss, Man sequence
      "\U0001f469\u200D\u2764\uFE0F\u200D\U0001f48b\u200D\U0001f468",
      // Signs of horns with skin tone modifier
      "\U0001f918\U0001f3fb",
      // Man, dark skin tone, red hair
      "\U0001f468\U0001f3ff\u200D\U0001f9b0"};

  for (auto test_string : test_strings) {
    String emoji_string = String::FromUTF8(test_string);
    TextRun text_run(emoji_string);
    const ShapeResult* word_result = nullptr;
    CachingWordShapeIterator iterator(cache.Get(), text_run, &font);

    ASSERT_TRUE(iterator.Next(&word_result));
    EXPECT_EQ(emoji_string.length(), word_result->NumCharacters())
        << " Length mismatch for sequence: " << test_string;

    ASSERT_FALSE(iterator.Next(&word_result));
  }
}

TEST_F(CachingWordShaperTest, SegmentEmojiExtraZWJPrefix) {
  Font font(font_description);

  // A ZWJ, followed by a family and a heart-kiss sequence.
  const UChar kStr[] = {0x200D, 0xD83D, 0xDC68, 0x200D, 0xD83D, 0xDC69,
                        0x200D, 0xD83D, 0xDC67, 0x200D, 0xD83D, 0xDC66,
                        0xD83D, 0xDC69, 0x200D, 0x2764, 0xFE0F, 0x200D,
                        0xD83D, 0xDC8B, 0x200D, 0xD83D, 0xDC68, 0x0};
  TextRun text_run(kStr, 23);

  const ShapeResult* word_result = nullptr;
  CachingWordShapeIterator iterator(cache.Get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(22u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentEmojiSubdivisionFlags) {
  Font font(font_description);

  // Subdivision flags for Wales, Scotland, England.
  const UChar kStr[] = {0xD83C, 0xDFF4, 0xDB40, 0xDC67, 0xDB40, 0xDC62, 0xDB40,
                        0xDC77, 0xDB40, 0xDC6C, 0xDB40, 0xDC73, 0xDB40, 0xDC7F,
                        0xD83C, 0xDFF4, 0xDB40, 0xDC67, 0xDB40, 0xDC62, 0xDB40,
                        0xDC73, 0xDB40, 0xDC63, 0xDB40, 0xDC74, 0xDB40, 0xDC7F,
                        0xD83C, 0xDFF4, 0xDB40, 0xDC67, 0xDB40, 0xDC62, 0xDB40,
                        0xDC65, 0xDB40, 0xDC6E, 0xDB40, 0xDC67, 0xDB40, 0xDC7F};
  TextRun text_run(kStr, std::size(kStr));

  const ShapeResult* word_result = nullptr;
  CachingWordShapeIterator iterator(cache.Get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(42u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentCJKCommon) {
  Font font(font_description);

  const UChar kStr[] = {0xFF08,  // FULLWIDTH LEFT PARENTHESIS (script=common)
                        0xFF08,  // FULLWIDTH LEFT PARENTHESIS (script=common)
                        0xFF08,  // FULLWIDTH LEFT PARENTHESIS (script=common)
                        0x0};
  TextRun text_run(kStr, 3);

  const ShapeResult* word_result = nullptr;
  CachingWordShapeIterator iterator(cache.Get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(3u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentCJKCommonAndNonCJK) {
  Font font(font_description);

  const UChar kStr[] = {0xFF08,  // FULLWIDTH LEFT PARENTHESIS (script=common)
                        'a', 'b', 0x0};
  TextRun text_run(kStr, 3);

  const ShapeResult* word_result = nullptr;
  CachingWordShapeIterator iterator(cache.Get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(1u, word_result->NumCharacters());

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(2u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentCJKSmallFormVariants) {
  Font font(font_description);

  const UChar kStr[] = {0x5916,  // CJK UNIFIED IDEOGRPAH
                        0xFE50,  // SMALL COMMA
                        0x0};
  TextRun text_run(kStr, 2);

  const ShapeResult* word_result = nullptr;
  CachingWordShapeIterator iterator(cache.Get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(2u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, SegmentHangulToneMark) {
  Font font(font_description);

  const UChar kStr[] = {0xC740,  // HANGUL SYLLABLE EUN
                        0x302E,  // HANGUL SINGLE DOT TONE MARK
                        0x0};
  TextRun text_run(kStr, 2);

  const ShapeResult* word_result = nullptr;
  CachingWordShapeIterator iterator(cache.Get(), text_run, &font);

  ASSERT_TRUE(iterator.Next(&word_result));
  EXPECT_EQ(2u, word_result->NumCharacters());

  ASSERT_FALSE(iterator.Next(&word_result));
}

TEST_F(CachingWordShaperTest, GlyphBoundsWithSpaces) {
  Font font(font_description);
  CachingWordShaper shaper(font);

  TextRun periods(reinterpret_cast<const LChar*>(".........."), 10);
  gfx::RectF periods_glyph_bounds;
  float periods_width = shaper.Width(periods, &periods_glyph_bounds);

  TextRun periods_and_spaces(
      reinterpret_cast<const LChar*>(". . . . . . . . . ."), 19);
  gfx::RectF periods_and_spaces_glyph_bounds;
  float periods_and_spaces_width =
      shaper.Width(periods_and_spaces, &periods_and_spaces_glyph_bounds);

  // The total width of periods and spaces should be longer than the width of
  // periods alone.
  ASSERT_GT(periods_and_spaces_width, periods_width);

  // The glyph bounds of periods and spaces should be longer than the glyph
  // bounds of periods alone.
  ASSERT_GT(periods_and_spaces_glyph_bounds.width(),
            periods_glyph_bounds.width());
}

}  // namespace blink
