// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font.h"

#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/fonts/font_variant_emoji.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/testing/font_test_base.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/text/tab_size.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

using blink::test::CreateTestFont;

namespace blink {

namespace {

Font CreateVerticalUprightTestFont(const AtomicString& family_name,
                                   const String& font_path,
                                   float size) {
  return CreateTestFont(
      family_name, font_path, size, /* ligatures */ nullptr,
      kNormalVariantEmoji, [](FontDescription* font_description) {
        font_description->SetOrientation(FontOrientation::kVerticalUpright);
      });
}

}  // namespace

class FontTest : public FontTestBase {
 public:
  Font CreateFontWithOrientation(const Font& base_font,
                                 FontOrientation orientation) {
    FontDescription font_description = base_font.GetFontDescription();
    font_description.SetOrientation(orientation);
    return Font(font_description);
  }
};

TEST_F(FontTest, FonteMetricsCapHeight) {
  const auto cap_height_of = [](const char* font_path, float size) {
    Font font = CreateTestFont(AtomicString("test"),
                               test::PlatformTestDataPath(font_path), size);
    const SimpleFontData* const font_data = font.PrimaryFont();
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
  Font font = test::CreateAhemFont(100);
  const SimpleFontData* font_data = font.PrimaryFont();
  const FontMetrics& metrics = font_data->GetFontMetrics();
  EXPECT_EQ(metrics.FixedAscent(), 80);
  EXPECT_EQ(metrics.FixedDescent(), 20);
  EXPECT_EQ(metrics.FixedAlphabetic(FontBaseline::kAlphabeticBaseline), 0);
  EXPECT_EQ(metrics.FixedAlphabetic(FontBaseline::kCentralBaseline), -30);
  EXPECT_EQ(metrics.FixedCapHeight(FontBaseline::kAlphabeticBaseline), 80);
  EXPECT_EQ(metrics.FixedCapHeight(FontBaseline::kCentralBaseline), 50);
}

TEST_F(FontTest, IdeographicFullWidthAhem) {
  Font font = CreateTestFont(AtomicString("Ahem"),
                             test::PlatformTestDataPath("Ahem.woff"), 16);
  const SimpleFontData* font_data = font.PrimaryFont();
  ASSERT_TRUE(font_data);
  EXPECT_FALSE(font_data->IdeographicInlineSize().has_value());
}

TEST_F(FontTest, IdeographicFullWidthCjkFull) {
  Font font = CreateTestFont(
      AtomicString("M PLUS 1p"),
      blink::test::BlinkWebTestsFontsTestDataPath("mplus-1p-regular.woff"), 16);
  const SimpleFontData* font_data = font.PrimaryFont();
  ASSERT_TRUE(font_data);
  EXPECT_TRUE(font_data->IdeographicInlineSize().has_value());
  EXPECT_EQ(*font_data->IdeographicInlineSize(), 16);
}

TEST_F(FontTest, IdeographicFullWidthCjkNarrow) {
  Font font = CreateTestFont(AtomicString("CSSHWOrientationTest"),
                             blink::test::BlinkWebTestsFontsTestDataPath(
                                 "adobe-fonts/CSSHWOrientationTest.otf"),
                             16);
  const SimpleFontData* font_data = font.PrimaryFont();
  ASSERT_TRUE(font_data);
  EXPECT_TRUE(font_data->IdeographicInlineSize().has_value());
  EXPECT_EQ(*font_data->IdeographicInlineSize(), 8);
}

// A font that does not have the CJK "water" glyph.
TEST_F(FontTest, IdeographicFullWidthUprightAhem) {
  Font font = CreateTestFont(AtomicString("Ahem"),
                             test::PlatformTestDataPath("Ahem.woff"), 16);
  const SimpleFontData* font_data = font.PrimaryFont();
  ASSERT_TRUE(font_data);
  EXPECT_FALSE(font_data->IdeographicInlineSize().has_value());
}

// A Japanese font, with the "water" glyph, but the `vmtx` table is missing.
TEST_F(FontTest, IdeographicFullWidthUprightCjkNoVmtx) {
  Font font = CreateVerticalUprightTestFont(
      AtomicString("M PLUS 1p"),
      blink::test::BlinkWebTestsFontsTestDataPath("mplus-1p-regular.woff"), 16);
  const SimpleFontData* font_data = font.PrimaryFont();
  ASSERT_TRUE(font_data);
  // If the `vmtx` table is missing, the vertical advance should be synthesized.
  ASSERT_TRUE(font_data->IdeographicInlineSize().has_value());
  EXPECT_EQ(*font_data->IdeographicInlineSize(),
            font_data->GetFontMetrics().Height());
}

// A Japanese font, with the "water" glyph, with the `vmtx` table.
TEST_F(FontTest, IdeographicFullWidthUprightCjkVmtx) {
  Font font =
      CreateVerticalUprightTestFont(AtomicString("CSSHWOrientationTest"),
                                    blink::test::BlinkWebTestsFontsTestDataPath(
                                        "adobe-fonts/CSSHWOrientationTest.otf"),
                                    16);
  const SimpleFontData* font_data = font.PrimaryFont();
  ASSERT_TRUE(font_data);
  ASSERT_TRUE(font_data->IdeographicInlineSize().has_value());
  EXPECT_EQ(*font_data->IdeographicInlineSize(), 16);
}

TEST_F(FontTest, TextIntercepts) {
  Font font = CreateTestFont(AtomicString("Ahem"),
                             test::PlatformTestDataPath("Ahem.woff"), 16);
  // A sequence of LATIN CAPITAL LETTER E WITH ACUTE and LATIN SMALL LETTER P
  // characters. E ACUTES are squares above the baseline in Ahem, while p's
  // are rectangles below the baseline.
  UChar ahem_above_below_baseline_string[] = {0xc9, 0x70, 0xc9, 0x70, 0xc9,
                                              0x70, 0xc9, 0x70, 0xc9};
  TextRun ahem_above_below_baseline(ahem_above_below_baseline_string, 9);
  TextRunPaintInfo text_run_paint_info(ahem_above_below_baseline);
  cc::PaintFlags default_paint;

  std::tuple<float, float> below_baseline_bounds = std::make_tuple(2, 4);
  Vector<Font::TextIntercept> text_intercepts;
  // 4 intercept ranges for below baseline p glyphs in the test string
  font.GetTextIntercepts(text_run_paint_info, default_paint,
                         below_baseline_bounds, text_intercepts);
  EXPECT_EQ(text_intercepts.size(), 4u);
  for (auto text_intercept : text_intercepts) {
    EXPECT_GT(text_intercept.end_, text_intercept.begin_);
  }

  std::tuple<float, float> above_baseline_bounds = std::make_tuple(-4, -2);
  // 5 intercept ranges for the above baseline E ACUTE glyphs
  font.GetTextIntercepts(text_run_paint_info, default_paint,
                         above_baseline_bounds, text_intercepts);
  EXPECT_EQ(text_intercepts.size(), 5u);
  for (auto text_intercept : text_intercepts) {
    EXPECT_GT(text_intercept.end_, text_intercept.begin_);
  }
}

TEST_F(FontTest, TabWidthZero) {
  Font font = CreateTestFont(AtomicString("Ahem"),
                             test::PlatformTestDataPath("Ahem.woff"), 0);
  TabSize tab_size(8);
  EXPECT_EQ(font.TabWidth(tab_size, .0f), .0f);
  EXPECT_EQ(font.TabWidth(tab_size, LayoutUnit()), LayoutUnit());
}

TEST_F(FontTest, NullifyPrimaryFontForTesting) {
  Font font = CreateTestFont(AtomicString("Ahem"),
                             test::PlatformTestDataPath("Ahem.woff"), 0);
  EXPECT_TRUE(font.PrimaryFont());
  font.NullifyPrimaryFontForTesting();
  EXPECT_FALSE(font.PrimaryFont());
}

}  // namespace blink
