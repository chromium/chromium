// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webthemeengine_impl_default.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/web/win/web_font_rendering.h"
#include "third_party/skia/include/core/SkFontLCDConfig.h"
#include "ui/gfx/font_render_params.h"

using blink::WebFontRendering;

namespace content {

void RenderViewImpl::UpdateFontRenderingFromRendererPrefs() {
  const blink::mojom::RendererPreferences& prefs = renderer_preferences_;

  // Cache the system font metrics in blink.
  blink::WebFontRendering::SetMenuFontMetrics(
      prefs.menu_font_family_name.c_str(), prefs.menu_font_height);

  blink::WebFontRendering::SetSmallCaptionFontMetrics(
      prefs.small_caption_font_family_name.c_str(),
      prefs.small_caption_font_height);

  blink::WebFontRendering::SetStatusFontMetrics(
      prefs.status_font_family_name.c_str(), prefs.status_font_height);

  SkFontLCDConfig::SetSubpixelOrder(
      gfx::FontRenderParams::SubpixelRenderingToSkiaLCDOrder(
          prefs.subpixel_rendering));
  SkFontLCDConfig::SetSubpixelOrientation(
      gfx::FontRenderParams::SubpixelRenderingToSkiaLCDOrientation(
          prefs.subpixel_rendering));

  blink::WebFontRendering::SetAntialiasedTextEnabled(
      prefs.should_antialias_text);
  blink::WebFontRendering::SetLCDTextEnabled(
      prefs.subpixel_rendering !=
      gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE);
}

void RenderViewImpl::UpdateThemePrefs() {
  WebThemeEngineDefault::cacheScrollBarMetrics(
      renderer_preferences_.vertical_scroll_bar_width_in_dips,
      renderer_preferences_.horizontal_scroll_bar_height_in_dips,
      renderer_preferences_.arrow_bitmap_height_vertical_scroll_bar_in_dips,
      renderer_preferences_.arrow_bitmap_width_horizontal_scroll_bar_in_dips);
}

}  // namespace content
