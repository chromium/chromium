// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/platform/web_font_render_style.h"
#include "ui/gfx/font_render_params.h"

using blink::WebFontRenderStyle;

namespace content {

namespace {

SkFontHinting RendererPreferencesToSkiaHinting(
    const blink::RendererPreferences& prefs) {
  if (!prefs.should_antialias_text) {
    // When anti-aliasing is off, GTK maps all non-zero hinting settings to
    // 'Normal' hinting so we do the same. Otherwise, folks who have 'Slight'
    // hinting selected will see readable text in everything expect Chromium.
    switch (prefs.hinting) {
      case gfx::FontRenderParams::HINTING_NONE:
        return SkFontHinting::kNone;
      case gfx::FontRenderParams::HINTING_SLIGHT:
      case gfx::FontRenderParams::HINTING_MEDIUM:
      case gfx::FontRenderParams::HINTING_FULL:
        return SkFontHinting::kNormal;
      default:
        NOTREACHED();
        return SkFontHinting::kNormal;
    }
  }

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
  const blink::RendererPreferences& prefs = renderer_preferences_;
  WebFontRenderStyle::SetHinting(RendererPreferencesToSkiaHinting(prefs));
  WebFontRenderStyle::SetAutoHint(prefs.use_autohinter);
  WebFontRenderStyle::SetUseBitmaps(prefs.use_bitmaps);
  skia::LegacyDisplayGlobals::SetCachedPixelGeometry(
      gfx::FontRenderParams::SubpixelRenderingToSkiaPixelGeometry(
          prefs.subpixel_rendering));
  WebFontRenderStyle::SetAntiAlias(prefs.should_antialias_text);
  WebFontRenderStyle::SetSubpixelRendering(
      prefs.subpixel_rendering !=
      gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE);
  WebFontRenderStyle::SetSubpixelPositioning(prefs.use_subpixel_positioning);
#if !defined(OS_ANDROID)
  if (!prefs.system_font_family_name.empty()) {
    WebFontRenderStyle::SetSystemFontFamily(
        blink::WebString::FromUTF8(prefs.system_font_family_name));
  }
#endif
}

}  // namespace content
