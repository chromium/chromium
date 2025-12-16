// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/font_size_functions.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

using FontSizeFunctionsTest = PageTestBase;

TEST_F(FontSizeFunctionsTest, GetComputedSizeFromSpecifiedSize_NoMinFontSize) {
  constexpr float zoom_factor = 2;
  constexpr int min_font_size = 100;
  constexpr bool is_absolute = true;
  constexpr bool is_logical = false;

  GetDocument().GetSettings()->SetMinimumFontSize(min_font_size);
  GetDocument().GetSettings()->SetMinimumLogicalFontSize(min_font_size);

  for (const int& font_size : {1, 10, 40, 120}) {
    EXPECT_EQ(font_size * zoom_factor,
              FontSizeFunctions::GetComputedSizeFromSpecifiedSize(
                  &GetDocument(), zoom_factor, is_absolute, font_size,
                  kDoNotApplyMinimumForFontSize));
    EXPECT_EQ(font_size * zoom_factor,
              FontSizeFunctions::GetComputedSizeFromSpecifiedSize(
                  &GetDocument(), zoom_factor, is_logical, font_size,
                  kDoNotApplyMinimumForFontSize));
  }
}

TEST_F(FontSizeFunctionsTest, GetComputedSizeFromSpecifiedSize_MinFontSize) {
  constexpr float zoom_factor = 2;
  constexpr int min_font_size = 100;
  constexpr bool is_absolute = true;
  constexpr bool is_logical = false;

  GetDocument().GetSettings()->SetMinimumFontSize(min_font_size);
  GetDocument().GetSettings()->SetMinimumLogicalFontSize(0);

  struct FontSizeTestData {
    const float specified_size;
    const float expected_computed_size;
  } test_cases[] = {
      {1, min_font_size}, {10, min_font_size}, {40, min_font_size}, {120, 120}};
  for (const auto font_sizes : test_cases) {
    EXPECT_EQ(font_sizes.expected_computed_size * zoom_factor,
              FontSizeFunctions::GetComputedSizeFromSpecifiedSize(
                  &GetDocument(), zoom_factor, is_absolute,
                  font_sizes.specified_size));
    EXPECT_EQ(font_sizes.expected_computed_size * zoom_factor,
              FontSizeFunctions::GetComputedSizeFromSpecifiedSize(
                  &GetDocument(), zoom_factor, is_logical,
                  font_sizes.specified_size));
  }
}

TEST_F(FontSizeFunctionsTest,
       GetComputedSizeFromSpecifiedSize_MinLogicalFontSize) {
  constexpr float zoom_factor = 2;
  constexpr int min_font_size = 100;
  constexpr bool is_absolute = true;
  constexpr bool is_logical = false;

  GetDocument().GetSettings()->SetMinimumFontSize(0);
  GetDocument().GetSettings()->SetMinimumLogicalFontSize(min_font_size);

  struct FontSizeTestData {
    const float specified_size;
    const float expected_computed_size;
  } test_cases[] = {
      {1, min_font_size}, {10, min_font_size}, {40, min_font_size}, {120, 120}};

  for (const auto font_sizes : test_cases) {
    EXPECT_EQ(font_sizes.specified_size * zoom_factor,
              FontSizeFunctions::GetComputedSizeFromSpecifiedSize(
                  &GetDocument(), zoom_factor, is_absolute,
                  font_sizes.specified_size));
    EXPECT_EQ(font_sizes.expected_computed_size * zoom_factor,
              FontSizeFunctions::GetComputedSizeFromSpecifiedSize(
                  &GetDocument(), zoom_factor, is_logical,
                  font_sizes.specified_size));
  }
}

TEST_F(FontSizeFunctionsTest, TestFontSizeForKeyword) {
  GetDocument().GetSettings()->SetDefaultFontSize(14);
  GetDocument().GetSettings()->SetDefaultFixedFontSize(11);

  struct {
    bool quirks_mode;
    bool monospace;
    unsigned keyword;
    float expected_font_size;
  } test_cases[] = {
      // Font sizes in no-quirks mode using the user settings.
      {false, false, FontSizeFunctions::KeywordSize(CSSValueID::kMedium), 14},
      {false, true, FontSizeFunctions::KeywordSize(CSSValueID::kSmall), 10},
      {false, false, FontSizeFunctions::KeywordSize(CSSValueID::kLarge), 17},

      // Font sizes in quirks mode using the user settings.
      {true, false, FontSizeFunctions::KeywordSize(CSSValueID::kMedium), 14},
      {true, true, FontSizeFunctions::KeywordSize(CSSValueID::kSmall), 9},
      {true, false, FontSizeFunctions::KeywordSize(CSSValueID::kLarge), 17},

  };

  for (const auto& test : test_cases) {
    GetDocument().SetCompatibilityMode(
        test.quirks_mode ? Document::CompatibilityMode::kQuirksMode
                         : Document::CompatibilityMode::kNoQuirksMode);
    EXPECT_EQ(test.expected_font_size,
              FontSizeFunctions::FontSizeForKeyword(
                  &GetDocument(), test.keyword, test.monospace));
  }
}

TEST_F(FontSizeFunctionsTest, TestFontSizeForKeyword_TextScaleMetaTag) {
  GetDocument().GetSettings()->SetDefaultFontSize(16);
  GetDocument().GetSettings()->SetDefaultFixedFontSize(10);
  GetDocument().GetSettings()->SetAccessibilityFontScaleFactor(2.0f);

  // First, test WITH text-scale meta tag present
  GetDocument().SetTextScaleMetaTagPresent(true);

  struct {
    bool monospace;
    unsigned keyword;
    float expected_font_size;
  } test_cases_with_meta_tag[] = {
      // When scaled, the medium size exceeds the table range, so we use the
      // formula: kFontSizeFactors[keyword] * medium_size.
      // Medium size (regular) = 16 * 2 = 32.
      // Medium size (fixed) = 10 * 2 = 20.
      {false, FontSizeFunctions::KeywordSize(CSSValueID::kMedium), 32.0f},
      {true, FontSizeFunctions::KeywordSize(CSSValueID::kSmall), 20 * 0.89f},
      {false, FontSizeFunctions::KeywordSize(CSSValueID::kLarge), 32 * 1.2f},
  };

  for (const auto& test : test_cases_with_meta_tag) {
    EXPECT_FLOAT_EQ(test.expected_font_size,
                    FontSizeFunctions::FontSizeForKeyword(
                        &GetDocument(), test.keyword, test.monospace));
  }

  // Now test WITHOUT text-scale meta tag -- fonts should not scale.
  GetDocument().SetTextScaleMetaTagPresent(false);

  struct {
    bool monospace;
    unsigned keyword;
    float expected_font_size;
  } test_cases_without_meta_tag[] = {
      // Medium (Reg) 16: Table row 7. Index 3 -> 16.
      // Small (Fixed) 10: Table row 1. Index 2 -> 9.
      // Large (Reg) 16: Table row 7. Index 4 -> 18.
      {false, FontSizeFunctions::KeywordSize(CSSValueID::kMedium), 16},
      {true, FontSizeFunctions::KeywordSize(CSSValueID::kSmall), 9},
      {false, FontSizeFunctions::KeywordSize(CSSValueID::kLarge), 18},
  };

  for (const auto& test : test_cases_without_meta_tag) {
    EXPECT_FLOAT_EQ(test.expected_font_size,
                    FontSizeFunctions::FontSizeForKeyword(
                        &GetDocument(), test.keyword, test.monospace));
  }
}

}  // namespace blink
