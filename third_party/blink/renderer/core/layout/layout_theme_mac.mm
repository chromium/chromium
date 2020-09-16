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
Color GetSystemColor(MacSystemColorID color_id, ColorScheme color_scheme) {
  // In tests, a WebSandboxSupport may not be set up. Just return a dummy
  // color, in this case, black.
  auto* sandbox_support = Platform::Current()->GetSandboxSupport();
  if (!sandbox_support)
    return Color();
  return sandbox_support->GetSystemColor(color_id, color_scheme);
}
}

String LayoutThemeMac::DisplayNameForFile(const File& file) const {
  if (file.GetUserVisibility() == File::kIsUserVisible)
    return [[NSFileManager defaultManager] displayNameAtPath:file.GetPath()];
  return file.name();
}

Color LayoutThemeMac::PlatformActiveSelectionBackgroundColor(
    ColorScheme color_scheme) const {
  return GetSystemColor(MacSystemColorID::kSelectedTextBackground,
                        color_scheme);
}

Color LayoutThemeMac::PlatformInactiveSelectionBackgroundColor(
    ColorScheme color_scheme) const {
  return GetSystemColor(MacSystemColorID::kSecondarySelectedControl,
                        color_scheme);
}

Color LayoutThemeMac::PlatformActiveSelectionForegroundColor(
    ColorScheme color_scheme) const {
  return Color::kBlack;
}

Color LayoutThemeMac::PlatformSpellingMarkerUnderlineColor() const {
  return Color(251, 45, 29);
}

Color LayoutThemeMac::PlatformGrammarMarkerUnderlineColor() const {
  return Color(107, 107, 107);
}

bool LayoutThemeMac::IsAccentColorCustomized(ColorScheme color_scheme) const {
  if (@available(macOS 10.14, *)) {
    static const Color kControlBlueAccentColor =
        GetSystemColor(MacSystemColorID::kControlAccentBlueColor, color_scheme);
    if (kControlBlueAccentColor ==
        GetSystemColor(MacSystemColorID::kControlAccentColor, color_scheme)) {
      return false;
    }
  } else {
    int user_custom_color = [[NSUserDefaults standardUserDefaults]
        integerForKey:@"AppleAquaColorVariant"];
    if (user_custom_color == NSBlueControlTint ||
        user_custom_color == NSDefaultControlTint) {
      return false;
    }
  }
  return true;
}

Color LayoutThemeMac::FocusRingColor() const {
  static const RGBA32 kDefaultFocusRingColor = 0xFF101010;
  if (UsesTestModeFocusRingColor()) {
    return HasCustomFocusRingColor() ? GetCustomFocusRingColor()
                                     : kDefaultFocusRingColor;
  }

  if (ui::NativeTheme::GetInstanceForWeb()->UsesHighContrastColors()) {
    // When high contrast is enabled, #101010 should be used.
    return Color(0xFF101010);
  }

  // TODO(crbug.com/929098) Need to pass an appropriate color scheme here.
  ColorScheme color_scheme = ComputedStyle::InitialStyle().UsedColorScheme();

  SkColor keyboard_focus_indicator = SkColor(
      GetSystemColor(MacSystemColorID::kKeyboardFocusIndicator, color_scheme));
  Color focus_ring =
      ui::NativeTheme::GetInstanceForWeb()->FocusRingColorForBaseColor(
          keyboard_focus_indicator);

  if (!HasCustomFocusRingColor())
    return focus_ring;
  // Use the custom focus ring color when the system accent color wasn't
  // changed.
  if (!IsAccentColorCustomized(color_scheme))
    return GetCustomFocusRingColor();
  return focus_ring;
}

bool LayoutThemeMac::UsesTestModeFocusRingColor() const {
  return WebTestSupport::IsRunningWebTest();
}

LayoutTheme& LayoutTheme::NativeTheme() {
  DCHECK(features::IsFormControlsRefreshEnabled());
  DEFINE_STATIC_REF(LayoutTheme, layout_theme, (LayoutThemeMac::Create()));
  return *layout_theme;
}

}  // namespace blink
