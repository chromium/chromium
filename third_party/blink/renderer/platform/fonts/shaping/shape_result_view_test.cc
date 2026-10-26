// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"

#include <unicode/uscript.h>

#include <array>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/character_range.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_test_utilities.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_run.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_spacing.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_test_info.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shaping_line_breaker.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

const ShapeResultTestInfo* TestInfo(const ShapeResult* result) {
  return static_cast<const ShapeResultTestInfo*>(result);
}

}  // namespace

class ShapeResultViewTest : public FontTestBase {
 protected:
  void SetUp() override { font_description.SetComputedSize(12.0); }

  void TearDown() override {}

  FontDescription font_description;
};

TEST_F(ShapeResultViewTest, ExpandRange) {
  auto get_expanded_range = [](const String& text, bool ltr, wtf_size_t from,
                               wtf_size_t to) -> Vector<wtf_size_t> {
    FontDescription::VariantLigatures ligatures(
        FontDescription::kEnabledLigaturesState);
    Font* font = test::CreateTestFont(
        AtomicString("roboto"),
        test::PlatformTestDataPath("third_party/Roboto/roboto-regular.woff2"),
        100, &ligatures);

    HarfBuzzShaper shaper(text);
    const ShapeResultView* shape_result = ShapeResultView::Create(
        shaper.Shape(font, ltr ? TextDirection::kLtr : TextDirection::kRtl));
    shape_result->ExpandRangeToIncludePartialGlyphs(&from, &to);
    return Vector<wtf_size_t>({from, to});
  };

  // "ffi" is a ligature, therefore a single glyph. Any range that includes one
  // of the letters must be expanded to all of them.
  EXPECT_EQ(get_expanded_range("efficient", true, 0, 1), Vector({0u, 1u}));
  EXPECT_EQ(get_expanded_range("efficient", true, 0, 2), Vector({0u, 4u}));
  EXPECT_EQ(get_expanded_range("efficient", true, 3, 4), Vector({1u, 4u}));
  EXPECT_EQ(get_expanded_range("efficient", true, 4, 6), Vector({4u, 6u}));
  EXPECT_EQ(get_expanded_range("efficient", true, 6, 7), Vector({6u, 7u}));
  EXPECT_EQ(get_expanded_range("efficient", true, 0, 9), Vector({0u, 9u}));

  EXPECT_EQ(get_expanded_range("tneiciffe", false, 0, 1), Vector({0u, 1u}));
  EXPECT_EQ(get_expanded_range("tneiciffe", false, 0, 2), Vector({0u, 2u}));
  EXPECT_EQ(get_expanded_range("tneiciffe", false, 3, 4), Vector({3u, 4u}));
  EXPECT_EQ(get_expanded_range("tneiciffe", false, 4, 6), Vector({4u, 8u}));
  EXPECT_EQ(get_expanded_range("tneiciffe", false, 6, 7), Vector({5u, 8u}));
  EXPECT_EQ(get_expanded_range("tneiciffe", false, 0, 9), Vector({0u, 9u}));
}

// http://crbug.com/1221008
TEST_F(ShapeResultViewTest,
       ExpandRangeToIncludePartialGlyphsWithCombiningCharacter) {
  Font* font = MakeGarbageCollected<Font>(font_description);

  String string(u"abc\u0E35\u0E35\u0E35\u0E35");
  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(font, TextDirection::kLtr);
  const ShapeResultView* view =
      ShapeResultView::Create(result, result->StartIndex(), result->EndIndex());
  wtf_size_t from = 0;
  wtf_size_t end = string.length();
  view->ExpandRangeToIncludePartialGlyphs(&from, &end);
  EXPECT_EQ(0u, from);
  EXPECT_EQ(string.length(), end);
}

TEST_F(ShapeResultViewTest, LatinSingleView) {
  Font* font = MakeGarbageCollected<Font>(font_description);

  String string =
      To16Bit("Test run with multiple words and breaking opportunities.");
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(font, direction);
  Vector<ShapeResultTestGlyphInfo> glyphs;
  result->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&glyphs));

  // Test view at the start of the result: "Test run with multiple"
  auto segments = std::to_array<ShapeResultView::Segment>({{result, 0, 22}});
  auto* first4 = ShapeResultView::Create(segments);

  EXPECT_EQ(first4->StartIndex(), 0u);
  EXPECT_EQ(first4->NumCharacters(), 22u);
  EXPECT_EQ(first4->NumGlyphs(), 22u);

  Vector<ShapeResultTestGlyphInfo> first4_glyphs;
  first4->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&first4_glyphs));
  EXPECT_EQ(first4_glyphs.size(), 22u);
  EXPECT_TRUE(CompareResultGlyphs(first4_glyphs, glyphs, 0u, 22u));

  // Test view in the middle of the result: "multiple words and breaking"
  segments[0] = {result, 14, 41};
  auto* middle4 = ShapeResultView::Create(segments);

  EXPECT_EQ(middle4->StartIndex(), 14u);
  EXPECT_EQ(middle4->NumCharacters(), 27u);
  EXPECT_EQ(middle4->NumGlyphs(), 27u);

  Vector<ShapeResultTestGlyphInfo> middle4_glyphs;
  middle4->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&middle4_glyphs));
  EXPECT_EQ(middle4_glyphs.size(), 27u);
  EXPECT_TRUE(CompareResultGlyphs(middle4_glyphs, glyphs, 14u, 27u));

  // Test view at the end of the result: "breaking opportunities."
  segments[0] = {result, 33, 56};
  auto* last2 = ShapeResultView::Create(segments);

  EXPECT_EQ(last2->StartIndex(), 33u);
  EXPECT_EQ(last2->NumCharacters(), 23u);
  EXPECT_EQ(last2->NumGlyphs(), 23u);

  Vector<ShapeResultTestGlyphInfo> last2_glyphs;
  last2->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&last2_glyphs));
  EXPECT_EQ(last2_glyphs.size(), 23u);
  EXPECT_TRUE(CompareResultGlyphs(last2_glyphs, glyphs, 33u, 23u));
}

