// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_test_utilities.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_test_info.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {
class FontsHolder : public GarbageCollected<FontsHolder> {
 public:
  void Trace(Visitor* visitor) const {
    for (const Font& font : fonts) {
      font.Trace(visitor);
    }
  }

  Font fonts[3];
};
}  // namespace

class ShapeResultTest : public FontTestBase {
 public:
  enum FontType {
    kLatinFont = 0,
    kArabicFont = 1,
    kCJKFont = 2,
  };

 protected:
  void SetUp() override {
    FontDescription::VariantLigatures ligatures;
    fonts_holder = MakeGarbageCollected<FontsHolder>();
    fonts_holder->fonts[0] = blink::test::CreateTestFont(
        AtomicString("Roboto"),
        blink::test::PlatformTestDataPath(
            "third_party/Roboto/roboto-regular.woff2"),
        12.0, &ligatures);

    fonts_holder->fonts[1] = blink::test::CreateTestFont(
        AtomicString("Noto"),
        blink::test::PlatformTestDataPath(
            "third_party/Noto/NotoNaskhArabic-regular.woff2"),
        12.0, &ligatures);

    fonts_holder->fonts[2] = blink::test::CreateTestFont(
        AtomicString("M PLUS 1p"),
        blink::test::BlinkWebTestsFontsTestDataPath("mplus-1p-regular.woff"),
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

  ShapeResult* CreateShapeResult(TextDirection direction) const {
    return MakeGarbageCollected<ShapeResult>(direction == TextDirection::kLtr
                                                 ? GetFont(kLatinFont)
                                                 : GetFont(kArabicFont),
                                             0, 0, direction);
  }

  const Font* GetFont(FontType type) const {
    return fonts_holder->fonts + static_cast<size_t>(type);
  }

  FontCachePurgePreventer font_cache_purge_preventer;
  FontDescription font_description;
  Persistent<FontsHolder> fonts_holder;
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

  ShapeResult* reference[num_ranges];
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

  ShapeResult* reference[num_ranges];
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
}

TEST_F(ShapeResultTest, CopyRangeLatin) {
  String string = "Testing ShapeResultIterator::CopyRange";
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(GetFont(kLatinFont), direction);
  TestCopyRangesLatin(result);
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
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(GetFont(kLatinFont), 0, 0, direction);
  shaper_a.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 5u, result);
  shaper_b.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 2u, result);
  shaper_c.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 25u, result);
  shaper_d.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 6u, result);
  TestCopyRangesLatin(result);
}

TEST_F(ShapeResultTest, CopyRangeLatinMultiRunWithHoles) {
  TextDirection direction = TextDirection::kLtr;
  String string = "Testing copying a range with holes";
  HarfBuzzShaper shaper_a(string.Substring(0, 5));
  HarfBuzzShaper shaper_b(string.Substring(5, 7));
  HarfBuzzShaper shaper_c(string.Substring(7, 32));
  HarfBuzzShaper shaper_d(string.Substring(32, 34));

  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(GetFont(kLatinFont), 0, 0, direction);
  shaper_a.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 5u, result);
  shaper_b.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 2u, result);
  shaper_c.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 25u, result);
  shaper_d.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 2u, result);

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

  ShapeResult* reference[3];
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
}

TEST_F(ShapeResultTest, CopyRangeArabic) {
  // نص اختبار العربية
  String string(
      u"\u0646\u0635\u0627\u062E\u062A\u0628\u0627\u0631\u0627\u0644\u0639"
      u"\u0631\u0628\u064A\u0629");
  TextDirection direction = TextDirection::kRtl;

  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(GetFont(kArabicFont), direction);
  TestCopyRangesArabic(result);
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
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(GetFont(kArabicFont), 0, 0, direction);
  shaper_a.Shape(GetFont(kArabicFont), direction)->CopyRange(0u, 2u, result);
  shaper_b.Shape(GetFont(kArabicFont), direction)->CopyRange(0u, 7u, result);
  shaper_c.Shape(GetFont(kArabicFont), direction)->CopyRange(0u, 8u, result);

  TestCopyRangesArabic(result);
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
  const ShapeResult* result = shaper.Shape(GetFont(kLatinFont), data.direction);
  if (data.end_offset)
    result = result->SubRange(data.start_offset, data.end_offset);
  EXPECT_EQ(result->IsStartSafeToBreak(), data.expected);
}

