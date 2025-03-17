// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/htmlcanvas/canvas_context_creation_attributes_helpers.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_context_creation_attributes_module.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_pixel_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_rendering_context_2d_settings.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_will_read_frequently.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

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
  result.desynchronized_specified = attrs->desynchronized();
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/945835): enable desynchronized on Mac.
  result.desynchronized = false;
#else
  result.desynchronized = result.desynchronized_specified;
#endif
  switch (attrs->colorType().AsEnum()) {
    case V8CanvasPixelFormat::Enum::kUnorm8:
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

CanvasRenderingContext2DSettings* ToCanvasRenderingContext2DSettings(
    const CanvasContextCreationAttributesCore& attrs) {
  CanvasRenderingContext2DSettings* settings =
      CanvasRenderingContext2DSettings::Create();
  settings->setAlpha(attrs.alpha);
  settings->setColorSpace(PredefinedColorSpaceToV8(attrs.color_space));
  if (RuntimeEnabledFeatures::CanvasFloatingPointEnabled()) {
    switch (attrs.pixel_format) {
      case CanvasPixelFormat::kF16:
        settings->setColorType(
            V8CanvasPixelFormat(V8CanvasPixelFormat::Enum::kFloat16));
        break;
      case CanvasPixelFormat::kUint8:
        settings->setColorType(
            V8CanvasPixelFormat(V8CanvasPixelFormat::Enum::kUnorm8));
        break;
    }
  }
  settings->setDesynchronized(attrs.desynchronized_specified);

  switch (attrs.will_read_frequently) {
    case CanvasContextCreationAttributesCore::WillReadFrequently::kTrue:
      settings->setWillReadFrequently(true);
      break;
    case CanvasContextCreationAttributesCore::WillReadFrequently::kFalse:
    case CanvasContextCreationAttributesCore::WillReadFrequently::kUndefined:
      settings->setWillReadFrequently(false);
      break;
  }
  return settings;
}

}  // namespace blink
