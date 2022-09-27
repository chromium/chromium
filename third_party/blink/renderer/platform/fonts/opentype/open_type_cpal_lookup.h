// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_OPENTYPE_OPEN_TYPE_CPAL_LOOKUP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_OPENTYPE_OPEN_TYPE_CPAL_LOOKUP_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

/* Tools for inspecting the font palette of a COLR/CPAL font to find dark/light
 * mode preferred palettes and resolve string-based palette overrides as
 * specified in font-palette or @font-palette-values CSS. */
class PLATFORM_EXPORT OpenTypeCpalLookup {
 public:
  enum PaletteUse { kUsableWithLightBackground, kUsableWithDarkBackground };

  /* Return the index of the first palette useful for the specified
   * palette use, dark or light. Important: The SkTypeface passed in
   * should allow efficient access to its internal data buffer using
   * SkTypeface::openStream, which is not the case for CoreText-backed
   * SkTypeface objects.
   */
  static absl::optional<uint16_t> FirstThemedPalette(sk_sp<SkTypeface> typeface,
                                                     PaletteUse palette_use);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_OPENTYPE_OPEN_TYPE_CPAL_LOOKUP_H_
