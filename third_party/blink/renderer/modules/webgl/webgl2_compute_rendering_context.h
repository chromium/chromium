// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_COMPUTE_RENDERING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_COMPUTE_RENDERING_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_compute_rendering_context_base.h"

namespace blink {

class CanvasContextCreationAttributesCore;
class EXTColorBufferFloat;
class EXTFloatBlend;
class EXTTextureFilterAnisotropic;
class OESTextureFloatLinear;
class WebGLDebugRendererInfo;
class WebGLLoseContext;
class WebGLVideoTexture;

class WebGL2ComputeRenderingContext : public WebGL2ComputeRenderingContextBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
   public:
    Factory() = default;
    ~Factory() override = default;

    CanvasRenderingContext* Create(
        CanvasRenderingContextHost*,
        const CanvasContextCreationAttributesCore&) override;
    CanvasRenderingContext::ContextType GetContextType() const override {
      return CanvasRenderingContext::kContextWebgl2Compute;
    }
    void OnError(HTMLCanvasElement*, const String& error) override;

   private:
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  WebGL2ComputeRenderingContext(
      CanvasRenderingContextHost*,
      std::unique_ptr<WebGraphicsContext3DProvider>,
      bool using_gpu_compositing,
      const CanvasContextCreationAttributesCore& requested_attributes);

  CanvasRenderingContext::ContextType GetContextType() const override {
    return CanvasRenderingContext::kContextWebgl2Compute;
  }
  ImageBitmap* TransferToImageBitmap(ScriptState*) final;
  String ContextName() const override {
    return "WebGL2ComputeRenderingContext";
  }
  void RegisterContextExtensions() override;
  void SetCanvasGetContextResult(RenderingContext&) final;
  void SetOffscreenCanvasGetContextResult(OffscreenRenderingContext&) final;

  void Trace(blink::Visitor*) override;

 protected:
  Member<EXTColorBufferFloat> ext_color_buffer_float_;
  Member<EXTDisjointTimerQueryWebGL2> ext_disjoint_timer_query_web_gl2_;
  Member<EXTFloatBlend> ext_float_blend_;
  Member<EXTTextureFilterAnisotropic> ext_texture_filter_anisotropic_;
  Member<OESTextureFloatLinear> oes_texture_float_linear_;
  Member<WebGLCompressedTextureASTC> webgl_compressed_texture_astc_;
  Member<WebGLCompressedTextureETC> webgl_compressed_texture_etc_;
  Member<WebGLCompressedTextureETC1> webgl_compressed_texture_etc1_;
  Member<WebGLCompressedTexturePVRTC> webgl_compressed_texture_pvrtc_;
  Member<WebGLCompressedTextureS3TC> webgl_compressed_texture_s3tc_;
  Member<WebGLCompressedTextureS3TCsRGB> webgl_compressed_texture_s3tc_srgb_;
  Member<WebGLDebugRendererInfo> webgl_debug_renderer_info_;
  Member<WebGLDebugShaders> webgl_debug_shaders_;
  Member<WebGLLoseContext> webgl_lose_context_;
  Member<WebGLVideoTexture> webgl_video_texture_;
};

DEFINE_TYPE_CASTS(WebGL2ComputeRenderingContext,
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
