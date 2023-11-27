// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_test_utilities.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_test_info.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class ShapeResultTest : public FontTestBase {
 protected:
  void SetUp() override {
    FontDescription::VariantLigatures ligatures;
    font = blink::test::CreateTestFont(
        AtomicString("Roboto"),
        blink::test::PlatformTestDataPath(
            "third_party/Roboto/roboto-regular.woff2"),
        12.0, &ligatures);

    arabic_font = blink::test::CreateTestFont(
        AtomicString("Noto"),
        blink::test::PlatformTestDataPath(
            "third_party/Noto/NotoNaskhArabic-regular.woff2"),
        12.0, &ligatures);
  }

  void TearDown() override {}

  void TestCopyRangesLatin(const ShapeResult*) const;
  void TestCopyRangesArabic(const ShapeResult*) const;

  static bool HasNonZeroGlyphOffsets(const ShapeResult& result) {
    for (const auto& run : result.RunsOrParts()) {
      if (run->glyph_data_.HasNonZeroOffsets())
        return true;
    }
    return false;
  }

  // Release the ShapeResults held inside an array of ShapeResult::ShapeRange
  // instances.
  static void ReleaseShapeRange(base::span<ShapeResult::ShapeRange> ranges) {
    for (auto& range : ranges) {
      range.target->Release();
    }
  }

  ShapeResult* CreateShapeResult(TextDirection direction) const {
    return new ShapeResult(
        direction == TextDirection::kLtr ? &font : &arabic_font, 0, 0,
        direction);
  }

  FontCachePurgePreventer font_cache_purge_preventer;
  FontDescription font_description;
  Font font;
  Font arabic_font;
};

void ShapeResultTest::TestCopyRangesLatin(const ShapeResult* result) const {
  const unsigned num_ranges = 4;
  ShapeResult::ShapeRange ranges[num_ranges] = {
      {0, 10, CreateShapeResult(TextDirection::kLtr)},
      {10, 20, CreateShapeResult(TextDirection::kLtr)},
      {20, 30, CreateShapeResult(TextDirection::kLtr)},
      {30, 38, CreateShapeResult(TextDirection::kLtr)}};
  result->CopyRanges(&ranges[0], num_ranges);

  Vector<ShapeResultTestGlyphInfo> glyphs[num_ranges];
  for (unsigned i = 0; i < num_ranges; i++)
    ComputeGlyphResults(*ranges[i].target, &glyphs[i]);
  EXPECT_EQ(glyphs[0].size(), 10u);
  EXPECT_EQ(glyphs[1].size(), 10u);
  EXPECT_EQ(glyphs[2].size(), 10u);
  EXPECT_EQ(glyphs[3].size(), 8u);

  scoped_refptr<ShapeResult> reference[num_ranges];
  reference[0] = result->SubRange(0, 10);
  reference[1] = result->SubRange(10, 20);
  reference[2] = result->SubRange(20, 30);
  reference[3] = result->SubRange(30, 38);
  Vector<ShapeResultTestGlyphInfo> reference_glyphs[num_ranges];
  for (unsigned i = 0; i < num_ranges; i++)
    ComputeGlyphResults(*reference[i], &reference_glyphs[i]);
  EXPECT_EQ(reference_glyphs[0].size(), 10u);
  EXPECT_EQ(reference_glyphs[1].size(), 10u);
  EXPECT_EQ(reference_glyphs[2].size(), 10u);
  EXPECT_EQ(reference_glyphs[3].size(), 8u);

  EXPECT_TRUE(CompareResultGlyphs(glyphs[0], reference_glyphs[0], 0u, 10u));
  EXPECT_TRUE(CompareResultGlyphs(glyphs[1], reference_glyphs[1], 0u, 10u));
  EXPECT_TRUE(CompareResultGlyphs(glyphs[2], reference_glyphs[2], 0u, 10u));
  EXPECT_TRUE(CompareResultGlyphs(glyphs[3], reference_glyphs[3], 0u, 8u));
  ReleaseShapeRange(ranges);
}

