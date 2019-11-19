// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_theme_win.h"

#include <windows.h>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

scoped_refptr<LayoutTheme> LayoutThemeWin::Create() {
  return base::AdoptRef(new LayoutThemeWin());
}

LayoutTheme& LayoutTheme::NativeTheme() {
  DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeWin::Create()));
  return *layout_theme;
}

Color LayoutThemeWin::SystemColor(CSSValueID css_value_id,
                                  WebColorScheme color_scheme) const {
  if (!RuntimeEnabledFeatures::UseWindowsSystemColorsEnabled()) {
    return LayoutThemeDefault::SystemColor(css_value_id, color_scheme);
  }

  blink::WebThemeEngine::SystemThemeColor theme_color;
  switch (css_value_id) {
    case CSSValueID::kActivetext:
    case CSSValueID::kLinktext:
    case CSSValueID::kVisitedtext:
      theme_color = blink::WebThemeEngine::SystemThemeColor::kHotlight;
      break;
    case CSSValueID::kButtonface:
      theme_color = blink::WebThemeEngine::SystemThemeColor::kButtonFace;
      break;
    case CSSValueID::kButtontext:
      theme_color = blink::WebThemeEngine::SystemThemeColor::kButtonText;
      break;
    case CSSValueID::kGraytext:
      theme_color = blink::WebThemeEngine::SystemThemeColor::kGrayText;
      break;
    case CSSValueID::kHighlight:
      theme_color = blink::WebThemeEngine::SystemThemeColor::kHighlight;
      break;
    case CSSValueID::kHighlighttext:
      theme_color = blink::WebThemeEngine::SystemThemeColor::kHighlightText;
      break;
    case CSSValueID::kWindow:
    case CSSValueID::kCanvas:
    case CSSValueID::kField:
      theme_color = blink::WebThemeEngine::SystemThemeColor::kWindow;
      break;
    case CSSValueID::kWindowtext:
    case CSSValueID::kCanvastext:
    case CSSValueID::kFieldtext:
      theme_color = blink::WebThemeEngine::SystemThemeColor::kWindowText;
      break;
    default:
      return LayoutThemeDefault::SystemColor(css_value_id, color_scheme);
  }

  if (Platform::Current() && Platform::Current()->ThemeEngine()) {
    const base::Optional<SkColor> system_color =
        Platform::Current()->ThemeEngine()->GetSystemColor(theme_color);
    if (system_color)
      return Color(system_color.value());
  }
  return LayoutThemeDefault::SystemColor(css_value_id, color_scheme);
}

}  // namespace blink
