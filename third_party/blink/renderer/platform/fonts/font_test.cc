// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font.h"

#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/font_variant_emoji.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/text/tab_size.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/utils/SkCustomTypeface.h"

using blink::test::CreateTestFont;

namespace blink {

namespace {

Font* CreateVerticalUprightTestFont(const AtomicString& family_name,
                                    const String& font_path,
                                    float size) {
  return CreateTestFont(
      family_name, font_path, size, /* ligatures */ nullptr,
      FontVariantEmoji::kNormal, [](FontDescription* font_description) {
        font_description->SetOrientation(FontOrientation::kVerticalUpright);
      });
}

}  // namespace

class FontTest : public FontTestBase {
 public:
  Font* CreateFontWithOrientation(const Font* base_font,
                                  FontOrientation orientation) {
    FontDescription font_description = base_font->GetFontDescription();
    font_description.SetOrientation(orientation);
    return MakeGarbageCollected<Font>(font_description);
  }
};

TEST_F(FontTest, FonteMetricsCapHeight) {
  const auto cap_height_of = [](const char* font_path, float size) {
    Font* font = CreateTestFont(AtomicString("test"),
                                test::PlatformTestDataPath(font_path), size);
    const SimpleFontData* const font_data = font->PrimaryFont();
    return font_data->GetFontMetrics().CapHeight();
  };

  EXPECT_FLOAT_EQ(80.0f, cap_height_of("Ahem.woff", 100));
  EXPECT_FLOAT_EQ(160.0f, cap_height_of("Ahem.woff", 200));

#if BUILDFLAG(IS_WIN)
  EXPECT_FLOAT_EQ(
      70.9961f, cap_height_of("third_party/Roboto/roboto-regular.woff2", 100));
  EXPECT_FLOAT_EQ(
      141.99219f,
      cap_height_of("third_party/Roboto/roboto-regular.woff2", 200));
#else
  EXPECT_FLOAT_EQ(
      71.09375f, cap_height_of("third_party/Roboto/roboto-regular.woff2", 100));
  EXPECT_FLOAT_EQ(
      142.1875f, cap_height_of("third_party/Roboto/roboto-regular.woff2", 200));
#endif
}

TEST_F(FontTest, ConvertBaseline) {
  Font* font = test::CreateAhemFont(100);
  const SimpleFontData* font_data = font->PrimaryFont();
  const FontMetrics& metrics = font_data->GetFontMetrics();
  EXPECT_EQ(metrics.FixedAscent(), 80);
  EXPECT_EQ(metrics.FixedDescent(), 20);
  EXPECT_EQ(metrics.FixedAlphabetic(FontBaseline::kAlphabetic), 0);
  EXPECT_EQ(metrics.FixedAlphabetic(FontBaseline::kCentral), -30);
  EXPECT_EQ(metrics.FixedCapHeight(FontBaseline::kAlphabetic), 80);
  EXPECT_EQ(metrics.FixedCapHeight(FontBaseline::kCentral), 50);
}

TEST_F(FontTest, PrimaryFontGenericFamilyOrLastResort) {
  Font* named_font = test::CreateAhemFont(100);
  const FontFallbackList::PrimaryFontData* named_data =
      named_font->EnsureFontFallbackList()->PrimaryFontDataWithSpace(
          named_font->GetFontDescription());
  EXPECT_FALSE(named_data->is_generic_family_or_last_resort);

  Font* generic_font = CreateTestFont(
      AtomicString("Ahem"), test::PlatformTestDataPath("Ahem.woff"), 100,
      /*ligatures=*/nullptr, FontVariantEmoji::kNormal,
      [](FontDescription* font_description) {
        font_description->SetFamily(FontFamily(
            AtomicString("AhemFamily"), FontFamily::Type::kGenericFamily));
      });
  const FontFallbackList::PrimaryFontData* generic_data =
      generic_font->EnsureFontFallbackList()->PrimaryFontDataWithSpace(
          generic_font->GetFontDescription());
  EXPECT_TRUE(generic_data->is_generic_family_or_last_resort);
}

TEST_F(FontTest, FixedPitchMatchesTypeface) {
  for (const char* path :
       {"Ahem.woff", "third_party/Roboto/roboto-regular.woff2"}) {
    SCOPED_TRACE(path);
    Font* font = CreateTestFont(AtomicString("test"),
                                test::PlatformTestDataPath(path), 16);
    const SimpleFontData* font_data = font->PrimaryFont();
    ASSERT_TRUE(font_data);
    const FontPlatformData& platform_data = font_data->PlatformData();
    ASSERT_TRUE(platform_data.Typeface());
    EXPECT_EQ(font_data->IsFixedPitch(),
              platform_data.Typeface()->isFixedPitch());

    const auto* zero_size_platform_data =
        MakeGarbageCollected<FontPlatformData>(platform_data, 0);
    const auto* zero_size_font_data =
        MakeGarbageCollected<SimpleFontData>(zero_size_platform_data);
    EXPECT_EQ(zero_size_font_data->IsFixedPitch(),
              platform_data.Typeface()->isFixedPitch());
  }
}

TEST_F(FontTest, IdeographicFullWidthAhem) {
  Font* font = CreateTestFont(AtomicString("Ahem"),
                              test::PlatformTestDataPath("Ahem.woff"), 16);
  const SimpleFontData* font_data = font->PrimaryFont();
  ASSERT_TRUE(font_data);
  EXPECT_FALSE(font_data->IdeographicInlineSize().has_value());
}

TEST_F(FontTest, IdeographicFullWidthCjkFull) {
  Font* font = CreateTestFont(
      AtomicString("M PLUS 1p"),
      blink::test::BlinkWebTestsFontsTestDataPath("mplus-1p-regular.woff"), 16);
  const SimpleFontData* font_data = font->PrimaryFont();
  ASSERT_TRUE(font_data);
  EXPECT_TRUE(font_data->IdeographicInlineSize().has_value());
  EXPECT_EQ(*font_data->IdeographicInlineSize(), 16);
}

TEST_F(FontTest, IdeographicFullWidthCjkNarrow) {
  ScopedNoFontAntialiasingForTest disable_no_font_antialiasing_for_test(false);

  Font* font = CreateTestFont(AtomicString("CSSHWOrientationTest"),
                              blink::test::BlinkWebTestsFontsTestDataPath(
                                  "adobe-fonts/CSSHWOrientationTest.otf"),
                              16);
  const SimpleFontData* font_data = font->PrimaryFont();
  ASSERT_TRUE(font_data);
  EXPECT_TRUE(font_data->IdeographicInlineSize().has_value());
  EXPECT_FLOAT_EQ(*font_data->IdeographicInlineSize(), 8.0f);
}

// A font that does not have the CJK "water" glyph.
TEST_F(FontTest, IdeographicFullWidthUprightAhem) {
  Font* font = CreateTestFont(AtomicString("Ahem"),
                              test::PlatformTestDataPath("Ahem.woff"), 16);
  const SimpleFontData* font_data = font->PrimaryFont();
  ASSERT_TRUE(font_data);
  EXPECT_FALSE(font_data->IdeographicInlineSize().has_value());
}

// A Japanese font, with the "water" glyph, but the `vmtx` table is missing.
TEST_F(FontTest, IdeographicFullWidthUprightCjkNoVmtx) {
  Font* font = CreateVerticalUprightTestFont(
      AtomicString("M PLUS 1p"),
      blink::test::BlinkWebTestsFontsTestDataPath("mplus-1p-regular.woff"), 16);
  const SimpleFontData* font_data = font->PrimaryFont();
  ASSERT_TRUE(font_data);
  // If the `vmtx` table is missing, the vertical advance should be synthesized.
  ASSERT_TRUE(font_data->IdeographicInlineSize().has_value());
  EXPECT_EQ(*font_data->IdeographicInlineSize(),
            font_data->GetFontMetrics().Height());
}

// A Japanese font, with the "water" glyph, with the `vmtx` table.
TEST_F(FontTest, IdeographicFullWidthUprightCjkVmtx) {
  Font* font =
      CreateVerticalUprightTestFont(AtomicString("CSSHWOrientationTest"),
                                    blink::test::BlinkWebTestsFontsTestDataPath(
                                        "adobe-fonts/CSSHWOrientationTest.otf"),
                                    16);
  const SimpleFontData* font_data = font->PrimaryFont();
  ASSERT_TRUE(font_data);
  ASSERT_TRUE(font_data->IdeographicInlineSize().has_value());
  EXPECT_EQ(*font_data->IdeographicInlineSize(), 16);
}

TEST_F(FontTest, TextIntercepts) {
  Font* font = CreateTestFont(AtomicString("Ahem"),
                              test::PlatformTestDataPath("Ahem.woff"), 16);
  // A sequence of LATIN CAPITAL LETTER E WITH ACUTE and LATIN SMALL LETTER P
  // characters. E ACUTES are squares above the baseline in Ahem, while p's
  // are rectangles below the baseline.
  UChar ahem_above_below_baseline_string[] = {0xc9, 0x70, 0xc9, 0x70, 0xc9,
                                              0x70, 0xc9, 0x70, 0xc9};
  String ahem_above_below_baseline{
      base::span(ahem_above_below_baseline_string)};
  ShapeResult* shape_result = HarfBuzzShaper(ahem_above_below_baseline)
                                  .Shape(font, TextDirection::kLtr);
  TextFragmentPaintInfo text_paint_info{ahem_above_below_baseline, 0,
                                        ahem_above_below_baseline.length(),
                                        ShapeResultView::Create(shape_result)};
  cc::PaintFlags default_paint;

  for (bool single_pass : {false, true}) {
    ScopedFontSinglePassTextInterceptsForTest scoped_single_pass(single_pass);
    std::tuple<float, float> below_baseline_bounds = std::make_tuple(2, 4);
    Vector<Font::TextIntercept> text_intercepts;
    // 4 intercept ranges for below baseline p glyphs in the test string
    font->GetTextIntercepts(
        text_paint_info, Font::InkSkipCjkHandling::kExcludeCjk, default_paint,
        below_baseline_bounds, text_intercepts);
    EXPECT_EQ(text_intercepts.size(), 4u);
    for (auto text_intercept : text_intercepts) {
      EXPECT_GT(text_intercept.end_, text_intercept.begin_);
    }

    std::tuple<float, float> above_baseline_bounds = std::make_tuple(-4, -2);
    // 5 intercept ranges for the above baseline E ACUTE glyphs
    font->GetTextIntercepts(
        text_paint_info, Font::InkSkipCjkHandling::kExcludeCjk, default_paint,
        above_baseline_bounds, text_intercepts);
    EXPECT_EQ(text_intercepts.size(), 5u);
    for (auto text_intercept : text_intercepts) {
      EXPECT_GT(text_intercept.end_, text_intercept.begin_);
    }
  }
}

// Regression test for crbug.com/590484039: synthetic bold stroking of a cubic
// with a subnormal-length initial handle produces a bevel join when subnormals
// are flushed to zero on x86, and a miter spike in IEEE float mode. Ensure
// that strike cache eviction during GetTextIntercepts() cannot cause a buffer
// overflow.
TEST_F(FontTest, TextInterceptsStrikeEvictionDuringLookup) {
  class SingleFontSelector final : public FontSelector {
   public:
    explicit SingleFontSelector(SimpleFontData* font_data)
        : font_data_(font_data) {}
    void Trace(Visitor* visitor) const override {
      visitor->Trace(font_data_);
      FontSelector::Trace(visitor);
    }
    FontData* GetFontData(const FontDescription&, const FontFamily&) override {
      return font_data_.Get();
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
    Member<SimpleFontData> font_data_;
  };

  auto make_font_for_typeface = [](sk_sp<SkTypeface> typeface,
                                   bool synthetic_bold) {
    auto* font_data = MakeGarbageCollected<SimpleFontData>(
        MakeGarbageCollected<FontPlatformData>(
            std::move(typeface), std::string(), /*text_size=*/64.0f,
            synthetic_bold, /*synthetic_italic=*/false,
            TextRenderingMode::kAuto, ResolvedFontFeatures{}));
    FontDescription desc;
    desc.SetFamily(
        FontFamily(AtomicString("custom"), FontFamily::Type::kFamilyName));
    desc.SetSpecifiedSize(64.0f);
    desc.SetComputedSize(64.0f);
    desc.SetSyntheticBold(synthetic_bold);
    return MakeGarbageCollected<Font>(
        desc, MakeGarbageCollected<SingleFontSelector>(font_data));
  };

  // SkCustomTypefaceBuilder paths are in 1-EM unit space and scaled by
  // text_size (64) in SkUserScalerContext.
  constexpr float kFontScale = 1.0f / 64.0f;
  SkCustomTypefaceBuilder builder1;
  // At (0, 0), the first cubic control point handle (1e-40, -1e-40) stays
  // subnormal even after Scale(64, 64) (6.4e-39 < FLT_MIN): under FTZ/DAZ it
  // flushes to (0, 0) (degenerate) and falls back to (1, -30) (exceeding miter
  // limit -> bevel join above y=1.5), whereas in IEEE mode it normalizes to a
  // 135-degree turn (miter spike past y=2.0).
  SkPath cubic_corner =
      SkPathBuilder()
          .moveTo(0.0f, -30.0f * kFontScale)
          .lineTo(0.0f, 0.0f)
          .cubicTo(1e-40f, -1e-40f, 1.0f * kFontScale, -30.0f * kFontScale,
                   10.0f * kFontScale, -30.0f * kFontScale)
          .close()
          .detach();
  builder1.setGlyph(1, 1.0f, cubic_corner);
  sk_sp<SkTypeface> typeface1 = builder1.detach();
  ASSERT_TRUE(typeface1);

  SkCustomTypefaceBuilder builder2;
  SkPath rect_glyph =
      SkPathBuilder()
          .addRect(SkRect::MakeLTRB(0.0f, -10.0f * kFontScale,
                                    20.0f * kFontScale, 10.0f * kFontScale))
          .detach();
  builder2.setGlyph(1, 1.0f, rect_glyph);
  sk_sp<SkTypeface> typeface2 = builder2.detach();
  ASSERT_TRUE(typeface2);

  Font* font1 = make_font_for_typeface(typeface1, /*synthetic_bold=*/true);
  Font* font2 = make_font_for_typeface(typeface2, /*synthetic_bold=*/false);
  Font* upright_ahem = CreateVerticalUprightTestFont(
      AtomicString("Ahem"), test::PlatformTestDataPath("Ahem.woff"), 64.0f);

  const ShapeResult* run1 = ShapeResult::CreateForStretchyMathOperator(
      font1, TextDirection::kLtr, /*glyph_variant=*/1, 64.0f);
  const ShapeResult* run2 =
      ShapeResult::CreateForStretchyMathOperator(font2, TextDirection::kLtr,
                                                 /*glyph_variant=*/1, 64.0f)
          ->CopyAdjustedOffset(1);
  // An upright vertical run between two horizontal runs splits the horizontal
  // runs into two separate kRegular blobs in ShapeResultBloberizer.
  const ShapeResult* upright_run = HarfBuzzShaper(String("a"))
                                       .Shape(upright_ahem, TextDirection::kLtr)
                                       ->CopyAdjustedOffset(2);
  // Reusing font1 in a second kRegular blob after run2 evicted typeface1's FTZ
  // strike also exercises rebuilding typeface1 in IEEE mode within a single
  // GetTextIntercepts() call.
  const ShapeResult* run4 =
      ShapeResult::CreateForStretchyMathOperator(font1, TextDirection::kLtr,
                                                 /*glyph_variant=*/1, 64.0f)
          ->CopyAdjustedOffset(3);

  cc::PaintFlags paint_flags;
  const std::tuple<float, float> bounds = std::make_tuple(1.5f, 2.0f);

  const int prev_count_limit = SkGraphics::SetFontCacheCountLimit(1);
  struct RestoreCacheLimit {
    int limit;
    ~RestoreCacheLimit() { SkGraphics::SetFontCacheCountLimit(limit); }
  } restore_limit{prev_count_limit};

  String single_char("a");
  TextFragmentPaintInfo warm_info{single_char, 0, 1,
                                  ShapeResultView::Create(run1)};

  // Verify IEEE mode produces 1 intercept on all architectures.
  {
    SkGraphics::PurgeFontCache();
    Vector<Font::TextIntercept> ieee_intercepts;
    font1->GetTextIntercepts(warm_info, Font::InkSkipCjkHandling::kIncludeCjk,
                             paint_flags, bounds, ieee_intercepts);
    EXPECT_EQ(ieee_intercepts.size(), 1u);
  }

  // Case 1: Single blob with two font runs. Pre-populate typeface1's strike
  // with subnormals flushed (0 intercepts on x86), then query in IEEE mode
  // while typeface2 evicts typeface1's strike mid-blob.
  {
    SkGraphics::PurgeFontCache();
    Vector<Font::TextIntercept> warm_intercepts;
    {
      cc::ScopedSubnormalFloatDisabler disable_subnormals;
      font1->GetTextIntercepts(warm_info, Font::InkSkipCjkHandling::kIncludeCjk,
                               paint_flags, bounds, warm_intercepts);
    }
#if defined(ARCH_CPU_X86_FAMILY)
    EXPECT_EQ(warm_intercepts.size(), 0u);
#endif

    String two_chars("ab");
    ShapeResultView::Segment segments[] = {{run1, 0, 1}, {run2, 1, 2}};
    TextFragmentPaintInfo paint_info{two_chars, 0, 2,
                                     ShapeResultView::Create(segments)};

    Vector<Font::TextIntercept> intercepts;
    font1->GetTextIntercepts(paint_info, Font::InkSkipCjkHandling::kIncludeCjk,
                             paint_flags, bounds, intercepts);
#if defined(ARCH_CPU_X86_FAMILY)
    EXPECT_EQ(intercepts.size(), 1u);
#else
    EXPECT_EQ(intercepts.size(), 2u);
#endif
  }

  // Case 2: Multiple non-upright blobs with strike eviction mid-blob.
  {
    SkGraphics::PurgeFontCache();
    Vector<Font::TextIntercept> warm_intercepts;
    {
      cc::ScopedSubnormalFloatDisabler disable_subnormals;
      font1->GetTextIntercepts(warm_info, Font::InkSkipCjkHandling::kIncludeCjk,
                               paint_flags, bounds, warm_intercepts);
    }
#if defined(ARCH_CPU_X86_FAMILY)
    EXPECT_EQ(warm_intercepts.size(), 0u);
#endif

    String four_chars("abcd");
    ShapeResultView::Segment segments[] = {
        {run1, 0, 1}, {run2, 1, 2}, {upright_run, 2, 3}, {run4, 3, 4}};
    TextFragmentPaintInfo paint_info{four_chars, 0, 4,
                                     ShapeResultView::Create(segments)};

    Vector<Font::TextIntercept> intercepts;
    font1->GetTextIntercepts(paint_info, Font::InkSkipCjkHandling::kIncludeCjk,
                             paint_flags, bounds, intercepts);
#if defined(ARCH_CPU_X86_FAMILY)
    EXPECT_EQ(intercepts.size(), 2u);
#else
    EXPECT_EQ(intercepts.size(), 3u);
#endif
  }
}

TEST_F(FontTest, TabWidthNegativePosition) {
  Font* font = CreateTestFont(AtomicString("Ahem"),
                              test::PlatformTestDataPath("Ahem.woff"), 10);
  TabSize tab_size(8);
  EXPECT_EQ(font->TabWidth(tab_size, -12.0f), 12.0f);
  EXPECT_EQ(font->TabWidth(tab_size, LayoutUnit(-12.0f)), LayoutUnit(12.0f));
}

TEST_F(FontTest, TabWidthZero) {
  Font* font = CreateTestFont(AtomicString("Ahem"),
                              test::PlatformTestDataPath("Ahem.woff"), 0);
  TabSize tab_size(8);
  EXPECT_EQ(font->TabWidth(tab_size, .0f), .0f);
  EXPECT_EQ(font->TabWidth(tab_size, LayoutUnit()), LayoutUnit());
}

TEST_F(FontTest, TabWidthWithSpacing) {
  Font* font = CreateTestFont(AtomicString("Ahem"),
                              test::PlatformTestDataPath("Ahem.woff"), 10);

  auto& font_description =
      const_cast<FontDescription&>(font->GetFontDescription());
  font_description.SetLetterSpacing(Length::Fixed(3));
  font_description.SetWordSpacing(Length::Fixed(20));
  TabSize tab_size(8);

  const float kTolerance = 1.0f / LayoutUnit::kFixedPointDenominator;
  float expected = 8 * (10 + 3 + 20);
  EXPECT_FLOAT_EQ(font->TabWidth(tab_size, .0f), expected);
  EXPECT_NEAR(font->TabWidth(tab_size, LayoutUnit()), LayoutUnit(expected),
              kTolerance);
}

TEST_F(FontTest, SimulateMissingLastResortFontForTesting) {
  Font* font = CreateTestFont(AtomicString("Ahem"),
                              test::PlatformTestDataPath("Ahem.woff"), 0);
  ASSERT_TRUE(font->PrimaryFont());
  EXPECT_GT(font->PrimaryFont()->PlatformData().Typeface()->countGlyphs(), 0);
  font->SimulateMissingLastResortFontForTesting();
  if (RuntimeEnabledFeatures::FontNonNullPrimaryFontEnabled()) {
    ASSERT_TRUE(font->PrimaryFont());
    EXPECT_EQ(font->PrimaryFont()->PlatformData().Typeface()->countGlyphs(), 0);
    EXPECT_TRUE(font->PrimaryFontWithDigitZero());
    EXPECT_TRUE(font->PrimaryFontWithCjkWater());
    EXPECT_TRUE(font->PrimaryFontForTabSize());
  } else {
    EXPECT_FALSE(font->PrimaryFont());
    EXPECT_FALSE(font->PrimaryFontWithDigitZero());
    EXPECT_FALSE(font->PrimaryFontWithCjkWater());
    EXPECT_FALSE(font->PrimaryFontForTabSize());
  }
}

}  // namespace blink
