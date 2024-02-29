// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/legacy_display_globals.h"

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace skia {

namespace {
SkPixelGeometry g_pixel_geometry = kRGB_H_SkPixelGeometry;

// Lock to prevent data races between setting and getting values. It is
// not ideal to have mismatched `SkSurfaceProps` between threads, but it
// is not catastrophic.
base::Lock& GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}
}

// static
void LegacyDisplayGlobals::SetCachedPixelGeometry(
    SkPixelGeometry pixel_geometry) {
  base::AutoLock lock(GetLock());
  g_pixel_geometry = pixel_geometry;
}

// static
SkSurfaceProps LegacyDisplayGlobals::GetSkSurfaceProps() {
  return GetSkSurfaceProps(0);
}

// static
SkSurfaceProps LegacyDisplayGlobals::GetSkSurfaceProps(uint32_t flags) {
  base::AutoLock lock(GetLock());
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
