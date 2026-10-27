// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_test_utilities.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_cursor.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_test_info.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {
class FontsHolder : public GarbageCollected<FontsHolder> {
 public:
  void Trace(Visitor* visitor) const {
    for (const Font* font : fonts) {
      font->Trace(visitor);
    }
  }

  std::array<Member<Font>, 3> fonts;
};
}  // namespace

class ShapeResultTest : public FontTestBase {
 public:
  enum FontType {
    kLatinFont = 0,
    kArabicFont = 1,
    kCjkFont = 2,
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

  static wtf_size_t PositionDataSize(const ShapeResult& result) {
    return result.character_position_.size();
  }

  static wtf_size_t PositionDataCapacity(const ShapeResult& result) {
    return result.character_position_.capacity();
  }

  static void ReserveSinglePositionEntry(ShapeResult& result) {
    result.character_position_.reserve(1);
  }

  static ShapeResultRun* InsertConstantAdvanceRun(ShapeResult* result,
                                                  unsigned start_index,
                                                  unsigned num_characters,
                                                  TextRunLayoutUnit advance,
                                                  bool compact) {
    ShapeResultRun* run = result->InsertRunForTesting(
        start_index, num_characters, result->Direction());
    for (HarfBuzzRunGlyphData& glyph : run->glyph_data_.MutableGlyphs()) {
      glyph.SetAdvance(advance);
      glyph.SetSafeToBreakBefore(SafeToBreak::kSafe);
    }
    run->width_ = InlineLayoutUnit::FromRawValue(int64_t{advance.RawValue()} *
                                                 num_characters)
                      .ToFloat();
    if (compact) {
      EXPECT_TRUE(run->glyph_data_.TryMakeCompact(run->IsRtl()));
    }
    return run;
  }

  ShapeResult* CreateShapeResult(TextDirection direction) const {
    return MakeGarbageCollected<ShapeResult>(0, 0, direction);
  }

  const Font* GetFont(FontType type) const {
    return fonts_holder->fonts.at(static_cast<size_t>(type));
  }

  FontDescription font_description;
  Persistent<FontsHolder> fonts_holder;
};

void ShapeResultTest::TestCopyRangesLatin(const ShapeResult* result) const {
  constexpr wtf_size_t kNumRanges = 4;
  std::array<ShapeResult::ShapeRange, kNumRanges> ranges = {{
      {0, 10, CreateShapeResult(TextDirection::kLtr)},
      {10, 20, CreateShapeResult(TextDirection::kLtr)},
      {20, 30, CreateShapeResult(TextDirection::kLtr)},
      {30, 38, CreateShapeResult(TextDirection::kLtr)},
  }};
  result->CopyRanges(ranges.data(), kNumRanges);

  std::array<Vector<ShapeResultTestGlyphInfo>, kNumRanges> glyphs;
  for (wtf_size_t i = 0; i < kNumRanges; ++i) {
    ComputeGlyphResults(*ranges[i].target, &glyphs[i]);
  }
  EXPECT_EQ(glyphs[0].size(), 10u);
  EXPECT_EQ(glyphs[1].size(), 10u);
  EXPECT_EQ(glyphs[2].size(), 10u);
  EXPECT_EQ(glyphs[3].size(), 8u);

  std::array<ShapeResult*, kNumRanges> reference;
  reference[0] = result->SubRange(0, 10);
  reference[1] = result->SubRange(10, 20);
  reference[2] = result->SubRange(20, 30);
  reference[3] = result->SubRange(30, 38);
  std::array<Vector<ShapeResultTestGlyphInfo>, kNumRanges> reference_glyphs;
  for (wtf_size_t i = 0; i < kNumRanges; ++i) {
    ComputeGlyphResults(*reference[i], &reference_glyphs[i]);
  }
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
  constexpr wtf_size_t kNumRanges = 4;
  std::array<ShapeResult::ShapeRange, kNumRanges> ranges = {{
      {0, 4, CreateShapeResult(TextDirection::kRtl)},
      {4, 7, CreateShapeResult(TextDirection::kRtl)},
      {7, 10, CreateShapeResult(TextDirection::kRtl)},
      {10, 15, CreateShapeResult(TextDirection::kRtl)},
  }};
  result->CopyRanges(ranges.data(), kNumRanges);

  std::array<Vector<ShapeResultTestGlyphInfo>, kNumRanges> glyphs;
  for (wtf_size_t i = 0; i < kNumRanges; ++i) {
    ComputeGlyphResults(*ranges[i].target, &glyphs[i]);
  }
  EXPECT_EQ(glyphs[0].size(), 4u);
  EXPECT_EQ(glyphs[1].size(), 3u);
  EXPECT_EQ(glyphs[2].size(), 3u);
  EXPECT_EQ(glyphs[3].size(), 5u);

  std::array<ShapeResult*, kNumRanges> reference;
  reference[0] = result->SubRange(0, 4);
  reference[1] = result->SubRange(4, 7);
  reference[2] = result->SubRange(7, 10);
  reference[3] = result->SubRange(10, 17);
  std::array<Vector<ShapeResultTestGlyphInfo>, kNumRanges> reference_glyphs;
  for (wtf_size_t i = 0; i < kNumRanges; ++i) {
    ComputeGlyphResults(*reference[i], &reference_glyphs[i]);
  }
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
  HarfBuzzShaper shaper_a(string.substr(0, 5));
  HarfBuzzShaper shaper_b(string.substr(5, 7));
  HarfBuzzShaper shaper_c(string.substr(7, 32));
  HarfBuzzShaper shaper_d(string.substr(32, 38));

  // Combine four separate results into a single one to ensure we have a result
  // with multiple runs.
  ShapeResult* result = MakeGarbageCollected<ShapeResult>(0, 0, direction);
  shaper_a.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 5u, result);
  shaper_b.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 2u, result);
  shaper_c.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 25u, result);
  shaper_d.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 6u, result);
  TestCopyRangesLatin(result);
}