TEST_F(ShapeResultViewTest, ArabicSingleView) {
  Font* font = MakeGarbageCollected<Font>(font_description);

  String string = To16Bit("عربى نص");
  TextDirection direction = TextDirection::kRtl;

  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(font, direction);
  Vector<ShapeResultTestGlyphInfo> glyphs;
  result->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&glyphs));

  // Test view at the start of the result: "عربى"
  auto segments = std::to_array<ShapeResultView::Segment>({{result, 0, 4}});
  auto* first_word = ShapeResultView::Create(segments);
  Vector<ShapeResultTestGlyphInfo> first_glyphs;
  first_word->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&first_glyphs));

  EXPECT_EQ(first_word->StartIndex(), 0u);
  EXPECT_EQ(first_word->NumCharacters(), 4u);
  EXPECT_EQ(first_word->NumGlyphs(), 4u);
  EXPECT_EQ(first_glyphs.size(), 4u);

  String first_reference_string = To16Bit("عربى");
  HarfBuzzShaper first_reference_shaper(first_reference_string);
  const ShapeResult* first_wortd_reference =
      first_reference_shaper.Shape(font, direction);
  Vector<ShapeResultTestGlyphInfo> first_reference_glyphs;
  first_wortd_reference->ForEachGlyph(
      0, AddGlyphInfo, static_cast<void*>(&first_reference_glyphs));
  EXPECT_EQ(first_reference_glyphs.size(), 4u);

  EXPECT_TRUE(
      CompareResultGlyphs(first_glyphs, first_reference_glyphs, 0u, 4u));
  EXPECT_TRUE(CompareResultGlyphs(first_glyphs, glyphs, 3u, 7u));

  // Test view at the end of the result: "نص"
  segments[0] = {result, 4, 7};
  auto* last_word = ShapeResultView::Create(segments);
  Vector<ShapeResultTestGlyphInfo> last_glyphs;
  last_word->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&last_glyphs));

  EXPECT_EQ(last_word->StartIndex(), 4u);
  EXPECT_EQ(last_word->NumCharacters(), 3u);
  EXPECT_EQ(last_word->NumGlyphs(), 3u);
  EXPECT_EQ(last_glyphs.size(), 3u);
}

TEST_F(ShapeResultViewTest, PreviousSafeToBreak) {
  Font* font = MakeGarbageCollected<Font>(font_description);

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
  const ShapeResult* shape_result =
      shaper.Shape(font, direction, 51, 131, range);

  wtf_size_t start_offset = 59;
  wtf_size_t end_offset = 118;
  const ShapeResultView* result_view =
      ShapeResultView::Create(shape_result, start_offset, end_offset);
  const ShapeResult* result = result_view->CreateShapeResult();

  wtf_size_t offset = end_offset;
  do {
    wtf_size_t safe = result_view->PreviousSafeToBreakOffset(offset);
    wtf_size_t cached_safe = result->CachedPreviousSafeToBreakOffset(offset);
    EXPECT_EQ(safe, cached_safe);
  } while (--offset > start_offset);
}

TEST_F(ShapeResultViewTest, NonCompactPartialViewsMatchSubRanges) {
  ScopedMonospaceGlyphCompactionForTest disable_compaction(false);
  FontDescription::VariantLigatures ligatures(
      FontDescription::kEnabledLigaturesState);
  Font* font = test::CreateTestFont(
      AtomicString("roboto"),
      test::PlatformTestDataPath("third_party/Roboto/roboto-regular.woff2"),
      12.5f, &ligatures);
  const String text("office affinity");
  for (TextDirection direction : {TextDirection::kLtr, TextDirection::kRtl}) {
    SCOPED_TRACE(direction);
    for (float letter_spacing : {0.0f, -5.0f, -20.0f}) {
      SCOPED_TRACE(letter_spacing);
      HarfBuzzShaper shaper(text);
      ShapeResult* result = shaper.Shape(font, direction);
      ShapeResultSpacing spacing(text);
      FontDescription spacing_font_description;
      spacing_font_description.SetLetterSpacing(Length::Fixed(letter_spacing));
      if (spacing.SetSpacing(spacing_font_description)) {
        result->ApplySpacing(spacing);
      }
      const auto* outer = ShapeResultView::Create(result, 1, text.length() - 1);
      for (wtf_size_t start = 2; start < text.length() - 2; ++start) {
        for (wtf_size_t end = start + 1; end < text.length() - 1; ++end) {
          SCOPED_TRACE(testing::Message() << start << ", " << end);
          const auto* view = ShapeResultView::Create(outer, start, end);
          const auto* expected = result->SubRange(start, end);
          EXPECT_EQ(expected->Width(), view->Width());
          expected->EnsurePositionData();
          // Views retain empty runs, unlike copied subranges.
          EXPECT_EQ(end, view->PreviousSafeToBreakOffset(end));
          for (wtf_size_t offset = start; offset < end; ++offset) {
            EXPECT_EQ(expected->CachedPreviousSafeToBreakOffset(offset),
                      view->PreviousSafeToBreakOffset(offset));
          }
        }
      }
    }
  }
}

