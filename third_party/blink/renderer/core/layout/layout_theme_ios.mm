// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_theme_ios.h"

#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "ui/base/ui_base_features.h"

namespace blink {

scoped_refptr<LayoutTheme> LayoutThemeIOS::Create() {
  return base::AdoptRef(new LayoutThemeIOS());
}

LayoutTheme& LayoutTheme::NativeTheme() {
  DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeIOS::Create()));
  return *layout_theme;
}

LayoutThemeIOS::~LayoutThemeIOS() {}

Color LayoutThemeIOS::PlatformActiveSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return color_scheme == mojom::blink::ColorScheme::kDark
             ? Color::FromRGBA32(0xFF99C8FF)
             : LayoutThemeMobile::PlatformActiveSelectionBackgroundColor(
                   color_scheme);
}

Color LayoutThemeIOS::PlatformActiveSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return color_scheme == mojom::blink::ColorScheme::kDark
             ? Color::FromRGBA32(0xFF3B3B3B)
             : LayoutThemeMobile::PlatformActiveSelectionForegroundColor(
                   color_scheme);
}

Color LayoutThemeIOS::PlatformSpellingMarkerUnderlineColor() const {
  // Use the same color as MacPort for spelling marker underline.
  // See LayoutThemeMac::PlatformSpellingMarkerUnderlineColor()
  return Color(255, 59, 48, 191);
}

Color LayoutThemeIOS::PlatformGrammarMarkerUnderlineColor() const {
  // Use the same color as MacPort for grammar marker underline.
  // See LayoutThemeMac::PlatformGrammarMarkerUnderlineColor()
  return Color(25, 175, 50, 191);
}

}  // namespace blink
