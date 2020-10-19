// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include "third_party/blink/public/platform/web_font_render_style.h"

namespace content {

namespace {

SkFontHinting RendererPreferencesToSkiaHinting(
    const blink::mojom::RendererPreferences& prefs) {
  switch (prefs.hinting) {
    case gfx::FontRenderParams::HINTING_NONE:
      return SkFontHinting::kNone;
    case gfx::FontRenderParams::HINTING_SLIGHT:
      return SkFontHinting::kSlight;
    case gfx::FontRenderParams::HINTING_MEDIUM:
      return SkFontHinting::kNormal;
    case gfx::FontRenderParams::HINTING_FULL:
      return SkFontHinting::kFull;
    default:
      NOTREACHED();
      return SkFontHinting::kNormal;
  }
}

}  // namespace

void RenderViewImpl::UpdateFontRenderingFromRendererPrefs() {
  const blink::mojom::RendererPreferences& prefs = renderer_preferences_;
  blink::WebFontRenderStyle::SetHinting(
      RendererPreferencesToSkiaHinting(prefs));
  blink::WebFontRenderStyle::SetAutoHint(prefs.use_autohinter);
  blink::WebFontRenderStyle::SetUseBitmaps(prefs.use_bitmaps);
  SkFontLCDConfig::SetSubpixelOrder(
      gfx::FontRenderParams::SubpixelRenderingToSkiaLCDOrder(
          prefs.subpixel_rendering));
  SkFontLCDConfig::SetSubpixelOrientation(
      gfx::FontRenderParams::SubpixelRenderingToSkiaLCDOrientation(
          prefs.subpixel_rendering));
  blink::WebFontRenderStyle::SetAntiAlias(prefs.should_antialias_text);
  blink::WebFontRenderStyle::SetSubpixelRendering(
      prefs.subpixel_rendering !=
      gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE);
  blink::WebFontRenderStyle::SetSubpixelPositioning(
      prefs.use_subpixel_positioning);
}

}  // namespace content