TEST_F(ShapeResultTest, AddUnsafeToBreakLtr) {
  HarfBuzzShaper shaper(u"ABC\u3042DEFG");
  ShapeResult* result = shaper.Shape(GetFont(kLatinFont), TextDirection::kLtr);
  Vector<unsigned> offsets{2, 5};
  for (const unsigned offset : offsets) {
    EXPECT_EQ(result->NextSafeToBreakOffset(offset), offset);
  }
  result->AddUnsafeToBreak(offsets);
  result->EnsurePositionData();
  for (const unsigned offset : offsets) {
    EXPECT_NE(result->NextSafeToBreakOffset(offset), offset);
    EXPECT_NE(result->CachedNextSafeToBreakOffset(offset), offset);
  }
}

TEST_F(ShapeResultTest, AddUnsafeToBreakRtl) {
  HarfBuzzShaper shaper(u"\u05d0\u05d1\u05d2\u05d3\u05d4\u05d5");
  ShapeResult* result = shaper.Shape(GetFont(kArabicFont), TextDirection::kRtl);
  Vector<unsigned> offsets{2, 5};
  for (const unsigned offset : offsets) {
    EXPECT_EQ(result->NextSafeToBreakOffset(offset), offset);
  }
  result->AddUnsafeToBreak(offsets);
  result->EnsurePositionData();
  for (const unsigned offset : offsets) {
    EXPECT_NE(result->NextSafeToBreakOffset(offset), offset);
    EXPECT_NE(result->CachedNextSafeToBreakOffset(offset), offset);
  }
}

TEST_F(ShapeResultTest, AddUnsafeToBreakRange) {
  const String string{u"0ABC\u3042DEFG"};
  HarfBuzzShaper shaper(string);
  ShapeResult* result = shaper.Shape(GetFont(kLatinFont), TextDirection::kLtr,
                                     1, string.length());
  Vector<unsigned> offsets{2, 5, 7};
  for (const unsigned offset : offsets) {
    EXPECT_EQ(result->NextSafeToBreakOffset(offset), offset);
  }
  result->AddUnsafeToBreak(offsets);
  result->EnsurePositionData();
  for (const unsigned offset : offsets) {
    EXPECT_NE(result->NextSafeToBreakOffset(offset), offset);
    EXPECT_NE(result->CachedNextSafeToBreakOffset(offset), offset);
  }
}

TEST_F(ShapeResultTest, ComputeInkBoundsWithZeroOffset) {
  String string(u"abc");
  HarfBuzzShaper shaper(string);
  const auto* result = shaper.Shape(GetFont(kLatinFont), TextDirection::kLtr);
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
  ShapeResult* result = shaper.Shape(GetFont(kLatinFont), TextDirection::kLtr);

  // Record the position before applying text-autospace, and fill the spacing
  // widths with different values.
  Vector<float> before_adding_spacing =
      RecordPositionBeforeApplyingSpacing(result, string.length());
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
  const auto* result = shaper.Shape(GetFont(kLatinFont), TextDirection::kLtr);
  ASSERT_TRUE(HasNonZeroGlyphOffsets(*result));
  EXPECT_FALSE(result->ComputeInkBounds().IsEmpty());
}

