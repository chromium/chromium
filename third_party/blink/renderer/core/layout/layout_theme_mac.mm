/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Google, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#import "third_party/blink/renderer/core/layout/layout_theme_mac.h"

#import <Cocoa/Cocoa.h>

#import "third_party/blink/public/common/sandbox_support/sandbox_support_mac.h"
#import "third_party/blink/public/platform/mac/web_sandbox_support.h"
#import "third_party/blink/public/platform/platform.h"
#import "third_party/blink/renderer/core/fileapi/file.h"
#import "third_party/blink/renderer/core/style/computed_style.h"
#import "third_party/blink/renderer/platform/web_test_support.h"
#import "ui/base/ui_base_features.h"
#import "ui/native_theme/native_theme.h"

namespace blink {

namespace {
Color GetSystemColor(MacSystemColorID color_id,
                     mojom::blink::ColorScheme color_scheme) {
  // TODO(almaher): Consider using the mac light and dark high-contrast themes
  // here instead if forced colors mode is enabled.

  // In tests, a WebSandboxSupport may not be set up. Just return a dummy
  // color, in this case opaque black.
  auto* sandbox_support = Platform::Current()->GetSandboxSupport();
  if (!sandbox_support)
    return Color(0, 0, 0, 255);
  return Color::FromSkColor(
      sandbox_support->GetSystemColor(color_id, color_scheme));
}
}

String LayoutThemeMac::DisplayNameForFile(const File& file) const {
  if (file.GetUserVisibility() == File::kIsUserVisible)
    return [[NSFileManager defaultManager] displayNameAtPath:file.GetPath()];
  return file.name();
}

Color LayoutThemeMac::PlatformActiveSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return GetSystemColor(MacSystemColorID::kSelectedTextBackground,
                        color_scheme);
}

Color LayoutThemeMac::PlatformInactiveSelectionBackgroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return GetSystemColor(MacSystemColorID::kSecondarySelectedControl,
                        color_scheme);
}

Color LayoutThemeMac::PlatformActiveSelectionForegroundColor(
    mojom::blink::ColorScheme color_scheme) const {
  return Color::kBlack;
}

Color LayoutThemeMac::PlatformSpellingMarkerUnderlineColor() const {
  // Using the same color than WebKit (see
  // https://github.com/WebKit/WebKit/blob/main/Source/WebCore/platform/graphics/cocoa/GraphicsContextCocoa.mm#L167).
  return Color(255, 59, 48, 191);
}

Color LayoutThemeMac::PlatformGrammarMarkerUnderlineColor() const {
  // Using the same color than WebKit (see
  // https://github.com/WebKit/WebKit/blob/main/Source/WebCore/platform/graphics/cocoa/GraphicsContextCocoa.mm#L175).
  return Color(25, 175, 50, 191);
}

bool LayoutThemeMac::IsAccentColorCustomized(
    mojom::blink::ColorScheme color_scheme) const {
  static const Color kControlBlueAccentColor =
      GetSystemColor(MacSystemColorID::kControlAccentBlueColor, color_scheme);
  if (kControlBlueAccentColor ==
      GetSystemColor(MacSystemColorID::kControlAccentColor, color_scheme)) {
    return false;
  }

  return true;
}

Color LayoutThemeMac::GetSystemAccentColor(
    mojom::blink::ColorScheme color_scheme) const {
  return GetSystemColor(MacSystemColorID::kControlAccentColor, color_scheme);
}

Color LayoutThemeMac::SystemHighlightFromColorProvider(
    mojom::blink::ColorScheme color_scheme,
    const ui::ColorProvider* color_provider) const {
  SkColor system_highlight_color =
      color_provider->GetColor(ui::kColorCssSystemHighlight);
  Color color = Color::FromSkColor(system_highlight_color);
  // BlendWithWhite() darkens Mac system colors too much.
  // Apply .8 (204/255) alpha instead, same as Safari.
  if (color_scheme == mojom::blink::ColorScheme::kDark) {
    return Color(color.Red(), color.Green(), color.Blue(), 204);
  }

  return color.BlendWithWhite();
}

Color LayoutThemeMac::GetCustomFocusRingColor(
    mojom::blink::ColorScheme color_scheme) const {
  return color_scheme == mojom::blink::ColorScheme::kDark
             ? Color::FromRGB(0x99, 0xC8, 0xFF)
             : LayoutTheme::GetCustomFocusRingColor();
}

Color LayoutThemeMac::FocusRingColor(
    mojom::blink::ColorScheme color_scheme) const {
  const Color kDefaultFocusRingColorLight =
      Color::FromRGBA(0x10, 0x10, 0x10, 0xFF);
  const Color kDefaultFocusRingColorDark =
      Color::FromRGBA(0x99, 0xC8, 0xFF, 0xFF);
  if (UsesTestModeFocusRingColor()) {
    return HasCustomFocusRingColor()
               ? GetCustomFocusRingColor(color_scheme)
               : color_scheme == mojom::blink::ColorScheme::kDark
                     ? kDefaultFocusRingColorDark
                     : kDefaultFocusRingColorLight;
  }

  if (ui::NativeTheme::GetInstanceForWeb()->UserHasContrastPreference()) {
    // When high contrast is enabled, #101010 should be used.
    return Color::FromRGBA(0x10, 0x10, 0x10, 0xFF);
  }

  SkColor4f keyboard_focus_indicator =
      GetSystemColor(MacSystemColorID::kKeyboardFocusIndicator, color_scheme)
          .toSkColor4f();
  Color focus_ring = Color::FromSkColor4f(
      ui::NativeTheme::GetInstanceForWeb()->FocusRingColorForBaseColor(
          keyboard_focus_indicator));

  if (!HasCustomFocusRingColor())
    return focus_ring;
  // Use the custom focus ring color when the system accent color wasn't
  // changed.
  if (!IsAccentColorCustomized(color_scheme))
    return GetCustomFocusRingColor(color_scheme);
  return focus_ring;
}

bool LayoutThemeMac::UsesTestModeFocusRingColor() const {
  return WebTestSupport::IsRunningWebTest();
}

LayoutTheme& LayoutTheme::NativeTheme() {
  DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeMac::Create()));
  return *layout_theme;
}

}  // namespace blink
