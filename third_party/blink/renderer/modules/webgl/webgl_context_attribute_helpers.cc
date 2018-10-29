// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_context_attribute_helpers.h"
#include "third_party/blink/renderer/modules/xr/xr_device.h"

#include "third_party/blink/renderer/core/frame/settings.h"

namespace blink {

WebGLContextAttributes ToWebGLContextAttributes(
    const CanvasContextCreationAttributesCore& attrs) {
  WebGLContextAttributes result;
  result.setAlpha(attrs.alpha);
  result.setDepth(attrs.depth);
  result.setStencil(attrs.stencil);
  result.setAntialias(attrs.antialias);
  result.setPremultipliedAlpha(attrs.premultiplied_alpha);
  result.setPreserveDrawingBuffer(attrs.preserve_drawing_buffer);
  result.setFailIfMajorPerformanceCaveat(
      attrs.fail_if_major_performance_caveat);
  result.setCompatibleXRDevice(
      static_cast<XRDevice*>(attrs.compatible_xr_device.Get()));
  result.setLowLatency(attrs.low_latency);
  return result;
}

Platform::ContextAttributes ToPlatformContextAttributes(
    const CanvasContextCreationAttributesCore& attrs,
    Platform::ContextType context_type,
    bool support_own_offscreen_surface) {
  Platform::ContextAttributes result;
  result.fail_if_major_performance_caveat =
      attrs.fail_if_major_performance_caveat;
  result.context_type = context_type;
  if (support_own_offscreen_surface) {
    // Only ask for alpha/depth/stencil/antialias if we may be using the default
    // framebuffer. They are not needed for standard offscreen rendering.
    result.support_alpha = attrs.alpha;
    result.support_depth = attrs.depth;
    result.support_stencil = attrs.stencil;
    result.support_antialias = attrs.antialias;
  }
  return result;
}

}  // namespace blink
