// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/mac/core_text_font_format_support.h"

#include <CoreText/CoreText.h>

namespace blink {

// Compare CoreText.h in an up to date SDK, redefining here since we don't seem
// to have access to this value when building against the 10.10 SDK in our
// standard Chrome build configuration.
static const uint32_t kBlinkLocalCTVersionNumber10_12 = 0x00090000;
static const uint32_t kBlinkLocalCTVersionNumber10_13 = 0x000A0000;

bool CoreTextVersionSupportsVariations() {
  return &CTGetCoreTextVersion &&
         CTGetCoreTextVersion() >= kBlinkLocalCTVersionNumber10_12;
}

// CoreText versions below 10.13 display COLR cpal as black/foreground-color
// glyphs and do not interpret color glyph layers correctly.
bool CoreTextVersionSupportsColrCpal() {
  return &CTGetCoreTextVersion &&
         CTGetCoreTextVersion() >= kBlinkLocalCTVersionNumber10_13;
}

}  // namespace blink
