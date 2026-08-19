// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_SPACE_GAMUT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_SPACE_GAMUT_H_

#include "third_party/blink/renderer/platform/platform_export.h"

namespace display {
struct ScreenInfo;
}

namespace blink {

enum class ColorSpaceGamut {
  SRGB = 3,
  P3 = 5,
  BT2020 = 8,
};

namespace color_space_utilities {

PLATFORM_EXPORT ColorSpaceGamut GetColorSpaceGamut(const display::ScreenInfo&);

}  // namespace color_space_utilities

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_COLOR_SPACE_GAMUT_H_
