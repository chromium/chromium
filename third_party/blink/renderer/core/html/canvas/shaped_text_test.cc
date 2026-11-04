// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/shaped_text.h"

#include <algorithm>
#include <cmath>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_align.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_text_baseline.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/canvas/glyph_run.h"
#include "third_party/blink/renderer/core/html/canvas/text_metrics.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/fonts/custom_font_data.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_family.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

namespace {

class FontsHolder : public GarbageCollected<FontsHolder> {
 public:
  void Trace(Visitor* visitor) const { visitor->Trace(fonts); }

  HeapVector<Member<Font>> fonts;
};

constexpr double kTolerance = 0.01;

// Maps each font family name to a font loaded from test data, so that tests
// can use a font-family fallback list that doesn't depend on the platform's
// fallback fonts.
class FallbackTestFontSelector final : public FontSelector {
 public:
  const SimpleFontData* AddFont(const FontDescription& font_description,
                                const AtomicString& family_name,
                                const String& font_path) {
    std::optional<Vector<char>> data = test::ReadFromFile(font_path);
    CHECK(data);
    scoped_refptr<SharedBuffer> buffer = SharedBuffer::Create(std::move(*data));
    String ots_parse_message;
    FontCustomPlatformData* custom_platform_data =
        FontCustomPlatformData::Create(buffer.get(), ots_parse_message);
    CHECK(custom_platform_data) << ots_parse_message;
    FontSelectionCapabilities normal_capabilities(
        {kNormalWidthValue, kNormalWidthValue},
        {kNormalSlopeValue, kNormalSlopeValue},
        {kNormalWeightValue, kNormalWeightValue});
    const FontPlatformData* platform_data =
        custom_platform_data->GetFontPlatformData(
            font_description.EffectiveFontSize(),
            font_description.AdjustedSpecifiedSize(), false, false, false,
            font_description.GetFontSelectionRequest(), normal_capabilities,
            font_description.FontOpticalSizing(),
            font_description.TextRendering(), {},
            font_description.Orientation());
    auto* font_data = MakeGarbageCollected<SimpleFontData>(
        platform_data, MakeGarbageCollected<CustomFontData>());
    fonts_.Set(family_name, font_data);
    return font_data;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(fonts_);
    FontSelector::Trace(visitor);
  }

  FontData* GetFontData(const FontDescription&,
                        const FontFamily& font_family) override {
    auto it = fonts_.find(font_family.FamilyName());
    return it != fonts_.end() ? it->value.Get() : nullptr;
  }

  void WillUseFontData(const FontDescription&,
                       const FontFamily&,
                       const String&) override {}
  void WillUseRange(const FontDescription&,
                    const AtomicString&,
                    const FontDataForRangeSet&) override {}
  void FontCacheInvalidated() override {}
  void ReportNotDefGlyph() const override {}
  ExecutionContext* GetExecutionContext() const override { return nullptr; }
  FontFaceCache* GetFontFaceCache() override { return nullptr; }
  void RegisterForInvalidationCallbacks(FontSelectorClient*) override {}
  void UnregisterForInvalidationCallbacks(FontSelectorClient*) override {}
  bool IsPlatformFamilyMatchAvailable(const FontDescription&,
                                      const FontFamily&) override {
    return false;
  }

 private:
  HeapHashMap<AtomicString, Member<SimpleFontData>> fonts_;
};

}  // namespace

class ShapedTextTest : public testing::Test {
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
    fonts_holder->fonts.push_back(blink::test::CreateTestFont(
        AtomicString("Roboto"),
        blink::test::PlatformTestDataPath(
            "third_party/Roboto/roboto-regular.woff2"),
        12.0, &ligatures));

    fonts_holder->fonts.push_back(blink::test::CreateTestFont(
        AtomicString("Noto"),
        blink::test::PlatformTestDataPath(
            "third_party/Noto/NotoNaskhArabic-regular.woff2"),
        12.0, &ligatures));

