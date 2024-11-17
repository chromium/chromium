// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/htmlcanvas/canvas_context_creation_attributes_helpers.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_context_creation_attributes_module.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"

namespace blink {

bool ToCanvasContextCreationAttributes(
    const CanvasContextCreationAttributesModule* attrs,
    CanvasContextCreationAttributesCore& result,
    ExceptionState& exception_state) {
  result.alpha = attrs->alpha();
  result.antialias = attrs->antialias();
  if (!ValidateAndConvertColorSpace(attrs->colorSpace(), result.color_space,
                                    exception_state)) {
    return false;
  }
  result.depth = attrs->depth();
  result.fail_if_major_performance_caveat =
      attrs->failIfMajorPerformanceCaveat();
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/945835): enable desynchronized on Mac.
  result.desynchronized = false;
#else
  result.desynchronized = attrs->desynchronized();
#endif
  switch (attrs->pixelFormat().AsEnum()) {
    case V8CanvasPixelFormat::Enum::kUint8:
      result.pixel_format = CanvasPixelFormat::kUint8;
      break;
    case V8CanvasPixelFormat::Enum::kFloat16:
      result.pixel_format = CanvasPixelFormat::kF16;
      break;
  }
  result.premultiplied_alpha = attrs->premultipliedAlpha();
  result.preserve_drawing_buffer = attrs->preserveDrawingBuffer();
  switch (attrs->powerPreference().AsEnum()) {
    case V8CanvasPowerPreference::Enum::kDefault:
      result.power_preference =
          CanvasContextCreationAttributesCore::PowerPreference::kDefault;
      break;
    case V8CanvasPowerPreference::Enum::kLowPower:
      result.power_preference =
          CanvasContextCreationAttributesCore::PowerPreference::kLowPower;
      break;
    case V8CanvasPowerPreference::Enum::kHighPerformance:
      result.power_preference = CanvasContextCreationAttributesCore::
          PowerPreference::kHighPerformance;
      break;
  }
  result.stencil = attrs->stencil();
  switch (attrs->willReadFrequently().AsEnum()) {
    case V8CanvasWillReadFrequently::Enum::kTrue:
      result.will_read_frequently =
          CanvasContextCreationAttributesCore::WillReadFrequently::kTrue;
      break;
    case V8CanvasWillReadFrequently::Enum::kFalse:
      result.will_read_frequently =
          CanvasContextCreationAttributesCore::WillReadFrequently::kFalse;
      break;
    default:
      result.will_read_frequently =
          CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined;
  }
  result.xr_compatible = attrs->xrCompatible();
  return true;
}

}  // namespace blink
