// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shaping_line_breaker.h"

#include <unicode/uscript.h>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_test_utilities.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_test_info.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"

namespace blink {

class ShapeResultViewTest : public FontTestBase {
 protected:
  void SetUp() override {
    font_description.SetComputedSize(12.0);
    font = Font(font_description);
  }

  void TearDown() override {}

  FontCachePurgePreventer font_cache_purge_preventer;
  FontDescription font_description;
  Font font;
};

// http://crbug.com/1221008
TEST_F(ShapeResultViewTest,
       ExpandRangeToIncludePartialGlyphsWithCombiningCharacter) {
  String string(u"abc\u0E35\u0E35\u0E35\u0E35");
  HarfBuzzShaper shaper(string);
  scoped_refptr<const ShapeResult> result =
      shaper.Shape(&font, TextDirection::kLtr);
  scoped_refptr<const ShapeResultView> view = ShapeResultView::Create(
      result.get(), result->StartIndex(), result->EndIndex());
  unsigned from = 0;
  unsigned end = string.length();
  view->ExpandRangeToIncludePartialGlyphs(&from, &end);
  EXPECT_EQ(0u, from);
  EXPECT_EQ(string.length(), end);
}

TEST_F(ShapeResultViewTest, LatinSingleView) {
  String string =
      To16Bit("Test run with multiple words and breaking opportunities.", 56);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  scoped_refptr<const ShapeResult> result = shaper.Shape(&font, direction);
  Vector<ShapeResultTestGlyphInfo> glyphs;
  result->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&glyphs));

  // Test view at the start of the result: "Test run with multiple"
  ShapeResultView::Segment segments[] = {{result.get(), 0, 22}};
  auto first4 = ShapeResultView::Create(segments);

  EXPECT_EQ(first4->StartIndex(), 0u);
  EXPECT_EQ(first4->NumCharacters(), 22u);
  EXPECT_EQ(first4->NumGlyphs(), 22u);

  Vector<ShapeResultTestGlyphInfo> first4_glyphs;
  first4->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&first4_glyphs));
  EXPECT_EQ(first4_glyphs.size(), 22u);
  EXPECT_TRUE(CompareResultGlyphs(first4_glyphs, glyphs, 0u, 22u));

  // Test view in the middle of the result: "multiple words and breaking"
  segments[0] = {result.get(), 14, 41};
  auto middle4 = ShapeResultView::Create(segments);

  EXPECT_EQ(middle4->StartIndex(), 14u);
  EXPECT_EQ(middle4->NumCharacters(), 27u);
  EXPECT_EQ(middle4->NumGlyphs(), 27u);

  Vector<ShapeResultTestGlyphInfo> middle4_glyphs;
  middle4->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&middle4_glyphs));
  EXPECT_EQ(middle4_glyphs.size(), 27u);
  EXPECT_TRUE(CompareResultGlyphs(middle4_glyphs, glyphs, 14u, 27u));

  // Test view at the end of the result: "breaking opportunities."
  segments[0] = {result.get(), 33, 56};
  auto last2 = ShapeResultView::Create(segments);

  EXPECT_EQ(last2->StartIndex(), 33u);
  EXPECT_EQ(last2->NumCharacters(), 23u);
  EXPECT_EQ(last2->NumGlyphs(), 23u);

  Vector<ShapeResultTestGlyphInfo> last2_glyphs;
  last2->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&last2_glyphs));
  EXPECT_EQ(last2_glyphs.size(), 23u);
  EXPECT_TRUE(CompareResultGlyphs(last2_glyphs, glyphs, 33u, 23u));
}

