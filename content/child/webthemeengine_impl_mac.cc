// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webthemeengine_impl_mac.h"

#include "content/child/webthemeengine_impl_conversions.h"
#include "ui/native_theme/native_theme.h"

namespace content {

blink::ForcedColors WebThemeEngineMac::GetForcedColors() const {
  return forced_colors_;
}

void WebThemeEngineMac::SetForcedColors(
    const blink::ForcedColors forced_colors) {
  forced_colors_ = forced_colors;
}

void WebThemeEngineMac::Paint(cc::PaintCanvas* canvas,
                              WebThemeEngine::Part part,
                              WebThemeEngine::State state,
                              const blink::WebRect& rect,
                              const WebThemeEngine::ExtraParams* extra_params,
                              blink::ColorScheme color_scheme) {
  if (IsScrollbarPart(part)) {
    PaintMacScrollBarParts(canvas, part, state, rect, extra_params,
                           color_scheme);
    return;
  }

  WebThemeEngineDefault::Paint(canvas, part, state, rect, extra_params,
                               color_scheme);
}

bool WebThemeEngineMac::IsScrollbarPart(WebThemeEngine::Part part) {
  switch (part) {
    case WebThemeEngine::kPartScrollbarHorizontalTrack:
    case WebThemeEngine::kPartScrollbarVerticalTrack:
    case WebThemeEngine::kPartScrollbarHorizontalThumb:
    case WebThemeEngine::kPartScrollbarVerticalThumb:
    case WebThemeEngine::kPartScrollbarCorner:
      return true;
    default:
      return false;
  }
}

void WebThemeEngineMac::PaintMacScrollBarParts(
    cc::PaintCanvas* canvas,
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const blink::WebRect& rect,
    const WebThemeEngine::ExtraParams* extra_params,
    blink::ColorScheme color_scheme) {
  ui::NativeTheme::ExtraParams native_theme_extra_params;
  native_theme_extra_params.scrollbar_extra.is_hovering =
      extra_params->scrollbar_extra.is_hovering;
  native_theme_extra_params.scrollbar_extra.is_overlay =
      extra_params->scrollbar_extra.is_overlay;
  switch (extra_params->scrollbar_extra.orientation) {
    case WebThemeEngine::kVerticalOnRight:
      native_theme_extra_params.scrollbar_extra.orientation =
          ui::NativeTheme::ScrollbarOrientation::kVerticalOnRight;
      break;
    case WebThemeEngine::kVerticalOnLeft:
      native_theme_extra_params.scrollbar_extra.orientation =
          ui::NativeTheme::ScrollbarOrientation::kVerticalOnLeft;
      break;
    case WebThemeEngine::kHorizontal:
      native_theme_extra_params.scrollbar_extra.orientation =
          ui::NativeTheme::ScrollbarOrientation::kHorizontal;
      break;
  }

  ui::NativeTheme::GetInstanceForNativeUi()->Paint(
      canvas, NativeThemePart(part), NativeThemeState(state), gfx::Rect(rect),
      native_theme_extra_params, NativeColorScheme(color_scheme));
}

}  // namespace content