void ShapeResultTest::TestCopyRangesArabic(const ShapeResult* result) const {
  const unsigned num_ranges = 4;
  ShapeResult::ShapeRange ranges[num_ranges] = {
      {0, 4, CreateShapeResult(TextDirection::kRtl)},
      {4, 7, CreateShapeResult(TextDirection::kRtl)},
      {7, 10, CreateShapeResult(TextDirection::kRtl)},
      {10, 15, CreateShapeResult(TextDirection::kRtl)}};
  result->CopyRanges(&ranges[0], num_ranges);

  Vector<ShapeResultTestGlyphInfo> glyphs[num_ranges];
  for (unsigned i = 0; i < num_ranges; i++)
    ComputeGlyphResults(*ranges[i].target, &glyphs[i]);
  EXPECT_EQ(glyphs[0].size(), 4u);
  EXPECT_EQ(glyphs[1].size(), 3u);
  EXPECT_EQ(glyphs[2].size(), 3u);
  EXPECT_EQ(glyphs[3].size(), 5u);

  scoped_refptr<ShapeResult> reference[num_ranges];
  reference[0] = result->SubRange(0, 4);
  reference[1] = result->SubRange(4, 7);
  reference[2] = result->SubRange(7, 10);
  reference[3] = result->SubRange(10, 17);
  Vector<ShapeResultTestGlyphInfo> reference_glyphs[num_ranges];
  for (unsigned i = 0; i < num_ranges; i++)
    ComputeGlyphResults(*reference[i], &reference_glyphs[i]);
  EXPECT_EQ(reference_glyphs[0].size(), 4u);
  EXPECT_EQ(reference_glyphs[1].size(), 3u);
  EXPECT_EQ(reference_glyphs[2].size(), 3u);
  EXPECT_EQ(reference_glyphs[3].size(), 5u);

  EXPECT_TRUE(CompareResultGlyphs(glyphs[0], reference_glyphs[0], 0u, 4u));
  EXPECT_TRUE(CompareResultGlyphs(glyphs[1], reference_glyphs[1], 0u, 3u));
  EXPECT_TRUE(CompareResultGlyphs(glyphs[2], reference_glyphs[2], 0u, 3u));
  EXPECT_TRUE(CompareResultGlyphs(glyphs[3], reference_glyphs[3], 0u, 5u));
  ReleaseShapeRange(ranges);
}

TEST_F(ShapeResultTest, CopyRangeLatin) {
  String string = "Testing ShapeResultIterator::CopyRange";
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font, direction);
  TestCopyRangesLatin(result.get());
}

// Identical to CopyRangeLatin except the source range shape result is split
// into multiple runs to test the handling of ranges spanning runs and runs
// spanning ranges.
TEST_F(ShapeResultTest, CopyRangeLatinMultiRun) {
  TextDirection direction = TextDirection::kLtr;
  String string = "Testing ShapeResultIterator::CopyRange";
  HarfBuzzShaper shaper_a(string.Substring(0, 5));
  HarfBuzzShaper shaper_b(string.Substring(5, 7));
  HarfBuzzShaper shaper_c(string.Substring(7, 32));
  HarfBuzzShaper shaper_d(string.Substring(32, 38));

  // Combine four separate results into a single one to ensure we have a result
  // with multiple runs.
  scoped_refptr<ShapeResult> result =
      ShapeResult::Create(&font, 0, 0, direction);
  shaper_a.Shape(&font, direction)->CopyRange(0u, 5u, result.get());
  shaper_b.Shape(&font, direction)->CopyRange(0u, 2u, result.get());
  shaper_c.Shape(&font, direction)->CopyRange(0u, 25u, result.get());
  shaper_d.Shape(&font, direction)->CopyRange(0u, 6u, result.get());
  TestCopyRangesLatin(result.get());
}

