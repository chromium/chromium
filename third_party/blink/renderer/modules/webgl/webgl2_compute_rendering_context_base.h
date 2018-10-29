// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_COMPUTE_RENDERING_CONTEXT_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_COMPUTE_RENDERING_CONTEXT_BASE_H_

#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"

namespace blink {

class WebGLTexture;

class WebGL2ComputeRenderingContextBase : public WebGL2RenderingContextBase {
 public:
  void DestroyContext() override;

  /* Launch one or more compute work groups */
  void dispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ);

  /* Bind a level of a texture to an image unit */
  void bindImageTexture(GLuint unit,
                        WebGLTexture* texture,
                        GLint level,
                        GLboolean layered,
                        GLint layer,
                        GLenum access,
                        GLenum format);

  /* Memory access synchronization */
  void memoryBarrier(GLbitfield barriers);
  void memoryBarrierByRegion(GLbitfield barriers);

  /* WebGLRenderingContextBase overrides */
  void InitializeNewContext() override;
  ScriptValue getParameter(ScriptState*, GLenum pname) override;

  void Trace(blink::Visitor*) override;

 protected:
  WebGL2ComputeRenderingContextBase(
      CanvasRenderingContextHost*,
      std::unique_ptr<WebGraphicsContext3DProvider>,
      bool using_gpu_compositing,
      const CanvasContextCreationAttributesCore& requested_attributes);
};

DEFINE_TYPE_CASTS(WebGL2ComputeRenderingContextBase,
                  CanvasRenderingContext,
                  context,
                  context->Is3d() &&
                      WebGLRenderingContextBase::GetWebGLVersion(context) ==
                          Platform::kWebGL2ComputeContextType,
                  context.Is3d() &&
                      WebGLRenderingContextBase::GetWebGLVersion(&context) ==
                          Platform::kWebGL2ComputeContextType);

}  // namespace blink

#endif