TEST_F(ShapeResultViewTest, CreateShapeResultMonospaceMaterialized) {
  Font* font = test::CreateTestFont(
      AtomicString("Noto Mono"),
      test::BlinkWebTestsDir() +
          "/third_party/NotoMono-hintedLocalizedFamilyName/"
          "NotoMono-Regular.subset.ttf",
      10);
  ASSERT_TRUE(font->PrimaryFont()->IsFixedPitch());

  auto Measure = [&](bool compaction,
                     TextDirection direction) -> Vector<float> {
    ScopedMonospaceGlyphCompactionForTest scoped_feature(compaction);
    String string = To16Bit(
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz");
    HarfBuzzShaper shaper(string);
    ShapeResult* result = shaper.Shape(font, direction);
    if (compaction) {
      const ShapeResultTestInfo* const test_info = TestInfo(result);
      EXPECT_EQ(1u, test_info->NumberOfRunsForTesting());
      if (test_info->NumberOfRunsForTesting() == 1) {
        EXPECT_TRUE(test_info->RunInfoForTesting(0)
                        .GetGlyphDataRange()
                        .IsCompactSource());
      }
    }

    ShapeResultSpacing spacing(string);
    FontDescription spacing_font_description;
    spacing_font_description.SetLetterSpacing(Length::Fixed(4));
    spacing.SetSpacing(spacing_font_description);
    result->ApplySpacing(spacing);

    const ShapeResultView* view1 = ShapeResultView::Create(result, 8, 72);
    const ShapeResultView* view2 = ShapeResultView::Create(view1, 16, 64);
    const ShapeResult* created = view2->CreateShapeResult();

    Vector<float> out;
    out.push_back(created->Width());
    out.push_back(static_cast<float>(created->NumCharacters()));
    for (unsigned offset = 0; offset <= created->NumCharacters(); ++offset) {
      out.push_back(created->PositionForOffset(offset));
    }
    return out;
  };
  for (TextDirection direction : {TextDirection::kLtr, TextDirection::kRtl}) {
    EXPECT_EQ(Measure(true, direction), Measure(false, direction));
  }
}

TEST_F(ShapeResultViewTest, NestedCompactViewCopyAtThreshold) {
  ScopedMonospaceGlyphCompactionForTest scoped_feature(true);
  Font* font = test::CreateTestFont(
      AtomicString("Noto Mono"),
      test::BlinkWebTestsDir() +
          "/third_party/NotoMono-hintedLocalizedFamilyName/"
          "NotoMono-Regular.subset.ttf",
      10);
  ASSERT_TRUE(font->PrimaryFont()->IsFixedPitch());
  const String string = To16Bit(
      "abcdefghijklmnopqrstuvwxyz"
      "abcdefghijklmnopqrstuvwxyz");
  HarfBuzzShaper shaper(string);
  for (TextDirection direction : {TextDirection::kLtr, TextDirection::kRtl}) {
    SCOPED_TRACE(direction);
    const ShapeResult* source = shaper.Shape(font, direction);
    ASSERT_EQ(1u, TestInfo(source)->NumberOfRunsForTesting());
    const ShapeResultRun& source_run = TestInfo(source)->RunInfoForTesting(0);
    ASSERT_TRUE(source_run.GetGlyphDataRange().IsCompactSource());

    const ShapeResultView* outer = ShapeResultView::Create(source, 8, 40);
    constexpr unsigned kStart = 13;
    for (unsigned num_glyphs : {7u, 8u, 9u}) {
      SCOPED_TRACE(num_glyphs);
      const ShapeResultView* inner =
          ShapeResultView::Create(outer, kStart, kStart + num_glyphs);
      ASSERT_TRUE(source_run.GetGlyphDataRange().IsCompactSource());
      const ShapeResult* actual = inner->CreateShapeResult();
      EXPECT_TRUE(source_run.GetGlyphDataRange().IsCompactSource());

      const ShapeResult* expected;
      {
        ScopedMonospaceGlyphCompactionForTest disabled(false);
        expected = shaper.Shape(font, direction, kStart, kStart + num_glyphs);
      }
      ASSERT_EQ(1u, TestInfo(actual)->NumberOfRunsForTesting());
      ASSERT_EQ(1u, TestInfo(expected)->NumberOfRunsForTesting());
      const ShapeResultRun& actual_run = TestInfo(actual)->RunInfoForTesting(0);
      const ShapeResultRun& expected_run =
          TestInfo(expected)->RunInfoForTesting(0);
      EXPECT_EQ(num_glyphs >= 8,
                actual_run.GetGlyphDataRange().IsCompactSource());
      EXPECT_FALSE(expected_run.GetGlyphDataRange().IsCompactSource());
      EXPECT_EQ(kStart, actual->StartIndex());
      EXPECT_EQ(num_glyphs, actual->NumCharacters());
      EXPECT_EQ(expected->Width(), actual->Width());

      const GlyphDataRange::Reader actual_glyphs(actual_run);
      const GlyphDataRange::Reader expected_glyphs(expected_run);
      ASSERT_EQ(num_glyphs, actual_glyphs.size());
      ASSERT_EQ(num_glyphs, expected_glyphs.size());
      for (unsigned i = 0; i < num_glyphs; ++i) {
        const HarfBuzzRunGlyphData actual_glyph = actual_glyphs[i];
        const HarfBuzzRunGlyphData expected_glyph = expected_glyphs[i];
        EXPECT_EQ(expected_glyph.glyph, actual_glyph.glyph);
        EXPECT_EQ(direction == TextDirection::kRtl ? num_glyphs - i - 1 : i,
                  actual_glyph.character_index);
        EXPECT_EQ(expected_glyph.character_index, actual_glyph.character_index);
        EXPECT_EQ(expected_glyph.advance, actual_glyph.advance);
        EXPECT_EQ(expected_glyph.IsSafeToBreakBefore(),
                  actual_glyph.IsSafeToBreakBefore());
      }
      for (unsigned i = 0; i <= num_glyphs; ++i) {
        EXPECT_EQ(expected->PositionForOffset(i), actual->PositionForOffset(i));
        EXPECT_EQ(expected->NextSafeToBreakOffset(kStart + i),
                  actual->NextSafeToBreakOffset(kStart + i));
        EXPECT_EQ(expected->PreviousSafeToBreakOffset(kStart + i),
                  actual->PreviousSafeToBreakOffset(kStart + i));
      }
      EXPECT_EQ(num_glyphs >= 8,
                actual_run.GetGlyphDataRange().IsCompactSource());
      EXPECT_TRUE(source_run.GetGlyphDataRange().IsCompactSource());
    }
  }
}

TEST_F(ShapeResultViewTest, ExistingViewSurvivesSourceMaterialization) {
  ScopedMonospaceGlyphCompactionForTest scoped_feature(true);
  Font* font = test::CreateTestFont(
      AtomicString("Noto Mono"),
      test::BlinkWebTestsDir() +
          "/third_party/NotoMono-hintedLocalizedFamilyName/"
          "NotoMono-Regular.subset.ttf",
      10);
  ASSERT_TRUE(font->PrimaryFont()->IsFixedPitch());

  for (TextDirection direction : {TextDirection::kLtr, TextDirection::kRtl}) {
    SCOPED_TRACE(direction);
    String string = To16Bit(
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz");
    HarfBuzzShaper shaper(string);
    ShapeResult* result = shaper.Shape(font, direction);
    ASSERT_TRUE(TestInfo(result)
                    ->RunInfoForTesting(0)
                    .GetGlyphDataRange()
                    .IsCompactSource());

    const ShapeResultView* view = ShapeResultView::Create(result, 8, 64);
    Vector<ShapeResultTestGlyphInfo> before;
    view->ForEachGlyph(0, AddGlyphInfo, &before);

    ShapeResultSpacing spacing(string);
    FontDescription spacing_font_description;
    spacing_font_description.SetLetterSpacing(Length::Fixed(1));
    ASSERT_TRUE(spacing.SetSpacing(spacing_font_description));
    result->ApplySpacing(spacing);
    EXPECT_FALSE(TestInfo(result)
                     ->RunInfoForTesting(0)
                     .GetGlyphDataRange()
                     .IsCompactSource());

    Vector<ShapeResultTestGlyphInfo> after;
    view->ForEachGlyph(0, AddGlyphInfo, &after);
    const ShapeResultView* expected_view =
        ShapeResultView::Create(result, 8, 64);
    Vector<ShapeResultTestGlyphInfo> expected;
    expected_view->ForEachGlyph(0, AddGlyphInfo, &expected);
    ASSERT_EQ(expected.size(), after.size());
    for (unsigned i = 0; i < expected.size(); ++i) {
      EXPECT_EQ(expected[i].character_index, after[i].character_index);
      EXPECT_EQ(expected[i].glyph, after[i].glyph);
      EXPECT_EQ(expected[i].advance, after[i].advance);
    }

    const ShapeResult* copy = view->CreateShapeResult();
    Vector<ShapeResultTestGlyphInfo> copied;
    copy->ForEachGlyph(0, AddGlyphInfo, &copied);
    ASSERT_EQ(after.size(), copied.size());
    for (unsigned i = 0; i < after.size(); ++i) {
      EXPECT_EQ(after[i].character_index, copied[i].character_index);
      EXPECT_EQ(after[i].glyph, copied[i].glyph);
      EXPECT_EQ(after[i].advance, copied[i].advance);
    }
  }
}

TEST_F(ShapeResultViewTest, LatinMultiRun) {
  Font* font = MakeGarbageCollected<Font>(font_description);

  TextDirection direction = TextDirection::kLtr;
  HarfBuzzShaper shaper_a(To16Bit("hello"));
  HarfBuzzShaper shaper_b(To16Bit(" w"));
  HarfBuzzShaper shaper_c(To16Bit("orld"));
  HarfBuzzShaper shaper_d(To16Bit("!"));

  // Combine four separate results into a single one to ensure we have a result
  // with multiple runs: "hello world!"
  ShapeResult* result = MakeGarbageCollected<ShapeResult>(0, 0, direction);
  shaper_a.Shape(font, direction)->CopyRange(0u, 5u, result);
  shaper_b.Shape(font, direction)->CopyRange(0u, 2u, result);
  shaper_c.Shape(font, direction)->CopyRange(0u, 4u, result);
  shaper_d.Shape(font, direction)->CopyRange(0u, 1u, result);

  Vector<ShapeResultTestGlyphInfo> result_glyphs;
  result->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&result_glyphs));

  // Create composite view out of multiple segments where at least some of the
  // segments have multiple runs: "hello wood wold!"
  ShapeResultView::Segment segments[5] = {
      {result, 0, 8},    // "hello wo"
      {result, 7, 8},    // "o"
      {result, 10, 11},  // "d"
      {result, 5, 8},    // " wo"
      {result, 9, 12},   // "ld!"
  };
  auto* composite_view = ShapeResultView::Create(segments);
  Vector<ShapeResultTestGlyphInfo> view_glyphs;
  composite_view->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&view_glyphs));

  EXPECT_EQ(composite_view->StartIndex(), 0u);
  EXPECT_EQ(composite_view->NumCharacters(), 16u);
  EXPECT_EQ(composite_view->NumGlyphs(), 16u);
  EXPECT_EQ(view_glyphs.size(), 16u);

  HarfBuzzShaper shaper2(To16Bit("hello world!"));
  const ShapeResult* result2 = shaper2.Shape(font, direction);
  Vector<ShapeResultTestGlyphInfo> glyphs2;
  result2->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&glyphs2));
  EXPECT_TRUE(CompareResultGlyphs(result_glyphs, glyphs2, 0u, 12u));

  HarfBuzzShaper reference_shaper(To16Bit("hello wood wold!"));
  const ShapeResult* reference_result = reference_shaper.Shape(font, direction);
  Vector<ShapeResultTestGlyphInfo> reference_glyphs;
  reference_result->ForEachGlyph(0, AddGlyphInfo,
                                 static_cast<void*>(&reference_glyphs));

  ShapeResult* composite_copy =
      MakeGarbageCollected<ShapeResult>(0, 0, direction);
  result->CopyRange(0, 8, composite_copy);
  result->CopyRange(7, 8, composite_copy);
  result->CopyRange(10, 11, composite_copy);
  result->CopyRange(5, 8, composite_copy);
  result->CopyRange(9, 12, composite_copy);

  Vector<ShapeResultTestGlyphInfo> composite_copy_glyphs;
  composite_copy->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&composite_copy_glyphs));

  EXPECT_TRUE(CompareResultGlyphs(view_glyphs, reference_glyphs, 0u, 16u));
  EXPECT_TRUE(
      CompareResultGlyphs(composite_copy_glyphs, reference_glyphs, 0u, 16u));
  EXPECT_EQ(composite_view->Width(), composite_copy->Width());
}

