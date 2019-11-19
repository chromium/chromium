// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/fallback_theme.h"

#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "ui/native_theme/native_theme_base.h"

namespace blink {

namespace {

class FallbackTheme : public ui::NativeThemeBase {
 public:
  SkColor GetSystemColor(ColorId color_id,
                         ColorScheme color_scheme) const override {
    // The paint routines in NativeThemeBase only use GetSystemColor for
    // button focus colors and the fallback theme is not used for buttons.
    NOTREACHED();
    return SK_ColorRED;
  }
};

}  // namespace

ui::NativeTheme& GetFallbackTheme() {
  DEFINE_STATIC_LOCAL(FallbackTheme, theme, ());
  return theme;
}

}  // namespace blink
