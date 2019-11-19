// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webthemeengine_impl_mac.h"

#include "content/child/webthemeengine_impl_conversions.h"
#include "ui/native_theme/native_theme.h"

namespace content {

blink::ForcedColors WebThemeEngineMac::GetForcedColors() const {
  return ui::NativeTheme::GetInstanceForWeb()->UsesHighContrastColors()
             ? blink::ForcedColors::kActive
             : blink::ForcedColors::kNone;
}

void WebThemeEngineMac::SetForcedColors(
    const blink::ForcedColors forced_colors) {
  ui::NativeTheme::GetInstanceForWeb()->set_high_contrast(
      forced_colors == blink::ForcedColors::kActive);
}

blink::PreferredColorScheme WebThemeEngineMac::PreferredColorScheme() const {
  ui::NativeTheme::PreferredColorScheme preferred_color_scheme =
      ui::NativeTheme::GetInstanceForWeb()->GetPreferredColorScheme();
  return WebPreferredColorScheme(preferred_color_scheme);
}

void WebThemeEngineMac::SetPreferredColorScheme(
    const blink::PreferredColorScheme preferred_color_scheme) {
  ui::NativeTheme::GetInstanceForWeb()->set_preferred_color_scheme(
      NativePreferredColorScheme(preferred_color_scheme));
}

}  // namespace content
