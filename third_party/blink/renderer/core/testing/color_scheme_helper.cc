// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

ColorSchemeHelper::ColorSchemeHelper(Document& document)
    : settings_(*document.GetSettings()) {
  DCHECK(Platform::Current() && Platform::Current()->ThemeEngine());
  web_theme_engine_ = Platform::Current()->ThemeEngine();
  default_preferred_color_scheme_ = settings_.GetPreferredColorScheme();
  default_preferred_contrast_ = settings_.GetPreferredContrast();
  default_forced_colors_ = web_theme_engine_->GetForcedColors();
}

ColorSchemeHelper::ColorSchemeHelper(Page& page)
    : settings_(page.GetSettings()) {
  DCHECK(Platform::Current() && Platform::Current()->ThemeEngine());
  web_theme_engine_ = Platform::Current()->ThemeEngine();
  default_preferred_color_scheme_ = settings_.GetPreferredColorScheme();
  default_preferred_contrast_ = settings_.GetPreferredContrast();
  default_forced_colors_ = web_theme_engine_->GetForcedColors();
}

ColorSchemeHelper::~ColorSchemeHelper() {
  // Reset preferred color scheme, preferred contrast and forced colors to their
  // original values.
  settings_.SetPreferredColorScheme(default_preferred_color_scheme_);
  settings_.SetPreferredContrast(default_preferred_contrast_);
  web_theme_engine_->SetForcedColors(default_forced_colors_);
}

void ColorSchemeHelper::SetPreferredColorScheme(
    const mojom::blink::PreferredColorScheme preferred_color_scheme) {
  settings_.SetPreferredColorScheme(preferred_color_scheme);
}

void ColorSchemeHelper::SetPreferredContrast(
    const mojom::blink::PreferredContrast preferred_contrast) {
  settings_.SetPreferredContrast(preferred_contrast);
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