TEST_F(ShapeResultViewTest, ArabicSingleView) {
  String string = To16Bit("عربى نص", 7);
  TextDirection direction = TextDirection::kRtl;

  HarfBuzzShaper shaper(string);
  scoped_refptr<const ShapeResult> result = shaper.Shape(&font, direction);
  Vector<ShapeResultTestGlyphInfo> glyphs;
  result->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&glyphs));

  // Test view at the start of the result: "عربى"
  ShapeResultView::Segment segments[] = {{result.get(), 0, 4}};
  auto first_word = ShapeResultView::Create(segments);
  Vector<ShapeResultTestGlyphInfo> first_glyphs;
  first_word->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&first_glyphs));

  EXPECT_EQ(first_word->StartIndex(), 0u);
  EXPECT_EQ(first_word->NumCharacters(), 4u);
  EXPECT_EQ(first_word->NumGlyphs(), 4u);
  EXPECT_EQ(first_glyphs.size(), 4u);

  String first_reference_string = To16Bit("عربى", 4);
  HarfBuzzShaper first_reference_shaper(first_reference_string);
  scoped_refptr<const ShapeResult> first_wortd_reference =
      first_reference_shaper.Shape(&font, direction);
  Vector<ShapeResultTestGlyphInfo> first_reference_glyphs;
  first_wortd_reference->ForEachGlyph(
      0, AddGlyphInfo, static_cast<void*>(&first_reference_glyphs));
  EXPECT_EQ(first_reference_glyphs.size(), 4u);

  EXPECT_TRUE(
      CompareResultGlyphs(first_glyphs, first_reference_glyphs, 0u, 4u));
  EXPECT_TRUE(CompareResultGlyphs(first_glyphs, glyphs, 3u, 7u));

  // Test view at the end of the result: "نص"
  segments[0] = {result.get(), 4, 7};
  auto last_word = ShapeResultView::Create(segments);
  Vector<ShapeResultTestGlyphInfo> last_glyphs;
  last_word->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&last_glyphs));

  EXPECT_EQ(last_word->StartIndex(), 4u);
  EXPECT_EQ(last_word->NumCharacters(), 3u);
  EXPECT_EQ(last_word->NumGlyphs(), 3u);
  EXPECT_EQ(last_glyphs.size(), 3u);
}

TEST_F(ShapeResultViewTest, PreviousSafeToBreak) {
  String string =
      u"\u0028\u05D1\u0029\u0020\u05D4\u05D1\u05DC\u0020\u05D0\u05DE\u05E8"
      u"\u0020\u05E2\u05DC\u0020"
      u"\u05D3\u05D1\u05E8\u05D9\u0020\u05D4\u05D1\u05DC\u05D9\u0020\u05D4"
      u"\u05E2\u05D5\u05DC\u05DD\u002C"
      u"\u0020\u05D5\u05E1\u05DE\u05DA\u0020\u05D4\u05B2\u05D1\u05B5\u05DC"
      u"\u0020\u05D0\u05DC\u0020\u05D4"
      u"\u05D1\u05DC\u05D9\u05DD\u0020\u05D5\u05D0\u05DD\u0020\u05DC\u05D0"
      u"\u0020\u05D9\u05DE\u05E6\u05D0"
      u"\u0020\u05DE\u05D4\u05E9\u05DE\u05D5\u05EA\u0020\u05E9\u05D4\u05DD"
      u"\u0020\u05E2\u05DC\u0020\u05DE"
      u"\u05E9\u05E7\u05DC\u0020\u05D0\u05E8\u05E5\u0020\u05E9\u05D9\u05E9"
      u"\u05EA\u05E0\u05D4\u0020\u05D7"
      u"\u05D5\u05E5\u0020\u05DE\u05B5\u05D7\u05B2\u05D3\u05B7\u05E8\u0020"
      u"\u05DE\u05B4\u05E9\u05B0\u05C1"
      u"\u05DB\u05B8\u05D1\u05B0\u05DA\u05B8\u0020\u0028\u05E9\u05DE\u05D5"
      u"\u05EA\u0020\u05D6\u05F3\u003A"
      u"\u05DB\u05F4\u05D7\u0029";
  TextDirection direction = TextDirection::kRtl;
  HarfBuzzShaper shaper(string);
  const RunSegmenter::RunSegmenterRange range = {
      51, 131, USCRIPT_HEBREW, blink::OrientationIterator::kOrientationKeep,
      blink::FontFallbackPriority::kText};
  scoped_refptr<ShapeResult> shape_result =
      shaper.Shape(&font, direction, 51, 131, range);

  unsigned start_offset = 59;
  unsigned end_offset = 118;
  scoped_refptr<const ShapeResultView> result_view =
      ShapeResultView::Create(shape_result.get(), start_offset, end_offset);
  scoped_refptr<ShapeResult> result = result_view->CreateShapeResult();

  unsigned offset = end_offset;
  do {
    unsigned safe = result_view->PreviousSafeToBreakOffset(offset);
    unsigned cached_safe = result->CachedPreviousSafeToBreakOffset(offset);
    EXPECT_EQ(safe, cached_safe);
  } while (--offset > start_offset);
}

