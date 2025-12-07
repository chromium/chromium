// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/predefined_color_space.h"

#include "base/notreached.h"
#include "ui/gfx/color_space.h"

namespace blink {

// The PredefinedColorSpace value definitions are specified in the CSS Color
// Level 4 specification.
gfx::ColorSpace PredefinedColorSpaceToGfxColorSpace(
    PredefinedColorSpace color_space) {
  switch (color_space) {
    case PredefinedColorSpace::kSRGB:
      return gfx::ColorSpace::CreateSRGB();
    case PredefinedColorSpace::kRec2020:
      return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                             gfx::ColorSpace::TransferID::GAMMA24);
    case PredefinedColorSpace::kP3:
      return gfx::ColorSpace::CreateDisplayP3D65();
    case PredefinedColorSpace::kRec2100HLG:
      return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                             gfx::ColorSpace::TransferID::HLG);
    case PredefinedColorSpace::kRec2100PQ:
      return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                             gfx::ColorSpace::TransferID::PQ);
    case PredefinedColorSpace::kSRGBLinear:
      return gfx::ColorSpace::CreateSRGBLinear();
    case PredefinedColorSpace::kDisplayP3Linear:
      return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::P3,
                             gfx::ColorSpace::TransferID::LINEAR);
    case PredefinedColorSpace::kRec2100Linear:
      return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                             gfx::ColorSpace::TransferID::LINEAR);
  }
  NOTREACHED();
}

sk_sp<SkColorSpace> PredefinedColorSpaceToSkColorSpace(
    PredefinedColorSpace color_space) {
  return PredefinedColorSpaceToGfxColorSpace(color_space).ToSkColorSpace();
}

}  // namespace blink