TEST_F(ShapeResultTest, CopyRangeLatinMultiRunWithHoles) {
  TextDirection direction = TextDirection::kLtr;
  String string = "Testing copying a range with holes";
  HarfBuzzShaper shaper_a(string.Substring(0, 5));
  HarfBuzzShaper shaper_b(string.Substring(5, 7));
  HarfBuzzShaper shaper_c(string.Substring(7, 32));
  HarfBuzzShaper shaper_d(string.Substring(32, 34));

  scoped_refptr<ShapeResult> result =
      ShapeResult::Create(&font, 0, 0, direction);
  shaper_a.Shape(&font, direction)->CopyRange(0u, 5u, result.get());
  shaper_b.Shape(&font, direction)->CopyRange(0u, 2u, result.get());
  shaper_c.Shape(&font, direction)->CopyRange(0u, 25u, result.get());
  shaper_d.Shape(&font, direction)->CopyRange(0u, 2u, result.get());

  ShapeResult::ShapeRange ranges[] = {
      {4, 17, CreateShapeResult(TextDirection::kLtr)},
      {20, 23, CreateShapeResult(TextDirection::kLtr)},
      {25, 31, CreateShapeResult(TextDirection::kLtr)}};
  result->CopyRanges(&ranges[0], 3);
  Vector<ShapeResultTestGlyphInfo> glyphs[3];
  ComputeGlyphResults(*ranges[0].target, &glyphs[0]);
  ComputeGlyphResults(*ranges[1].target, &glyphs[1]);
  ComputeGlyphResults(*ranges[2].target, &glyphs[2]);
  EXPECT_EQ(glyphs[0].size(), 13u);
  EXPECT_EQ(glyphs[1].size(), 3u);
  EXPECT_EQ(glyphs[2].size(), 6u);

  scoped_refptr<ShapeResult> reference[3];
  reference[0] = result->SubRange(4, 17);
  reference[1] = result->SubRange(20, 23);
  reference[2] = result->SubRange(25, 31);
  Vector<ShapeResultTestGlyphInfo> reference_glyphs[3];
  ComputeGlyphResults(*reference[0], &reference_glyphs[0]);
  ComputeGlyphResults(*reference[1], &reference_glyphs[1]);
  ComputeGlyphResults(*reference[2], &reference_glyphs[2]);
  EXPECT_EQ(reference_glyphs[0].size(), 13u);
  EXPECT_EQ(reference_glyphs[1].size(), 3u);
  EXPECT_EQ(reference_glyphs[2].size(), 6u);

  EXPECT_TRUE(CompareResultGlyphs(glyphs[0], reference_glyphs[0], 0u, 13u));
  EXPECT_TRUE(CompareResultGlyphs(glyphs[1], reference_glyphs[1], 0u, 3u));
  EXPECT_TRUE(CompareResultGlyphs(glyphs[2], reference_glyphs[2], 0u, 6u));
  ReleaseShapeRange(ranges);
}

TEST_F(ShapeResultTest, CopyRangeArabic) {
  // نص اختبار العربية
  String string(
      u"\u0646\u0635\u0627\u062E\u062A\u0628\u0627\u0631\u0627\u0644\u0639"
      u"\u0631\u0628\u064A\u0629");
  TextDirection direction = TextDirection::kRtl;

  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&arabic_font, direction);
  TestCopyRangesArabic(result.get());
}

// Identical to CopyRangeArabic except the source range shape result is split
// into multiple runs to test the handling of ranges spanning runs and runs
// spanning ranges.
TEST_F(ShapeResultTest, CopyRangeArabicMultiRun) {
  // نص اختبار العربية
  String string(
      u"\u0646\u0635\u0627\u062E\u062A\u0628\u0627\u0631\u0627\u0644\u0639"
      u"\u0631\u0628\u064A\u0629");
  TextDirection direction = TextDirection::kRtl;

  HarfBuzzShaper shaper_a(string.Substring(0, 2));
  HarfBuzzShaper shaper_b(string.Substring(2, 9));
  HarfBuzzShaper shaper_c(string.Substring(9, 15));

  // Combine three separate results into a single one to ensure we have a result
  // with multiple runs.
  scoped_refptr<ShapeResult> result =
      ShapeResult::Create(&arabic_font, 0, 0, direction);
  shaper_a.Shape(&arabic_font, direction)->CopyRange(0u, 2u, result.get());
  shaper_b.Shape(&arabic_font, direction)->CopyRange(0u, 7u, result.get());
  shaper_c.Shape(&arabic_font, direction)->CopyRange(0u, 8u, result.get());

  TestCopyRangesArabic(result.get());
}

