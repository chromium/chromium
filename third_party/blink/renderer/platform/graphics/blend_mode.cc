// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/blend_mode.h"

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

SkBlendMode ToSkBlendMode(CompositeOperator op, BlendMode blend_mode) {
  if (blend_mode != BlendMode::kNormal) {
    DCHECK(op == kCompositeSourceOver);
    return ToSkBlendMode(blend_mode);
  }

  switch (op) {
    case kCompositeClear:
      return SkBlendMode::kClear;
    case kCompositeCopy:
      return SkBlendMode::kSrc;
    case kCompositeSourceOver:
      return SkBlendMode::kSrcOver;
    case kCompositeSourceIn:
      return SkBlendMode::kSrcIn;
    case kCompositeSourceOut:
      return SkBlendMode::kSrcOut;
    case kCompositeSourceAtop:
      return SkBlendMode::kSrcATop;
    case kCompositeDestinationOver:
      return SkBlendMode::kDstOver;
    case kCompositeDestinationIn:
      return SkBlendMode::kDstIn;
    case kCompositeDestinationOut:
      return SkBlendMode::kDstOut;
    case kCompositeDestinationAtop:
      return SkBlendMode::kDstATop;
    case kCompositeXOR:
      return SkBlendMode::kXor;
    case kCompositePlusLighter:
      return SkBlendMode::kPlus;
  }

  NOTREACHED();
}

SkBlendMode ToSkBlendMode(BlendMode blend_mode) {
  switch (blend_mode) {
    case BlendMode::kNormal:
      return SkBlendMode::kSrcOver;
    case BlendMode::kMultiply:
      return SkBlendMode::kMultiply;
    case BlendMode::kScreen:
      return SkBlendMode::kScreen;
    case BlendMode::kOverlay:
      return SkBlendMode::kOverlay;
    case BlendMode::kDarken:
      return SkBlendMode::kDarken;
    case BlendMode::kLighten:
      return SkBlendMode::kLighten;
    case BlendMode::kColorDodge:
      return SkBlendMode::kColorDodge;
    case BlendMode::kColorBurn:
      return SkBlendMode::kColorBurn;
    case BlendMode::kHardLight:
      return SkBlendMode::kHardLight;
    case BlendMode::kSoftLight:
      return SkBlendMode::kSoftLight;
    case BlendMode::kDifference:
      return SkBlendMode::kDifference;
    case BlendMode::kExclusion:
      return SkBlendMode::kExclusion;
    case BlendMode::kHue:
      return SkBlendMode::kHue;
    case BlendMode::kSaturation:
      return SkBlendMode::kSaturation;
    case BlendMode::kColor:
      return SkBlendMode::kColor;
    case BlendMode::kLuminosity:
      return SkBlendMode::kLuminosity;
    case BlendMode::kPlusLighter:
      return SkBlendMode::kPlus;
  }

  NOTREACHED();
}

String BlendModeToString(BlendMode blend_op) {
  switch (blend_op) {
    case BlendMode::kNormal:
      return "normal";
    case BlendMode::kMultiply:
      return "multiply";
    case BlendMode::kScreen:
      return "screen";
    case BlendMode::kOverlay:
      return "overlay";
    case BlendMode::kDarken:
      return "darken";
    case BlendMode::kLighten:
      return "lighten";
    case BlendMode::kColorDodge:
      return "color-dodge";
    case BlendMode::kColorBurn:
      return "color-burn";
    case BlendMode::kHardLight:
      return "hard-light";
    case BlendMode::kSoftLight:
      return "soft-light";
    case BlendMode::kDifference:
      return "difference";
    case BlendMode::kExclusion:
      return "exclusion";
    case BlendMode::kHue:
      return "hue";
    case BlendMode::kSaturation:
      return "saturation";
    case BlendMode::kColor:
      return "color";
    case BlendMode::kLuminosity:
      return "luminosity";
    case BlendMode::kPlusLighter:
      return "plus-lighter";
  }
  NOTREACHED();
}

}  // namespace blink
