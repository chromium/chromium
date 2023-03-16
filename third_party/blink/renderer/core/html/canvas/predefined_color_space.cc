// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_high_dynamic_range_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_canvas_smpte_st_2086_metadata.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_predefined_color_space.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

bool ValidateAndConvertColorSpace(const V8PredefinedColorSpace& v8_color_space,
                                  PredefinedColorSpace& color_space,
                                  ExceptionState& exception_state) {
  bool needs_hdr = false;
  switch (v8_color_space.AsEnum()) {
    case V8PredefinedColorSpace::Enum::kSRGB:
      color_space = PredefinedColorSpace::kSRGB;
      break;
    case V8PredefinedColorSpace::Enum::kRec2020:
      color_space = PredefinedColorSpace::kRec2020;
      needs_hdr = true;
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
  if (needs_hdr && !RuntimeEnabledFeatures::CanvasHDREnabled()) {
    exception_state.ThrowTypeError(
        "The provided value '" + v8_color_space.AsString() +
        "' is not a valid enum value of the type PredefinedColorSpace.");
    return false;
  }
  return true;
}

V8PredefinedColorSpace PredefinedColorSpaceToV8(
    PredefinedColorSpace color_space) {
  switch (color_space) {
    case PredefinedColorSpace::kSRGB:
      return V8PredefinedColorSpace(V8PredefinedColorSpace::Enum::kSRGB);
    case PredefinedColorSpace::kRec2020:
      return V8PredefinedColorSpace(V8PredefinedColorSpace::Enum::kRec2020);
    case PredefinedColorSpace::kP3:
      return V8PredefinedColorSpace(V8PredefinedColorSpace::Enum::kDisplayP3);
    case PredefinedColorSpace::kRec2100HLG:
      return V8PredefinedColorSpace(V8PredefinedColorSpace::Enum::kRec2100Hlg);
    case PredefinedColorSpace::kRec2100PQ:
      return V8PredefinedColorSpace(V8PredefinedColorSpace::Enum::kRec2100Pq);
    case PredefinedColorSpace::kSRGBLinear:
      return V8PredefinedColorSpace(V8PredefinedColorSpace::Enum::kSRGBLinear);
  }
}

void ParseCanvasHighDynamicRangeOptions(
    const CanvasHighDynamicRangeOptions* options,
    gfx::HDRMode& hdr_mode,
    absl::optional<gfx::HDRMetadata>& hdr_metadata) {
  hdr_mode = gfx::HDRMode::kDefault;
  hdr_metadata = absl::nullopt;
  if (!options) {
    return;
  }
  if (options->hasMode()) {
    switch (options->mode().AsEnum()) {
      case V8CanvasHighDynamicRangeMode::Enum::kDefault:
        hdr_mode = gfx::HDRMode::kDefault;
        break;
      case V8CanvasHighDynamicRangeMode::Enum::kExtended:
        hdr_mode = gfx::HDRMode::kExtended;
        break;
    }
  }
  if (options->hasSmpteSt2086Metadata()) {
    hdr_metadata = gfx::HDRMetadata();
    auto& color_volume_metadata = hdr_metadata->color_volume_metadata;
    const auto* v8_metadata = options->smpteSt2086Metadata();
    color_volume_metadata.primaries = {
        v8_metadata->redPrimaryX(),   v8_metadata->redPrimaryY(),
        v8_metadata->greenPrimaryX(), v8_metadata->greenPrimaryY(),
        v8_metadata->bluePrimaryX(),  v8_metadata->bluePrimaryY(),
        v8_metadata->whitePointX(),   v8_metadata->whitePointY(),
    };
    color_volume_metadata.luminance_min = v8_metadata->minimumLuminance();
    color_volume_metadata.luminance_max = v8_metadata->maximumLuminance();
  }
}

}  // namespace blink