static struct IsStartSafeToBreakData {
  bool expected;
  const char16_t* text;
  TextDirection direction = TextDirection::kLtr;
  unsigned start_offset = 0;
  unsigned end_offset = 0;
} is_start_safe_to_break_data[] = {
    {true, u"XX", TextDirection::kLtr},
    {true, u"XX", TextDirection::kRtl},
    // SubRange, assuming there is no kerning between "XX".
    {true, u"XX", TextDirection::kLtr, 1, 2},
    {true, u"XX", TextDirection::kRtl, 1, 2},
    // Between "A" and "V" usually have a kerning.
    {false, u"AV", TextDirection::kLtr, 1, 2},
    {false, u"AV", TextDirection::kRtl, 1, 2},
    // SubRange at the middle of a cluster.
    // U+06D7 ARABIC SMALL HIGH LIGATURE QAF WITH LAM WITH ALEF MAKSURA
    {false, u" \u06D7", TextDirection::kLtr, 1, 2},
    {false, u" \u06D7", TextDirection::kRtl, 1, 2},
    {false, u" \u06D7.", TextDirection::kLtr, 1, 3},
    {false, u" \u06D7.", TextDirection::kRtl, 1, 3},
};

class IsStartSafeToBreakDataTest
    : public ShapeResultTest,
      public testing::WithParamInterface<IsStartSafeToBreakData> {};

INSTANTIATE_TEST_SUITE_P(ShapeResultTest,
                         IsStartSafeToBreakDataTest,
                         testing::ValuesIn(is_start_safe_to_break_data));

TEST_P(IsStartSafeToBreakDataTest, IsStartSafeToBreakData) {
  const IsStartSafeToBreakData data = GetParam();
  String string(data.text);
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font, data.direction);
  if (data.end_offset)
    result = result->SubRange(data.start_offset, data.end_offset);
  EXPECT_EQ(result->IsStartSafeToBreak(), data.expected);
}

TEST_F(ShapeResultTest, ComputeInkBoundsWithZeroOffset) {
  String string(u"abc");
  HarfBuzzShaper shaper(string);
  auto result = shaper.Shape(&font, TextDirection::kLtr);
  EXPECT_FALSE(HasNonZeroGlyphOffsets(*result));
  EXPECT_FALSE(result->ComputeInkBounds().IsEmpty());
}

struct TextAutoSpaceTextData {
  // The string that should be processed.
  const UChar* string;
  // Precalculated insertion points' offsets.
  std::vector<wtf_size_t> offsets;

} text_auto_space_test_data[] = {
    {u"Abcあああ", {3}},
    {u"ああ123あああ", {2, 5}},
    {u"ああ123ああ", {2, 5}},
    {u"ああ123ああ", {1, 2, 3, 4, 5, 6, 7}},
};
class TextAutoSpaceResultText
    : public ShapeResultTest,
      public testing::WithParamInterface<TextAutoSpaceTextData> {};
INSTANTIATE_TEST_SUITE_P(ShapeResultTest,
                         TextAutoSpaceResultText,
                         testing::ValuesIn(text_auto_space_test_data));

Vector<float> RecordPositionBeforeApplyingSpacing(ShapeResult* result,
                                                  wtf_size_t size) {
  Vector<float> before_adding_spacing(size);
  std::generate(before_adding_spacing.begin(), before_adding_spacing.end(),
                [&, i = 0]() mutable {
                  float position = result->PositionForOffset(i);
                  i++;
                  return position;
                });
  return before_adding_spacing;
}

Vector<OffsetWithSpacing, 16> RecordExpectedSpacing(
    const std::vector<wtf_size_t>& offsets_data) {
  Vector<OffsetWithSpacing, 16> offsets(offsets_data.size());
  std::generate_n(offsets.begin(), offsets_data.size(), [&, i = -1]() mutable {
    ++i;
    return OffsetWithSpacing{.offset = offsets_data[i],
                             .spacing = static_cast<float>(0.1 * (i + 1))};
  });
  return offsets;
}