TEST_F(ShapeResultViewTest, LatinCompositeView) {
  Font* font = MakeGarbageCollected<Font>(font_description);

  String string =
      To16Bit("Test run with multiple words and breaking opportunities.");
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(font, direction);
  Vector<ShapeResultTestGlyphInfo> glyphs;
  result->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&glyphs));

  String reference_string = To16Bit("multiple breaking opportunities Test");
  HarfBuzzShaper reference_shaper(reference_string);
  const ShapeResult* reference_result = reference_shaper.Shape(font, direction);
  Vector<ShapeResultTestGlyphInfo> reference_glyphs;

  // Match the character index logic of ShapeResult::CopyRange where the the
  // character index of the first result is preserved and all subsequent ones
  // are adjusted to be sequential.
  // TODO(layout-dev): Arguably both should be updated to renumber the first
  // result as well but some callers depend on the existing behavior.
  ShapeResult* composite_copy =
      MakeGarbageCollected<ShapeResult>(0, 0, direction);
  result->CopyRange(14, 23, composite_copy);
  result->CopyRange(33, 55, composite_copy);
  result->CopyRange(4, 5, composite_copy);
  result->CopyRange(0, 4, composite_copy);
  EXPECT_EQ(composite_copy->NumCharacters(), reference_result->NumCharacters());
  EXPECT_EQ(composite_copy->NumGlyphs(), reference_result->NumGlyphs());
  composite_copy->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&reference_glyphs));

  // Create composite view out of multiple segments:
  ShapeResultView::Segment segments[4] = {
      {result, 14, 23},  // "multiple "
      {result, 33, 55},  // "breaking opportunities"
      {result, 4, 5},    // " "
      {result, 0, 4}     // "Test"
  };
  auto* composite_view = ShapeResultView::Create(segments);

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
  Font* font = MakeGarbageCollected<Font>(font_description);

  String string_a = To16Bit("Test with multiple 字体 ");
  String string_b = To16Bit("and 本書.");
  TextDirection direction = TextDirection::kLtr;

  HarfBuzzShaper shaper_a(string_a);
  const ShapeResult* result_a = shaper_a.Shape(font, direction);
  HarfBuzzShaper shaper_b(string_b);
  const ShapeResult* result_b = shaper_b.Shape(font, direction);

  String reference_string = To16Bit("Test with multiple 字体 and 本書.");
  HarfBuzzShaper reference_shaper(reference_string);
  const ShapeResult* reference_result = reference_shaper.Shape(font, direction);

  // Create a copy using CopyRange and compare with that to ensure that the same
  // fonts are used for both the composite and the reference. The combined
  // reference_result data might use different fonts, resulting in different
  // glyph ids and metrics.
  ShapeResult* composite_copy =
      MakeGarbageCollected<ShapeResult>(0, 0, direction);
  result_a->CopyRange(0, 22, composite_copy);
  result_b->CopyRange(0, 7, composite_copy);
  EXPECT_EQ(composite_copy->NumCharacters(), reference_result->NumCharacters());
  EXPECT_EQ(composite_copy->NumGlyphs(), reference_result->NumGlyphs());
  Vector<ShapeResultTestGlyphInfo> reference_glyphs;
  composite_copy->ForEachGlyph(0, AddGlyphInfo,
                               static_cast<void*>(&reference_glyphs));

  ShapeResultView::Segment segments[] = {{result_a, 0, 22}, {result_b, 0, 7}};
  auto* composite_view = ShapeResultView::Create(segments);

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
  Font* font = MakeGarbageCollected<Font>(font_description);

  String string = To16Bit("12345678901234567890");
  TextDirection direction = TextDirection::kLtr;
  HarfBuzzShaper shaper(string);
  const ShapeResult* result = shaper.Shape(font, direction);

  // Create a view from 5 to 20.
  const ShapeResultView* view1 = ShapeResultView::Create(result, 5, 20);
  EXPECT_EQ(view1->NumCharacters(), 15u);
  EXPECT_EQ(view1->NumGlyphs(), 15u);

  // Trim the last character from the view.
  const ShapeResultView* view2 = ShapeResultView::Create(view1, 5, 19);
  EXPECT_EQ(view2->NumCharacters(), 14u);
  EXPECT_EQ(view2->NumGlyphs(), 14u);
}

