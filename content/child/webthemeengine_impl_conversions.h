// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBTHEMEENGINE_IMPL_CONVERSIONS_H_
#define CONTENT_CHILD_WEBTHEMEENGINE_IMPL_CONVERSIONS_H_

#include "content/child/webthemeengine_impl_default.h"
#include "content/common/content_export.h"
#include "ui/native_theme/native_theme.h"

namespace content {

CONTENT_EXPORT ui::NativeTheme::Part NativeThemePart(
    blink::WebThemeEngine::Part part);

CONTENT_EXPORT ui::NativeTheme::ScrollbarOverlayColorTheme
NativeThemeScrollbarOverlayColorTheme(
    blink::WebScrollbarOverlayColorTheme theme);

CONTENT_EXPORT ui::NativeTheme::State NativeThemeState(
    blink::WebThemeEngine::State state);

CONTENT_EXPORT ui::NativeTheme::ColorScheme NativeColorScheme(
    blink::WebColorScheme color_scheme);

CONTENT_EXPORT ui::NativeTheme::SystemThemeColor NativeSystemThemeColor(
    blink::WebThemeEngine::SystemThemeColor theme_color);

CONTENT_EXPORT ui::NativeTheme::PreferredColorScheme NativePreferredColorScheme(
    blink::PreferredColorScheme preferred_color_scheme);

CONTENT_EXPORT blink::PreferredColorScheme WebPreferredColorScheme(
    ui::NativeTheme::PreferredColorScheme preferred_color_scheme);

}  // namespace content

#endif  // CONTENT_CHILD_WEBTHEMEENGINE_IMPL_CONVERSIONS_H_