// Tests the spacing should be appended at the correct positions.
TEST_P(TextAutoSpaceResultText, AddAutoSpacingToIdeograph) {
  const auto& test_data = GetParam();
  String string(test_data.string);
  HarfBuzzShaper shaper(string);
  scoped_refptr<ShapeResult> result = shaper.Shape(&font, TextDirection::kLtr);

  // Record the position before applying text-autospace, and fill the spacing
  // widths with different values.
  Vector<float> before_adding_spacing =
      RecordPositionBeforeApplyingSpacing(result.get(), string.length());
  Vector<OffsetWithSpacing, 16> offsets =
      RecordExpectedSpacing(test_data.offsets);
  result->ApplyTextAutoSpacing(offsets);
  float accumulated_spacing = 0.0;
  for (wtf_size_t i = 0, j = 0; i < string.length(); i++) {
    if (j < test_data.offsets.size() && offsets[j].offset == i) {
      accumulated_spacing += offsets[j].spacing;
      j++;
    }
    EXPECT_NEAR(accumulated_spacing,
                result->PositionForOffset(i) - before_adding_spacing[i],
                /* abs_error= */ 1e-5);
  }
}

// TDOO(yosin): We should use a font including U+0A81 or other code point
// having non-zero glyph offset.
TEST_F(ShapeResultTest, DISABLED_ComputeInkBoundsWithNonZeroOffset) {
  // U+0A81 has non-zero glyph offset
  String string(u"xy\u0A81z");
  HarfBuzzShaper shaper(string);
  auto result = shaper.Shape(&font, TextDirection::kLtr);
  ASSERT_TRUE(HasNonZeroGlyphOffsets(*result));
  EXPECT_FALSE(result->ComputeInkBounds().IsEmpty());
}