TEST_F(ShapeResultTest, CopyRangeLatinMultiRunWithHoles) {
  TextDirection direction = TextDirection::kLtr;
  String string = "Testing copying a range with holes";
  HarfBuzzShaper shaper_a(string.substr(0, 5));
  HarfBuzzShaper shaper_b(string.substr(5, 7));
  HarfBuzzShaper shaper_c(string.substr(7, 32));
  HarfBuzzShaper shaper_d(string.substr(32, 34));

  ShapeResult* result = MakeGarbageCollected<ShapeResult>(0, 0, direction);
  shaper_a.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 5u, result);
  shaper_b.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 2u, result);
  shaper_c.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 25u, result);
  shaper_d.Shape(GetFont(kLatinFont), direction)->CopyRange(0u, 2u, result);

  auto ranges = std::to_array<ShapeResult::ShapeRange>({
      {4, 17, CreateShapeResult(TextDirection::kLtr)},
      {20, 23, CreateShapeResult(TextDirection::kLtr)},
      {25, 31, CreateShapeResult(TextDirection::kLtr)},
  });
  result->CopyRanges(ranges.data(), 3);
  std::array<Vector<ShapeResultTestGlyphInfo>, 3> glyphs;
  ComputeGlyphResults(*ranges[0].target, &glyphs[0]);
  ComputeGlyphResults(*ranges[1].target, &glyphs[1]);
  ComputeGlyphResults(*ranges[2].target, &glyphs[2]);
  EXPECT_EQ(glyphs[0].size(), 13u);
  EXPECT_EQ(glyphs[1].size(), 3u);
  EXPECT_EQ(glyphs[2].size(), 6u);

  std::array<ShapeResult*, 3> reference;
  reference[0] = result->SubRange(4, 17);
  reference[1] = result->SubRange(20, 23);
  reference[2] = result->SubRange(25, 31);
  std::array<Vector<ShapeResultTestGlyphInfo>, 3> reference_glyphs;
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

  HarfBuzzShaper shaper_a(string.substr(0, 2));
  HarfBuzzShaper shaper_b(string.substr(2, 9));
  HarfBuzzShaper shaper_c(string.substr(9, 15));

  // Combine three separate results into a single one to ensure we have a result
  // with multiple runs.
  ShapeResult* result = MakeGarbageCollected<ShapeResult>(0, 0, direction);
  shaper_a.Shape(GetFont(kArabicFont), direction)->CopyRange(0u, 2u, result);
  shaper_b.Shape(GetFont(kArabicFont), direction)->CopyRange(0u, 7u, result);
  shaper_c.Shape(GetFont(kArabicFont), direction)->CopyRange(0u, 8u, result);

  TestCopyRangesArabic(result);
}

static struct IsStartSafeToBreakData {
  bool expected;
  const char16_t* text;
  TextDirection direction = TextDirection::kLtr;
  wtf_size_t start_offset = 0;
  wtf_size_t end_offset = 0;
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
  Vector<wtf_size_t> offsets{2, 5};
  for (const wtf_size_t offset : offsets) {
    EXPECT_EQ(result->NextSafeToBreakOffset(offset), offset);
  }
  result->AddUnsafeToBreak(offsets);
  result->EnsurePositionData();
  for (const wtf_size_t offset : offsets) {
    EXPECT_NE(result->NextSafeToBreakOffset(offset), offset);
    EXPECT_NE(result->CachedNextSafeToBreakOffset(offset), offset);
  }
}

TEST_F(ShapeResultTest, AddUnsafeToBreakRtl) {
  HarfBuzzShaper shaper(u"\u05d0\u05d1\u05d2\u05d3\u05d4\u05d5");
  ShapeResult* result = shaper.Shape(GetFont(kArabicFont), TextDirection::kRtl);
  Vector<wtf_size_t> offsets{2, 5};
  for (const wtf_size_t offset : offsets) {
    EXPECT_EQ(result->NextSafeToBreakOffset(offset), offset);
  }
  result->AddUnsafeToBreak(offsets);
  result->EnsurePositionData();
  for (const wtf_size_t offset : offsets) {
    EXPECT_NE(result->NextSafeToBreakOffset(offset), offset);
    EXPECT_NE(result->CachedNextSafeToBreakOffset(offset), offset);
  }
}

TEST_F(ShapeResultTest, AddUnsafeToBreakRange) {
  const String string{u"0ABC\u3042DEFG"};
  HarfBuzzShaper shaper(string);
  ShapeResult* result = shaper.Shape(GetFont(kLatinFont), TextDirection::kLtr,
                                     1, string.length());
  Vector<wtf_size_t> offsets{2, 5, 7};
  for (const wtf_size_t offset : offsets) {
    EXPECT_EQ(result->NextSafeToBreakOffset(offset), offset);
  }
  result->AddUnsafeToBreak(offsets);
  result->EnsurePositionData();
  for (const wtf_size_t offset : offsets) {
    EXPECT_NE(result->NextSafeToBreakOffset(offset), offset);
    EXPECT_NE(result->CachedNextSafeToBreakOffset(offset), offset);
  }
}

class ShapeResultPositionCacheTest
    : public ShapeResultTest,
      public testing::WithParamInterface<TextDirection> {
 private:
  ScopedMonospaceGlyphCompactionForTest scoped_feature_{true};
};

INSTANTIATE_TEST_SUITE_P(ShapeResultTest,
                         ShapeResultPositionCacheTest,
                         testing::Values(TextDirection::kLtr,
                                         TextDirection::kRtl));

