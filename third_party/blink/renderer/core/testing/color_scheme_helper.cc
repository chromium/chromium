// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

ColorSchemeHelper::ColorSchemeHelper() {
  DCHECK(Platform::Current() && Platform::Current()->ThemeEngine());
  web_theme_engine_ = Platform::Current()->ThemeEngine();
  default_preferred_color_scheme_ = web_theme_engine_->PreferredColorScheme();
  default_forced_colors_ = web_theme_engine_->GetForcedColors();
}

ColorSchemeHelper::~ColorSchemeHelper() {
  // Reset preferred color scheme and forced colors to their original values.
  web_theme_engine_->SetPreferredColorScheme(default_preferred_color_scheme_);
  web_theme_engine_->SetForcedColors(default_forced_colors_);
}

void ColorSchemeHelper::SetPreferredColorScheme(
    Document& document,
    const PreferredColorScheme preferred_color_scheme) {
  web_theme_engine_->SetPreferredColorScheme(preferred_color_scheme);
  document.ColorSchemeChanged();
}

void ColorSchemeHelper::SetPreferredColorScheme(
    Page& page,
    const PreferredColorScheme preferred_color_scheme) {
  web_theme_engine_->SetPreferredColorScheme(preferred_color_scheme);
  page.ColorSchemeChanged();
}

void ColorSchemeHelper::SetForcedColors(Document& document,
                                        const ForcedColors forced_colors) {
  web_theme_engine_->SetForcedColors(forced_colors);
  document.ColorSchemeChanged();
}

void ColorSchemeHelper::SetForcedColors(Page& page,
                                        const ForcedColors forced_colors) {
  web_theme_engine_->SetForcedColors(forced_colors);
  page.ColorSchemeChanged();
}

}  // namespace blink
