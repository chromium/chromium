// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_RENDERING_CONTEXT_WEBGPU_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_RENDERING_CONTEXT_WEBGPU_H_

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_webgpu_base.h"

namespace blink {

class WebGL2RenderingContextWebGPU final
    : public WebGLRenderingContextWebGPUBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  WebGL2RenderingContextWebGPU(
      CanvasRenderingContextHost* host,
      const CanvasContextCreationAttributesCore& requested_attributes);

  // CanvasRenderingContext implementation
  V8RenderingContext* AsV8RenderingContext() final;
  V8OffscreenRenderingContext* AsV8OffscreenRenderingContext() final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_RENDERING_CONTEXT_WEBGPU_H_
