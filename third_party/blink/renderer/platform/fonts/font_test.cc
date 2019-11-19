// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font.h"

#include "cc/paint/paint_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/text/tab_size.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

using blink::test::CreateTestFont;

namespace blink {

class FontTest : public ::testing::Test {
 public:
  Vector<int> GetExpandedRange(const String& text, bool ltr, int from, int to) {
    FontDescription::VariantLigatures ligatures(
        FontDescription::kEnabledLigaturesState);
    Font font = CreateTestFont(
        "roboto",
        test::PlatformTestDataPath("third_party/Roboto/roboto-regular.woff2"),
        100, &ligatures);

    TextRun text_run(
        text, /* xpos */ 0, /* expansion */ 0,
        TextRun::kAllowTrailingExpansion | TextRun::kForbidLeadingExpansion,
        ltr ? TextDirection::kLtr : TextDirection::kRtl, false);

    font.ExpandRangeToIncludePartialGlyphs(text_run, &from, &to);
    return Vector<int>({from, to});
  }
};

TEST_F(FontTest, TextIntercepts) {
  Font font =
      CreateTestFont("Ahem", test::PlatformTestDataPath("Ahem.woff"), 16);
  // A sequence of LATIN CAPITAL LETTER E WITH ACUTE and LATIN SMALL LETTER P
  // characters. E ACUTES are squares above the baseline in Ahem, while p's
  // are rectangles below the baseline.
  UChar ahem_above_below_baseline_string[] = {0xc9, 0x70, 0xc9, 0x70, 0xc9,
                                              0x70, 0xc9, 0x70, 0xc9};
  TextRun ahem_above_below_baseline(ahem_above_below_baseline_string, 9);
  TextRunPaintInfo text_run_paint_info(ahem_above_below_baseline);
  cc::PaintFlags default_paint;
  float device_scale_factor = 1;

  std::tuple<float, float> below_baseline_bounds = std::make_tuple(2, 4);
  Vector<Font::TextIntercept> text_intercepts;
  // 4 intercept ranges for below baseline p glyphs in the test string
  font.GetTextIntercepts(text_run_paint_info, device_scale_factor,
                         default_paint, below_baseline_bounds, text_intercepts);
  EXPECT_EQ(text_intercepts.size(), 4u);
  for (auto text_intercept : text_intercepts) {
    EXPECT_GT(text_intercept.end_, text_intercept.begin_);
  }

  std::tuple<float, float> above_baseline_bounds = std::make_tuple(-4, -2);
  // 5 intercept ranges for the above baseline E ACUTE glyphs
  font.GetTextIntercepts(text_run_paint_info, device_scale_factor,
                         default_paint, above_baseline_bounds, text_intercepts);
  EXPECT_EQ(text_intercepts.size(), 5u);
  for (auto text_intercept : text_intercepts) {
    EXPECT_GT(text_intercept.end_, text_intercept.begin_);
  }
}

TEST_F(FontTest, ExpandRange) {
  // "ffi" is a ligature, therefore a single glyph. Any range that includes one
  // of the letters must be expanded to all of them.
  EXPECT_EQ(GetExpandedRange("efficient", true, 0, 1), Vector<int>({0, 1}));
  EXPECT_EQ(GetExpandedRange("efficient", true, 0, 2), Vector<int>({0, 4}));
  EXPECT_EQ(GetExpandedRange("efficient", true, 3, 4), Vector<int>({1, 4}));
  EXPECT_EQ(GetExpandedRange("efficient", true, 4, 6), Vector<int>({4, 6}));
  EXPECT_EQ(GetExpandedRange("efficient", true, 6, 7), Vector<int>({6, 7}));
  EXPECT_EQ(GetExpandedRange("efficient", true, 0, 9), Vector<int>({0, 9}));

  EXPECT_EQ(GetExpandedRange("tneiciffe", false, 0, 1), Vector<int>({0, 1}));
  EXPECT_EQ(GetExpandedRange("tneiciffe", false, 0, 2), Vector<int>({0, 2}));
  EXPECT_EQ(GetExpandedRange("tneiciffe", false, 3, 4), Vector<int>({3, 4}));
  EXPECT_EQ(GetExpandedRange("tneiciffe", false, 4, 6), Vector<int>({4, 8}));
  EXPECT_EQ(GetExpandedRange("tneiciffe", false, 6, 7), Vector<int>({5, 8}));
  EXPECT_EQ(GetExpandedRange("tneiciffe", false, 0, 9), Vector<int>({0, 9}));
}

TEST_F(FontTest, TabWidthZero) {
  Font font =
      CreateTestFont("Ahem", test::PlatformTestDataPath("Ahem.woff"), 0);
  TabSize tab_size(8);
  EXPECT_EQ(font.TabWidth(tab_size, .0f), .0f);
  EXPECT_EQ(font.TabWidth(tab_size, LayoutUnit()), LayoutUnit());
}

}  // namespace blink