TEST_F(ShapeResultViewTest, SpacesInLtr) {
  Font* font = MakeGarbageCollected<Font>(font_description);

  constexpr wtf_size_t kStartIndex = 0;
  constexpr wtf_size_t kLength = 2;
  constexpr float kWidth = 8;
  const auto* result = ShapeResult::CreateForSpaces(
      font, TextDirection::kLtr, kStartIndex, kLength, kWidth);

  const auto* view0 = ShapeResultView::Create(result, 0, 2);
  EXPECT_EQ(view0->NumCharacters(), 2u);
  EXPECT_EQ(view0->NumGlyphs(), 2u);

  const auto* view1 = ShapeResultView::Create(result, 0, 1);
  EXPECT_EQ(view1->NumCharacters(), 1u);
  EXPECT_EQ(view1->NumGlyphs(), 1u);

  const auto* view2 = ShapeResultView::Create(result, 1, 2);
  EXPECT_EQ(view2->NumCharacters(), 1u);
  EXPECT_EQ(view2->NumGlyphs(), 1u);
}

// http://crbug.com/1160582
TEST_F(ShapeResultViewTest, SpacesInRtl) {
  Font* font = MakeGarbageCollected<Font>(font_description);

  constexpr wtf_size_t kStartIndex = 0;
  constexpr wtf_size_t kLength = 2;
  constexpr float kWidth = 8;
  const auto* result = ShapeResult::CreateForSpaces(
      font, TextDirection::kRtl, kStartIndex, kLength, kWidth);

  const auto* view0 = ShapeResultView::Create(result, 0, 2);
  EXPECT_EQ(view0->NumCharacters(), 2u);
  EXPECT_EQ(view0->NumGlyphs(), 2u);

  const auto* view1 = ShapeResultView::Create(result, 0, 1);
  EXPECT_EQ(view1->NumCharacters(), 1u);
  EXPECT_EQ(view1->NumGlyphs(), 1u);

  const auto* view2 = ShapeResultView::Create(result, 1, 2);
  EXPECT_EQ(view2->NumCharacters(), 1u);
  EXPECT_EQ(view2->NumGlyphs(), 1u);
}