TEST_P(ShapeResultPositionCacheTest, PositionCompactionRespectsFeatureFlag) {
  constexpr unsigned kStart = 13;
  constexpr unsigned kGlyphsPerRun = 8;
  constexpr unsigned kNumCharacters = 2 * kGlyphsPerRun;
  for (bool enabled : {false, true}) {
    SCOPED_TRACE(enabled);
    ScopedMonospaceGlyphCompactionForTest scoped_feature(enabled);
    for (unsigned compact_runs : {0u, 1u, 2u}) {
      SCOPED_TRACE(compact_runs);
      ShapeResult* result =
          MakeGarbageCollected<ShapeResult>(kStart, kNumCharacters, GetParam());
      ShapeResult* reference =
          MakeGarbageCollected<ShapeResult>(kStart, kNumCharacters, GetParam());
      for (unsigned i = 0; i < 2; ++i) {
        const unsigned run_start = kStart + i * kGlyphsPerRun;
        InsertConstantAdvanceRun(result, run_start, kGlyphsPerRun,
                                 TextRunLayoutUnit(10), i < compact_runs);
        InsertConstantAdvanceRun(reference, run_start, kGlyphsPerRun,
                                 TextRunLayoutUnit(10), /*compact=*/false);
      }
      reference->EnsurePositionData(/*allow_compaction=*/false);
      result->EnsurePositionData();
      EXPECT_EQ(enabled || IsLtr(GetParam()) ? 1u : kNumCharacters,
                PositionDataSize(*result));
      EXPECT_EQ(reference->Width(), result->Width());
      for (unsigned offset = 0; offset <= kNumCharacters; ++offset) {
        EXPECT_EQ(reference->CachedPositionForOffset(offset),
                  result->CachedPositionForOffset(offset));
        if (offset < kNumCharacters) {
          EXPECT_EQ(reference->CachedNextSafeToBreakOffset(kStart + offset),
                    result->CachedNextSafeToBreakOffset(kStart + offset));
        }
        EXPECT_EQ(reference->CachedPreviousSafeToBreakOffset(kStart + offset),
                  result->CachedPreviousSafeToBreakOffset(kStart + offset));
      }
      for (int x = -1; x <= reference->Width() + 1; ++x) {
        EXPECT_EQ(reference->CachedOffsetForPosition(LayoutUnit(x)),
                  result->CachedOffsetForPosition(LayoutUnit(x)));
      }
    }
  }
}

TEST_P(ShapeResultPositionCacheTest,
       ExistingCompactPositionDataSurvivesFeatureDisable) {
  constexpr unsigned kNumCharacters = 16;
  for (bool compact_glyphs : {false, true}) {
    SCOPED_TRACE(compact_glyphs);
    ShapeResult* result =
        MakeGarbageCollected<ShapeResult>(0, kNumCharacters, GetParam());
    InsertConstantAdvanceRun(result, 0, kNumCharacters, TextRunLayoutUnit(10),
                             compact_glyphs);
    result->EnsurePositionData();
    ASSERT_EQ(1u, PositionDataSize(*result));

    ScopedMonospaceGlyphCompactionForTest scoped_feature(false);
    result->EnsurePositionData();
    EXPECT_EQ(1u, PositionDataSize(*result));
    for (unsigned offset = 0; offset <= kNumCharacters; ++offset) {
      const LayoutUnit position(
          (IsLtr(GetParam()) ? offset : kNumCharacters - offset) * 10);
      EXPECT_EQ(position, result->CachedPositionForOffset(offset));
      EXPECT_EQ(offset, result->CachedOffsetForPosition(position));
    }
    result->EnsurePositionData(/*allow_compaction=*/false);
    EXPECT_EQ(kNumCharacters, PositionDataSize(*result));
  }
}

TEST_P(ShapeResultPositionCacheTest, UnsafeBreaksPreventPositionCompaction) {
  ShapeResult* result = MakeGarbageCollected<ShapeResult>(0, 4, GetParam());
  result->InsertRunForTesting(0, 4, GetParam(), {0, 2});
  result->EnsurePositionData(/*allow_compaction=*/true);
  EXPECT_EQ(4u, PositionDataSize(*result));
  EXPECT_EQ(2u, result->CachedNextSafeToBreakOffset(1));
  EXPECT_EQ(0u, result->CachedPreviousSafeToBreakOffset(1));

  result = MakeGarbageCollected<ShapeResult>(0, 4, GetParam());
  result->InsertRunForTesting(0, 4, GetParam(), {1, 2, 3});
  result->EnsurePositionData(/*allow_compaction=*/true);
  EXPECT_EQ(4u, PositionDataSize(*result));
  EXPECT_EQ(1u, result->CachedNextSafeToBreakOffset(0));
}

TEST_P(ShapeResultPositionCacheTest, CompactPositionDataRebuildsFullTable) {
  constexpr wtf_size_t kNumCharacters = 8;
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(0, kNumCharacters, GetParam());
  result->InsertRunForTesting(0, kNumCharacters, GetParam(),
                              {0, 1, 2, 3, 4, 5, 6, 7});

  result->EnsurePositionData(/*allow_compaction=*/true);
  ASSERT_EQ(1u, PositionDataSize(*result));
  for (wtf_size_t offset = 0; offset < kNumCharacters; ++offset) {
    EXPECT_EQ(LayoutUnit(), result->CachedPositionForOffset(offset));
  }

  result->EnsurePositionData(/*allow_compaction=*/false);
  ASSERT_EQ(kNumCharacters, PositionDataSize(*result));
  for (wtf_size_t offset = 0; offset < kNumCharacters; ++offset) {
    const ShapeResultCharacterData& data = result->CharacterData(offset);
    EXPECT_EQ(LayoutUnit(), data.x_position);
    EXPECT_TRUE(data.is_cluster_base);
    EXPECT_TRUE(data.safe_to_break_before);
  }
}

