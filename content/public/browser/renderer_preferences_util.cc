// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/renderer_preferences_util.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "ui/gfx/font_render_params.h"

namespace content {

void UpdateFontRendererPreferencesFromSystemSettings(
    blink::RendererPreferences* prefs) {
  static const gfx::FontRenderParams params(
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr));
  prefs->should_antialias_text = params.antialiasing;
  prefs->use_subpixel_positioning = params.subpixel_positioning;
  prefs->hinting = params.hinting;
  prefs->use_autohinter = params.autohinter;
  prefs->use_bitmaps = params.use_bitmaps;
  prefs->subpixel_rendering = params.subpixel_rendering;
#if BUILDFLAG(IS_WIN)
  prefs->text_contrast = params.text_contrast;
  prefs->text_gamma = params.text_gamma;
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace content