TEST_F(ShapeResultViewTest, TabulationCharactersInLtr) {
  Font* font = MakeGarbageCollected<Font>(font_description);

  constexpr float kPosition = 0;
  constexpr wtf_size_t kStartIndex = 0;
  constexpr wtf_size_t kLength = 2;
  const auto* result = ShapeResult::CreateForTabulationCharacters(
      font, TextDirection::kLtr, TabSize(8), kPosition, kStartIndex, kLength);

  const auto* view0 = ShapeResultView::Create(result, 0, 2);
  EXPECT_EQ(view0->NumCharacters(), 2u);
  EXPECT_EQ(view0->NumGlyphs(), 2u);

  const auto* view1 = ShapeResultView::Create(result, 0, 1);
  EXPECT_EQ(view1->NumCharacters(), 1u);
  EXPECT_EQ(view1->NumGlyphs(), 1u);

  const auto* view2 = ShapeResultView::Create(result, 1, 2);
  EXPECT_EQ(view2->NumCharacters(), 1u);
  EXPECT_EQ(view2->NumGlyphs(), 1u);
}

// http://crbug.com/1255310
TEST_F(ShapeResultViewTest, TabulationCharactersInRtl) {
  Font* font = MakeGarbageCollected<Font>(font_description);

  constexpr float kPosition = 0;
  constexpr wtf_size_t kStartIndex = 0;
  constexpr wtf_size_t kLength = 2;
  const auto* result = ShapeResult::CreateForTabulationCharacters(
      font, TextDirection::kRtl, TabSize(8), kPosition, kStartIndex, kLength);

  const auto* view0 = ShapeResultView::Create(result, 0, 2);
  EXPECT_EQ(view0->NumCharacters(), 2u);
  EXPECT_EQ(view0->NumGlyphs(), 2u);

  const auto* view1 = ShapeResultView::Create(result, 0, 1);
  EXPECT_EQ(view1->NumCharacters(), 1u);
  EXPECT_EQ(view1->NumGlyphs(), 1u);

  const auto* view2 = ShapeResultView::Create(result, 1, 2);
  EXPECT_EQ(view2->NumCharacters(), 1u);
  EXPECT_EQ(view2->NumGlyphs(), 1u);
}