TEST_F(ShapeResultTest, RtlCompactGlyphStorageBuildsPositionData) {
  ScopedMonospaceGlyphCompactionForTest enable_compaction(true);
  Font* font = test::CreateTestFont(
      AtomicString("Noto Mono"),
      test::BlinkWebTestsDir() +
          "/third_party/NotoMono-hintedLocalizedFamilyName/"
          "NotoMono-Regular.subset.ttf",
      16);
  ASSERT_TRUE(font->PrimaryFont()->IsFixedPitch());
  const String string = To16Bit("Thequickbrownfoxjumpsoverthelazydog");

  ShapeResult* full_result;
  {
    ScopedMonospaceGlyphCompactionForTest scoped_feature(false);
    HarfBuzzShaper shaper(string);
    full_result = shaper.Shape(font, TextDirection::kRtl);
  }
  ShapeResult* compact_result;
  {
    ScopedMonospaceGlyphCompactionForTest scoped_feature(true);
    HarfBuzzShaper shaper(string);
    compact_result = shaper.Shape(font, TextDirection::kRtl);
  }
  const auto* compact_test_info =
      static_cast<const ShapeResultTestInfo*>(compact_result);
  ASSERT_TRUE(compact_test_info->RunInfoForTesting(0)
                  .GetGlyphDataRange()
                  .IsCompactSource());

  full_result->EnsurePositionData(/*allow_compaction=*/false);
  compact_result->EnsurePositionData();
  ASSERT_EQ(string.length(), PositionDataSize(*full_result));
  ASSERT_EQ(1u, PositionDataSize(*compact_result));
  for (unsigned offset = 0; offset <= string.length(); ++offset) {
    EXPECT_EQ(full_result->CachedPositionForOffset(offset),
              compact_result->CachedPositionForOffset(offset));
    EXPECT_EQ(full_result->CachedNextSafeToBreakOffset(offset),
              compact_result->CachedNextSafeToBreakOffset(offset));
    EXPECT_EQ(full_result->CachedPreviousSafeToBreakOffset(offset),
              compact_result->CachedPreviousSafeToBreakOffset(offset));
  }
  for (unsigned offset = 0; offset < string.length(); ++offset) {
    const LayoutUnit position = full_result->CachedPositionForOffset(offset);
    EXPECT_EQ(full_result->CachedOffsetForPosition(position),
              compact_result->CachedOffsetForPosition(position));
  }
  EXPECT_TRUE(compact_test_info->RunInfoForTesting(0)
                  .GetGlyphDataRange()
                  .IsCompactSource());

  ShapeResultCursor cursor(compact_result);
  for (unsigned offset = 0; offset < string.length(); ++offset) {
    cursor.MoveToCharacter(offset);
    EXPECT_EQ(offset, cursor.CharacterIndex());
  }
}

TEST_P(ShapeResultPositionCacheTest,
       CompactRunsBuildPositionDataWithoutFullTable) {
  constexpr unsigned kStart = 13;
  constexpr unsigned kGlyphsPerRun = 32768;
  for (unsigned num_runs : {1u, 2u}) {
    SCOPED_TRACE(num_runs);
    const unsigned num_characters = num_runs * kGlyphsPerRun;
    for (int advance_raw : {0, 1, 1023, 65536, 629146, 6124123, 4402067,
                            TextRunLayoutUnit::Max().RawValue(), -629146}) {
      SCOPED_TRACE(advance_raw);
      const TextRunLayoutUnit advance =
          TextRunLayoutUnit::FromRawValue(advance_raw);
      ShapeResult* result =
          MakeGarbageCollected<ShapeResult>(kStart, num_characters, GetParam());
      ShapeResult* reference =
          MakeGarbageCollected<ShapeResult>(kStart, num_characters, GetParam());
      for (unsigned i = 0; i < num_runs; ++i) {
        const unsigned run_start = kStart + i * kGlyphsPerRun;
        InsertConstantAdvanceRun(result, run_start, kGlyphsPerRun, advance,
                                 /*compact=*/true);
        InsertConstantAdvanceRun(reference, run_start, kGlyphsPerRun, advance,
                                 /*compact=*/false);
      }
      reference->EnsurePositionData(/*allow_compaction=*/false);

      // Only the final shared-advance entry is available. Allocating a
      // temporary per-character table would violate NoAllocationScope.
      ReserveSinglePositionEntry(*result);
      ASSERT_EQ(0u, PositionDataSize(*result));
      ASSERT_EQ(1u, PositionDataCapacity(*result));
      {
        ThreadState::NoAllocationScope no_allocation(ThreadState::Current());
        result->EnsurePositionData();
        result->EnsurePositionData();
      }
      ASSERT_EQ(1u, PositionDataSize(*result));
      EXPECT_EQ(1u, PositionDataCapacity(*result));
      EXPECT_EQ(reference->Width(), result->Width());
      EXPECT_TRUE(result->CharacterData(kStart).is_cluster_base);
      EXPECT_TRUE(result->CharacterData(kStart).safe_to_break_before);
      for (unsigned offset :
           {0u, 1u, 269u, kGlyphsPerRun - 1, kGlyphsPerRun, num_characters}) {
        SCOPED_TRACE(offset);
        const LayoutUnit position = reference->CachedPositionForOffset(offset);
        EXPECT_EQ(position, result->CachedPositionForOffset(offset));
        EXPECT_EQ(reference->CachedWidth(kStart, kStart + offset),
                  result->CachedWidth(kStart, kStart + offset));
        for (int delta : {-1, 0, 1}) {
          const LayoutUnit x = position + LayoutUnit::FromRawValue(delta);
          EXPECT_EQ(reference->CachedOffsetForPosition(x),
                    result->CachedOffsetForPosition(x));
        }
        if (offset < num_characters) {
          EXPECT_EQ(reference->CachedNextSafeToBreakOffset(kStart + offset),
                    result->CachedNextSafeToBreakOffset(kStart + offset));
        }
        EXPECT_EQ(reference->CachedPreviousSafeToBreakOffset(kStart + offset),
                  result->CachedPreviousSafeToBreakOffset(kStart + offset));
      }
    }
  }
}

