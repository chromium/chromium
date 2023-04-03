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

void SystemColorsDidChange(int aqua_color_variant) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Register the defaults in the NSArgumentDomain, which is considered
  // volatile. Registering in the normal application domain fails from within
  // the sandbox.
  [defaults removeVolatileDomainForName:NSArgumentDomain];

  // LayoutThemeMac reads AppleAquaColorVariant on macOS versions before 10.14.
  NSDictionary* domain_values = @{
    @"AppleAquaColorVariant" : @(aqua_color_variant),
  };
  [defaults setVolatileDomain:domain_values forName:NSArgumentDomain];
}

bool IsSubpixelAntialiasingAvailable() {
  if (__builtin_available(macOS 10.14, *)) {
    // See https://trac.webkit.org/changeset/239306/webkit for more info.
    return !CGFontRenderingGetFontSmoothingDisabled();
  }
  return true;
}

}  // namespace content
