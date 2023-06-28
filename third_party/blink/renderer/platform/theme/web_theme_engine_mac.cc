// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/theme/web_theme_engine_mac.h"

#include "third_party/blink/renderer/platform/theme/web_theme_engine_conversions.h"
#include "ui/native_theme/native_theme.h"

namespace blink {

void WebThemeEngineMac::Paint(cc::PaintCanvas* canvas,
                              WebThemeEngine::Part part,
                              WebThemeEngine::State state,
                              const gfx::Rect& rect,
                              const WebThemeEngine::ExtraParams* extra_params,
                              mojom::ColorScheme color_scheme,
                              const absl::optional<SkColor>& accent_color) {
  if (IsScrollbarPart(part)) {
    PaintMacScrollBarParts(canvas, GetColorProviderForPainting(color_scheme),
                           part, state, rect, extra_params, color_scheme);
    return;
  }

  WebThemeEngineDefault::Paint(canvas, part, state, rect, extra_params,
                               color_scheme, accent_color);
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
    const ui::ColorProvider* color_provider,
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const gfx::Rect& rect,
    const WebThemeEngine::ExtraParams* extra_params,
    mojom::ColorScheme color_scheme) {
  ui::NativeTheme::ExtraParams native_theme_extra_params;
  native_theme_extra_params.scrollbar_extra.is_hovering =
      extra_params->scrollbar_extra.is_hovering;
  native_theme_extra_params.scrollbar_extra.is_overlay =
      extra_params->scrollbar_extra.is_overlay;
  native_theme_extra_params.scrollbar_extra.scale_from_dip =
      extra_params->scrollbar_extra.scale_from_dip;
  native_theme_extra_params.scrollbar_extra.track_color =
      extra_params->scrollbar_extra.track_color;
  native_theme_extra_params.scrollbar_extra.thumb_color =
      extra_params->scrollbar_extra.thumb_color;
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
      canvas, color_provider, NativeThemePart(part), NativeThemeState(state),
      rect, native_theme_extra_params, NativeColorScheme(color_scheme));
}

}  // namespace blink
