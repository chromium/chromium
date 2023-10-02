// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_RENDERING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_RENDERING_CONTEXT_H_

#include <memory>

#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context_base.h"

namespace blink {

class CanvasContextCreationAttributesCore;
class EXTBlendFuncExtended;
class EXTClipControl;
class EXTColorBufferFloat;
class EXTColorBufferHalfFloat;
class EXTConservativeDepth;
class EXTDepthClamp;
class EXTFloatBlend;
class EXTPolygonOffsetClamp;
class EXTRenderSnorm;
class EXTTextureCompressionBPTC;
class EXTTextureCompressionRGTC;
class EXTTextureFilterAnisotropic;
class EXTTextureMirrorClampToEdge;
class EXTTextureNorm16;
class KHRParallelShaderCompile;
class NVShaderNoperspectiveInterpolation;
class OESDrawBuffersIndexed;
class OESSampleVariables;
class OESShaderMultisampleInterpolation;
class OESTextureFloatLinear;
class OVRMultiview2;
class WebGLClipCullDistance;
class WebGLDebugRendererInfo;
class WebGLDrawInstancedBaseVertexBaseInstance;
class WebGLLoseContext;
class WebGLMultiDraw;
class WebGLMultiDrawInstancedBaseVertexBaseInstance;
class WebGLPolygonMode;
class WebGLProvokingVertex;
class WebGLRenderSharedExponent;
class WebGLShaderPixelLocalStorage;
class WebGLStencilTexturing;
class WebGLVideoTexture;

class WebGL2RenderingContext : public WebGL2RenderingContextBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
   public:
    Factory() = default;

    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

    ~Factory() override = default;

    CanvasRenderingContext* Create(
        CanvasRenderingContextHost*,
        const CanvasContextCreationAttributesCore&) override;
    CanvasRenderingContext::CanvasRenderingAPI GetRenderingAPI()
        const override {
      return CanvasRenderingContext::CanvasRenderingAPI::kWebgl2;
    }
    void OnError(HTMLCanvasElement*, const String& error) override;
  };

  WebGL2RenderingContext(
      CanvasRenderingContextHost*,
      std::unique_ptr<WebGraphicsContext3DProvider>,
      const Platform::GraphicsInfo&,
      const CanvasContextCreationAttributesCore& requested_attributes);

  ImageBitmap* TransferToImageBitmap(ScriptState*) final;
  String ContextName() const override { return "WebGL2RenderingContext"; }
  void RegisterContextExtensions() override;
  V8RenderingContext* AsV8RenderingContext() final;
  V8OffscreenRenderingContext* AsV8OffscreenRenderingContext() final;

  void Trace(Visitor*) const override;

 protected:
  Member<EXTBlendFuncExtended> ext_blend_func_extended_;
  Member<EXTClipControl> ext_clip_control_;
  Member<EXTColorBufferFloat> ext_color_buffer_float_;
  Member<EXTColorBufferHalfFloat> ext_color_buffer_half_float_;
  Member<EXTConservativeDepth> ext_conservative_depth_;
  Member<EXTDepthClamp> ext_depth_clamp_;
  Member<EXTDisjointTimerQueryWebGL2> ext_disjoint_timer_query_web_gl2_;
  Member<EXTFloatBlend> ext_float_blend_;
  Member<EXTPolygonOffsetClamp> ext_polygon_offset_clamp_;
  Member<EXTRenderSnorm> ext_render_snorm_;
  Member<EXTTextureCompressionBPTC> ext_texture_compression_bptc_;
  Member<EXTTextureCompressionRGTC> ext_texture_compression_rgtc_;
  Member<EXTTextureFilterAnisotropic> ext_texture_filter_anisotropic_;
  Member<EXTTextureMirrorClampToEdge> ext_texture_mirror_clamp_to_edge_;
  Member<EXTTextureNorm16> ext_texture_norm16_;
  Member<KHRParallelShaderCompile> khr_parallel_shader_compile_;
  Member<NVShaderNoperspectiveInterpolation>
      nv_shader_noperspective_interpolation_;
  Member<OESDrawBuffersIndexed> oes_draw_buffers_indexed_;
  Member<OESSampleVariables> oes_sample_variables_;
  Member<OESShaderMultisampleInterpolation>
      oes_shader_multisample_interpolation_;
  Member<OESTextureFloatLinear> oes_texture_float_linear_;
  Member<OVRMultiview2> ovr_multiview2_;
  Member<WebGLClipCullDistance> webgl_clip_cull_distance_;
  Member<WebGLCompressedTextureASTC> webgl_compressed_texture_astc_;
  Member<WebGLCompressedTextureETC> webgl_compressed_texture_etc_;
  Member<WebGLCompressedTextureETC1> webgl_compressed_texture_etc1_;
  Member<WebGLCompressedTexturePVRTC> webgl_compressed_texture_pvrtc_;
  Member<WebGLCompressedTextureS3TC> webgl_compressed_texture_s3tc_;
  Member<WebGLCompressedTextureS3TCsRGB> webgl_compressed_texture_s3tc_srgb_;
  Member<WebGLDebugRendererInfo> webgl_debug_renderer_info_;
  Member<WebGLDebugShaders> webgl_debug_shaders_;
  Member<WebGLDrawInstancedBaseVertexBaseInstance>
      webgl_draw_instanced_base_vertex_base_instance_;
  Member<WebGLLoseContext> webgl_lose_context_;
  Member<WebGLMultiDraw> webgl_multi_draw_;
  Member<WebGLMultiDrawInstancedBaseVertexBaseInstance>
      webgl_multi_draw_instanced_base_vertex_base_instance_;
  Member<WebGLPolygonMode> webgl_polygon_mode_;
  Member<WebGLProvokingVertex> webgl_provoking_vertex_;
  Member<WebGLRenderSharedExponent> webgl_render_shared_exponent_;
  Member<WebGLShaderPixelLocalStorage> webgl_shader_pixel_local_storage_;
  Member<WebGLStencilTexturing> webgl_stencil_texturing_;
  Member<WebGLVideoTexture> webgl_video_texture_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL2_RENDERING_CONTEXT_H_