// Tests for CaretPositionForOffset
struct CaretPositionForOffsetTestData {
  // The string that should be processed.
  const UChar* string;
  // Text direction to test
  TextDirection direction;
  // The offsets to test.
  std::vector<wtf_size_t> offsets;
  // Expected positions.
  std::vector<float> positions;
  // The font to use
  ShapeResultTest::FontType font;
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
     ShapeResultTest::kLatinFont,
     AdjustMidCluster::kToStart},

    // 1
    {u"012345678901234567890123456789",
     TextDirection::kRtl,
     {0, 1, 4, 5, 12, 18, 30, 32},
#if BUILDFLAG(IS_APPLE)
     {202.148, 195.410, 175.195, 168.457, 121.289, 80.859, 0, 0},
#else
     {210, 203, 182, 175, 126, 84, 0, 0},
#endif
     ShapeResultTest::kLatinFont,
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
     ShapeResultTest::kLatinFont,
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
     ShapeResultTest::kLatinFont,
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
     ShapeResultTest::kArabicFont,
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
     ShapeResultTest::kArabicFont,
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
     ShapeResultTest::kArabicFont,
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
     ShapeResultTest::kArabicFont,
     AdjustMidCluster::kToEnd},

    // 8
    {u"あ1あمَ2あمَあ3あمَあمَ4あمَあمَあ5",
     TextDirection::kLtr,
     {0, 1, 2, 3, 4, 5, 10, 15, 20, 25, 26},
#if BUILDFLAG(IS_APPLE)
     {0, 12, 18.86, 30.86, 30.86, 36.73, 73.45, 110.18, 134.91, 170.64, 177.5},
#else
     {0, 12, 19, 31, 31, 37, 74, 111, 136, 172, 179},
#endif
     ShapeResultTest::kArabicFont,
     AdjustMidCluster::kToStart},

    // 9
    {u"あ1あمَ2あمَあ3あمَあمَ4あمَあمَあ5",
     TextDirection::kLtr,
     {0, 1, 2, 3, 4, 5, 10, 15, 20, 26},
#if BUILDFLAG(IS_APPLE)
     {0, 12, 18.86, 30.86, 36.73, 36.73, 73.45, 110.18, 140.77, 177.5},
#else
     {0, 12, 19, 31, 37, 37, 74, 111, 142, 179},
#endif
     ShapeResultTest::kArabicFont,
     AdjustMidCluster::kToEnd},

    // 10
    {u"あ1あمَ2あمَあ3あمَあمَ4あمَあمَあ5",
     TextDirection::kRtl,
     {0, 1, 2, 3, 4, 5, 10, 15, 20, 26},
#if BUILDFLAG(IS_APPLE)
     {177.5, 165.5, 158.64, 146.64, 146.64, 140.77, 104.04, 67.32, 42.59, 0},
#else
     {179, 167, 160, 148, 148, 142, 105, 68, 43, 0},
#endif
     ShapeResultTest::kArabicFont,
     AdjustMidCluster::kToStart},

    // 11
    {u"あ1あمَ2あمَあ3あمَあمَ4あمَあمَあ5",
     TextDirection::kRtl,
     {0, 1, 2, 3, 4, 5, 10, 15, 20, 26},
#if BUILDFLAG(IS_APPLE)
     {177.5, 165.5, 158.64, 146.64, 140.77, 140.77, 104.04, 67.32, 36.73, 0},
#else
     {179, 167, 160, 148, 142, 142, 105, 68, 37, 0},
#endif
     ShapeResultTest::kArabicFont,
     AdjustMidCluster::kToEnd},

    // 12
    {u"楽しいドライブ、012345楽しいドライブ、",
     TextDirection::kLtr,
     {0, 1, 2, 3, 4, 5, 10, 15, 20, 22},
#if BUILDFLAG(IS_APPLE)
     {0, 12, 24, 36, 48, 60, 110.88, 152.64, 212.63, 236.64},
#else
     {0, 12, 24, 36, 48, 60, 110, 150, 210, 234},
#endif
     ShapeResultTest::kCJKFont,
     AdjustMidCluster::kToStart},

    // 13
    {u"楽しいドライブ、012345楽しいドライブ、",
     TextDirection::kLtr,
     {0, 1, 2, 3, 4, 5, 10, 15, 20, 22},
#if BUILDFLAG(IS_APPLE)
     {0, 12, 24, 36, 48, 60, 110.88, 152.64, 212.63, 236.64},
#else
     {0, 12, 24, 36, 48, 60, 110, 150, 210, 234},
#endif
     ShapeResultTest::kCJKFont,
     AdjustMidCluster::kToEnd},

    // 14
    {u"楽しいドライブ、012345楽しいドライブ、",
     TextDirection::kRtl,
     {0, 1, 2, 3, 4, 5, 10, 15, 20, 22},
#if BUILDFLAG(IS_APPLE)
     {236.64, 224.64, 212.64, 200.64, 188.64, 176.64, 125.76, 84, 24, 0},
#else
     {234, 222, 210, 198, 186, 174, 124, 84, 24, 0},
#endif
     ShapeResultTest::kCJKFont,
     AdjustMidCluster::kToStart},

    // 15
    {u"楽しいドライブ、012345楽しいドライブ、",
     TextDirection::kRtl,
     {0, 1, 2, 3, 4, 5, 10, 15, 20, 22},
#if BUILDFLAG(IS_APPLE)
     {236.64, 224.64, 212.64, 200.64, 188.64, 176.64, 125.76, 84, 24, 0},
#else
     {234, 222, 210, 198, 186, 174, 124, 84, 24, 0},
#endif
     ShapeResultTest::kCJKFont,
     AdjustMidCluster::kToEnd},
};
class CaretPositionForOffsetTest
    : public ShapeResultTest,
      public testing::WithParamInterface<CaretPositionForOffsetTestData> {};