    fonts_holder->fonts.push_back(blink::test::CreateTestFont(
        AtomicString("M PLUS 1p"),
        blink::test::BlinkWebTestsFontsTestDataPath("mplus-1p-regular.woff"),
        12.0, &ligatures));
  }

  const Font* GetFont(FontType type) const { return fonts_holder->fonts[type]; }

  ShapedText* Shape(
      const String& text,
      FontType font = kLatinFont,
      TextDirection direction = TextDirection::kLtr,
      V8CanvasTextAlign::Enum align = V8CanvasTextAlign::Enum::kLeft,
      V8CanvasTextBaseline::Enum baseline =
          V8CanvasTextBaseline::Enum::kAlphabetic) const {
    return Shape(text, GetFont(font), direction, align, baseline);
  }

  ShapedText* Shape(
      const String& text,
      const Font* font,
      TextDirection direction = TextDirection::kLtr,
      V8CanvasTextAlign::Enum align = V8CanvasTextAlign::Enum::kLeft,
      V8CanvasTextBaseline::Enum baseline =
          V8CanvasTextBaseline::Enum::kAlphabetic) const {
    return MakeGarbageCollected<ShapedText>(
        font, direction, baseline, align, text,
        *MakeGarbageCollected<PlainTextPainter>(PlainTextPainter::kCanvas));
  }

  struct FallbackFont {
    STACK_ALLOCATED();

   public:
    const Font* font;
    const SimpleFontData* latin_font;
    const SimpleFontData* cjk_font;
  };

  // Returns a font with font-family: Roboto, "M PLUS 1p", with both fonts
  // loaded from test data. Roboto has no CJK characters, so these use
  // "M PLUS 1p" on every platform, independently of the platform's fallback
  // fonts.
  static FallbackFont CreateFallbackFont() {
    FontDescription font_description;
    font_description.SetSpecifiedSize(12.0);
    font_description.SetComputedSize(12.0);
    auto* selector = MakeGarbageCollected<FallbackTestFontSelector>();
    const SimpleFontData* latin_font = selector->AddFont(
        font_description, AtomicString("Roboto"),
        test::PlatformTestDataPath("third_party/Roboto/roboto-regular.woff2"));
    const SimpleFontData* cjk_font = selector->AddFont(
        font_description, AtomicString("M PLUS 1p"),
        test::BlinkWebTestsFontsTestDataPath("mplus-1p-regular.woff"));
    font_description.SetFamily(
        FontFamily(AtomicString("Roboto"), FontFamily::Type::kFamilyName,
                   SharedFontFamily::Create(AtomicString("M PLUS 1p"),
                                            FontFamily::Type::kFamilyName)));
    const Font* font = MakeGarbageCollected<Font>(font_description, selector);
    CHECK_EQ(latin_font, font->PrimaryFont());
    return {font, latin_font, cjk_font};
  }

  test::TaskEnvironment task_environment_;
  ScopedNoFontAntialiasingForTest disable_no_font_antialiasing_for_test_{false};
  Persistent<FontsHolder> fonts_holder;
};

// Tests for advanceToPosition() with mixed bidi text.
struct CaretPositionForOffsetBidiTestData {
  // The string that should be processed.
  const UChar* string;
  // Text direction to test
  TextDirection direction;
  // The expected positions to test.
  std::vector<unsigned> positions;
  // Points to test.
  std::vector<double> points;
  // The font to use
  ShapedTextTest::FontType font;
} caret_position_for_offset_test_data[] = {
    // Values are carefully chosen to verify that the bidi correction rules
    // behave as expected.
    // 0
    {u"0123456789",
     TextDirection::kLtr,
     {0, 0, 0, 1, 5, 5, 9, 10, 10, 10},
#if BUILDFLAG(IS_APPLE)
     {-5, 0, 2, 5, 32, 36, 62, 66, 67.38, 70},
#else
     {-5, 0, 2, 5, 33, 37, 65, 68, 70, 75},
#endif
     ShapedTextTest::kLatinFont},

    // 1
    {u"0123456789",
     TextDirection::kRtl,
     {0, 0, 0, 1, 5, 5, 9, 10, 10, 10},
#if BUILDFLAG(IS_APPLE)
     {-5, 0, 2, 5, 32, 36, 62, 66, 67.38, 70},
#else
     {-5, 0, 2, 5, 33, 37, 65, 68, 70, 75},
#endif
     ShapedTextTest::kLatinFont},

    // 2
    {u"0fi1fi23fif456fifi",
     TextDirection::kLtr,
     {0, 0, 0, 1, 10, 11, 16, 17, 18, 18},
#if BUILDFLAG(IS_WIN)
     {-5, 0, 2, 5, 49, 53, 81, 85, 88, 90},
#else
     {-5, 0, 2, 5, 46, 50, 77, 80, 83, 85},
#endif
     ShapedTextTest::kLatinFont},

    // 3
    {u"0fi1fi23fif456fifi",
     TextDirection::kRtl,
     {0, 0, 0, 1, 10, 11, 16, 17, 18, 18},
#if BUILDFLAG(IS_WIN)
     {-5, 0, 2, 5, 49, 53, 81, 85, 88, 90},
#else
     {-5, 0, 2, 5, 46, 50, 77, 80, 83, 85},
#endif
     ShapedTextTest::kLatinFont},

    // 4
    {u"مَ1مَمَ23مَمَمَ345مَمَمَمَ",
     TextDirection::kLtr,
     {26, 26, 26, 15, 15, 18, 18, 7, 7, 9, 9, 2, 2, 3, 3, 0, 0, 0},
#if BUILDFLAG(IS_APPLE)
     {-5, 0, 3, 20, 23, 40, 45, 57, 61, 71, 74, 82, 86, 90, 93, 96, 97.306,
      105},
#elif BUILDFLAG(IS_WIN)
     {-5, 0, 3, 20, 22, 40, 44, 56, 60, 70, 74, 81, 85, 89, 91, 94, 96, 105},
#else
     {-5, 0, 3, 21, 25, 41, 47, 60, 64, 74, 78, 87, 91, 94, 96, 100, 102, 105},
#endif
     ShapedTextTest::kArabicFont},

    // 5
    {u"مَ1مَمَ23مَمَمَ345مَمَمَمَ",
     TextDirection::kRtl,
     {26, 26, 26, 18, 18, 15, 15, 9, 9, 7, 7, 3, 3, 2, 2, 0, 0, 0},
#if BUILDFLAG(IS_APPLE)
     {-5, 0, 3, 20, 23, 40, 45, 57, 61, 71, 74, 82, 86, 90, 93, 96, 97.306,
      105},
#elif BUILDFLAG(IS_WIN)
     {-5, 0, 3, 20, 22, 40, 44, 56, 60, 70, 74, 81, 85, 89, 91, 94, 96, 105},
#else
     {-5, 0, 3, 21, 25, 41, 47, 60, 64, 74, 78, 87, 91, 94, 96, 100, 102, 105},
#endif
     ShapedTextTest::kArabicFont},

    // 6
    {u"あ1あمَ23あمَあ345",
     TextDirection::kLtr,
#if BUILDFLAG(IS_FUCHSIA)  // Very very narrrow glyph
     {0, 0, 0, 3, 5, 7, 7, 7, 8, 8, 8, 10, 10, 14, 14, 14},
#else
     {0, 0, 0, 3, 5, 7, 7, 7, 7, 8, 8, 10, 10, 14, 14, 14},
#endif
#if BUILDFLAG(IS_FUCHSIA)
     {-5, 0, 4, 21, 25, 35, 39, 46, 49, 50, 53, 55, 59, 84, 86, 95},
#else
     {-5, 0, 4, 29, 33, 43, 47, 50, 53, 61, 65, 67, 71, 100, 102, 110},
#endif
     ShapedTextTest::kArabicFont},

    // 7
    {u"あ1あمَ23あمَあ345",
     TextDirection::kRtl,
     {10, 10, 10, 10, 10, 8, 8, 5, 5, 3, 3, 3, 3, 3},
#if BUILDFLAG(IS_FUCHSIA)
     {-5, 0, 4, 27, 31, 33, 37, 55, 59, 61, 65, 84, 86, 95},
#else
     {-5, 0, 3, 31, 35, 37, 41, 63, 67, 69, 73, 100, 102, 110},
#endif
     ShapedTextTest::kArabicFont},

    // 8
    {u"楽しいドライブ、012345楽しいドライブ、",
     TextDirection::kLtr,
     {0, 0, 0, 1, 20, 20, 21, 22, 22},
     {-5, 0, 1, 10, 210, 215, 228, 234, 250},
     ShapedTextTest::kCJKFont},

    // 9
    {u"楽しいドライブ、012345楽しいドライブ、",
     TextDirection::kRtl,
     {22, 22, 22, 21, 21, 21, 1, 21, 21, 21},
     {-5, 0, 1, 11, 12, 14, 20, 234, 237, 250},
     ShapedTextTest::kCJKFont},

    // 10
    {u"123楽しいドライブ、0123",
     TextDirection::kLtr,
     {0, 0, 0, 1, 7, 8, 14, 15, 15, 15},
#if BUILDFLAG(IS_APPLE)
     {-5, 0, 2, 5, 72, 78, 142, 145, 148, 152},
#else
     {-5, 0, 2, 5, 72, 78, 140, 143, 145, 150},
#endif
     ShapedTextTest::kCJKFont},

    // 11
    {u"123楽しいドライブ、0123",
     TextDirection::kRtl,
     {0, 0, 0, 1, 7, 8, 14, 15, 15, 15},
#if BUILDFLAG(IS_APPLE)
     {-5, 0, 2, 5, 72, 78, 142, 145, 148, 152},
#else
     {-5, 0, 2, 5, 72, 78, 140, 143, 145, 150},
#endif
     ShapedTextTest::kCJKFont},
};
class CaretPositionForOffsetBidiTest
    : public ShapedTextTest,
      public testing::WithParamInterface<CaretPositionForOffsetBidiTestData> {};
