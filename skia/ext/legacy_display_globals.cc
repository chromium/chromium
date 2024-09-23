// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/legacy_display_globals.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "skia/ext/switches.h"
#include "ui/base/ui_base_features.h"

namespace skia {

namespace {
SkPixelGeometry g_pixel_geometry = kRGB_H_SkPixelGeometry;
float g_text_contrast = SK_GAMMA_CONTRAST;
float g_text_gamma = SK_GAMMA_EXPONENT;

// Lock to prevent data races between setting and getting values. It is
// not ideal to have mismatched `SkSurfaceProps` between threads, but it
// is not catastrophic.
base::Lock& GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

void GetContrastAndGammaValuesFromCommandLine(float& out_contrast,
                                              float& out_gamma) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string contrast_string =
      command_line->GetSwitchValueASCII(switches::kTextContrast);
  double contrast_double;
  if (base::StringToDouble(contrast_string, &contrast_double)) {
    out_contrast = base::checked_cast<float>(contrast_double);
  }
  std::string gamma_string =
      command_line->GetSwitchValueASCII(switches::kTextGamma);
  double gamma_double;
  if (base::StringToDouble(gamma_string, &gamma_double)) {
    out_gamma = base::checked_cast<float>(gamma_double);
  }
}
}

// static
void LegacyDisplayGlobals::SetCachedParams(SkPixelGeometry pixel_geometry,
                                           float text_contrast,
                                           float text_gamma) {
  base::AutoLock lock(GetLock());
  g_pixel_geometry = pixel_geometry;
#if BUILDFLAG(IS_WIN)
  g_text_contrast = text_contrast;
  g_text_gamma = text_gamma;
#endif  // BUILDFLAG(IS_WIN)
}

// static
SkSurfaceProps LegacyDisplayGlobals::GetSkSurfaceProps() {
  return GetSkSurfaceProps(0);
}

// static
SkSurfaceProps LegacyDisplayGlobals::GetSkSurfaceProps(uint32_t flags) {
  base::AutoLock lock(GetLock());
  float contrast = g_text_contrast;
  float gamma = g_text_gamma;

  // Prioritize values from the command line for testing.
  GetContrastAndGammaValuesFromCommandLine(contrast, gamma);
  return SkSurfaceProps{flags, g_pixel_geometry, contrast, gamma};
}

SkSurfaceProps LegacyDisplayGlobals::ComputeSurfaceProps(
    bool can_use_lcd_text) {
  uint32_t flags = 0;
  if (can_use_lcd_text) {
    return LegacyDisplayGlobals::GetSkSurfaceProps(flags);
  }
  base::AutoLock lock(GetLock());
  float contrast = g_text_contrast;
  float gamma = g_text_gamma;

  // Prioritize values from the command line for testing.
  GetContrastAndGammaValuesFromCommandLine(contrast, gamma);

  // Use unknown pixel geometry to disable LCD text.
  return SkSurfaceProps{flags, kUnknown_SkPixelGeometry, contrast, gamma};
}

}  // namespace skia
