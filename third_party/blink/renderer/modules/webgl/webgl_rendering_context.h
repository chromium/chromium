/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_RENDERING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_RENDERING_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"

namespace blink {

class ANGLEInstancedArrays;
class CanvasContextCreationAttributesCore;
class EXTBlendMinMax;
class EXTColorBufferHalfFloat;
class EXTFloatBlend;
class EXTFragDepth;
class EXTShaderTextureLOD;
class EXTsRGB;
class EXTTextureFilterAnisotropic;
class KHRParallelShaderCompile;
class OESElementIndexUint;
class OESFboRenderMipmap;
class OESStandardDerivatives;
class OESTextureFloat;
class OESTextureFloatLinear;
class OESTextureHalfFloat;
class OESTextureHalfFloatLinear;
class WebGLColorBufferFloat;
class WebGLDebugRendererInfo;
class WebGLDepthTexture;
class WebGLLoseContext;
class WebGLMultiDraw;
class WebGLVideoTexture;

class WebGLRenderingContext final : public WebGLRenderingContextBase {
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
      return CanvasRenderingContext::kContextWebgl;
    }
    void OnError(HTMLCanvasElement*, const String& error) override;

   private:
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  WebGLRenderingContext(CanvasRenderingContextHost*,
                        std::unique_ptr<WebGraphicsContext3DProvider>,
                        bool using_gpu_compositing,
                        const CanvasContextCreationAttributesCore&);

  CanvasRenderingContext::ContextType GetContextType() const override {
    return CanvasRenderingContext::kContextWebgl;
  }
  ImageBitmap* TransferToImageBitmap(ScriptState*) final;
  String ContextName() const override { return "WebGLRenderingContext"; }
  void RegisterContextExtensions() override;
  void SetCanvasGetContextResult(RenderingContext&) final;
  void SetOffscreenCanvasGetContextResult(OffscreenRenderingContext&) final;

  void Trace(blink::Visitor*) override;

 private:
  // Enabled extension objects.
  Member<ANGLEInstancedArrays> angle_instanced_arrays_;
  Member<EXTBlendMinMax> ext_blend_min_max_;
  Member<EXTColorBufferHalfFloat> ext_color_buffer_half_float_;
  Member<EXTDisjointTimerQuery> ext_disjoint_timer_query_;
  Member<EXTFloatBlend> ext_float_blend_;
  Member<EXTFragDepth> ext_frag_depth_;
  Member<EXTShaderTextureLOD> ext_shader_texture_lod_;
  Member<EXTTextureFilterAnisotropic> ext_texture_filter_anisotropic_;
  Member<EXTsRGB> exts_rgb_;
  Member<KHRParallelShaderCompile> khr_parallel_shader_compile_;
  Member<OESElementIndexUint> oes_element_index_uint_;
  Member<OESFboRenderMipmap> oes_fbo_render_mipmap_;
  Member<OESStandardDerivatives> oes_standard_derivatives_;
  Member<OESTextureFloat> oes_texture_float_;
  Member<OESTextureFloatLinear> oes_texture_float_linear_;
  Member<OESTextureHalfFloat> oes_texture_half_float_;
  Member<OESTextureHalfFloatLinear> oes_texture_half_float_linear_;
  Member<OESVertexArrayObject> oes_vertex_array_object_;
  Member<WebGLColorBufferFloat> webgl_color_buffer_float_;
  Member<WebGLCompressedTextureASTC> webgl_compressed_texture_astc_;
  Member<WebGLCompressedTextureETC> webgl_compressed_texture_etc_;
  Member<WebGLCompressedTextureETC1> webgl_compressed_texture_etc1_;
  Member<WebGLCompressedTexturePVRTC> webgl_compressed_texture_pvrtc_;
  Member<WebGLCompressedTextureS3TC> webgl_compressed_texture_s3tc_;
  Member<WebGLCompressedTextureS3TCsRGB> webgl_compressed_texture_s3tc_srgb_;
  Member<WebGLDebugRendererInfo> webgl_debug_renderer_info_;
  Member<WebGLDebugShaders> webgl_debug_shaders_;
  Member<WebGLDepthTexture> webgl_depth_texture_;
  Member<WebGLDrawBuffers> webgl_draw_buffers_;
  Member<WebGLLoseContext> webgl_lose_context_;
  Member<WebGLMultiDraw> webgl_multi_draw_;
  Member<WebGLVideoTexture> webgl_video_texture_;
};

DEFINE_TYPE_CASTS(WebGLRenderingContext,
                  CanvasRenderingContext,
                  context,
                  context->Is3d() &&
                      WebGLRenderingContextBase::GetWebGLVersion(context) ==
                          Platform::kWebGL1ContextType,
                  context.Is3d() &&
                      WebGLRenderingContextBase::GetWebGLVersion(&context) ==
                          Platform::kWebGL1ContextType);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_RENDERING_CONTEXT_H_
