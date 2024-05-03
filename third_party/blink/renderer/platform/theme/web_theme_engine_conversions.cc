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

ui::NativeTheme::SystemThemeColor NativeSystemThemeColor(
    WebThemeEngine::SystemThemeColor theme_color) {
  switch (theme_color) {
    case WebThemeEngine::SystemThemeColor::kButtonFace:
      return ui::NativeTheme::SystemThemeColor::kButtonFace;
    case WebThemeEngine::SystemThemeColor::kButtonText:
      return ui::NativeTheme::SystemThemeColor::kButtonText;
    case WebThemeEngine::SystemThemeColor::kGrayText:
      return ui::NativeTheme::SystemThemeColor::kGrayText;
    case WebThemeEngine::SystemThemeColor::kHighlight:
      return ui::NativeTheme::SystemThemeColor::kHighlight;
    case WebThemeEngine::SystemThemeColor::kHighlightText:
      return ui::NativeTheme::SystemThemeColor::kHighlightText;
    case WebThemeEngine::SystemThemeColor::kHotlight:
      return ui::NativeTheme::SystemThemeColor::kHotlight;
    case WebThemeEngine::SystemThemeColor::kWindow:
      return ui::NativeTheme::SystemThemeColor::kWindow;
    case WebThemeEngine::SystemThemeColor::kWindowText:
      return ui::NativeTheme::SystemThemeColor::kWindowText;
    default:
      return ui::NativeTheme::SystemThemeColor::kNotSupported;
  }
}

WebThemeEngine::SystemThemeColor WebThemeSystemThemeColor(
    ui::NativeTheme::SystemThemeColor theme_color) {
  switch (theme_color) {
    case ui::NativeTheme::SystemThemeColor::kButtonFace:
      return WebThemeEngine::SystemThemeColor::kButtonFace;
    case ui::NativeTheme::SystemThemeColor::kButtonText:
      return WebThemeEngine::SystemThemeColor::kButtonText;
    case ui::NativeTheme::SystemThemeColor::kGrayText:
      return WebThemeEngine::SystemThemeColor::kGrayText;
    case ui::NativeTheme::SystemThemeColor::kHighlight:
      return WebThemeEngine::SystemThemeColor::kHighlight;
    case ui::NativeTheme::SystemThemeColor::kHighlightText:
      return WebThemeEngine::SystemThemeColor::kHighlightText;
    case ui::NativeTheme::SystemThemeColor::kHotlight:
      return WebThemeEngine::SystemThemeColor::kHotlight;
    case ui::NativeTheme::SystemThemeColor::kWindow:
      return WebThemeEngine::SystemThemeColor::kWindow;
    case ui::NativeTheme::SystemThemeColor::kWindowText:
      return WebThemeEngine::SystemThemeColor::kWindowText;
    default:
      return WebThemeEngine::SystemThemeColor::kNotSupported;
  }
}

}  // namespace blink