// https://crbug.com/1304876
// In a text containing only Latin characters and without ligatures (or where
// ligatures are not close to the end of the view), PreviousSafeToBreakOffset in
// some cases used to return the length of the view, rather than a position into
// the view.
TEST_F(ShapeResultViewTest, PreviousSafeOffsetInsideView) {
  Font* font = MakeGarbageCollected<Font>(font_description);

  HarfBuzzShaper shaper("Blah bla test something. ");
  const ShapeResult* result = shaper.Shape(font, TextDirection::kLtr);

  // Used to be 14 - 9 = 5, which is before the start of the view.
  auto* view1 = ShapeResultView::Create(result, 9, 14);
  EXPECT_EQ(view1->PreviousSafeToBreakOffset(14), 14u);

  // Used to be 25 - 9 = 16, which is inside the view's range, but not the last
  // safe offset.
  auto* view2 = ShapeResultView::Create(result, 9, 25);
  EXPECT_EQ(view2->PreviousSafeToBreakOffset(24), 24u);
}

// https://issues.chromium.org/issues/531062336
// A ShapeResultView part can cover characters that map to no glyphs (e.g. the
// trailing characters of a ligature). FindGlyphDataRange then returns an empty
// GlyphDataRange, and because RunInfoPart derives its run from that range, the
// run must be preserved even when the range is empty.
TEST_F(ShapeResultViewTest, EmptyGlyphRangePreservesRun) {
  FontDescription::VariantLigatures ligatures(
      FontDescription::kEnabledLigaturesState);
  Font* font = test::CreateTestFont(
      AtomicString("roboto"),
      test::PlatformTestDataPath("third_party/Roboto/roboto-regular.woff2"),
      100, &ligatures);

  // "ffi" shapes to a single ligature glyph at character index 0, so the
  // trailing characters 1 and 2 have no glyph of their own.
  HarfBuzzShaper shaper(String("ffi"));
  const ShapeResult* result = shaper.Shape(font, TextDirection::kLtr);
  ASSERT_EQ(result->NumGlyphs(), 1u) << "Expected the 'ffi' ligature.";

  // A view over the trailing characters [1, 3) therefore contains no glyphs,
  // but still spans those two characters.
  const ShapeResultView* view = ShapeResultView::Create(result, 1, 3);
  EXPECT_EQ(view->NumCharacters(), 2u);
  EXPECT_EQ(view->NumGlyphs(), 0u);
  EXPECT_EQ(view->Width(), 0.0f);

  // The run is preserved, so operations over the empty view produce empty
  // results rather than dereferencing a null run. The ink bounds are empty
  // because there are no glyphs.
  EXPECT_TRUE(view->ComputeInkBounds().IsEmpty());
  EXPECT_EQ(view->PreviousSafeToBreakOffset(1), 1u);
  EXPECT_EQ(view->PreviousSafeToBreakOffset(2), 1u);
  EXPECT_EQ(view->PreviousSafeToBreakOffset(3), 3u);

  const ShapeResult* sub_result = view->CreateShapeResult();
  EXPECT_EQ(sub_result->NumCharacters(), 2u);
  EXPECT_EQ(sub_result->NumGlyphs(), 0u);

  // The preserved run's font is still reachable through the empty part.
  EXPECT_EQ(view->UsedFonts().size(), 1u);

  Vector<ShapeResultTestGlyphInfo> glyphs;
  view->ForEachGlyph(0, AddGlyphInfo, static_cast<void*>(&glyphs));
  EXPECT_TRUE(glyphs.empty());
}

