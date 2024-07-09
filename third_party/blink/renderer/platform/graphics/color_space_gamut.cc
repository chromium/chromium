// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/color_space_gamut.h"

#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/display/screen_info.h"

namespace blink {

namespace color_space_utilities {

ColorSpaceGamut GetColorSpaceGamut(const display::ScreenInfo& screen_info) {
  const gfx::ColorSpace& color_space =
      screen_info.display_color_spaces.GetScreenInfoColorSpace();
  if (!color_space.IsValid())
    return ColorSpaceGamut::kUnknown;

  // TODO(crbug.com/1385853): Perform a better computation, using the available
  // SkColorSpacePrimaries.
  if (color_space.IsHDR())
    return ColorSpaceGamut::P3;

  sk_sp<SkColorSpace> sk_color_space = color_space.ToSkColorSpace();
  if (!sk_color_space)
    return ColorSpaceGamut::kUnknown;

  skcms_ICCProfile color_profile;
  sk_color_space->toProfile(&color_profile);
  return GetColorSpaceGamut(&color_profile);
}

ColorSpaceGamut GetColorSpaceGamut(const skcms_ICCProfile* color_profile) {
  if (!color_profile)
    return ColorSpaceGamut::kUnknown;

  skcms_ICCProfile sc_rgb = *skcms_sRGB_profile();
  skcms_SetTransferFunction(&sc_rgb, skcms_Identity_TransferFunction());

  unsigned char in[3][3];
  float out[3][3];
  memset(in, 0, sizeof(in));
  in[0][0] = 255;
  in[1][1] = 255;
  in[2][2] = 255;
  bool color_converison_successful = skcms_Transform(
      in, skcms_PixelFormat_RGB_888, skcms_AlphaFormat_Unpremul, color_profile,
      out, skcms_PixelFormat_RGB_fff, skcms_AlphaFormat_Unpremul, &sc_rgb, 3);
  DCHECK(color_converison_successful);
  float score = out[0][0] * out[1][1] * out[2][2];

  if (score < 0.9)
    return ColorSpaceGamut::kLessThanNTSC;
  if (score < 0.95)
    return ColorSpaceGamut::NTSC;  // actual score 0.912839
  if (score < 1.1)
    return ColorSpaceGamut::SRGB;  // actual score 1.0
  if (score < 1.3)
    return ColorSpaceGamut::kAlmostP3;
  if (score < 1.425)
    return ColorSpaceGamut::P3;  // actual score 1.401899
  if (score < 1.5)
    return ColorSpaceGamut::kAdobeRGB;  // actual score 1.458385
  if (score < 2.0)
    return ColorSpaceGamut::kWide;
  if (score < 2.2)
    return ColorSpaceGamut::BT2020;  // actual score 2.104520
  if (score < 2.7)
    return ColorSpaceGamut::kProPhoto;  // actual score 2.913247
  return ColorSpaceGamut::kUltraWide;
}

}  // namespace color_space_utilities

}  // namespace blink
