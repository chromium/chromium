// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

bool ColorSpaceNameIsValid(const String& color_space_name,
                           ExceptionState& exception_state) {
  bool color_space_name_valid = true;
  if (color_space_name == "rec2020") {
    color_space_name_valid =
        RuntimeEnabledFeatures::CanvasColorManagementV2Enabled();
  } else if (color_space_name == "rec2100-hlg" ||
             color_space_name == "rec2100-pq" ||
             color_space_name == "srgb-linear") {
    color_space_name_valid = RuntimeEnabledFeatures::CanvasHDREnabled();
  }
  if (color_space_name_valid)
    return true;

  exception_state.ThrowTypeError(
      "The provided value '" + color_space_name +
      "' is not a valid enum value of the type PredefinedColorSpace.");
  return false;
}  // namespace blink

}  // namespace blink
