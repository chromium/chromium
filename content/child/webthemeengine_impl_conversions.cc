// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webthemeengine_impl_conversions.h"

namespace content {

// TODO(https://crbug.com/988434): The mapping functions below are duplicated
// inside Blink and in the Android implementation of WebThemeEngine. They should
// be implemented in one place where dependencies between Blink and
// ui::NativeTheme make sense.
ui::NativeTheme::Part NativeThemePart(blink::WebThemeEngine::Part part) {
  switch (part) {
    case blink::WebThemeEngine::kPartScrollbarDownArrow:
      return ui::NativeTheme::kScrollbarDownArrow;
    case blink::WebThemeEngine::kPartScrollbarLeftArrow:
      return ui::NativeTheme::kScrollbarLeftArrow;
    case blink::WebThemeEngine::kPartScrollbarRightArrow:
      return ui::NativeTheme::kScrollbarRightArrow;
    case blink::WebThemeEngine::kPartScrollbarUpArrow:
      return ui::NativeTheme::kScrollbarUpArrow;
    case blink::WebThemeEngine::kPartScrollbarHorizontalThumb:
      return ui::NativeTheme::kScrollbarHorizontalThumb;
    case blink::WebThemeEngine::kPartScrollbarVerticalThumb:
      return ui::NativeTheme::kScrollbarVerticalThumb;
    case blink::WebThemeEngine::kPartScrollbarHorizontalTrack:
      return ui::NativeTheme::kScrollbarHorizontalTrack;
    case blink::WebThemeEngine::kPartScrollbarVerticalTrack:
      return ui::NativeTheme::kScrollbarVerticalTrack;
    case blink::WebThemeEngine::kPartScrollbarCorner:
      return ui::NativeTheme::kScrollbarCorner;
    case blink::WebThemeEngine::kPartCheckbox:
      return ui::NativeTheme::kCheckbox;
    case blink::WebThemeEngine::kPartRadio:
      return ui::NativeTheme::kRadio;
    case blink::WebThemeEngine::kPartButton:
      return ui::NativeTheme::kPushButton;
    case blink::WebThemeEngine::kPartTextField:
      return ui::NativeTheme::kTextField;
    case blink::WebThemeEngine::kPartMenuList:
      return ui::NativeTheme::kMenuList;
    case blink::WebThemeEngine::kPartSliderTrack:
      return ui::NativeTheme::kSliderTrack;
    case blink::WebThemeEngine::kPartSliderThumb:
      return ui::NativeTheme::kSliderThumb;
    case blink::WebThemeEngine::kPartInnerSpinButton:
      return ui::NativeTheme::kInnerSpinButton;
    case blink::WebThemeEngine::kPartProgressBar:
      return ui::NativeTheme::kProgressBar;
    default:
      return ui::NativeTheme::kScrollbarDownArrow;
  }
}

ui::NativeTheme::ScrollbarOverlayColorTheme
NativeThemeScrollbarOverlayColorTheme(
    blink::WebScrollbarOverlayColorTheme theme) {
  switch (theme) {
    case blink::WebScrollbarOverlayColorTheme::
        kWebScrollbarOverlayColorThemeLight:
      return ui::NativeTheme::ScrollbarOverlayColorThemeLight;
    case blink::WebScrollbarOverlayColorTheme::
        kWebScrollbarOverlayColorThemeDark:
      return ui::NativeTheme::ScrollbarOverlayColorThemeDark;
    default:
      return ui::NativeTheme::ScrollbarOverlayColorThemeDark;
  }
}

ui::NativeTheme::State NativeThemeState(blink::WebThemeEngine::State state) {
  switch (state) {
    case blink::WebThemeEngine::kStateDisabled:
      return ui::NativeTheme::kDisabled;
    case blink::WebThemeEngine::kStateHover:
      return ui::NativeTheme::kHovered;
    case blink::WebThemeEngine::kStateNormal:
      return ui::NativeTheme::kNormal;
    case blink::WebThemeEngine::kStatePressed:
      return ui::NativeTheme::kPressed;
    default:
      return ui::NativeTheme::kDisabled;
  }
}

ui::NativeTheme::ColorScheme NativeColorScheme(
    blink::WebColorScheme color_scheme) {
  switch (color_scheme) {
    case blink::WebColorScheme::kLight:
      return ui::NativeTheme::ColorScheme::kLight;
    case blink::WebColorScheme::kDark:
      return ui::NativeTheme::ColorScheme::kDark;
  }
}

ui::NativeTheme::SystemThemeColor NativeSystemThemeColor(
    blink::WebThemeEngine::SystemThemeColor theme_color) {
  switch (theme_color) {
    case blink::WebThemeEngine::SystemThemeColor::kButtonFace:
      return ui::NativeTheme::SystemThemeColor::kButtonFace;
    case blink::WebThemeEngine::SystemThemeColor::kButtonText:
      return ui::NativeTheme::SystemThemeColor::kButtonText;
    case blink::WebThemeEngine::SystemThemeColor::kGrayText:
      return ui::NativeTheme::SystemThemeColor::kGrayText;
    case blink::WebThemeEngine::SystemThemeColor::kHighlight:
      return ui::NativeTheme::SystemThemeColor::kHighlight;
    case blink::WebThemeEngine::SystemThemeColor::kHighlightText:
      return ui::NativeTheme::SystemThemeColor::kHighlightText;
    case blink::WebThemeEngine::SystemThemeColor::kHotlight:
      return ui::NativeTheme::SystemThemeColor::kHotlight;
    case blink::WebThemeEngine::SystemThemeColor::kWindow:
      return ui::NativeTheme::SystemThemeColor::kWindow;
    case blink::WebThemeEngine::SystemThemeColor::kWindowText:
      return ui::NativeTheme::SystemThemeColor::kWindowText;
    default:
      return ui::NativeTheme::SystemThemeColor::kNotSupported;
  }
}

ui::NativeTheme::PreferredColorScheme NativePreferredColorScheme(
    blink::PreferredColorScheme preferred_color_scheme) {
  switch (preferred_color_scheme) {
    case blink::PreferredColorScheme::kDark:
      return ui::NativeTheme::PreferredColorScheme::kDark;
    case blink::PreferredColorScheme::kLight:
      return ui::NativeTheme::PreferredColorScheme::kLight;
    case blink::PreferredColorScheme::kNoPreference:
      return ui::NativeTheme::PreferredColorScheme::kNoPreference;
  }
}

blink::PreferredColorScheme WebPreferredColorScheme(
    ui::NativeTheme::PreferredColorScheme preferred_color_scheme) {
  switch (preferred_color_scheme) {
    case ui::NativeTheme::PreferredColorScheme::kDark:
      return blink::PreferredColorScheme::kDark;
    case ui::NativeTheme::PreferredColorScheme::kLight:
      return blink::PreferredColorScheme::kLight;
    case ui::NativeTheme::PreferredColorScheme::kNoPreference:
      return blink::PreferredColorScheme::kNoPreference;
  }
}

}  // namespace content