TEST_F(ShapeResultViewTest, LatinMultiRun) {
  TextDirection direction = TextDirection::kLtr;
  HarfBuzzShaper shaper_a(To16Bit("hello", 5));
  HarfBuzzShaper shaper_b(To16Bit(" w", 2));
  HarfBuzzShaper shaper_c(To16Bit("orld", 4));
  HarfBuzzShaper shaper_d(To16Bit("!", 1));

  // Combine four separate results into a single one to ensure we have a result
  // with multiple runs: "hello world!"
  scoped_refptr<ShapeResult> result =
      ShapeResult::Create(&font, 0, 0, direction);
  shaper_a.Shape(&font, direction)->CopyRange(0u, 5u, result.get());
  shaper_b.Shape(&font, direction)->CopyRange(0u, 2u, result.get());
  shaper_c.Shape(&font, direction)->CopyRange(0u, 4u, result.get());
  shaper_d.Shape(&font, direction)->CopyRange(0u, 1u, result.get());

  Vector<ShapeResultTestGlyphInfo> result_glyphs;
  result->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&result_glyphs));

  // Create composite view out of multiple segments where at least some of the
  // segments have multiple runs: "hello wood wold!"
  ShapeResultView::Segment segments[5] = {
      {result.get(), 0, 8},    // "hello wo"
      {result.get(), 7, 8},    // "o"
      {result.get(), 10, 11},  // "d"
      {result.get(), 5, 8},    // " wo"
      {result.get(), 9, 12},   // "ld!"
  };
  auto composite_view = ShapeResultView::Create(segments);
  Vector<ShapeResultTestGlyphInfo> view_glyphs;
  composite_view->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&view_glyphs));

  EXPECT_EQ(composite_view->StartIndex(), 0u);
  EXPECT_EQ(composite_view->NumCharacters(), 16u);
  EXPECT_EQ(composite_view->NumGlyphs(), 16u);
  EXPECT_EQ(view_glyphs.size(), 16u);

  HarfBuzzShaper shaper2(To16Bit("hello world!", 12));
  scoped_refptr<const ShapeResult> result2 = shaper2.Shape(&font, direction);
  Vector<ShapeResultTestGlyphInfo> glyphs2;
  result2->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&glyphs2));
  EXPECT_TRUE(CompareResultGlyphs(result_glyphs, glyphs2, 0u, 12u));

  HarfBuzzShaper reference_shaper(To16Bit("hello wood wold!", 16));
  scoped_refptr<const ShapeResult> reference_result =
      reference_shaper.Shape(&font, direction);
  Vector<ShapeResultTestGlyphInfo> reference_glyphs;
  reference_result->ForEachGlyph(0, AddGlyphInfo,
                                 static_cast<void*>(&reference_glyphs));

  scoped_refptr<ShapeResult> composite_copy =
      ShapeResult::Create(&font, 0, 0, direction);
  result->CopyRange(0, 8, composite_copy.get());
  result->CopyRange(7, 8, composite_copy.get());
  result->CopyRange(10, 11, composite_copy.get());
  result->CopyRange(5, 8, composite_copy.get());
  result->CopyRange(9, 12, composite_copy.get());

  Vector<ShapeResultTestGlyphInfo> composite_copy_glyphs;
  composite_copy->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&composite_copy_glyphs));

  EXPECT_TRUE(CompareResultGlyphs(view_glyphs, reference_glyphs, 0u, 16u));
  EXPECT_TRUE(
      CompareResultGlyphs(composite_copy_glyphs, reference_glyphs, 0u, 16u));
  EXPECT_EQ(composite_view->Width(), composite_copy->Width());
}