TEST_P(ShapeResultPositionCacheTest, CompactPositionDataSaturatedOffsets) {
  constexpr unsigned kNumCharacters = 32768;
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(0, kNumCharacters, GetParam());
  ShapeResult* reference =
      MakeGarbageCollected<ShapeResult>(0, kNumCharacters, GetParam());
  InsertConstantAdvanceRun(result, 0, kNumCharacters, TextRunLayoutUnit(2048),
                           /*compact=*/true);
  InsertConstantAdvanceRun(reference, 0, kNumCharacters,
                           TextRunLayoutUnit(2048),
                           /*compact=*/false);
  result->EnsurePositionData();
  reference->EnsurePositionData(/*allow_compaction=*/false);
  ASSERT_EQ(1u, PositionDataSize(*result));
  ASSERT_EQ(kNumCharacters, PositionDataSize(*reference));
  ASSERT_GT(result->Width(), LayoutUnit::Max().ToFloat());

  for (LayoutUnit x :
       {LayoutUnit(), LayoutUnit(2048),
        LayoutUnit::Max() - LayoutUnit::FromRawValue(1), LayoutUnit::Max()}) {
    SCOPED_TRACE(x);
    EXPECT_EQ(reference->CachedOffsetForPosition(x),
              result->CachedOffsetForPosition(x));
  }
  EXPECT_EQ(IsLtr(GetParam()) ? kNumCharacters - 1 : 1u,
            result->CachedOffsetForPosition(LayoutUnit::Max()));
}

TEST_P(ShapeResultPositionCacheTest, CompactPositionDataFastPathFallbacks) {
  constexpr unsigned kStart = 13;
  struct TestCase {
    unsigned first_run_offset;
    unsigned second_run_offset;
    unsigned num_characters;
    int second_advance;
    bool second_is_compact;
    unsigned expected_cache_size;
  };
  const TestCase test_cases[] = {
      {0, 8, 16, 11, true, 16},   // Different advances.
      {0, 10, 18, 10, true, 18},  // Gap between runs.
      {2, 10, 18, 10, true, 18},  // Leading characters without glyphs.
      {0, 8, 18, 10, true, 18},   // Trailing characters without glyphs.
      {0, 8, 16, 10, false,
       1},  // Mixed storage still compacts via the fallback.
  };
  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.first_run_offset);
    SCOPED_TRACE(test_case.second_run_offset);
    SCOPED_TRACE(test_case.num_characters);
    SCOPED_TRACE(test_case.second_advance);
    SCOPED_TRACE(test_case.second_is_compact);
    ShapeResult* result = MakeGarbageCollected<ShapeResult>(
        kStart, test_case.num_characters, GetParam());
    ShapeResult* reference = MakeGarbageCollected<ShapeResult>(
        kStart, test_case.num_characters, GetParam());
    for (ShapeResult* target : {result, reference}) {
      InsertConstantAdvanceRun(target, kStart + test_case.first_run_offset, 8,
                               TextRunLayoutUnit(10),
                               /*compact=*/target == result);
      InsertConstantAdvanceRun(
          target, kStart + test_case.second_run_offset, 8,
          TextRunLayoutUnit(test_case.second_advance),
          /*compact=*/target == result && test_case.second_is_compact);
    }
    reference->EnsurePositionData(/*allow_compaction=*/false);
    result->EnsurePositionData();
    EXPECT_EQ(test_case.expected_cache_size, PositionDataSize(*result));
    EXPECT_EQ(reference->Width(), result->Width());
    for (unsigned offset = 0; offset <= test_case.num_characters; ++offset) {
      EXPECT_EQ(reference->CachedPositionForOffset(offset),
                result->CachedPositionForOffset(offset));
      if (offset < test_case.num_characters) {
        EXPECT_EQ(reference->CachedNextSafeToBreakOffset(kStart + offset),
                  result->CachedNextSafeToBreakOffset(kStart + offset));
      }
      EXPECT_EQ(reference->CachedPreviousSafeToBreakOffset(kStart + offset),
                result->CachedPreviousSafeToBreakOffset(kStart + offset));
    }
    for (int x = -1; x <= reference->Width() + 1; ++x) {
      EXPECT_EQ(reference->CachedOffsetForPosition(LayoutUnit(x)),
                result->CachedOffsetForPosition(LayoutUnit(x)));
    }
  }
}

TEST_P(ShapeResultPositionCacheTest, CompactRunsForceFullPositionData) {
  constexpr unsigned kNumCharacters = 16;
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(0, kNumCharacters, GetParam());
  ShapeResultRun* run = InsertConstantAdvanceRun(
      result, 0, kNumCharacters, TextRunLayoutUnit(10), /*compact=*/true);
  result->EnsurePositionData(/*allow_compaction=*/false);
  ASSERT_EQ(kNumCharacters, PositionDataSize(*result));
  EXPECT_TRUE(run->GetGlyphDataRange().IsCompactSource());
  for (unsigned offset = 0; offset < kNumCharacters; ++offset) {
    const ShapeResultCharacterData& data = result->CharacterData(offset);
    EXPECT_EQ(LayoutUnit(offset * 10), data.x_position);
    EXPECT_TRUE(data.is_cluster_base);
    EXPECT_TRUE(data.safe_to_break_before);
  }
  result->EnsurePositionData();
  EXPECT_EQ(kNumCharacters, PositionDataSize(*result));
}

TEST_F(ShapeResultTest, CompactPositionDataWithGapsMatchesFullStorage) {
  constexpr unsigned kStartIndex = 5;
  constexpr unsigned kNumCharacters = 24;
  for (TextDirection direction : {TextDirection::kLtr, TextDirection::kRtl}) {
    SCOPED_TRACE(direction);
    ShapeResult* full_result = MakeGarbageCollected<ShapeResult>(
        kStartIndex, kNumCharacters, direction);
    ShapeResult* compact_result = MakeGarbageCollected<ShapeResult>(
        kStartIndex, kNumCharacters, direction);
    for (ShapeResult* result : {full_result, compact_result}) {
      InsertConstantAdvanceRun(result, kStartIndex + 2, 8,
                               TextRunLayoutUnit(10),
                               /*compact=*/result == compact_result);
      InsertConstantAdvanceRun(result, kStartIndex + 13, 8,
                               TextRunLayoutUnit(20),
                               /*compact=*/result == compact_result);
      result->EnsurePositionData(/*allow_compaction=*/false);
      ASSERT_EQ(kNumCharacters, PositionDataSize(*result));
    }

    for (unsigned offset = kStartIndex; offset < kStartIndex + kNumCharacters;
         ++offset) {
      SCOPED_TRACE(offset);
      const ShapeResultCharacterData& expected =
          full_result->CharacterData(offset);
      const ShapeResultCharacterData& actual =
          compact_result->CharacterData(offset);
      EXPECT_EQ(expected.x_position, actual.x_position);
      EXPECT_EQ(expected.is_cluster_base, actual.is_cluster_base);
      EXPECT_EQ(expected.safe_to_break_before, actual.safe_to_break_before);
    }
    EXPECT_EQ(full_result->Width(), compact_result->Width());
  }
}