INSTANTIATE_TEST_SUITE_P(
    ShapeResult,
    CaretPositionForOffsetTest,
    testing::ValuesIn(caret_position_for_offset_test_data));

TEST_P(CaretPositionForOffsetTest, CaretPositionForOffsets) {
  const auto& test_data = GetParam();
  String text_string(test_data.string);
  HarfBuzzShaper shaper(text_string);
  const ShapeResult* result =
      shaper.Shape(GetFont(test_data.font), test_data.direction);
  StringView text_view(text_string);

  for (wtf_size_t i = 0; i < test_data.offsets.size(); ++i) {
    EXPECT_NEAR(test_data.positions[i],
                result->CaretPositionForOffset(test_data.offsets[i], text_view,
                                               test_data.adjust_mid_cluster),
                0.01f);
  }
}

// Tests for OffsetForPosition
struct CaretOffsetForPositionTestData {
  // The string that should be processed.
  const UChar* string;
  // Text direction to test
  TextDirection direction;
  // The positions to test.
  std::vector<wtf_size_t> positions;
  // The expected offsets.
  std::vector<wtf_size_t> offsets;
  // The font to use
  ShapeResultTest::FontType font;
  // IncludePartialGlyphsOption value
  IncludePartialGlyphsOption partial_glyphs_option;
  // BreakGlyphsOption value
  BreakGlyphsOption break_glyphs_option;
} caret_offset_for_position_test_data[] = {
    // 0
    {u"0123456789",
     TextDirection::kLtr,
#if BUILDFLAG(IS_APPLE)
     {1,  6,  7,  13, 14, 20, 21, 26, 27, 33,
      34, 40, 41, 47, 48, 53, 54, 60, 61, 67},
     {0,  0,  1,  1,  2,  2,  3,  3,  4,  4,
      5,  5,  6,  6,  7,  7,  8,  8,  9,  9},
#else
     {1,  6,  7,  13, 14, 20, 21, 27, 28, 34,
      35, 41, 42, 48, 49, 55, 56, 62, 63, 69},
     {0,  0,  1,  1,  2,  2,  3,  3,  4,  4,
      5,  5,  6,  6,  7,  7,  8,  8,  9,  9},
#endif
     ShapeResultTest::kLatinFont,
     kOnlyFullGlyphs,
     BreakGlyphsOption(false)},

    // 1
    {u"0123456789",
     TextDirection::kRtl,
#if BUILDFLAG(IS_APPLE)
     {1,  6,  7,  13, 14, 20, 21, 26, 27, 33,
      34, 40, 41, 47, 48, 53, 54, 60, 61, 67},
     {9,  9,  8,  8,  7,  7,  6,  6,  5,  5,
      4,  4,  3,  3,  2,  2,  1,  1,  0,  0},
#else
     {1,  7,  8,  14, 15, 21, 22, 28, 29, 35,
      36, 42, 43, 49, 50, 56, 57, 63, 64, 69},
     {9,  9,  8,  8,  7,  7,  6,  6,  5,  5,
      4,  4,  3,  3,  2,  2,  1,  1,  0,  0},
#endif
     ShapeResultTest::kLatinFont,
     kOnlyFullGlyphs,
     BreakGlyphsOption(false)},

    // 2
    {u"0ff1fff23ff",
     TextDirection::kLtr,
#if BUILDFLAG(IS_APPLE)
     {1,  6,  7,  10, 11, 15, 16, 21, 22, 25, 26,
      30, 31, 34, 35, 41, 42, 47, 48, 51, 52, 56},
     {0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,
      5,  6,  6,  7,  7,  8,  8,  9,  9,  10, 10},
#else
     {1,  6,  7,  10, 11, 14, 15, 21, 22, 25, 26,
      29, 30, 33, 34, 40, 41, 47, 48, 51, 52, 55},
     {0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,
      5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10},
#endif
     ShapeResultTest::kLatinFont,
     kOnlyFullGlyphs,
     BreakGlyphsOption(false)},

    // 3
    {u"0ff1fff23ff",
     TextDirection::kRtl,
#if BUILDFLAG(IS_APPLE)
     {1,  4,  5,  8,  9,  15, 16, 21, 22, 25, 26,
      30, 31, 34, 35, 41, 42, 45, 46, 49, 50, 56},
     {10, 10, 9,  9,  8,  8,  7,  7,  6,  6,  5,
      5,  4,  4,  3,  3,  2,  2,  1,  1,  0,  0},
#else
     {1,  4,  5,  8,  9,  15, 16, 22, 23, 26, 27,
      30, 31, 34, 35, 41, 42, 45, 46, 49, 50, 55},
     {10, 10, 9,  9,  8,  8,  7,  7,  6,  6,  5,
      5,  4,  4,  3,  3,  2,  2,  1,  1,  0,  0},
#endif
     ShapeResultTest::kLatinFont,
     kOnlyFullGlyphs,
     BreakGlyphsOption(false)},

    // 4
    {u"مَ1مَمَ2مَمَمَ3",
     TextDirection::kLtr,
#if BUILDFLAG(IS_APPLE)
     {1, 5, 6, 12, 13, 19, 20, 24, 25, 31, 32, 37, 38, 42, 43, 48, 49, 55},
     {0, 0, 2, 2,  3,  3,  5,  5,  7,  7,  8,  8,  10, 10, 12, 12, 14, 14},
#elif BUILDFLAG(IS_WIN)
     {1, 5, 6, 12, 13, 18, 19, 23, 24, 30, 31, 36, 37, 41, 42, 46, 47, 53},
     {0, 0, 2, 2,  3,  3,  5,  5,  7,  7,  8,  8,  10, 10, 12, 12, 14, 14},
#else
     {1, 5, 6, 12, 13, 19, 20, 25, 26, 32, 33, 39, 40, 44, 45, 50, 51, 57},
     {0, 0, 2, 2,  3,  3,  5,  5,  7,  7,  8,  8,  10, 10, 12, 12, 14, 14},
#endif
     ShapeResultTest::kArabicFont,
     kOnlyFullGlyphs,
     BreakGlyphsOption(false)},

    // 5
    {u"مَ1مَمَ2مَمَمَ3",
     TextDirection::kLtr,
#if BUILDFLAG(IS_APPLE)
     {1,  2,  3,  9,  10, 15, 16, 21, 22, 27,
      28, 34, 35, 40, 41, 45, 46, 51, 52, 55},
     {0,  0,  2,  2,  3,  3,  5,  5,  7,  7,
      8,  8,  10, 10, 12, 12, 14, 14, 15, 15},
#elif BUILDFLAG(IS_WIN)
     {1,  3,  4,  9,  10, 16, 17, 21, 22, 27,
      28, 34, 35, 39, 40, 44, 45, 50, 51, 53},
     {0,  0,  2,  2,  3,  3,  5,  5,  7,  7,
      8,  8,  10, 10, 12, 12, 14, 14, 15, 15},
#else
     {1,  3,  4,  9,  10, 16, 17, 23, 24, 29,
      30, 36, 37, 42, 43, 48, 49, 54, 55, 57},
     {0,  0,  2,  2,  3,  3,  5,  5,  7,  7,
      8,  8,  10, 10, 12, 12, 14, 14, 15, 15},
#endif
     ShapeResultTest::kArabicFont,
     kIncludePartialGlyphs,
     BreakGlyphsOption(false)},

    // 6
    {u"مَ1مَمَ2مَمَمَ3",
     TextDirection::kLtr,
#if BUILDFLAG(IS_APPLE)
     {1,  2,  3,  9,  10, 15, 16, 21, 22, 27,
      28, 34, 35, 40, 41, 45, 46, 51, 52, 55},
     {0,  0,  2,  2,  3,  3,  5,  5,  7,  7,
      8,  8,  10, 10, 12, 12, 14, 14, 15, 15},
#elif BUILDFLAG(IS_WIN)
     {1,  3,  4,  9,  10, 16, 17, 21, 22, 27,
      28, 34, 35, 39, 40, 44, 45, 50, 51, 54},
     {0,  0,  2,  2,  3,  3,  5,  5,  7,  7,
      8,  8,  10, 10, 12, 12, 14, 14, 15, 15},
#else
     {1,  3,  4,  9,  10, 16, 17, 23, 24, 29,
      30, 36, 37, 42, 43, 48, 49, 54, 55, 57},
     {0,  0,  2,  2,  3,  3,  5,  5,  7,  7,
      8,  8,  10, 10, 12, 12, 14, 14, 15, 15},
#endif
     ShapeResultTest::kArabicFont,
     kIncludePartialGlyphs,
     BreakGlyphsOption(true)},

    // 7
    {u"مَ1مَمَ2مَمَمَ3",
     TextDirection::kRtl,
#if BUILDFLAG(IS_APPLE)
     {1,  6,  7,  13, 14, 18, 19, 23, 24, 30, 31, 36, 37, 42, 43, 49, 50, 55},
     {14, 14, 12, 12, 10, 10, 8,  8,  7,  7,  5,  5,  3,  3,  2,  2,  0,  0},
#elif BUILDFLAG(IS_WIN)
     {1,  7,  8,  13, 14, 18, 19, 23, 24, 30, 31, 36, 37, 41, 42, 48, 49, 53},
     {14, 14, 12, 12, 10, 10, 8,  8,  7,  7,  5,  5,  3,  3,  2,  2,  0,  0},
#else
     {1,  7,  8,  14, 15, 19, 20, 25, 26, 32, 33, 39, 40, 45, 46, 52, 53, 57},
     {14, 14, 12, 12, 10, 10, 8,  8,  7,  7,  5,  5,  3,  3,  2,  2,  0,  0},
#endif
     ShapeResultTest::kArabicFont,
     kOnlyFullGlyphs,
     BreakGlyphsOption(false)},

    // 8
    {u"مَ1مَمَ2مَمَمَ3",
     TextDirection::kRtl,
#if BUILDFLAG(IS_APPLE)
     {1,  3,  4,  10, 11, 15, 16, 20, 21, 27,
      28, 33, 34, 39, 40, 45, 46, 52, 53, 55},
     {15, 15, 14, 14, 12, 12, 10, 10, 8,  8,
      7,  7,  5,  5,  3,  3,  2,  2,  0,  0},
#elif BUILDFLAG(IS_WIN)
     {1,  3,  4,  10, 11, 15, 16, 20, 21, 26,
      27, 33, 34, 38, 39, 44, 45, 51, 52, 53},
     {15, 15, 14, 14, 12, 12, 10, 10, 8,  8,
      7,  7,  5,  5,  3,  3,  2,  2,  0,  0},
#else
     {1,  3,  4,  10, 11, 16, 17, 22, 23, 28,
      29, 35, 36, 42, 43, 48, 49, 55, 56, 57},
     {15, 15, 14, 14, 12, 12, 10, 10, 8,  8,
      7,  7,  5,  5,  3,  3,  2,  2,  0, 0},
#endif
     ShapeResultTest::kArabicFont,
     kIncludePartialGlyphs,
     BreakGlyphsOption(true)},

    // 9
    {u"あ1あمَ2あمَあ",
     TextDirection::kLtr,
#if BUILDFLAG(IS_APPLE)
     {0, 11, 12, 18, 19, 30, 31, 36, 37, 43, 44, 55, 56, 61, 62, 73},
     {0, 0,  1,  1,  2,  2,  3,  3,  5,  5,  6,  6,  7,  7,  9,  9},
#else
     {1, 11, 12, 18, 19, 30, 31, 36, 37, 43, 44, 55, 56, 61, 62, 73},
     {0, 0,  1,  1,  2,  2,  3,  3,  5,  5,  6,  6,  7,  7,  9,  9},
#endif
     ShapeResultTest::kArabicFont,
     kOnlyFullGlyphs,
     BreakGlyphsOption(false)},

    // 10
    {u"あ1あمَ2あمَあ",
     TextDirection::kLtr,
#if BUILDFLAG(IS_APPLE)
     {1, 6, 7, 15, 16, 24, 25, 33, 34, 40, 41, 49, 50, 58, 59, 67, 68, 73},
     {0, 0, 1, 1,  2,  2,  3,  3,  5,  5,  6,  6,  7,  7,  9,  9,  10, 10},
#else
     {1, 6, 7, 15, 16, 25, 26, 34, 35, 40, 41, 50, 51, 59, 60, 68, 69, 73},
     {0, 0, 1, 1,  2,  2,  3,  3,  5,  5,  6,  6,  7,  7,  9,  9,  10, 10},
#endif
     ShapeResultTest::kArabicFont,
     kIncludePartialGlyphs,
     BreakGlyphsOption(false)},

    // 11
    {u"あ1あمَ2あمَあ",
     TextDirection::kLtr,
#if BUILDFLAG(IS_APPLE)
     {1, 6, 7, 15, 16, 24, 25, 33, 34, 40, 41, 49, 50, 58, 59, 67, 68, 73},
     {0, 0, 1, 1,  2,  2,  3,  3,  5,  5,  6,  6,  7,  7,  9,  9,  10, 10},
#else
     {1, 6, 7, 15, 16, 25, 26, 34, 35, 40, 41, 50, 51, 59, 60, 68, 69, 73},
     {0, 0, 1, 1,  2,  2,  3,  3,  5,  5,  6,  6,  7,  7,  9,  9,  10, 10},
#endif
     ShapeResultTest::kArabicFont,
     kIncludePartialGlyphs,
     BreakGlyphsOption(true)},

    // 12
    {u"あ1あمَ2あمَあ",
     TextDirection::kRtl,
#if BUILDFLAG(IS_APPLE)
     {1, 12, 13, 17, 18, 29, 30, 36, 37, 42, 43, 54, 55, 61, 62, 73},
     {9, 9,  7,  7,  6,  6,  5,  5,  3,  3,  2,  2,  1,  1,  0,  0},
#else
     {1, 12, 13, 18, 19, 30, 31, 37, 38, 43, 44, 55, 56, 62, 63, 74},
     {9, 9,  7,  7,  6,  6,  5,  5,  3,  3,  2,  2,  1,  1,  0,  0},
#endif
     ShapeResultTest::kArabicFont,
     kOnlyFullGlyphs,
     BreakGlyphsOption(false)},

    // 13
    {u"あ1あمَ2あمَあ",
     TextDirection::kRtl,
#if BUILDFLAG(IS_APPLE)
     {1,  6,  7, 14, 15, 23, 24, 33, 34, 39, 40, 48, 49, 58, 59, 67, 68, 73},
     {10, 10, 9, 9,  7,  7,  6,  6,  5,  5,  3,  3,  2,  2,  1,  1,  0,  0},
#else
     {1,  6,  7, 15, 16, 24, 25, 33, 34, 40, 41, 49, 50, 58, 59, 68, 69, 73},
     {10, 10, 9, 9,  7,  7,  6,  6,  5,  5,  3,  3,  2,  2,  1,  1,  0,  0},
#endif
     ShapeResultTest::kArabicFont,
     kIncludePartialGlyphs,
     BreakGlyphsOption(true)},

    // 14
    {u"楽しいドライブ、0",
     TextDirection::kLtr,
#if BUILDFLAG(IS_APPLE)
     {1, 11, 12, 23, 24, 35, 36, 47, 48, 59, 60, 71, 72, 83, 84, 95, 96, 103},
     {0, 0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8},
#else
     {1, 11, 12, 23, 24, 35, 36, 47, 48, 59, 60, 71, 72, 83, 84, 95, 96, 102},
     {0, 0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8},
#endif
     ShapeResultTest::kCJKFont,
     kOnlyFullGlyphs,
     BreakGlyphsOption(false)},

    // 15
    {u"楽しいドライブ、0",
     TextDirection::kLtr,
#if BUILDFLAG(IS_APPLE)
     {1,  6,  7,  18, 19, 30, 31, 42, 43,  54,
      55, 66, 67, 78, 79, 90, 91, 99, 100, 103},
     {0,  0,  1,  1,  2,  2,  3,  3,  4,   4,
      5,  5,  6,  6,  7,  7,  8,  8,  9,   9},
#else
     {1,  6,  7,  18, 19, 30, 31, 42, 43,  54,
      55, 66, 67, 78, 79, 90, 91, 99, 100, 102},
     {0,  0,  1,  1,  2,  2,  3,  3,  4,   4,
      5,  5,  6,  6,  7,  7,  8,  8,  9,   9},
#endif
     ShapeResultTest::kCJKFont,
     kIncludePartialGlyphs,
     BreakGlyphsOption(true)},

    // 16
    {u"楽しいドライブ、0",
     TextDirection::kLtr,
#if BUILDFLAG(IS_APPLE)
     {1,  6,  7,  18, 19, 30, 31, 42, 43,  54,
      55, 66, 67, 78, 79, 90, 91, 99, 100, 103},
     {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9},
#else
     {1,  6,  7,  18, 19, 30, 31, 42, 43,  54,
      55, 66, 67, 78, 79, 90, 91, 99, 100, 102},
     {0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9},
#endif
     ShapeResultTest::kCJKFont,
     kIncludePartialGlyphs,
     BreakGlyphsOption(false)},

    // 17
    {u"楽しいドライブ、0",
     TextDirection::kRtl,
#if BUILDFLAG(IS_APPLE)
     {1, 7, 8, 19, 20, 31, 32, 43, 44, 55, 56, 67, 68, 79, 80, 91, 92, 103},
     {8, 8, 7, 7,  6,  6,  5,  5,  4,  4,  3,  3,  2,  2,  1,  1,  0,  0},
#else
     {1, 7, 8, 19, 20, 31, 32, 43, 44, 55, 56, 67, 68, 79, 80, 91, 92, 102},
     {8, 8, 7, 7,  6,  6,  5,  5,  4,  4,  3,  3,  2,  2,  1,  1,  0,  0},
#endif
     ShapeResultTest::kCJKFont,
     kOnlyFullGlyphs,
     BreakGlyphsOption(false)},

    // 18
    {u"楽しいドライブ、0",
     TextDirection::kRtl,
#if BUILDFLAG(IS_APPLE)
     {1,  3,  4,  13, 14, 25, 26, 37, 38, 49,
      50, 61, 62, 73, 74, 85, 86, 97, 98, 103},
     {9,  9,  8,  8,  7,  7,  6,  6,  5,  5,
      4,  4,  3,  3,  2,  2,  1,  1,  0,  0},
#else
     {1,  3,  4,  13, 14, 25, 26, 37, 38, 49,
      50, 61, 62, 73, 74, 85, 86, 97, 98, 102},
     {9,  9,  8,  8,  7,  7,  6,  6,  5,  5,
      4,  4,  3,  3,  2,  2,  1,  1,  0,  0},
#endif
     ShapeResultTest::kCJKFont,
     kIncludePartialGlyphs,
     BreakGlyphsOption(true)},
};
class CaretOffsetForPositionTest
    : public ShapeResultTest,
      public testing::WithParamInterface<CaretOffsetForPositionTestData> {};
