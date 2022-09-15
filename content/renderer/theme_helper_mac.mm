// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/theme_helper_mac.h"

#include <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"

extern "C" {
bool CGFontRenderingGetFontSmoothingDisabled(void) API_AVAILABLE(macos(10.14));
}

namespace content {

void SystemColorsDidChange(int aqua_color_variant,
                           const std::string& highlight_text_color,
                           const std::string& highlight_color) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Register the defaults in the NSArgumentDomain, which is considered
  // volatile. Registering in the normal application domain fails from within
  // the sandbox.
  [defaults removeVolatileDomainForName:NSArgumentDomain];

  NSDictionary* domain_values = @{
    @"AppleAquaColorVariant" : @(aqua_color_variant),
    @"AppleHighlightedTextColor" :
        base::SysUTF8ToNSString(highlight_text_color),
    @"AppleHighlightColor" : base::SysUTF8ToNSString(highlight_color)
  };
  [defaults setVolatileDomain:domain_values forName:NSArgumentDomain];

  // CoreUI expects two distributed notifications to be posted as a result of
  // the Aqua color variant being changed: AppleAquaColorVariantChanged and
  // AppleColorPreferencesChangedNotification. These cannot be posted from
  // within the sandbox, as distributed notifications are always delivered
  // from the distnoted server, not directly from the posting app. As a result,
  // the Aqua control color will not change as it should.

  // However, the highlight color variant can be updated as a result of
  // posting local notifications. Post the notifications required to make that
  // change visible.
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

  // Trigger a NSDynamicSystemColorStore recache. Both Will and Did
  // must be posted.
  [center postNotificationName:@"NSSystemColorsWillChangeNotification"
                        object:nil];
  [center postNotificationName:NSSystemColorsDidChangeNotification
                        object:nil];
  // Post the notification that CoreUI would trigger as a result of the Aqua
  // color change.
  [center postNotificationName:NSControlTintDidChangeNotification
                        object:nil];
}

bool IsSubpixelAntialiasingAvailable() {
  if (__builtin_available(macOS 10.14, *)) {
    // See https://trac.webkit.org/changeset/239306/webkit for more info.
    return !CGFontRenderingGetFontSmoothingDisabled();
  }
  return true;
}

}  // namespace content