TEST_P(ShapeResultPositionCacheTest,
       DirectCompactPositionDataPreservesAutoSpacing) {
  constexpr unsigned kNumCharacters = 16;
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(0, kNumCharacters, GetParam());
  ShapeResult* reference =
      MakeGarbageCollected<ShapeResult>(0, kNumCharacters, GetParam());
  for (ShapeResult* target : {result, reference}) {
    InsertConstantAdvanceRun(target, 0, kNumCharacters, TextRunLayoutUnit(10),
                             /*compact=*/target == result);
  }
  result->EnsurePositionData();
  ASSERT_EQ(1u, PositionDataSize(*result));
  EXPECT_EQ(1u, PositionDataCapacity(*result));
  reference->EnsurePositionData(/*allow_compaction=*/false);
  for (unsigned spacing_offset : {4u, 6u}) {
    for (ShapeResult* target : {result, reference}) {
      target->ApplyTextAutoSpacing({{spacing_offset, 2}});
    }
    result->EnsurePositionData();
    ASSERT_EQ(kNumCharacters, PositionDataSize(*result));
    EXPECT_EQ(reference->Width(), result->Width());
    EXPECT_TRUE(result->HasAutoSpacingAfter(3));
    EXPECT_TRUE(result->HasAutoSpacingBefore(spacing_offset));
    for (unsigned offset = 0; offset <= kNumCharacters; ++offset) {
      EXPECT_EQ(reference->CachedPositionForOffset(offset),
                result->CachedPositionForOffset(offset));
      EXPECT_EQ(reference->HasAutoSpacingAfter(offset),
                result->HasAutoSpacingAfter(offset));
    }
  }
}