TEST_F(ShapeResultViewTest, LatinCompositeView) {
  String string =
      To16Bit("Test run with multiple words and breaking opportunities.", 56);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  scoped_refptr<const ShapeResult> result = shaper.Shape(&font, direction);
  Vector<ShapeResultTestGlyphInfo> glyphs;
  result->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&glyphs));

  String reference_string = To16Bit("multiple breaking opportunities Test", 36);
  HarfBuzzShaper reference_shaper(reference_string);
  scoped_refptr<const ShapeResult> reference_result =
      reference_shaper.Shape(&font, direction);
  Vector<ShapeResultTestGlyphInfo> reference_glyphs;

  // Match the character index logic of ShapeResult::CopyRange where the the
  // character index of the first result is preserved and all subsequent ones
  // are adjusted to be sequential.
  // TODO(layout-dev): Arguably both should be updated to renumber the first
  // result as well but some callers depend on the existing behavior.
  scoped_refptr<ShapeResult> composite_copy =
      ShapeResult::Create(&font, 0, 0, direction);
  result->CopyRange(14, 23, composite_copy.get());
  result->CopyRange(33, 55, composite_copy.get());
  result->CopyRange(4, 5, composite_copy.get());
  result->CopyRange(0, 4, composite_copy.get());
  EXPECT_EQ(composite_copy->NumCharacters(), reference_result->NumCharacters());
  EXPECT_EQ(composite_copy->NumGlyphs(), reference_result->NumGlyphs());
  composite_copy->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&reference_glyphs));

  // Create composite view out of multiple segments:
  ShapeResultView::Segment segments[4] = {
      {result.get(), 14, 23},  // "multiple "
      {result.get(), 33, 55},  // "breaking opportunities"
      {result.get(), 4, 5},    // " "
      {result.get(), 0, 4}     // "Test"
  };
  auto composite_view = ShapeResultView::Create(segments);

  EXPECT_EQ(composite_view->StartIndex(), composite_copy->StartIndex());
  EXPECT_EQ(composite_view->NumCharacters(), reference_result->NumCharacters());
  EXPECT_EQ(composite_view->NumGlyphs(), reference_result->NumGlyphs());

  Vector<ShapeResultTestGlyphInfo> composite_glyphs;
  composite_view->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&composite_glyphs));
  EXPECT_EQ(composite_glyphs.size(), 36u);
  EXPECT_TRUE(CompareResultGlyphs(composite_glyphs, reference_glyphs, 0u, 22u));
  EXPECT_EQ(composite_view->Width(), composite_copy->Width());
}

