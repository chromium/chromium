/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_theme_mobile.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

Color LayoutThemeMobile::active_selection_background_color_ =
    LayoutThemeMobile::kDefaultActiveSelectionBackgroundColor;
Color LayoutThemeMobile::active_selection_foreground_color_ =
    LayoutThemeMobile::kDefaultActiveSelectionForegroundColor;
Color LayoutThemeMobile::inactive_selection_background_color_ =
    Color::FromRGBA32(kDefaultInactiveSelectionBgColor);
Color LayoutThemeMobile::inactive_selection_foreground_color_ =
    Color::FromRGBA32(kDefaultInactiveSelectionFgColor);

scoped_refptr<LayoutTheme> LayoutThemeMobile::Create() {
  return base::AdoptRef(new LayoutThemeMobile());
}

LayoutThemeMobile::~LayoutThemeMobile() = default;

String LayoutThemeMobile::ExtraDefaultStyleSheet() {
  String stylesheet = StrCat(
      {LayoutThemeDefault::ExtraDefaultStyleSheet(),
       UncompressResourceAsASCIIString(IDR_UASTYLE_THEME_CHROMIUM_LINUX_CSS),
       UncompressResourceAsASCIIString(
           IDR_UASTYLE_THEME_CHROMIUM_ANDROID_CSS)});
  return stylesheet;
}

String LayoutThemeMobile::ExtraFullscreenStyleSheet() {
  return UncompressResourceAsASCIIString(IDR_UASTYLE_FULLSCREEN_ANDROID_CSS);
}

void LayoutThemeMobile::AdjustInnerSpinButtonStyle(
    ComputedStyleBuilder& builder) const {
  // Match Linux spin button style in web tests.
  // FIXME: Consider removing the conditional if a future Android theme matches
  // this.
  if (WebTestSupport::IsRunningWebTest())
    LayoutThemeDefault::AdjustInnerSpinButtonStyle(builder);
}

Color LayoutThemeMobile::PlatformActiveSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return LayoutThemeMobile::active_selection_foreground_color_;
}

Color LayoutThemeMobile::PlatformActiveSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return LayoutThemeMobile::active_selection_background_color_;
}

Color LayoutThemeMobile::PlatformInactiveSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return LayoutThemeMobile::inactive_selection_foreground_color_;
}

Color LayoutThemeMobile::PlatformInactiveSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return LayoutThemeMobile::inactive_selection_background_color_;
}

void LayoutThemeMobile::SetSelectionColors(Color active_background_color,
                                           Color active_foreground_color,
                                           Color inactive_background_color,
                                           Color inactive_foreground_color) {
  bool selection_colors_updated = false;
  if (active_selection_background_color_ != active_background_color ||
      active_selection_foreground_color_ != active_foreground_color ||
      inactive_selection_background_color_ != inactive_background_color ||
      inactive_selection_foreground_color_ != inactive_foreground_color) {
    active_selection_background_color_ = active_background_color;
    active_selection_foreground_color_ = active_foreground_color;
    inactive_selection_background_color_ = inactive_background_color;
    inactive_selection_foreground_color_ = inactive_foreground_color;
    selection_colors_updated = true;
  }
  // SetSelectionColors is expected to affect both default and mobile layout
  // theme. Update the default layout theme selection color if an update is
  // needed. Only run PlatformColorsDidChange when needed to minimize the
  // recalculation.
  if (LayoutThemeDefault::PlatformActiveSelectionBackgroundColor(
          mojom::blink::ColorScheme::kLight) != active_background_color ||
      LayoutThemeDefault::PlatformActiveSelectionForegroundColor(
          mojom::blink::ColorScheme::kLight) != active_foreground_color ||
      LayoutThemeDefault::PlatformInactiveSelectionBackgroundColor(
          mojom::blink::ColorScheme::kLight) != inactive_background_color ||
      LayoutThemeDefault::PlatformInactiveSelectionForegroundColor(
          mojom::blink::ColorScheme::kLight) != inactive_foreground_color) {
    LayoutThemeDefault::SetSelectionColors(
        active_background_color, active_foreground_color,
        inactive_background_color, inactive_foreground_color);
  } else if (selection_colors_updated) {
    PlatformColorsDidChange();
  }
}
}  // namespace blink
