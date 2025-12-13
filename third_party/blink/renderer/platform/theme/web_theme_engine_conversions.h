// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_CONVERSIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_CONVERSIONS_H_

#include "third_party/blink/public/mojom/css/preferred_contrast.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-shared.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_default.h"
#include "ui/native_theme/native_theme.h"

namespace blink {

PLATFORM_EXPORT ui::NativeTheme::Part NativeThemePart(
    WebThemeEngine::Part part);

PLATFORM_EXPORT ui::NativeTheme::State NativeThemeState(
    WebThemeEngine::State state);

PLATFORM_EXPORT ui::NativeTheme::PreferredColorScheme NativeColorScheme(
    mojom::blink::ColorScheme color_scheme);

PLATFORM_EXPORT ui::NativeTheme::PreferredContrast NativeContrast(
    mojom::blink::PreferredContrast contrast);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_THEME_WEB_THEME_ENGINE_CONVERSIONS_H_
