// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_COLOR_SCHEME_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_COLOR_SCHEME_HELPER_H_

#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom-shared.h"
#include "third_party/blink/public/mojom/css/preferred_contrast.mojom-shared.h"

namespace blink {

class Document;
class Page;
class Settings;
class WebThemeEngine;

// ColorSchemeHelper is used to update the values of PreferredColorScheme,
// PreferredContrast and ForcedColors for testing. ColorSchemeHelper will reset
// PreferredColorScheme, PreferredContrast and ForcedColors back to their
// default values upon deconstruction.
class ColorSchemeHelper {
 public:
  ColorSchemeHelper(Document& document);
  ColorSchemeHelper(Page& page);
  ~ColorSchemeHelper();

  void SetPreferredColorScheme(
      mojom::PreferredColorScheme preferred_color_scheme);
  void SetPreferredContrast(mojom::PreferredContrast preferred_contrast);
  void SetForcedColors(Document& document, ForcedColors forced_colors);
  void SetForcedColors(Page& page, ForcedColors forced_colors);
  void SetEmulatedForcedColors(Document& document, bool is_dark_theme);

 private:
  WebThemeEngine* web_theme_engine_ = nullptr;
  Settings& settings_;
  mojom::PreferredColorScheme default_preferred_color_scheme_ =
      mojom::PreferredColorScheme::kLight;
  mojom::PreferredContrast default_preferred_contrast_ =
      mojom::PreferredContrast::kNoPreference;
  ForcedColors default_forced_colors_ = ForcedColors::kNone;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_COLOR_SCHEME_HELPER_H_
