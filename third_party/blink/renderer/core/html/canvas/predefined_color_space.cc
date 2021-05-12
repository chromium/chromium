// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"

#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

bool ColorSpaceNameIsValid(const String& color_space_name,
                           ExceptionState& exception_state) {
  if (!RuntimeEnabledFeatures::CanvasColorManagementEnabled()) {
    // The enum value 'rec2020' is not valid unless CanvasColorManagement is
    // enabled.
    if (color_space_name == kRec2020CanvasColorSpaceName) {
      exception_state.ThrowTypeError(
          "The provided value '" + color_space_name +
          "' is not a valid enum value of the type PredefinedColorSpace.");
      return false;
    }
  }
  return true;
}  // namespace blink

}  // namespace blink