TEST_F(ShapeResultViewTest, MixedScriptsCompositeView) {
  String string_a = To16Bit("Test with multiple 字体 ", 22);
  String string_b = To16Bit("and 本書.", 7);
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper_a(string_a);
  scoped_refptr<const ShapeResult> result_a = shaper_a.Shape(&font, direction);
  HarfBuzzShaper shaper_b(string_b);
  scoped_refptr<const ShapeResult> result_b = shaper_b.Shape(&font, direction);

  String reference_string = To16Bit("Test with multiple 字体 and 本書.", 29);
  HarfBuzzShaper reference_shaper(reference_string);
  scoped_refptr<const ShapeResult> reference_result =
      reference_shaper.Shape(&font, direction);

  // Create a copy using CopyRange and compare with that to ensure that the same
  // fonts are used for both the composite and the reference. The combined
  // reference_result data might use different fonts, resulting in different
  // glyph ids and metrics.
  scoped_refptr<ShapeResult> composite_copy =
      ShapeResult::Create(&font, 0, 0, direction);
  result_a->CopyRange(0, 22, composite_copy.get());
  result_b->CopyRange(0, 7, composite_copy.get());
  EXPECT_EQ(composite_copy->NumCharacters(), reference_result->NumCharacters());
  EXPECT_EQ(composite_copy->NumGlyphs(), reference_result->NumGlyphs());
  Vector<ShapeResultTestGlyphInfo> reference_glyphs;
  composite_copy->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&reference_glyphs));

  ShapeResultView::Segment segments[] = {{result_a.get(), 0, 22},
                                         {result_b.get(), 0, 7}};
  auto composite_view = ShapeResultView::Create(segments);

  EXPECT_EQ(composite_view->StartIndex(), 0u);
  EXPECT_EQ(composite_view->NumCharacters(), reference_result->NumCharacters());
  EXPECT_EQ(composite_view->NumGlyphs(), reference_result->NumGlyphs());

  Vector<ShapeResultTestGlyphInfo> composite_glyphs;
  composite_view->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&composite_glyphs));
  EXPECT_TRUE(CompareResultGlyphs(composite_glyphs, reference_glyphs, 0u,
                                  reference_glyphs.size()));
  EXPECT_EQ(composite_view->Width(), composite_copy->Width());
}

TEST_F(ShapeResultViewTest, TrimEndOfView) {
  String string = To16Bit("12345678901234567890", 20);
  TextDirection direction = TextDirection::kLtr;
  HarfBuzzShaper shaper(string);
  scoped_refptr<const ShapeResult> result = shaper.Shape(&font, direction);

  // Create a view from 5 to 20.
  scoped_refptr<const ShapeResultView> view1 =
      ShapeResultView::Create(result.get(), 5, 20);
  EXPECT_EQ(view1->NumCharacters(), 15u);
  EXPECT_EQ(view1->NumGlyphs(), 15u);

  // Trim the last character from the view.
  scoped_refptr<const ShapeResultView> view2 =
      ShapeResultView::Create(view1.get(), 5, 19);
  EXPECT_EQ(view2->NumCharacters(), 14u);
  EXPECT_EQ(view2->NumGlyphs(), 14u);
}

TEST_F(ShapeResultViewTest, MarkerAndTrailingSpace) {
  String string = u"\u2067\u2022\u0020";
  TextDirection direction = TextDirection::kRtl;
  LayoutUnit symbol_width = LayoutUnit(7);
  scoped_refptr<const ShapeResult> result =
      ShapeResult::CreateForSpaces(&font, direction, 1, 2, symbol_width);

  ShapeResultView::Segment segments[] = {{result.get(), 1, 2}};
  auto shape_result_view = ShapeResultView::Create(segments);
  scoped_refptr<ShapeResult> shape_result =
      shape_result_view->CreateShapeResult();

  Vector<CharacterRange> ranges;
  shape_result->IndividualCharacterRanges(&ranges);
}

TEST_F(ShapeResultViewTest, SpacesInLTR) {
  constexpr unsigned kStartIndex = 0;
  constexpr unsigned kLength = 2;
  constexpr float kWidth = 8;
  const auto result = ShapeResult::CreateForSpaces(
      &font, TextDirection::kLtr, kStartIndex, kLength, kWidth);

  const auto view0 = ShapeResultView::Create(result.get(), 0, 2);
  EXPECT_EQ(view0->NumCharacters(), 2u);
  EXPECT_EQ(view0->NumGlyphs(), 2u);

  const auto view1 = ShapeResultView::Create(result.get(), 0, 1);
  EXPECT_EQ(view1->NumCharacters(), 1u);
  EXPECT_EQ(view1->NumGlyphs(), 1u);

  const auto view2 = ShapeResultView::Create(result.get(), 1, 2);
  EXPECT_EQ(view2->NumCharacters(), 1u);
  EXPECT_EQ(view2->NumGlyphs(), 1u);
}

