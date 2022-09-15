// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/legacy_display_globals.h"

namespace skia {

namespace {
SkPixelGeometry g_pixel_geometry = kRGB_H_SkPixelGeometry;
}

// static
void LegacyDisplayGlobals::SetCachedPixelGeometry(
    SkPixelGeometry pixel_geometry) {
  g_pixel_geometry = pixel_geometry;
}

// static
SkPixelGeometry LegacyDisplayGlobals::GetCachedPixelGeometry() {
  return g_pixel_geometry;
}

// static
SkSurfaceProps LegacyDisplayGlobals::GetSkSurfaceProps() {
  return GetSkSurfaceProps(0);
}

// static
SkSurfaceProps LegacyDisplayGlobals::GetSkSurfaceProps(uint32_t flags) {
  return SkSurfaceProps{flags, g_pixel_geometry};
}

SkSurfaceProps LegacyDisplayGlobals::ComputeSurfaceProps(
    bool can_use_lcd_text) {
  uint32_t flags = 0;
  if (can_use_lcd_text) {
    return LegacyDisplayGlobals::GetSkSurfaceProps(flags);
  }
  // Use unknown pixel geometry to disable LCD text.
  return SkSurfaceProps{flags, kUnknown_SkPixelGeometry};
}

}  // namespace skia
