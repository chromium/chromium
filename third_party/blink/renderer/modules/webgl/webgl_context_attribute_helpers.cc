// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_context_attribute_helpers.h"

#include "third_party/blink/renderer/core/frame/settings.h"
#include "ui/gl/gpu_preference.h"

namespace blink {

V8WebGLPowerPreference::Enum ToGLPowerPreference(
    CanvasContextCreationAttributesCore::PowerPreference power_preference) {
  switch (power_preference) {
    case CanvasContextCreationAttributesCore::PowerPreference::kDefault:
      return V8WebGLPowerPreference::Enum::kDefault;
    case CanvasContextCreationAttributesCore::PowerPreference::kLowPower:
      return V8WebGLPowerPreference::Enum::kLowPower;
    case CanvasContextCreationAttributesCore::PowerPreference::kHighPerformance:
      return V8WebGLPowerPreference::Enum::kHighPerformance;
  }
}

WebGLContextAttributes* ToWebGLContextAttributes(
    const CanvasContextCreationAttributesCore& attrs) {
  WebGLContextAttributes* result = WebGLContextAttributes::Create();
  result->setAlpha(attrs.alpha);
  result->setDepth(attrs.depth);
  result->setStencil(attrs.stencil);
  result->setAntialias(attrs.antialias);
  result->setPremultipliedAlpha(attrs.premultiplied_alpha);
  result->setPreserveDrawingBuffer(attrs.preserve_drawing_buffer);
  result->setPowerPreference(ToGLPowerPreference(attrs.power_preference));
  result->setFailIfMajorPerformanceCaveat(
      attrs.fail_if_major_performance_caveat);
  result->setXrCompatible(attrs.xr_compatible);
  result->setDesynchronized(attrs.desynchronized);
  return result;
}

Platform::ContextAttributes ToPlatformContextAttributes(
    const CanvasContextCreationAttributesCore& attrs,
    Platform::ContextType context_type) {
  Platform::ContextAttributes result;
  result.prefer_low_power_gpu =
      (PowerPreferenceToGpuPreference(attrs.power_preference) ==
       gl::GpuPreference::kLowPower);
  result.fail_if_major_performance_caveat =
      attrs.fail_if_major_performance_caveat;
  result.context_type = context_type;
  return result;
}

gl::GpuPreference PowerPreferenceToGpuPreference(
    CanvasContextCreationAttributesCore::PowerPreference power_preference) {
  // This code determines the handling of the "default" power preference.
  if (power_preference ==
      CanvasContextCreationAttributesCore::PowerPreference::kHighPerformance) {
    return gl::GpuPreference::kHighPerformance;
  }
  return gl::GpuPreference::kLowPower;
}

}  // namespace blink
