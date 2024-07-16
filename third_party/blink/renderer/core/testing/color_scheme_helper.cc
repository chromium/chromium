// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/color_scheme_helper.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

ColorSchemeHelper::ColorSchemeHelper(Document& document)
    : settings_(*document.GetSettings()) {
  default_preferred_root_scrollbar_color_scheme_ =
      settings_.GetPreferredRootScrollbarColorScheme();
  default_preferred_color_scheme_ = settings_.GetPreferredColorScheme();
  default_preferred_contrast_ = settings_.GetPreferredContrast();
  default_in_forced_colors_ = settings_.GetInForcedColors();
}

ColorSchemeHelper::ColorSchemeHelper(Page& page)
    : settings_(page.GetSettings()) {
  default_preferred_root_scrollbar_color_scheme_ =
      settings_.GetPreferredRootScrollbarColorScheme();
  default_preferred_color_scheme_ = settings_.GetPreferredColorScheme();
  default_preferred_contrast_ = settings_.GetPreferredContrast();
  default_in_forced_colors_ = settings_.GetInForcedColors();
}

ColorSchemeHelper::~ColorSchemeHelper() {
  // Reset preferred color scheme, preferred contrast and forced colors to their
  // original values.
  settings_.SetInForcedColors(default_in_forced_colors_);
  settings_.SetPreferredRootScrollbarColorScheme(
      default_preferred_root_scrollbar_color_scheme_);
  settings_.SetPreferredColorScheme(default_preferred_color_scheme_);
  settings_.SetPreferredContrast(default_preferred_contrast_);
}

void ColorSchemeHelper::SetPreferredRootScrollbarColorScheme(
    blink::mojom::PreferredColorScheme preferred_root_scrollbar_color_scheme) {
  settings_.SetPreferredRootScrollbarColorScheme(
      preferred_root_scrollbar_color_scheme);
}

void ColorSchemeHelper::SetPreferredColorScheme(
    mojom::blink::PreferredColorScheme preferred_color_scheme) {
  settings_.SetPreferredColorScheme(preferred_color_scheme);
}

void ColorSchemeHelper::SetPreferredContrast(
    mojom::blink::PreferredContrast preferred_contrast) {
  settings_.SetPreferredContrast(preferred_contrast);
}

void ColorSchemeHelper::SetInForcedColors(Document& document,
                                          bool in_forced_colors) {
  settings_.SetInForcedColors(in_forced_colors);
  document.ColorSchemeChanged();
}

void ColorSchemeHelper::SetInForcedColors(bool in_forced_colors) {
  settings_.SetInForcedColors(in_forced_colors);
}

void ColorSchemeHelper::SetEmulatedForcedColors(Document& document,
                                                bool is_dark_theme) {
  document.GetPage()->EmulateForcedColors(is_dark_theme);
  document.ColorSchemeChanged();
}
}  // namespace blink
