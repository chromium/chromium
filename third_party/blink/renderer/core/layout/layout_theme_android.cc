// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_theme_android.h"

#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "ui/base/ui_base_features.h"

namespace blink {

scoped_refptr<LayoutTheme> LayoutThemeAndroid::Create() {
  return base::AdoptRef(new LayoutThemeAndroid());
}

LayoutTheme& LayoutTheme::NativeTheme() {
  DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeAndroid::Create()));
  return *layout_theme;
}

LayoutThemeAndroid::~LayoutThemeAndroid() {}

Color LayoutThemeAndroid::SystemColor(CSSValueID css_value_id,
                                      mojom::blink::ColorScheme color_scheme,
                                      const ui::ColorProvider* color_provider,
                                      bool is_in_web_app_scope) const {
  // Color providers are not supported for Android, so we should always use
  // DefaultSystemColor() for system colors.
  // TODO(crbug.com/40779801): This override can be removed if we can always
  // guarantee the provider is nullptr for Android.
  return DefaultSystemColor(css_value_id, color_scheme, color_provider,
                            is_in_web_app_scope);
}

Color LayoutThemeAndroid::PlatformActiveSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return color_scheme == mojom::blink::ColorScheme::kDark
             ? Color::FromRGBA32(0xFF99C8FF)
             : LayoutThemeMobile::PlatformActiveSelectionBackgroundColor(
                   color_scheme);
}

Color LayoutThemeAndroid::PlatformActiveSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return color_scheme == mojom::blink::ColorScheme::kDark
             ? Color::FromRGBA32(0xFF3B3B3B)
             : LayoutThemeMobile::PlatformActiveSelectionForegroundColor(
                   color_scheme);
}

}  // namespace blink
