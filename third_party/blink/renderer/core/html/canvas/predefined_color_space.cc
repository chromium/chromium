// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_predefined_color_space.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

bool ValidateAndConvertColorSpace(const V8PredefinedColorSpace& v8_color_space,
                                  PredefinedColorSpace& color_space,
                                  ExceptionState& exception_state) {
  bool needs_v2 = false;
  bool needs_hdr = false;
  switch (v8_color_space.AsEnum()) {
    case V8PredefinedColorSpace::Enum::kSRGB:
      color_space = PredefinedColorSpace::kSRGB;
      break;
    case V8PredefinedColorSpace::Enum::kRec2020:
      color_space = PredefinedColorSpace::kRec2020;
      needs_v2 = true;
      break;
    case V8PredefinedColorSpace::Enum::kDisplayP3:
      color_space = PredefinedColorSpace::kP3;
      break;
    case V8PredefinedColorSpace::Enum::kRec2100Hlg:
      color_space = PredefinedColorSpace::kRec2100HLG;
      needs_hdr = true;
      break;
    case V8PredefinedColorSpace::Enum::kRec2100Pq:
      color_space = PredefinedColorSpace::kRec2100PQ;
      needs_hdr = true;
      break;
    case V8PredefinedColorSpace::Enum::kSRGBLinear:
      color_space = PredefinedColorSpace::kSRGBLinear;
      needs_hdr = true;
      break;
  }
  if ((needs_v2 && !RuntimeEnabledFeatures::CanvasColorManagementV2Enabled()) ||
      (needs_hdr && !RuntimeEnabledFeatures::CanvasHDREnabled())) {
    exception_state.ThrowTypeError(
        "The provided value '" + v8_color_space.AsString() +
        "' is not a valid enum value of the type PredefinedColorSpace.");
    return false;
  }
  return true;
}

}  // namespace blink
