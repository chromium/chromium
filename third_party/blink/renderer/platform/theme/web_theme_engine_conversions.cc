// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/theme/web_theme_engine_conversions.h"

namespace blink {

// TODO(https://crbug.com/988434): The mapping functions below are duplicated
// inside Blink and in the Android implementation of WebThemeEngine. They should
// be implemented in one place where dependencies between Blink and
// ui::NativeTheme make sense.
ui::NativeTheme::Part NativeThemePart(WebThemeEngine::Part part) {
  switch (part) {
    case WebThemeEngine::kPartScrollbarDownArrow:
      return ui::NativeTheme::kScrollbarDownArrow;
    case WebThemeEngine::kPartScrollbarLeftArrow:
      return ui::NativeTheme::kScrollbarLeftArrow;
    case WebThemeEngine::kPartScrollbarRightArrow:
      return ui::NativeTheme::kScrollbarRightArrow;
    case WebThemeEngine::kPartScrollbarUpArrow:
      return ui::NativeTheme::kScrollbarUpArrow;
    case WebThemeEngine::kPartScrollbarHorizontalThumb:
      return ui::NativeTheme::kScrollbarHorizontalThumb;
    case WebThemeEngine::kPartScrollbarVerticalThumb:
      return ui::NativeTheme::kScrollbarVerticalThumb;
    case WebThemeEngine::kPartScrollbarHorizontalTrack:
      return ui::NativeTheme::kScrollbarHorizontalTrack;
    case WebThemeEngine::kPartScrollbarVerticalTrack:
      return ui::NativeTheme::kScrollbarVerticalTrack;
    case WebThemeEngine::kPartScrollbarCorner:
      return ui::NativeTheme::kScrollbarCorner;
    case WebThemeEngine::kPartCheckbox:
      return ui::NativeTheme::kCheckbox;
    case WebThemeEngine::kPartRadio:
      return ui::NativeTheme::kRadio;
    case WebThemeEngine::kPartButton:
      return ui::NativeTheme::kPushButton;
    case WebThemeEngine::kPartTextField:
      return ui::NativeTheme::kTextField;
    case WebThemeEngine::kPartMenuList:
      return ui::NativeTheme::kMenuList;
    case WebThemeEngine::kPartSliderTrack:
      return ui::NativeTheme::kSliderTrack;
    case WebThemeEngine::kPartSliderThumb:
      return ui::NativeTheme::kSliderThumb;
    case WebThemeEngine::kPartInnerSpinButton:
      return ui::NativeTheme::kInnerSpinButton;
    case WebThemeEngine::kPartProgressBar:
      return ui::NativeTheme::kProgressBar;
    default:
      return ui::NativeTheme::kScrollbarDownArrow;
  }
}

ui::NativeTheme::State NativeThemeState(WebThemeEngine::State state) {
  switch (state) {
    case WebThemeEngine::kStateDisabled:
      return ui::NativeTheme::kDisabled;
    case WebThemeEngine::kStateHover:
      return ui::NativeTheme::kHovered;
    case WebThemeEngine::kStateNormal:
      return ui::NativeTheme::kNormal;
    case WebThemeEngine::kStatePressed:
      return ui::NativeTheme::kPressed;
    default:
      return ui::NativeTheme::kDisabled;
  }
}

ui::NativeTheme::ColorScheme NativeColorScheme(
    mojom::ColorScheme color_scheme) {
  switch (color_scheme) {
    case mojom::ColorScheme::kLight:
      return ui::NativeTheme::ColorScheme::kLight;
    case mojom::ColorScheme::kDark:
      return ui::NativeTheme::ColorScheme::kDark;
  }
}

}  // namespace blink
