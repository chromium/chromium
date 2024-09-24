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

}  // namespace blink