TEST_P(ShapeResultPositionCacheTest, CompactPositionDataMatchesFullTable) {
  constexpr wtf_size_t kNumCharacters = 16;
  for (int advance_raw : {1, 511, 1023, 1024, 1025, 65536, 629146}) {
    SCOPED_TRACE(advance_raw);
    const TextRunLayoutUnit advance =
        TextRunLayoutUnit::FromRawValue(advance_raw);
    for (bool compact_glyphs : {false, true}) {
      SCOPED_TRACE(compact_glyphs);
      ShapeResult* result =
          MakeGarbageCollected<ShapeResult>(0, kNumCharacters, GetParam());
      ShapeResult* reference =
          MakeGarbageCollected<ShapeResult>(0, kNumCharacters, GetParam());
      InsertConstantAdvanceRun(result, 0, kNumCharacters, advance,
                               compact_glyphs);
      InsertConstantAdvanceRun(reference, 0, kNumCharacters, advance,
                               /*compact=*/false);
      reference->EnsurePositionData(/*allow_compaction=*/false);
      result->EnsurePositionData();
      ASSERT_EQ(1u, PositionDataSize(*result));
      for (wtf_size_t offset = 0; offset <= kNumCharacters; ++offset) {
        EXPECT_EQ(reference->CachedPositionForOffset(offset),
                  result->CachedPositionForOffset(offset));
      }
      for (int raw_x = -1;
           raw_x <=
           LayoutUnit::FromFloatCeil(reference->Width()).RawValue() + 1;
           ++raw_x) {
        const LayoutUnit x = LayoutUnit::FromRawValue(raw_x);
        EXPECT_EQ(reference->CachedOffsetForPosition(x),
                  result->CachedOffsetForPosition(x))
            << x;
      }
    }
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
  Vector<OffsetWithSpacing, 16> offsets(
      base::checked_cast<wtf_size_t>(offsets_data.size()));
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

TEST_F(ShapeResultTest, LetterSpacingAppliedToSpacesInCursiveScripts) {
  // خطية النصية (two Arabic words separated by a space)
  String string(
      u"\u062E\u0637\u0651\u064E\u064A\u0651\u064E\u0020"
      u"\u0627\u0644\u0646\u0651\u064E\u0635\u0651\u064E");

  HarfBuzzShaper shaper(string);
  auto* result = shaper.Shape(GetFont(kArabicFont), TextDirection::kRtl);

  // Letter spacing should not be applied to cursive characters, but is
  // applied to the 'space' between words per CSS Text 4 §8.2.1.
  ShapeResultSpacing spacing(string);
  FontDescription font_description;
  font_description.SetLetterSpacing(Length::Fixed(5));
  font_description.SetWordSpacing(Length::Fixed(20));
  spacing.SetSpacing(font_description);
  result->ApplySpacing(spacing);
  EXPECT_TRUE(spacing.IsLetterSpacingAppliedForTesting());
  EXPECT_TRUE(spacing.IsWordSpacingAppliedForTesting());
}

TEST_F(ShapeResultTest, LetterSpacingNotAppliedWithinCursiveWord) {
  // خطية (single Arabic word with no spaces)
  String string(u"\u062E\u0637\u0651\u064E\u064A\u0651\u064E");

  HarfBuzzShaper shaper(string);
  auto* result = shaper.Shape(GetFont(kArabicFont), TextDirection::kRtl);

  // Letter spacing is not applied between cursive script characters.
  ShapeResultSpacing spacing(string);
  FontDescription font_description;
  font_description.SetLetterSpacing(Length::Fixed(5));
  spacing.SetSpacing(font_description);
  result->ApplySpacing(spacing);
  EXPECT_FALSE(spacing.IsLetterSpacingAppliedForTesting());
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
     ShapeResultTest::kCjkFont,
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
     ShapeResultTest::kCjkFont,
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
     ShapeResultTest::kCjkFont,
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
     ShapeResultTest::kCjkFont,
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
  ScopedNoFontAntialiasingForTest disable_no_font_antialiasing_for_test(false);

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

// Tests for CaretOffsetForHitTest
struct CaretOffsetForHitTestTestData {
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
} caret_offset_for_position_test_data[] = {

    {u"مَ1مَمَ2مَمَمَ3",
     TextDirection::kLtr,
#if BUILDFLAG(IS_APPLE)
     {1,  2,  3,  9,  10, 15, 16, 21, 22, 27,
      28, 34, 35, 40, 41, 45, 46, 51, 52, 55},
     {0, 0, 2, 2, 3, 3, 5, 5, 7, 7, 8, 8, 10, 10, 12, 12, 14, 14, 15, 15},
#elif BUILDFLAG(IS_WIN)
     {1,  3,  4,  9,  10, 16, 17, 21, 22, 27,
      28, 34, 35, 39, 40, 44, 45, 50, 51, 54},
     {0, 0, 2, 2, 3, 3, 5, 5, 7, 7, 8, 8, 10, 10, 12, 12, 14, 14, 15, 15},
#else
     {1,  3,  4,  9,  10, 16, 17, 23, 24, 29,
      30, 36, 37, 42, 43, 48, 49, 54, 55, 57},
     {0, 0, 2, 2, 3, 3, 5, 5, 7, 7, 8, 8, 10, 10, 12, 12, 14, 14, 15, 15},
#endif
     ShapeResultTest::kArabicFont},

    {u"مَ1مَمَ2مَمَمَ3",
     TextDirection::kRtl,
#if BUILDFLAG(IS_APPLE)
     {1,  3,  4,  10, 11, 15, 16, 20, 21, 27,
      28, 33, 34, 39, 40, 45, 46, 52, 53, 55},
     {15, 15, 14, 14, 12, 12, 10, 10, 8, 8, 7, 7, 5, 5, 3, 3, 2, 2, 0, 0},
#elif BUILDFLAG(IS_WIN)
     {1,  3,  4,  10, 11, 15, 16, 20, 21, 26,
      27, 33, 34, 38, 39, 44, 45, 51, 52, 53},
     {15, 15, 14, 14, 12, 12, 10, 10, 8, 8, 7, 7, 5, 5, 3, 3, 2, 2, 0, 0},
#else
     {1,  3,  4,  10, 11, 16, 17, 22, 23, 28,
      29, 35, 36, 42, 43, 48, 49, 55, 56, 57},
     {15, 15, 14, 14, 12, 12, 10, 10, 8, 8, 7, 7, 5, 5, 3, 3, 2, 2, 0, 0},
#endif
     ShapeResultTest::kArabicFont},

    {u"あ1あمَ2あمَあ",
     TextDirection::kLtr,
#if BUILDFLAG(IS_APPLE)
     {1, 6, 7, 15, 16, 24, 25, 33, 34, 40, 41, 49, 50, 58, 59, 67, 68, 73},
     {0, 0, 1, 1, 2, 2, 3, 3, 5, 5, 6, 6, 7, 7, 9, 9, 10, 10},
#else
     {1, 6, 7, 15, 16, 25, 26, 34, 35, 40, 41, 50, 51, 59, 60, 68, 69, 73},
     {0, 0, 1, 1, 2, 2, 3, 3, 5, 5, 6, 6, 7, 7, 9, 9, 10, 10},
#endif
     ShapeResultTest::kArabicFont},

    {u"あ1あمَ2あمَあ",
     TextDirection::kRtl,
#if BUILDFLAG(IS_APPLE)
     {1, 6, 7, 14, 15, 23, 24, 33, 34, 39, 40, 48, 49, 58, 59, 67, 68, 73},
     {10, 10, 9, 9, 7, 7, 6, 6, 5, 5, 3, 3, 2, 2, 1, 1, 0, 0},
#else
     {1, 6, 7, 15, 16, 24, 25, 33, 34, 40, 41, 49, 50, 58, 59, 68, 69, 73},
     {10, 10, 9, 9, 7, 7, 6, 6, 5, 5, 3, 3, 2, 2, 1, 1, 0, 0},
#endif
     ShapeResultTest::kArabicFont},

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
     ShapeResultTest::kCjkFont},

    {u"楽しいドライブ、0",
     TextDirection::kRtl,
#if BUILDFLAG(IS_APPLE)
     {1,  3,  4,  13, 14, 25, 26, 37, 38, 49,
      50, 61, 62, 73, 74, 85, 86, 97, 98, 103},
     {9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0},
#else
     {1,  3,  4,  13, 14, 25, 26, 37, 38, 49,
      50, 61, 62, 73, 74, 85, 86, 97, 98, 102},
     {9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0},
#endif
     ShapeResultTest::kCjkFont},
};
class CaretOffsetForHitTestTest
    : public ShapeResultTest,
      public testing::WithParamInterface<CaretOffsetForHitTestTestData> {};
INSTANTIATE_TEST_SUITE_P(
    ShapeResult,
    CaretOffsetForHitTestTest,
    testing::ValuesIn(caret_offset_for_position_test_data));

TEST_P(CaretOffsetForHitTestTest, CaretOffsetForHitTest) {
  ScopedNoFontAntialiasingForTest disable_no_font_antialiasing_for_test(false);

  const auto& test_data = GetParam();
  String text_string(test_data.string);
  HarfBuzzShaper shaper(text_string);
  const ShapeResult* result =
      shaper.Shape(GetFont(test_data.font), test_data.direction);
  StringView text_view(text_string);

  float text_width = result->Width();
  if (IsLtr(test_data.direction)) {
    EXPECT_EQ(0u, result->CaretOffsetForHitTest(-1, text_view));
    EXPECT_EQ(0u, result->CaretOffsetForHitTest(0, text_view));
    EXPECT_EQ(text_string.length(),
              result->CaretOffsetForHitTest(text_width, text_view));
    EXPECT_EQ(text_string.length(),
              result->CaretOffsetForHitTest(text_width + 10, text_view));
  } else {
    EXPECT_EQ(0u, result->CaretOffsetForHitTest(text_width + 10, text_view));
    EXPECT_EQ(0u, result->CaretOffsetForHitTest(text_width, text_view));
    EXPECT_EQ(text_string.length(),
              result->CaretOffsetForHitTest(0, text_view));
    EXPECT_EQ(text_string.length(),
              result->CaretOffsetForHitTest(-1, text_view));
  }

  for (wtf_size_t i = 0; i < test_data.positions.size(); i++) {
    EXPECT_EQ(test_data.offsets[i],
              result->CaretOffsetForHitTest(test_data.positions[i], text_view))
        << "index " << i;
  }
}

class ShapeResultCursorTest : public ShapeResultTest {};

TEST_F(ShapeResultCursorTest, Ltr) {
  String string("0123456789");
  HarfBuzzShaper shaper(string);
  ShapeResult* result = shaper.Shape(GetFont(kLatinFont), TextDirection::kLtr);
  ShapeResultCursor cursor(result);
  EXPECT_EQ(cursor.CharacterIndex(), 0u);
  EXPECT_EQ(cursor.GlyphDataForTest().glyph, 20u);

  cursor.MoveToCharacter(4);
  EXPECT_EQ(cursor.glyph_index_, 4u);
  EXPECT_EQ(cursor.CharacterIndex(), 4u);
  EXPECT_EQ(cursor.GlyphDataForTest().glyph, 24u);

  const TextRunLayoutUnit advance = cursor.ClusterAdvance();
  EXPECT_EQ(advance, cursor.GlyphDataForTest().advance);
  const TextRunLayoutUnit space(10);
  cursor.AddSpaceToRight(space);
  EXPECT_EQ(advance + space, cursor.GlyphDataForTest().advance);
  EXPECT_FALSE(cursor.run_->glyph_data_.HasNonZeroOffsets());

  cursor.AddSpaceToLeft(-space);
  EXPECT_EQ(advance, cursor.GlyphDataForTest().advance);
  EXPECT_EQ(cursor.run_->glyph_data_.Offsets()[cursor.glyph_index_],
            GlyphOffset(-space, 0));
}

TEST_F(ShapeResultCursorTest, Rtl) {
  // نص اختبار العربية
  String string(
      u"\u0646\u0635\u0627\u062E\u062A\u0628\u0627\u0631\u0627\u0644\u0639"
      u"\u0631\u0628\u064A\u0629");
  HarfBuzzShaper shaper(string);
  ShapeResult* result = shaper.Shape(GetFont(kArabicFont), TextDirection::kRtl);
  ShapeResultCursor cursor(result);
  EXPECT_EQ(cursor.CharacterIndex(), 0u);
  EXPECT_EQ(cursor.GlyphDataForTest().glyph, 497u);

  cursor.MoveToCharacter(4);
  EXPECT_EQ(cursor.glyph_index_, 10u);
  EXPECT_EQ(cursor.CharacterIndex(), 4u);
  EXPECT_EQ(cursor.GlyphDataForTest().glyph, 440u);

  const TextRunLayoutUnit advance = cursor.ClusterAdvance();
  EXPECT_EQ(advance, cursor.GlyphDataForTest().advance);
  const TextRunLayoutUnit space(10);
  cursor.AddSpaceToRight(space);
  EXPECT_EQ(advance + space, cursor.GlyphDataForTest().advance);
  EXPECT_FALSE(cursor.run_->glyph_data_.HasNonZeroOffsets());

  cursor.AddSpaceToLeft(-space);
  EXPECT_EQ(advance, cursor.GlyphDataForTest().advance);
  EXPECT_EQ(cursor.run_->glyph_data_.Offsets()[cursor.glyph_index_],
            GlyphOffset(-space, 0));
}

TEST_F(ShapeResultCursorTest, StartIndex) {
  String string("0123456789");
  HarfBuzzShaper shaper(string);
  ShapeResult* result =
      shaper.Shape(GetFont(kLatinFont), TextDirection::kLtr, 2, 10);
  EXPECT_EQ(result->StartIndex(), 2u);
  ShapeResultCursor cursor(result);
  EXPECT_EQ(cursor.CharacterIndex(), 2u);
  EXPECT_EQ(cursor.GlyphDataForTest().glyph, 22u);

  cursor.MoveToCharacter(4);
  EXPECT_EQ(cursor.glyph_index_, 2u);
  EXPECT_EQ(cursor.CharacterIndex(), 4u);
  EXPECT_EQ(cursor.GlyphDataForTest().glyph, 24u);
}

TEST_F(ShapeResultTest, ForEachGraphemeClustersBoundsCheck) {
  ShapeResult* result =
      MakeGarbageCollected<ShapeResult>(0, 10, TextDirection::kLtr);
  result->InsertRunForTesting(0, 10, TextDirection::kLtr);
  const String text = "0123456789";

  struct Context {
    Vector<wtf_size_t> called_indices;
  };
  const auto callback = [](void* context_ptr, wtf_size_t character_index,
                           float total_advance, wtf_size_t graphemes_in_cluster,
                           float cluster_advance,
                           CanvasRotationInVertical rotation) {
    auto* ctx = static_cast<Context*>(context_ptr);
    ctx->called_indices.push_back(character_index);
  };
  {
    Context context;
    result->ForEachGraphemeClusters(text, 0.0f, 0, 8, 0, callback, &context);
    EXPECT_EQ(context.called_indices.size(), 8u);
    for (wtf_size_t i = 0; i < context.called_indices.size(); ++i) {
      EXPECT_EQ(context.called_indices[i], i);
    }
  }
  {
    const ShapeResultView* view = ShapeResultView::Create(result);
    Context context;
    view->ForEachGraphemeClusters(text, 0.0f, 0, 8, 0, callback, &context);
    EXPECT_EQ(context.called_indices.size(), 8u);
    for (wtf_size_t i = 0; i < 8; ++i) {
      EXPECT_EQ(context.called_indices[i], i);
    }
  }
}

}  // namespace blink