INSTANTIATE_TEST_SUITE_P(
    ShapedText,
    CaretPositionForOffsetBidiTest,
    testing::ValuesIn(caret_position_for_offset_test_data));

TEST_P(CaretPositionForOffsetBidiTest, CaretPositionForOffsetsBidi) {
  const auto& test_data = GetParam();
  ShapedText* shaped_text =
      Shape(String(test_data.string), test_data.font, test_data.direction);

  for (wtf_size_t i = 0; i < test_data.points.size(); ++i) {
    EXPECT_EQ(test_data.positions[i],
              shaped_text->advanceToPosition(test_data.points[i]))
        << "at index " << i;
  }
}

namespace {

// Expects `runs` to be in visual order, adjacent to each other, and to cover
// every code unit of `shaped_text` exactly once.
void ExpectVisualPartition(const HeapVector<Member<GlyphRun>>& runs,
                           const ShapedText& shaped_text) {
  ASSERT_FALSE(runs.empty());
  EXPECT_NEAR(runs.front()->x(), -shaped_text.TextAlignDx(), kTolerance);
  double total_width = 0;
  Vector<bool> covered(shaped_text.GetText().length(), false);
  for (wtf_size_t i = 0; i < runs.size(); ++i) {
    const GlyphRun& run = *runs[i];
    EXPECT_LT(run.start(), run.end()) << "run " << i;
    EXPECT_GT(run.width(), 0) << "run " << i;
    EXPECT_EQ(0.0, run.y()) << "run " << i;
    if (i > 0) {
      EXPECT_NEAR(runs[i - 1]->x() + runs[i - 1]->width(), run.x(), kTolerance)
          << "run " << i;
    }
    for (unsigned j = run.start(); j < run.end(); ++j) {
      EXPECT_FALSE(covered[j]) << "code unit " << j;
      covered[j] = true;
    }
    total_width += run.width();
  }
  EXPECT_NEAR(shaped_text.width(), total_width, kTolerance);
  for (wtf_size_t j = 0; j < covered.size(); ++j) {
    EXPECT_TRUE(covered[j]) << "code unit " << j;
  }
}

}  // namespace