TEST_F(ShapeResultViewTest, IndividualCharacterRangesMatchesCreateShapeResult) {
  auto verify_matches_create_shape_result = [](const ShapeResultView* view) {
    const Vector<CharacterRange> view_ranges =
        view->IndividualCharacterRanges();
    const Vector<CharacterRange> result_ranges =
        view->CreateShapeResult()->IndividualCharacterRanges();
    ASSERT_EQ(view_ranges.size(), result_ranges.size());
    for (wtf_size_t i = 0; i < view_ranges.size(); ++i) {
      EXPECT_FLOAT_EQ(view_ranges[i].start, result_ranges[i].start)
          << "i=" << i;
      EXPECT_FLOAT_EQ(view_ranges[i].end, result_ranges[i].end) << "i=" << i;
      EXPECT_FLOAT_EQ(view_ranges[i].ascent, result_ranges[i].ascent)
          << "i=" << i;
      EXPECT_FLOAT_EQ(view_ranges[i].descent, result_ranges[i].descent)
          << "i=" << i;
    }
  };

  Font* font = MakeGarbageCollected<Font>(font_description);

  // 1. LTR single run and sub-ranges.
  HarfBuzzShaper ltr_shaper(
      To16Bit("Test run with multiple words and breaking opportunities."));
  const ShapeResult* ltr_result = ltr_shaper.Shape(font, TextDirection::kLtr);
  verify_matches_create_shape_result(ShapeResultView::Create(ltr_result));
  verify_matches_create_shape_result(
      ShapeResultView::Create(ltr_result, 0, 22));
  verify_matches_create_shape_result(
      ShapeResultView::Create(ltr_result, 14, 41));
  verify_matches_create_shape_result(
      ShapeResultView::Create(ltr_result, 33, 56));

  // 2. RTL single run and sub-ranges.
  HarfBuzzShaper rtl_shaper(To16Bit("عربى نص"));
  const ShapeResult* rtl_result = rtl_shaper.Shape(font, TextDirection::kRtl);
  verify_matches_create_shape_result(ShapeResultView::Create(rtl_result));
  verify_matches_create_shape_result(ShapeResultView::Create(rtl_result, 0, 4));
  verify_matches_create_shape_result(ShapeResultView::Create(rtl_result, 4, 7));

  // 3. Multi-run and multi-segment composite views (LTR and RTL).
  ShapeResult* multi_ltr =
      MakeGarbageCollected<ShapeResult>(0, 0, TextDirection::kLtr);
  HarfBuzzShaper(To16Bit("hello"))
      .Shape(font, TextDirection::kLtr)
      ->CopyRange(0u, 5u, multi_ltr);
  HarfBuzzShaper(To16Bit(" world!"))
      .Shape(font, TextDirection::kLtr)
      ->CopyRange(0u, 7u, multi_ltr);
  ShapeResultView::Segment ltr_segments[] = {
      {multi_ltr, 0, 8},
      {multi_ltr, 5, 12},
  };
  verify_matches_create_shape_result(ShapeResultView::Create(ltr_segments));

  ShapeResult* multi_rtl =
      MakeGarbageCollected<ShapeResult>(0, 0, TextDirection::kRtl);
  rtl_result->CopyRange(0u, 4u, multi_rtl);
  rtl_result->CopyRange(4u, 7u, multi_rtl);
  ShapeResultView::Segment rtl_segments[] = {
      {multi_rtl, 0, 5},
      {multi_rtl, 2, 7},
  };
  verify_matches_create_shape_result(ShapeResultView::Create(rtl_segments));

  // 4. Compact monospace run and sub-ranges.
  ScopedMonospaceGlyphCompactionForTest scoped_compact(true);
  Font* ahem = test::CreateAhemFont(12);
  HarfBuzzShaper compact_shaper(String("0123456789abcdef"));
  const ShapeResult* compact_result =
      compact_shaper.Shape(ahem, TextDirection::kLtr);
  verify_matches_create_shape_result(ShapeResultView::Create(compact_result));
  verify_matches_create_shape_result(
      ShapeResultView::Create(compact_result, 0, 5));
  verify_matches_create_shape_result(
      ShapeResultView::Create(compact_result, 5, 12));
  verify_matches_create_shape_result(
      ShapeResultView::Create(compact_result, 12, 16));
}

}  // namespace blink
