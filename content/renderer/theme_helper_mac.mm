// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/theme_helper_mac.h"

#include <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"

extern "C" {
bool CGFontRenderingGetFontSmoothingDisabled(void);
}

namespace content {

void SystemColorsDidChange(int aqua_color_variant) {
  NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;

  // Register the defaults in the NSArgumentDomain, which is considered
  // volatile. Registering in the normal application domain fails from within
  // the sandbox.
  [defaults removeVolatileDomainForName:NSArgumentDomain];
}

bool IsSubpixelAntialiasingAvailable() {
  // See https://trac.webkit.org/changeset/239306/webkit for more info.
  return !CGFontRenderingGetFontSmoothingDisabled();
}

}  // namespace content
