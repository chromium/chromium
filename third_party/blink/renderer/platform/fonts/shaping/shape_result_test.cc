// Copyright (c) 2019 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class ShapeResultTest : public testing::Test {
 protected:
  void SetUp() override {
    font_description.SetComputedSize(12.0);
    font = Font(font_description);
    font.Update(nullptr);

    FontDescription::VariantLigatures ligatures;
    arabic_font = blink::test::CreateTestFont(
        "Noto",
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

TEST_F(ShapeResultTest, ComputeInkBoundsWithZeroOffset) {
  String string(u"abc");
  HarfBuzzShaper shaper(string);
  auto result = shaper.Shape(&font, TextDirection::kLtr);
  EXPECT_FALSE(HasNonZeroGlyphOffsets(*result));
  EXPECT_FALSE(result->ComputeInkBounds().IsZero());
}

// TDOO(yosin): We should use a font including U+0A81 or other code point
// having non-zero glyph offset.
TEST_F(ShapeResultTest, DISABLED_ComputeInkBoundsWithNonZeroOffset) {
  // U+0A81 has non-zero glyph offset
  String string(u"xy\u0A81z");
  HarfBuzzShaper shaper(string);
  auto result = shaper.Shape(&font, TextDirection::kLtr);
  ASSERT_TRUE(HasNonZeroGlyphOffsets(*result));
  EXPECT_FALSE(result->ComputeInkBounds().IsZero());
}

}  // namespace blink
