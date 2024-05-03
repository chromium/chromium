// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/theme//web_theme_engine_conversions.h"

#include <vector>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_theme_engine.h"

namespace blink {

TEST(WebThemeEngineTest, NativeSystemThemeColor) {
  std::vector<blink::WebThemeEngine::SystemThemeColor> blink_inputs = {
      blink::WebThemeEngine::SystemThemeColor::kButtonFace,
      blink::WebThemeEngine::SystemThemeColor::kButtonText,
      blink::WebThemeEngine::SystemThemeColor::kGrayText,
      blink::WebThemeEngine::SystemThemeColor::kHighlight,
      blink::WebThemeEngine::SystemThemeColor::kHighlightText,
      blink::WebThemeEngine::SystemThemeColor::kHotlight,
      blink::WebThemeEngine::SystemThemeColor::kWindow,
      blink::WebThemeEngine::SystemThemeColor::kWindowText};

  std::vector<ui::NativeTheme::SystemThemeColor> native_theme_outputs = {
      ui::NativeTheme::SystemThemeColor::kButtonFace,
      ui::NativeTheme::SystemThemeColor::kButtonText,
      ui::NativeTheme::SystemThemeColor::kGrayText,
      ui::NativeTheme::SystemThemeColor::kHighlight,
      ui::NativeTheme::SystemThemeColor::kHighlightText,
      ui::NativeTheme::SystemThemeColor::kHotlight,
      ui::NativeTheme::SystemThemeColor::kWindow,
      ui::NativeTheme::SystemThemeColor::kWindowText};

  for (size_t i = 0; i < blink_inputs.size(); ++i)
    EXPECT_EQ(NativeSystemThemeColor(blink_inputs[i]), native_theme_outputs[i]);
}

TEST(WebThemeEngineTest, NativeSystemThemePart) {
  std::vector<blink::WebThemeEngine::Part> blink_inputs = {
      blink::WebThemeEngine::kPartScrollbarDownArrow,
      blink::WebThemeEngine::kPartScrollbarLeftArrow,
      blink::WebThemeEngine::kPartScrollbarRightArrow,
      blink::WebThemeEngine::kPartScrollbarUpArrow,
      blink::WebThemeEngine::kPartScrollbarHorizontalThumb,
      blink::WebThemeEngine::kPartScrollbarVerticalThumb,
      blink::WebThemeEngine::kPartScrollbarHorizontalTrack,
      blink::WebThemeEngine::kPartScrollbarVerticalTrack,
      blink::WebThemeEngine::kPartScrollbarCorner,
      blink::WebThemeEngine::kPartCheckbox,
      blink::WebThemeEngine::kPartRadio,
      blink::WebThemeEngine::kPartButton,
      blink::WebThemeEngine::kPartTextField,
      blink::WebThemeEngine::kPartMenuList,
      blink::WebThemeEngine::kPartSliderTrack,
      blink::WebThemeEngine::kPartSliderThumb,
      blink::WebThemeEngine::kPartInnerSpinButton,
      blink::WebThemeEngine::kPartProgressBar};

  std::vector<ui::NativeTheme::Part> native_theme_outputs = {
      ui::NativeTheme::kScrollbarDownArrow,
      ui::NativeTheme::kScrollbarLeftArrow,
      ui::NativeTheme::kScrollbarRightArrow,
      ui::NativeTheme::kScrollbarUpArrow,
      ui::NativeTheme::kScrollbarHorizontalThumb,
      ui::NativeTheme::kScrollbarVerticalThumb,
      ui::NativeTheme::kScrollbarHorizontalTrack,
      ui::NativeTheme::kScrollbarVerticalTrack,
      ui::NativeTheme::kScrollbarCorner,
      ui::NativeTheme::kCheckbox,
      ui::NativeTheme::kRadio,
      ui::NativeTheme::kPushButton,
      ui::NativeTheme::kTextField,
      ui::NativeTheme::kMenuList,
      ui::NativeTheme::kSliderTrack,
      ui::NativeTheme::kSliderThumb,
      ui::NativeTheme::kInnerSpinButton,
      ui::NativeTheme::kProgressBar};

  for (size_t i = 0; i < blink_inputs.size(); ++i)
    EXPECT_EQ(NativeThemePart(blink_inputs[i]), native_theme_outputs[i]);
}

TEST(WebThemeEngineTest, NativeSystemThemeState) {
  std::vector<blink::WebThemeEngine::State> blink_inputs = {
      blink::WebThemeEngine::kStateDisabled,
      blink::WebThemeEngine::kStateHover,
      blink::WebThemeEngine::kStateNormal,
      blink::WebThemeEngine::kStatePressed,
  };

  std::vector<ui::NativeTheme::State> native_theme_outputs = {
      ui::NativeTheme::kDisabled, ui::NativeTheme::kHovered,
      ui::NativeTheme::kNormal, ui::NativeTheme::kPressed};

  for (size_t i = 0; i < blink_inputs.size(); ++i)
    EXPECT_EQ(NativeThemeState(blink_inputs[i]), native_theme_outputs[i]);
}

TEST(WebThemeEngineTest, NativeColorScheme) {
  std::vector<blink::mojom::ColorScheme> blink_inputs = {
      blink::mojom::ColorScheme::kLight, blink::mojom::ColorScheme::kDark};

  std::vector<ui::NativeTheme::ColorScheme> native_theme_outputs = {
      ui::NativeTheme::ColorScheme::kLight,
      ui::NativeTheme::ColorScheme::kDark};

  for (size_t i = 0; i < blink_inputs.size(); ++i)
    EXPECT_EQ(NativeColorScheme(blink_inputs[i]), native_theme_outputs[i]);
}

}  // namespace blink