// http://crbug.com/1160582
TEST_F(ShapeResultViewTest, SpacesInRTL) {
  constexpr unsigned kStartIndex = 0;
  constexpr unsigned kLength = 2;
  constexpr float kWidth = 8;
  const auto result = ShapeResult::CreateForSpaces(
      &font, TextDirection::kRtl, kStartIndex, kLength, kWidth);

  const auto view0 = ShapeResultView::Create(result.get(), 0, 2);
  EXPECT_EQ(view0->NumCharacters(), 2u);
  EXPECT_EQ(view0->NumGlyphs(), 2u);

  const auto view1 = ShapeResultView::Create(result.get(), 0, 1);
  EXPECT_EQ(view1->NumCharacters(), 1u);
  EXPECT_EQ(view1->NumGlyphs(), 1u);

  const auto view2 = ShapeResultView::Create(result.get(), 1, 2);
  EXPECT_EQ(view2->NumCharacters(), 1u);
  EXPECT_EQ(view2->NumGlyphs(), 1u);
}

TEST_F(ShapeResultViewTest, TabulationCharactersInLTR) {
  constexpr float kPosition = 0;
  constexpr unsigned kStartIndex = 0;
  constexpr unsigned kLength = 2;
  const auto result = ShapeResult::CreateForTabulationCharacters(
      &font, TextDirection::kLtr, TabSize(8), kPosition, kStartIndex, kLength);

  const auto view0 = ShapeResultView::Create(result.get(), 0, 2);
  EXPECT_EQ(view0->NumCharacters(), 2u);
  EXPECT_EQ(view0->NumGlyphs(), 2u);

  const auto view1 = ShapeResultView::Create(result.get(), 0, 1);
  EXPECT_EQ(view1->NumCharacters(), 1u);
  EXPECT_EQ(view1->NumGlyphs(), 1u);

  const auto view2 = ShapeResultView::Create(result.get(), 1, 2);
  EXPECT_EQ(view2->NumCharacters(), 1u);
  EXPECT_EQ(view2->NumGlyphs(), 1u);
}

// http://crbug.com/1255310
TEST_F(ShapeResultViewTest, TabulationCharactersInRTL) {
  constexpr float kPosition = 0;
  constexpr unsigned kStartIndex = 0;
  constexpr unsigned kLength = 2;
  const auto result = ShapeResult::CreateForTabulationCharacters(
      &font, TextDirection::kRtl, TabSize(8), kPosition, kStartIndex, kLength);

  const auto view0 = ShapeResultView::Create(result.get(), 0, 2);
  EXPECT_EQ(view0->NumCharacters(), 2u);
  EXPECT_EQ(view0->NumGlyphs(), 2u);

  const auto view1 = ShapeResultView::Create(result.get(), 0, 1);
  EXPECT_EQ(view1->NumCharacters(), 1u);
  EXPECT_EQ(view1->NumGlyphs(), 1u);

  const auto view2 = ShapeResultView::Create(result.get(), 1, 2);
  EXPECT_EQ(view2->NumCharacters(), 1u);
  EXPECT_EQ(view2->NumGlyphs(), 1u);
}

// https://crbug.com/1304876
// In a text containing only Latin characters and without ligatures (or where
// ligatures are not close to the end of the view), PreviousSafeToBreakOffset in
// some cases used to return the length of the view, rather than a position into
// the view.
TEST_F(ShapeResultViewTest, PreviousSafeOffsetInsideView) {
  HarfBuzzShaper shaper("Blah bla test something. ");
  scoped_refptr<const ShapeResult> result =
      shaper.Shape(&font, TextDirection::kLtr);

  // Used to be 14 - 9 = 5, which is before the start of the view.
  auto view1 = ShapeResultView::Create(result.get(), 9, 14);
  EXPECT_EQ(view1->PreviousSafeToBreakOffset(14), 14u);

  // Used to be 25 - 9 = 16, which is inside the view's range, but not the last
  // safe offset.
  auto view2 = ShapeResultView::Create(result.get(), 9, 25);
  EXPECT_EQ(view2->PreviousSafeToBreakOffset(24), 24u);
}

}  // namespace blink
