// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/color_space_gamut.h"

#include "skia/ext/skcolorspace_primaries.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/display_color_spaces.h"

namespace blink {

namespace color_space_utilities {

ColorSpaceGamut GetColorSpaceGamut(const display::ScreenInfo& screen_info) {
  const gfx::ColorSpace& color_space =
      screen_info.display_color_spaces.GetScreenInfoColorSpace();
  if (!color_space.IsValid()) {
    return ColorSpaceGamut::SRGB;
  }

  // TODO(crbug.com/1385853): Perform a better computation, using the available
  // SkColorSpacePrimaries.
  if (color_space.IsHDR())
    return ColorSpaceGamut::P3;

  sk_sp<SkColorSpace> sk_color_space = color_space.ToSkColorSpace();
  if (sk_color_space) {
    // Report that the screen supports a color gamut if it covers 90% of the
    // color gamut.
    if (skia::FractionGamutCovered(sk_color_space.get(),
                                   SkNamedPrimaries::kRec2020) >= 0.9f) {
      return ColorSpaceGamut::BT2020;
    }

    if (skia::FractionGamutCovered(sk_color_space.get(),
                                   SkNamedPrimariesExt::kP3) >= 0.9f) {
      return ColorSpaceGamut::P3;
    }
  }

  return ColorSpaceGamut::SRGB;
}

}  // namespace color_space_utilities

}  // namespace blink
