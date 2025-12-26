// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/resolver/filter_operation_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"
#include "third_party/blink/renderer/core/css/threaded/multi_threaded_test_util.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_shaper.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_view.h"
#include "third_party/blink/renderer/platform/fonts/text_fragment_paint_info.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

using blink::test::CreateTestFont;

namespace blink {

TSAN_TEST(FontObjectThreadedTest, Language) {
  RunOnThreads([]() { EXPECT_EQ(DefaultLanguage(), "en-US"); });
}

TSAN_TEST(FontObjectThreadedTest, GetFontDefinition) {
  RunOnThreads([]() {
    auto* style =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
    CSSParser::ParseValue(style, CSSPropertyID::kFont, "15px Ahem", true);

    FontDescription desc = FontStyleResolver::ComputeFont(*style, nullptr);

    EXPECT_EQ(desc.SpecifiedSize(), 15);
    EXPECT_EQ(desc.ComputedSize(), 15);
    EXPECT_EQ(desc.Family().FamilyName(), "Ahem");
  });
}

TSAN_TEST(FontObjectThreadedTest, GetDefaultFontData) {
  callbacks_per_thread_ = 30;
  num_threads_ = 5;
  RunOnThreads([]() {
    for (FontDescription::GenericFamilyType family_type :
         {FontDescription::kStandardFamily, FontDescription::kWebkitBodyFamily,
          FontDescription::kSerifFamily, FontDescription::kSansSerifFamily,
          FontDescription::kMonospaceFamily, FontDescription::kCursiveFamily,
          FontDescription::kFantasyFamily}) {
      FontDescription font_description;
      font_description.SetComputedSize(12.0);
      font_description.SetLocale(LayoutLocale::Get(AtomicString("en")));
      ASSERT_EQ(USCRIPT_LATIN, font_description.GetScript());
      font_description.SetGenericFamily(family_type);

      Font* font = MakeGarbageCollected<Font>(font_description);
      ASSERT_TRUE(font->PrimaryFont());
    }
  });
}

// This test passes by not crashing TSAN.
TSAN_TEST(FontObjectThreadedTest, FontSelector) {
  RunOnThreads([]() {
    CreateTestFont(AtomicString("Ahem"), test::CoreTestDataPath("Ahem.ttf"),
                   16);
  });
}

TSAN_TEST(FontObjectThreadedTest, TextIntercepts) {
  callbacks_per_thread_ = 10;
  RunOnThreads([]() {
    Font* font = CreateTestFont(AtomicString("Ahem"),
                                test::CoreTestDataPath("Ahem.ttf"), 16);
    // A sequence of LATIN CAPITAL LETTER E WITH ACUTE and LATIN SMALL LETTER P
    // characters. E ACUTES are squares above the baseline in Ahem, while p's
    // are rectangles below the baseline.
    UChar ahem_above_below_baseline_string[] = {0xc9, 0x70, 0xc9, 0x70, 0xc9,
                                                0x70, 0xc9, 0x70, 0xc9};
    String ahem_above_below_baseline{
        base::span(ahem_above_below_baseline_string)};
    ShapeResult* shape_result = HarfBuzzShaper(ahem_above_below_baseline)
                                    .Shape(font, TextDirection::kLtr);
    TextFragmentPaintInfo text_paint_info{
        ahem_above_below_baseline, 0, ahem_above_below_baseline.length(),
        ShapeResultView::Create(shape_result)};
    cc::PaintFlags default_paint;
    std::tuple<float, float> below_baseline_bounds = std::make_tuple(2, 4);
    Vector<Font::TextIntercept> text_intercepts;

    // 4 intercept ranges for below baseline p glyphs in the test string
    font->GetTextIntercepts(text_paint_info, default_paint,
                            below_baseline_bounds, text_intercepts);
    EXPECT_EQ(text_intercepts.size(), 4u);
    for (auto text_intercept : text_intercepts) {
      EXPECT_GT(text_intercept.end_, text_intercept.begin_);
    }

    std::tuple<float, float> above_baseline_bounds = std::make_tuple(-4, -2);
    // 5 intercept ranges for the above baseline E ACUTE glyphs
    font->GetTextIntercepts(text_paint_info, default_paint,
                            above_baseline_bounds, text_intercepts);
    EXPECT_EQ(text_intercepts.size(), 5u);
    for (auto text_intercept : text_intercepts) {
      EXPECT_GT(text_intercept.end_, text_intercept.begin_);
    }
  });
}

TSAN_TEST(FontObjectThreadedTest, WordShaperTest) {
  RunOnThreads([]() {
    FontDescription font_description;
    font_description.SetComputedSize(12.0);
    font_description.SetLocale(LayoutLocale::Get(AtomicString("en")));
    ASSERT_EQ(USCRIPT_LATIN, font_description.GetScript());
    font_description.SetGenericFamily(FontDescription::kStandardFamily);

    Font* font = MakeGarbageCollected<Font>(font_description);
    ASSERT_TRUE(font->CanShapeWordByWord());

    TextRun text_run(base::byte_span_from_cstring("ABC DEF."));
    auto* node = MakeGarbageCollected<PlainTextNode>(
        text_run, /* normalize_space */ true, *font, /* supports_bidi */ true,
        nullptr);

    ASSERT_EQ(3u, node->ItemList().size());
    const ShapeResult* result = node->ItemList()[0].GetShapeResult();
    EXPECT_EQ(0u, result->StartIndex());
    EXPECT_EQ(3u, result->EndIndex());

    result = node->ItemList()[1].GetShapeResult();
    EXPECT_EQ(0u, result->StartIndex());
    EXPECT_EQ(1u, result->EndIndex());

    result = node->ItemList()[2].GetShapeResult();
    EXPECT_EQ(0u, result->StartIndex());
    EXPECT_EQ(4u, result->EndIndex());
  });
}

}  // namespace blink