INSTANTIATE_TEST_SUITE_P(
    ShapeResult,
    CaretOffsetForPositionTest,
    testing::ValuesIn(caret_offset_for_position_test_data));

TEST_P(CaretOffsetForPositionTest, OffsetForPositions) {
  const auto& test_data = GetParam();
  String text_string(test_data.string);
  HarfBuzzShaper shaper(text_string);
  const ShapeResult* result =
      shaper.Shape(GetFont(test_data.font), test_data.direction);
  StringView text_view(text_string);

  float text_width = result->Width();
  if (IsLtr(test_data.direction)) {
    EXPECT_EQ(0u, result->OffsetForPosition(-1, text_view,
                                            test_data.partial_glyphs_option,
                                            test_data.break_glyphs_option));
    EXPECT_EQ(0u, result->OffsetForPosition(0, text_view,
                                            test_data.partial_glyphs_option,
                                            test_data.break_glyphs_option));
    EXPECT_EQ(text_string.length(),
              result->OffsetForPosition(text_width, text_view,
                                        test_data.partial_glyphs_option,
                                        test_data.break_glyphs_option));
    EXPECT_EQ(text_string.length(),
              result->OffsetForPosition(text_width + 10, text_view,
                                        test_data.partial_glyphs_option,
                                        test_data.break_glyphs_option));
  } else {
    EXPECT_EQ(0u, result->OffsetForPosition(text_width + 10, text_view,
                                            test_data.partial_glyphs_option,
                                            test_data.break_glyphs_option));
    EXPECT_EQ(0u, result->OffsetForPosition(text_width, text_view,
                                            test_data.partial_glyphs_option,
                                            test_data.break_glyphs_option));
    EXPECT_EQ(
        text_string.length(),
        result->OffsetForPosition(0, text_view, test_data.partial_glyphs_option,
                                  test_data.break_glyphs_option));
    EXPECT_EQ(text_string.length(),
              result->OffsetForPosition(-1, text_view,
                                        test_data.partial_glyphs_option,
                                        test_data.break_glyphs_option));
  }

  for (wtf_size_t i = 0; i < test_data.positions.size(); i++) {
    EXPECT_EQ(test_data.offsets[i],
              result->OffsetForPosition(test_data.positions[i], text_view,
                                        test_data.partial_glyphs_option,
                                        test_data.break_glyphs_option))
        << "index " << i;
  }
}

}  // namespace blink