TEST_F(ShapedTextTest, PositionToAdvanceAndGlyphRuns) {
  String text_string(u"0123456789");
  ShapedText* shaped_text = Shape(text_string);

  DummyExceptionStateForTesting exception_state;
  EXPECT_DOUBLE_EQ(0.0, shaped_text->positionToAdvance(0, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_NEAR(shaped_text->width(),
              shaped_text->positionToAdvance(10, exception_state), 0.01);
  EXPECT_FALSE(exception_state.HadException());

  auto clusters = shaped_text->glyphClusters();
  ASSERT_EQ(10u, clusters.size());
  for (wtf_size_t i = 0; i < clusters.size(); ++i) {
    EXPECT_EQ(i, clusters[i]->start());
    EXPECT_EQ(i + 1, clusters[i]->end());
    EXPECT_NEAR(clusters[i]->x(),
                shaped_text->positionToAdvance(i, exception_state), 0.01);
    EXPECT_FALSE(exception_state.HadException());
  }

  auto runs = shaped_text->glyphRuns();
  ASSERT_EQ(1u, runs.size());
  EXPECT_EQ(0u, runs[0]->start());
  EXPECT_EQ(10u, runs[0]->end());
  EXPECT_NEAR(shaped_text->width(), runs[0]->width(), 0.01);
}

TEST_F(ShapedTextTest, EmptyShapedText) {
  // A default-constructed ShapedText is what shapeText() returns in frame-less
  // documents.
  ShapedText* shaped_text = MakeGarbageCollected<ShapedText>();

  EXPECT_EQ(0.0, shaped_text->width());
  EXPECT_TRUE(shaped_text->glyphRuns().empty());
  EXPECT_TRUE(shaped_text->glyphClusters().empty());
  EXPECT_EQ(0u, shaped_text->advanceToPosition(10));

  DummyExceptionStateForTesting exception_state;
  EXPECT_EQ(0.0, shaped_text->positionToAdvance(0, exception_state));
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(shaped_text->selectionRects(0, 0, exception_state).empty());
  EXPECT_FALSE(exception_state.HadException());

  shaped_text->positionToAdvance(1, exception_state);
  EXPECT_EQ(DOMExceptionCode::kIndexSizeError,
            exception_state.CodeAs<DOMExceptionCode>());

  TextMetrics* text_metrics = MakeGarbageCollected<TextMetrics>(*shaped_text);
  EXPECT_EQ(0.0, text_metrics->width());
  EXPECT_EQ(0.0, text_metrics->fontBoundingBoxAscent());
  EXPECT_EQ(0.0, text_metrics->fontBoundingBoxDescent());
}

TEST_F(ShapedTextTest, PositionToAdvanceRoundTrip) {
  const String text(u"0123456789");
  ShapedText* shaped_text = Shape(text);

  DummyExceptionStateForTesting exception_state;
  double previous = -1;
  for (unsigned i = 0; i <= text.length(); ++i) {
    const double advance = shaped_text->positionToAdvance(i, exception_state);
    EXPECT_FALSE(exception_state.HadException());
    EXPECT_GT(advance, previous) << "position " << i;
    EXPECT_EQ(i, shaped_text->advanceToPosition(advance + kTolerance))
        << "position " << i;
    previous = advance;
  }
}

TEST_F(ShapedTextTest, PositionToAdvanceRtl) {
  const String text(u"\u0645\u0631\u062D\u0628\u0627");
  ShapedText* shaped_text = Shape(text, kArabicFont, TextDirection::kRtl);
  ASSERT_GT(shaped_text->width(), 0);

  DummyExceptionStateForTesting exception_state;
  EXPECT_NEAR(shaped_text->width(),
              shaped_text->positionToAdvance(0, exception_state), kTolerance);
  EXPECT_NEAR(0.0,
              shaped_text->positionToAdvance(text.length(), exception_state),
              kTolerance);
  double previous = shaped_text->width() + 1;
  for (unsigned i = 0; i <= text.length(); ++i) {
    const double advance = shaped_text->positionToAdvance(i, exception_state);
    EXPECT_LT(advance, previous) << "position " << i;
    previous = advance;
  }
  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(ShapedTextTest, PositionToAdvanceBidiBoundaries) {
  // A position maps to the leading edge of the character at the position, so
  // both logical edges of an opposite-direction word map to the same visual
  // edge: the right edge of an RTL word in LTR text, and the left edge of an
  // LTR word in RTL text. This documents a known limitation.
  struct {
    const char* name;
    const char16_t* text;
    TextDirection direction;
    // The logical range of the opposite-direction word.
    unsigned word_start;
    unsigned word_end;
    bool at_right_edge;
  } cases[] = {
      {"ltr", u"abc \u0645\u0631\u062D\u0628\u0627 def", TextDirection::kLtr, 4,
       9, true},
      {"rtl", u"\u0645\u0631\u062D\u0628\u0627 abc \u0634\u0643\u0631\u0627",
       TextDirection::kRtl, 6, 9, false},
  };
  for (const auto& c : cases) {
    SCOPED_TRACE(c.name);
    ShapedText* shaped_text = Shape(String(c.text), kArabicFont, c.direction);

    auto runs = shaped_text->glyphRuns();
    auto word = std::ranges::find_if(
        runs, [&](const auto& run) { return run->start() == c.word_start; });
    ASSERT_NE(word, runs.end());
    ASSERT_EQ(c.word_end, (*word)->end());
    const double edge =
        c.at_right_edge ? (*word)->x() + (*word)->width() : (*word)->x();

    DummyExceptionStateForTesting exception_state;
    EXPECT_NEAR(edge,
                shaped_text->positionToAdvance(c.word_start, exception_state),
                kTolerance);
    EXPECT_NEAR(edge,
                shaped_text->positionToAdvance(c.word_end, exception_state),
                kTolerance);
    EXPECT_FALSE(exception_state.HadException());
  }
}

TEST_F(ShapedTextTest, PositionToAdvanceOutOfRange) {
  ShapedText* shaped_text = Shape(u"0123456789");

  DummyExceptionStateForTesting exception_state;
  EXPECT_EQ(0.0, shaped_text->positionToAdvance(11, exception_state));
  EXPECT_EQ(DOMExceptionCode::kIndexSizeError,
            exception_state.CodeAs<DOMExceptionCode>());
}

TEST_F(ShapedTextTest, TextAlignOffsetsAllQueries) {
  const String text(u"0123456789");
  ShapedText* left = Shape(text);
  const double width = left->width();
  DummyExceptionStateForTesting exception_state;
  const DOMRectReadOnly* left_bounds =
      left->actualBoundingBox(0, text.length(), exception_state);

  struct {
    V8CanvasTextAlign::Enum align;
    TextDirection direction;
    double expected_dx;
  } cases[] = {
      {V8CanvasTextAlign::Enum::kLeft, TextDirection::kLtr, 0},
      {V8CanvasTextAlign::Enum::kStart, TextDirection::kLtr, 0},
      {V8CanvasTextAlign::Enum::kCenter, TextDirection::kLtr, width / 2},
      {V8CanvasTextAlign::Enum::kRight, TextDirection::kLtr, width},
      {V8CanvasTextAlign::Enum::kEnd, TextDirection::kLtr, width},
      {V8CanvasTextAlign::Enum::kStart, TextDirection::kRtl, width},
      {V8CanvasTextAlign::Enum::kEnd, TextDirection::kRtl, 0},
  };
  for (const auto& c : cases) {
    SCOPED_TRACE(testing::Message()
                 << "align " << static_cast<int>(c.align) << ", "
                 << (c.direction == TextDirection::kLtr ? "ltr" : "rtl"));
    ShapedText* shaped_text = Shape(text, kLatinFont, c.direction, c.align);
    EXPECT_NEAR(c.expected_dx, shaped_text->TextAlignDx(), kTolerance);

    EXPECT_NEAR(-c.expected_dx,
                shaped_text->positionToAdvance(0, exception_state), kTolerance);
    EXPECT_EQ(0u, shaped_text->advanceToPosition(-c.expected_dx + kTolerance));

    auto runs = shaped_text->glyphRuns();
    ASSERT_EQ(1u, runs.size());
    EXPECT_NEAR(-c.expected_dx, runs[0]->x(), kTolerance);

    auto clusters = shaped_text->glyphClusters();
    ASSERT_FALSE(clusters.empty());
    EXPECT_NEAR(-c.expected_dx, clusters[0]->x(), kTolerance);

    auto rects = shaped_text->selectionRects(0, text.length(), exception_state);
    ASSERT_EQ(1u, rects.size());
    EXPECT_NEAR(-c.expected_dx, rects[0]->x(), kTolerance);

    const DOMRectReadOnly* bounds =
        shaped_text->actualBoundingBox(0, text.length(), exception_state);
    EXPECT_NEAR(left_bounds->x() - c.expected_dx, bounds->x(), kTolerance);
    EXPECT_NEAR(left_bounds->width(), bounds->width(), kTolerance);
    EXPECT_FALSE(exception_state.HadException());
  }
}

TEST_F(ShapedTextTest, GlyphRunsMixedBidi) {
  // "abc " + Arabic + " def": the Arabic word is a separate RTL run.
  const String text(u"abc \u0645\u0631\u062D\u0628\u0627 def");
  ShapedText* shaped_text = Shape(text, kArabicFont);

  auto runs = shaped_text->glyphRuns();
  ExpectVisualPartition(runs, *shaped_text);
  EXPECT_GE(runs.size(), 3u);
  EXPECT_TRUE(std::any_of(runs.begin(), runs.end(), [](const auto& run) {
    return run->start() == 4u && run->end() == 9u;
  }));
}

TEST_F(ShapedTextTest, GlyphRunsSplitAtFallbackFonts) {
  const auto [font, latin_font, cjk_font] = CreateFallbackFont();
  const String text(u"abc\u697D\u3057\u3044def");
  ShapedText* shaped_text = Shape(text, font);

  auto runs = shaped_text->glyphRuns();
  ExpectVisualPartition(runs, *shaped_text);
  ASSERT_EQ(3u, runs.size());
  EXPECT_EQ(0u, runs[0]->start());
  EXPECT_EQ(3u, runs[0]->end());
  EXPECT_EQ(latin_font, runs[0]->GetFontData());
  EXPECT_EQ(3u, runs[1]->start());
  EXPECT_EQ(6u, runs[1]->end());
  EXPECT_EQ(cjk_font, runs[1]->GetFontData());
  EXPECT_EQ(6u, runs[2]->start());
  EXPECT_EQ(9u, runs[2]->end());
  EXPECT_EQ(latin_font, runs[2]->GetFontData());

  // Clusters carry the same fonts as the runs that contain them.
  for (const auto& cluster : shaped_text->glyphClusters()) {
    const bool is_cjk = cluster->start() >= 3u && cluster->end() <= 6u;
    EXPECT_EQ(is_cjk ? cjk_font : latin_font, cluster->GetFontData())
        << "cluster at " << cluster->start();
  }
}

TEST_F(ShapedTextTest, FallbackGlyphRunMetricsUsePrimaryFontBaseline) {
  // All the runs are drawn at the primary font's textBaseline anchor, so the
  // vertical metrics of a fallback font run must be relative to it too.
  const auto [font, latin_font, cjk_font] = CreateFallbackFont();
  const String text(u"abc\u697D\u3057\u3044def");

  auto alphabetic_runs = Shape(text, font)->glyphRuns();
  ASSERT_EQ(3u, alphabetic_runs.size());
  TextMetrics* alphabetic_metrics = alphabetic_runs[1]->textMetrics();

  for (const auto baseline : {
           V8CanvasTextBaseline::Enum::kTop,
           V8CanvasTextBaseline::Enum::kHanging,
           V8CanvasTextBaseline::Enum::kMiddle,
           V8CanvasTextBaseline::Enum::kAlphabetic,
           V8CanvasTextBaseline::Enum::kIdeographic,
           V8CanvasTextBaseline::Enum::kBottom,
       }) {
    SCOPED_TRACE(V8CanvasTextBaseline(baseline).AsCStr());
    // The fonts disagree on the anchor position for this baseline, otherwise
    // this test can't tell them apart.
    if (baseline != V8CanvasTextBaseline::Enum::kAlphabetic) {
      ASSERT_GT(std::abs(TextMetrics::GetFontBaseline(baseline, *latin_font) -
                         TextMetrics::GetFontBaseline(baseline, *cjk_font)),
                kTolerance);
    }

    auto runs = Shape(text, font, TextDirection::kLtr,
                      V8CanvasTextAlign::Enum::kLeft, baseline)
                    ->glyphRuns();
    ASSERT_EQ(3u, runs.size());
    ASSERT_EQ(cjk_font, runs[1]->GetFontData());
    TextMetrics* metrics = runs[1]->textMetrics();

    // Moving the anchor from the alphabetic baseline moves all the metrics by
    // the primary font's offset for `baseline`.
    const double dy = TextMetrics::GetFontBaseline(baseline, *latin_font);
    EXPECT_NEAR(alphabetic_metrics->fontBoundingBoxAscent() - dy,
                metrics->fontBoundingBoxAscent(), kTolerance);
    EXPECT_NEAR(alphabetic_metrics->fontBoundingBoxDescent() + dy,
                metrics->fontBoundingBoxDescent(), kTolerance);
    EXPECT_NEAR(alphabetic_metrics->actualBoundingBoxAscent() - dy,
                metrics->actualBoundingBoxAscent(), kTolerance);
    EXPECT_NEAR(alphabetic_metrics->actualBoundingBoxDescent() + dy,
                metrics->actualBoundingBoxDescent(), kTolerance);
    EXPECT_NEAR(alphabetic_metrics->emHeightAscent() - dy,
                metrics->emHeightAscent(), kTolerance);
    EXPECT_NEAR(alphabetic_metrics->emHeightDescent() + dy,
                metrics->emHeightDescent(), kTolerance);
    EXPECT_NEAR(alphabetic_metrics->alphabeticBaseline() - dy,
                metrics->alphabeticBaseline(), kTolerance);
    EXPECT_NEAR(alphabetic_metrics->hangingBaseline() - dy,
                metrics->hangingBaseline(), kTolerance);
    EXPECT_NEAR(alphabetic_metrics->ideographicBaseline() - dy,
                metrics->ideographicBaseline(), kTolerance);
  }
}

TEST_F(ShapedTextTest, GlyphClustersLigature) {
  // Roboto has an "fi" ligature, so "fi" is a single cluster.
  ShapedText* shaped_text = Shape(u"0fi1");

  auto clusters = shaped_text->glyphClusters();
  ASSERT_EQ(3u, clusters.size());
  EXPECT_EQ(0u, clusters[0]->start());
  EXPECT_EQ(1u, clusters[0]->end());
  EXPECT_EQ(1u, clusters[1]->start());
  EXPECT_EQ(3u, clusters[1]->end());
  EXPECT_EQ(3u, clusters[2]->start());
  EXPECT_EQ(4u, clusters[2]->end());

  // A range that starts inside the ligature includes the whole cluster.
  DummyExceptionStateForTesting exception_state;
  auto range = shaped_text->glyphClusters(2, 3, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_EQ(1u, range.size());
  EXPECT_EQ(1u, range[0]->start());
  EXPECT_EQ(3u, range[0]->end());
}

TEST_F(ShapedTextTest, GlyphClustersRange) {
  ShapedText* shaped_text = Shape(u"0123456789");

  DummyExceptionStateForTesting exception_state;
  auto clusters = shaped_text->glyphClusters(2, 5, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  ASSERT_EQ(3u, clusters.size());
  for (wtf_size_t i = 0; i < clusters.size(); ++i) {
    EXPECT_EQ(i + 2, clusters[i]->start());
    EXPECT_EQ(i + 3, clusters[i]->end());
    EXPECT_NEAR(shaped_text->positionToAdvance(i + 2, exception_state),
                clusters[i]->x(), kTolerance);
    EXPECT_NEAR(shaped_text->positionToAdvance(i + 3, exception_state) -
                    shaped_text->positionToAdvance(i + 2, exception_state),
                clusters[i]->width(), kTolerance);
  }

  EXPECT_TRUE(shaped_text->glyphClusters(3, 3, exception_state).empty());
  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(ShapedTextTest, GlyphClustersOutOfRange) {
  ShapedText* shaped_text = Shape(u"0123456789");

  {
    DummyExceptionStateForTesting exception_state;
    EXPECT_TRUE(shaped_text->glyphClusters(0, 11, exception_state).empty());
    EXPECT_EQ(DOMExceptionCode::kIndexSizeError,
              exception_state.CodeAs<DOMExceptionCode>());
  }
  {
    DummyExceptionStateForTesting exception_state;
    EXPECT_TRUE(shaped_text->glyphClusters(10, 10, exception_state).empty());
    EXPECT_EQ(DOMExceptionCode::kIndexSizeError,
              exception_state.CodeAs<DOMExceptionCode>());
  }
}

TEST_F(ShapedTextTest, GlyphRunTextMetrics) {
  ShapedText* shaped_text = Shape(u"0123456789");
  TextMetrics* text_metrics = MakeGarbageCollected<TextMetrics>(*shaped_text);

  auto runs = shaped_text->glyphRuns();
  ASSERT_EQ(1u, runs.size());
  TextMetrics* run_metrics = runs[0]->textMetrics();
  EXPECT_EQ(run_metrics, runs[0]->textMetrics());
  EXPECT_NEAR(runs[0]->width(), run_metrics->width(), kTolerance);
  EXPECT_NEAR(text_metrics->fontBoundingBoxAscent(),
              run_metrics->fontBoundingBoxAscent(), kTolerance);
  EXPECT_NEAR(text_metrics->fontBoundingBoxDescent(),
              run_metrics->fontBoundingBoxDescent(), kTolerance);
  EXPECT_NEAR(text_metrics->actualBoundingBoxLeft(),
              run_metrics->actualBoundingBoxLeft(), kTolerance);
  EXPECT_NEAR(text_metrics->actualBoundingBoxRight(),
              run_metrics->actualBoundingBoxRight(), kTolerance);
  EXPECT_NEAR(text_metrics->actualBoundingBoxAscent(),
              run_metrics->actualBoundingBoxAscent(), kTolerance);
  EXPECT_NEAR(text_metrics->actualBoundingBoxDescent(),
              run_metrics->actualBoundingBoxDescent(), kTolerance);

  for (const auto& cluster : shaped_text->glyphClusters()) {
    TextMetrics* cluster_metrics = cluster->textMetrics();
    EXPECT_NEAR(cluster->width(), cluster_metrics->width(), kTolerance);
    // Ink bounds are relative to the cluster's own origin.
    EXPECT_LT(-cluster_metrics->actualBoundingBoxLeft(),
              cluster_metrics->actualBoundingBoxRight());
    EXPECT_LE(cluster_metrics->actualBoundingBoxRight(), cluster->width() + 1);
  }
}

TEST_F(ShapedTextTest, SelectionRects) {
  const String text(u"0123456789");
  ShapedText* shaped_text = Shape(text);
  TextMetrics* text_metrics = MakeGarbageCollected<TextMetrics>(*shaped_text);
  const double y = -text_metrics->fontBoundingBoxAscent();
  const double height = text_metrics->fontBoundingBoxAscent() +
                        text_metrics->fontBoundingBoxDescent();
  DummyExceptionStateForTesting exception_state;

  auto rects = shaped_text->selectionRects(0, text.length(), exception_state);
  ASSERT_EQ(1u, rects.size());
  EXPECT_NEAR(0.0, rects[0]->x(), kTolerance);
  EXPECT_NEAR(shaped_text->width(), rects[0]->width(), kTolerance);
  EXPECT_NEAR(y, rects[0]->y(), kTolerance);
  EXPECT_NEAR(height, rects[0]->height(), kTolerance);

  rects = shaped_text->selectionRects(2, 5, exception_state);
  ASSERT_EQ(1u, rects.size());
  const double from = shaped_text->positionToAdvance(2, exception_state);
  const double to = shaped_text->positionToAdvance(5, exception_state);
  EXPECT_NEAR(from, rects[0]->x(), kTolerance);
  EXPECT_NEAR(to - from, rects[0]->width(), kTolerance);

  // A collapsed range is a caret: a zero-width rect at that position.
  rects = shaped_text->selectionRects(3, 3, exception_state);
  ASSERT_EQ(1u, rects.size());
  EXPECT_NEAR(shaped_text->positionToAdvance(3, exception_state), rects[0]->x(),
              kTolerance);
  EXPECT_NEAR(0.0, rects[0]->width(), kTolerance);
  EXPECT_FALSE(exception_state.HadException());

  EXPECT_TRUE(shaped_text->selectionRects(0, 11, exception_state).empty());
  EXPECT_EQ(DOMExceptionCode::kIndexSizeError,
            exception_state.CodeAs<DOMExceptionCode>());
}

TEST_F(ShapedTextTest, SelectionRectsMixedBidi) {
  const String text(u"abc \u0645\u0631\u062D\u0628\u0627 def");
  ShapedText* shaped_text = Shape(text, kArabicFont);
  DummyExceptionStateForTesting exception_state;

  // One rect per direction run; together they span the whole text.
  auto rects = shaped_text->selectionRects(0, text.length(), exception_state);
  ASSERT_GE(rects.size(), 3u);
  double total_width = 0;
  for (const auto& rect : rects) {
    total_width += rect->width();
  }
  EXPECT_NEAR(shaped_text->width(), total_width, kTolerance);

  // A range inside the Arabic word is a single rect, inside that word's run.
  const GlyphRun* arabic_run = nullptr;
  for (const auto& run : shaped_text->glyphRuns()) {
    if (run->start() == 4u && run->end() == 9u) {
      arabic_run = run.Get();
    }
  }
  ASSERT_TRUE(arabic_run);
  rects = shaped_text->selectionRects(5, 7, exception_state);
  ASSERT_EQ(1u, rects.size());
  EXPECT_GT(rects[0]->width(), 0);
  EXPECT_GE(rects[0]->x(), arabic_run->x() - kTolerance);
  EXPECT_LE(rects[0]->right(),
            arabic_run->x() + arabic_run->width() + kTolerance);
  EXPECT_FALSE(exception_state.HadException());
}

TEST_F(ShapedTextTest, VerticalMetricsMatchTextMetrics) {
  const String text(u"0123456789");
  for (auto baseline :
       {V8CanvasTextBaseline::Enum::kTop, V8CanvasTextBaseline::Enum::kHanging,
        V8CanvasTextBaseline::Enum::kMiddle,
        V8CanvasTextBaseline::Enum::kAlphabetic,
        V8CanvasTextBaseline::Enum::kIdeographic,
        V8CanvasTextBaseline::Enum::kBottom}) {
    SCOPED_TRACE(testing::Message()
                 << "baseline " << static_cast<int>(baseline));
    ShapedText* shaped_text = Shape(text, kLatinFont, TextDirection::kLtr,
                                    V8CanvasTextAlign::Enum::kLeft, baseline);
    TextMetrics* text_metrics = MakeGarbageCollected<TextMetrics>(*shaped_text);
    DummyExceptionStateForTesting exception_state;

    auto rects = shaped_text->selectionRects(0, text.length(), exception_state);
    ASSERT_EQ(1u, rects.size());
    EXPECT_NEAR(-text_metrics->fontBoundingBoxAscent(), rects[0]->top(),
                kTolerance);
    EXPECT_NEAR(text_metrics->fontBoundingBoxDescent(), rects[0]->bottom(),
                kTolerance);

    const DOMRectReadOnly* bounds =
        shaped_text->actualBoundingBox(0, text.length(), exception_state);
    EXPECT_NEAR(-text_metrics->actualBoundingBoxAscent(), bounds->top(),
                kTolerance);
    EXPECT_NEAR(text_metrics->actualBoundingBoxDescent(), bounds->bottom(),
                kTolerance);
    EXPECT_NEAR(-text_metrics->actualBoundingBoxLeft(), bounds->left(),
                kTolerance);
    EXPECT_NEAR(text_metrics->actualBoundingBoxRight(), bounds->right(),
                kTolerance);
    EXPECT_FALSE(exception_state.HadException());
  }
}

TEST_F(ShapedTextTest, ActualBoundingBoxRange) {
  const String text(u"0123456789");
  ShapedText* shaped_text = Shape(text);
  DummyExceptionStateForTesting exception_state;

  const DOMRectReadOnly* full =
      shaped_text->actualBoundingBox(0, text.length(), exception_state);
  const DOMRectReadOnly* part =
      shaped_text->actualBoundingBox(2, 5, exception_state);
  EXPECT_FALSE(exception_state.HadException());
  EXPECT_GT(part->width(), 0);
  EXPECT_LT(part->width(), full->width());
  EXPECT_GE(part->left(), full->left() - kTolerance);
  EXPECT_LE(part->right(), full->right() + kTolerance);
  EXPECT_GE(part->top(), full->top() - kTolerance);
  EXPECT_LE(part->bottom(), full->bottom() + kTolerance);
  // The ink of "234" starts after the advance of "01".
  EXPECT_GE(part->left(),
            shaped_text->positionToAdvance(2, exception_state) - 1);

  shaped_text->actualBoundingBox(10, 10, exception_state);
  EXPECT_EQ(DOMExceptionCode::kIndexSizeError,
            exception_state.CodeAs<DOMExceptionCode>());
}

}  // namespace blink