// Tests for CaretPositionForOffset
struct CaretPositionForOffsetTextData {
  // The string that should be processed.
  const UChar* string;
  // Text direction to test
  TextDirection direction;
  // The offsets to test.
  std::vector<wtf_size_t> offsets;
  // Expected positions. The width is 240.
  std::vector<float> positions;
  // True to use latin font, otherwise arabic
  bool is_latin;
  // Adjust mid cluster value
  AdjustMidCluster adjust_mid_cluster;
} caret_position_for_offset_test_data[] = {
    // 0
    {u"012345678901234567890123456789",
     TextDirection::kLtr,
     {0, 1, 4, 5, 12, 18, 30, 32},
#if BUILDFLAG(IS_APPLE)
     {0, 6.738, 26.953, 33.691, 80.859, 121.289, 202.148, 0},
#else
     {0, 7, 28, 35, 84, 126, 210, 0},
#endif
     true,
     AdjustMidCluster::kToStart},

    // 1
    {u"012345678901234567890123456789",  // 1
     TextDirection::kRtl,
     {0, 1, 4, 5, 12, 18, 30, 32},
#if BUILDFLAG(IS_APPLE)
     {202.148, 195.410, 175.195, 168.457, 121.289, 80.859, 0, 0},
#else
     {210, 203, 182, 175, 126, 84, 0, 0},
#endif
     true,
     AdjustMidCluster::kToStart},

    // 2
    {u"0ff1ff23fff456ffff7890fffff12345ffffff6789",
     TextDirection::kLtr,
     {0, 1, 4, 5, 12, 18, 42, 43},
#if BUILDFLAG(IS_APPLE)
     {0, 6.738, 21.809, 25.975, 62.85, 92.994, 226.418, 0},
#else
     {0, 7, 22, 26, 63, 93, 228, 0},
#endif
     true,
     AdjustMidCluster::kToStart},

    // 3
    {u"0ff1ff23fff456ffff7890fffff12345ffffff6789",
     TextDirection::kRtl,
     {0, 1, 4, 5, 12, 18, 42, 43},
#if BUILDFLAG(IS_APPLE)
     {226.418, 219.680, 204.609, 200.443, 163.564, 133.424, 0, 0},
#else
     {228, 221, 206, 202, 165, 135, 0, 0},
#endif
     true,
     AdjustMidCluster::kToStart},

    // 4
    {u"مَ1مَمَ2مَمَمَ3مَمَمَمَ4مَمَمَمَمَ5مَمَمَمَمَمَ",
     TextDirection::kLtr,
     {0, 1, 2, 3, 4, 5, 10, 15, 20, 30, 47},
#if BUILDFLAG(IS_APPLE)
     {0, 0, 5.865, 12.727, 12.727, 19.061, 37.723, 55.008, 66.299, 99.832,
      148.746},
#elif BUILDFLAG(IS_WIN)
     {0, 0, 6, 13, 13, 19, 37, 54, 65, 98, 146},
#else
     {0, 0, 6, 13, 13, 20, 40, 58, 70, 105, 156},
#endif
     false,
     AdjustMidCluster::kToStart},

    // 5
    {u"مَ1مَمَ2مَمَمَ3مَمَمَمَ4مَمَمَمَمَ5مَمَمَمَمَمَ",
     TextDirection::kLtr,
     {0, 1, 2, 3, 4, 5, 10, 15, 20, 30, 47},
#if BUILDFLAG(IS_APPLE)
     {0, 5.865, 5.865, 12.727, 19.061, 19.061, 37.723, 55.008, 71.256, 99.832,
      148.746},
#elif BUILDFLAG(IS_WIN)
     {0, 6, 6, 13, 19, 19, 37, 54, 70, 98, 146},
#else
     {0, 6, 6, 13, 20, 20, 40, 58, 75, 105, 156},
#endif
     false,
     AdjustMidCluster::kToEnd},

    // 6
    {u"مَ1مَمَ2مَمَمَ3مَمَمَمَ4مَمَمَمَمَ5مَمَمَمَمَمَ",
     TextDirection::kRtl,
     {0, 1, 2, 3, 4, 5, 10, 15, 20, 30, 47},
#if BUILDFLAG(IS_APPLE)
     {148.746, 148.746, 142.881, 136.02, 136.02, 130.553, 111.891, 93.738,
      83.315, 49.781, 0},
#elif BUILDFLAG(IS_WIN)
     {146, 146, 140, 133, 133, 128, 110, 92, 82, 49, 0},
#else
     {156, 156, 150, 143, 143, 137, 117, 98, 87, 52, 0},
#endif
     false,
     AdjustMidCluster::kToStart},

    // 7
    {u"مَ1مَمَ2مَمَمَ3مَمَمَمَ4مَمَمَمَمَ5مَمَمَمَمَمَ",
     TextDirection::kRtl,
     {0, 1, 2, 3, 4, 5, 10, 15, 20, 30, 47},
#if BUILDFLAG(IS_APPLE)
     {148.746, 142.881, 142.881, 136.02, 130.553, 130.553, 111.891, 93.738,
      78.357, 49.781, 0},
#elif BUILDFLAG(IS_WIN)
     {146, 140, 140, 133, 128, 128, 110, 92, 77, 49, 0},
#else
     {156, 150, 150, 143, 137, 137, 117, 98, 82, 52, 0},
#endif
     false,
     AdjustMidCluster::kToEnd},
};
class CaretPositionForOffsetText
    : public ShapeResultTest,
      public testing::WithParamInterface<CaretPositionForOffsetTextData> {};
INSTANTIATE_TEST_SUITE_P(
    ShapeResultTest,
    CaretPositionForOffsetText,
    testing::ValuesIn(caret_position_for_offset_test_data));

TEST_P(CaretPositionForOffsetText, CaretPositionForOffsets) {
  const auto& test_data = GetParam();
  String text_string(test_data.string);
  HarfBuzzShaper shaper(text_string);
  scoped_refptr<ShapeResult> result = shaper.Shape(
      test_data.is_latin ? &font : &arabic_font, test_data.direction);
  StringView text_view(text_string);

  for (wtf_size_t i = 0; i < test_data.offsets.size(); ++i) {
    EXPECT_NEAR(test_data.positions[i],
                result->CaretPositionForOffset(test_data.offsets[i], text_view,
                                               test_data.adjust_mid_cluster),
                0.01f);
  }
}

}  // namespace blink
