// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_LEGACY_DISPLAY_GLOBALS_H_
#define SKIA_EXT_LEGACY_DISPLAY_GLOBALS_H_

#include "third_party/skia/include/core/SkSurfaceProps.h"

namespace skia {

class SK_API LegacyDisplayGlobals {
 public:
  static void SetCachedParams(SkPixelGeometry pixel_geometry,
                              float text_contrast,
                              float text_gamma);

  // Returns a SkSurfaceProps with the cached geometry settings.
  static SkSurfaceProps GetSkSurfaceProps();
  static SkSurfaceProps GetSkSurfaceProps(uint32_t flags);

  // Will turn off LCD text if |can_use_lcd_text| is false.
  static SkSurfaceProps ComputeSurfaceProps(bool can_use_lcd_text);
};

}  // namespace skia

#endif  // SKIA_EXT_LEGACY_DISPLAY_GLOBALS_H_
