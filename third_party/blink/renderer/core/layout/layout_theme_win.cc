// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_theme_win.h"

#include <windows.h>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

scoped_refptr<LayoutTheme> LayoutThemeWin::Create() {
  return base::AdoptRef(new LayoutThemeWin());
}

LayoutTheme& LayoutTheme::NativeTheme() {
  DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeWin::Create()));
  return *layout_theme;
}

Color LayoutThemeWin::SystemHighlightFromColorProvider(
    mojom::blink::ColorScheme color_scheme,
    const ui::ColorProvider* color_provider) const {
  SkColor system_highlight_color =
      color_provider->GetColor(ui::kColorCssSystemHighlight);
  return Color::FromSkColor(system_highlight_color);
}

}  // namespace blink
