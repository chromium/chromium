// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_webgpu.h"

#include "base/notimplemented.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_offscreen_rendering_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_rendering_context.h"

namespace blink {

WebGLRenderingContextWebGPU::WebGLRenderingContextWebGPU(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& requested_attributes)
    : WebGLRenderingContextWebGPUBase(host,
                                      requested_attributes,
                                      CanvasRenderingAPI::kWebgl) {}

V8RenderingContext* WebGLRenderingContextWebGPU::AsV8RenderingContext() {
  return MakeGarbageCollected<V8RenderingContext>(this);
}

V8OffscreenRenderingContext*
WebGLRenderingContextWebGPU::AsV8OffscreenRenderingContext() {
  return MakeGarbageCollected<V8OffscreenRenderingContext>(this);
}

}  // namespace blink
