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

bool IsSubpixelAntialiasingAvailable() {
  // See https://trac.webkit.org/changeset/239306/webkit for more info.
  return !CGFontRenderingGetFontSmoothingDisabled();
}

}  // namespace content
