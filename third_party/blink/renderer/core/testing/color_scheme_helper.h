// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_COLOR_SCHEME_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_COLOR_SCHEME_HELPER_H_

#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/public/common/css/preferred_color_scheme.h"

namespace blink {

class Document;
class Page;
class WebThemeEngine;

// ColorSchemeHelper is used to update the values of PreferredColorScheme and
// ForcedColors for testing. ColorSchemeHelper will reset PreferredColorScheme
// and ForcedColors back to their default values upon deconstruction.
class ColorSchemeHelper {
 public:
  ColorSchemeHelper();
  ~ColorSchemeHelper();

  void SetPreferredColorScheme(
      Document& document,
      const PreferredColorScheme preferred_color_scheme);
  void SetPreferredColorScheme(
      Page& page,
      const PreferredColorScheme preferred_color_scheme);
  void SetForcedColors(Document& document, const ForcedColors forced_colors);
  void SetForcedColors(Page& page, const ForcedColors forced_colors);

 private:
  WebThemeEngine* web_theme_engine_ = nullptr;
  PreferredColorScheme default_preferred_color_scheme_ =
      PreferredColorScheme::kNoPreference;
  ForcedColors default_forced_colors_ = ForcedColors::kNone;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_COLOR_SCHEME_HELPER_H_
