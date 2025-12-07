// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_theme_engine.h"

#include <array>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_conversions.h"

namespace blink {

TEST(WebThemeEngineTest, NativeSystemThemePart) {
  static constexpr auto kParts = std::to_array<
      std::pair<ui::NativeTheme::Part, blink::WebThemeEngine::Part>>(
      {{ui::NativeTheme::kScrollbarDownArrow,
        blink::WebThemeEngine::kPartScrollbarDownArrow},
       {ui::NativeTheme::kScrollbarLeftArrow,
        blink::WebThemeEngine::kPartScrollbarLeftArrow},
       {ui::NativeTheme::kScrollbarRightArrow,
        blink::WebThemeEngine::kPartScrollbarRightArrow},
       {ui::NativeTheme::kScrollbarUpArrow,
        blink::WebThemeEngine::kPartScrollbarUpArrow},
       {ui::NativeTheme::kScrollbarHorizontalThumb,
        blink::WebThemeEngine::kPartScrollbarHorizontalThumb},
       {ui::NativeTheme::kScrollbarVerticalThumb,
        blink::WebThemeEngine::kPartScrollbarVerticalThumb},
       {ui::NativeTheme::kScrollbarHorizontalTrack,
        blink::WebThemeEngine::kPartScrollbarHorizontalTrack},
       {ui::NativeTheme::kScrollbarVerticalTrack,
        blink::WebThemeEngine::kPartScrollbarVerticalTrack},
       {ui::NativeTheme::kScrollbarCorner,
        blink::WebThemeEngine::kPartScrollbarCorner},
       {ui::NativeTheme::kCheckbox, blink::WebThemeEngine::kPartCheckbox},
       {ui::NativeTheme::kRadio, blink::WebThemeEngine::kPartRadio},
       {ui::NativeTheme::kPushButton, blink::WebThemeEngine::kPartButton},
       {ui::NativeTheme::kTextField, blink::WebThemeEngine::kPartTextField},
       {ui::NativeTheme::kMenuList, blink::WebThemeEngine::kPartMenuList},
       {ui::NativeTheme::kSliderTrack, blink::WebThemeEngine::kPartSliderTrack},
       {ui::NativeTheme::kSliderThumb, blink::WebThemeEngine::kPartSliderThumb},
       {ui::NativeTheme::kInnerSpinButton,
        blink::WebThemeEngine::kPartInnerSpinButton},
       {ui::NativeTheme::kProgressBar,
        blink::WebThemeEngine::kPartProgressBar}});
  for (const auto& part : kParts) {
    EXPECT_EQ(part.first, NativeThemePart(part.second));
  }
}

TEST(WebThemeEngineTest, NativeSystemThemeState) {
  EXPECT_EQ(ui::NativeTheme::kDisabled,
            NativeThemeState(blink::WebThemeEngine::kStateDisabled));
  EXPECT_EQ(ui::NativeTheme::kHovered,
            NativeThemeState(blink::WebThemeEngine::kStateHover));
  EXPECT_EQ(ui::NativeTheme::kNormal,
            NativeThemeState(blink::WebThemeEngine::kStateNormal));
  EXPECT_EQ(ui::NativeTheme::kPressed,
            NativeThemeState(blink::WebThemeEngine::kStatePressed));
}

TEST(WebThemeEngineTest, NativeColorScheme) {
  EXPECT_EQ(ui::NativeTheme::PreferredColorScheme::kLight,
            NativeColorScheme(blink::mojom::ColorScheme::kLight));
  EXPECT_EQ(ui::NativeTheme::PreferredColorScheme::kDark,
            NativeColorScheme(blink::mojom::ColorScheme::kDark));
}

TEST(WebThemeEngineTest, NativeContrast) {
  EXPECT_EQ(ui::NativeTheme::PreferredContrast::kMore,
            NativeContrast(mojom::blink::PreferredContrast::kMore));
  EXPECT_EQ(ui::NativeTheme::PreferredContrast::kLess,
            NativeContrast(mojom::blink::PreferredContrast::kLess));
  EXPECT_EQ(ui::NativeTheme::PreferredContrast::kNoPreference,
            NativeContrast(mojom::blink::PreferredContrast::kNoPreference));
  EXPECT_EQ(ui::NativeTheme::PreferredContrast::kCustom,
            NativeContrast(mojom::blink::PreferredContrast::kCustom));
}

}  // namespace blink
