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

#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context.h"

#include <memory>
#include "base/numerics/checked_math.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/offscreen_rendering_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/rendering_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/modules/webgl/angle_instanced_arrays.h"
#include "third_party/blink/renderer/modules/webgl/ext_blend_min_max.h"
#include "third_party/blink/renderer/modules/webgl/ext_color_buffer_half_float.h"
#include "third_party/blink/renderer/modules/webgl/ext_disjoint_timer_query.h"
#include "third_party/blink/renderer/modules/webgl/ext_float_blend.h"
#include "third_party/blink/renderer/modules/webgl/ext_frag_depth.h"
#include "third_party/blink/renderer/modules/webgl/ext_shader_texture_lod.h"
#include "third_party/blink/renderer/modules/webgl/ext_srgb.h"
#include "third_party/blink/renderer/modules/webgl/ext_texture_filter_anisotropic.h"
#include "third_party/blink/renderer/modules/webgl/khr_parallel_shader_compile.h"
#include "third_party/blink/renderer/modules/webgl/oes_element_index_uint.h"
#include "third_party/blink/renderer/modules/webgl/oes_fbo_render_mipmap.h"
#include "third_party/blink/renderer/modules/webgl/oes_standard_derivatives.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_float.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_float_linear.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_half_float.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_half_float_linear.h"
#include "third_party/blink/renderer/modules/webgl/oes_vertex_array_object.h"
#include "third_party/blink/renderer/modules/webgl/webgl_color_buffer_float.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_astc.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_etc.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_etc1.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_pvrtc.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_s3tc.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_s3tc_srgb.h"
#include "third_party/blink/renderer/modules/webgl/webgl_context_event.h"
#include "third_party/blink/renderer/modules/webgl/webgl_debug_renderer_info.h"
#include "third_party/blink/renderer/modules/webgl/webgl_debug_shaders.h"
#include "third_party/blink/renderer/modules/webgl/webgl_depth_texture.h"
#include "third_party/blink/renderer/modules/webgl/webgl_draw_buffers.h"
#include "third_party/blink/renderer/modules/webgl/webgl_lose_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_multi_draw.h"
#include "third_party/blink/renderer/modules/webgl/webgl_multi_draw_instanced_base_vertex_base_instance.h"
#include "third_party/blink/renderer/modules/webgl/webgl_video_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"

namespace blink {

// An helper function for the two create() methods. The return value is an
// indicate of whether the create() should return nullptr or not.
static bool ShouldCreateContext(
    WebGraphicsContext3DProvider* context_provider) {
  if (!context_provider)
    return false;
  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();
  std::unique_ptr<Extensions3DUtil> extensions_util =
      Extensions3DUtil::Create(gl);
  if (!extensions_util)
    return false;
  if (extensions_util->SupportsExtension("GL_EXT_debug_marker")) {
    String context_label(
        String::Format("WebGLRenderingContext-%p", context_provider));
    gl->PushGroupMarkerEXT(0, context_label.Ascii().c_str());
  }
  return true;
}

CanvasRenderingContext* WebGLRenderingContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  bool using_gpu_compositing;
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider(
      CreateWebGraphicsContext3DProvider(
          host, attrs, Platform::kWebGL1ContextType, &using_gpu_compositing));
  if (!ShouldCreateContext(context_provider.get()))
    return nullptr;

  WebGLRenderingContext* rendering_context =
      MakeGarbageCollected<WebGLRenderingContext>(
          host, std::move(context_provider), using_gpu_compositing, attrs);
  if (!rendering_context->GetDrawingBuffer()) {
    host->HostDispatchEvent(
        WebGLContextEvent::Create(event_type_names::kWebglcontextcreationerror,
                                  "Could not create a WebGL context."));
    return nullptr;
  }
  rendering_context->InitializeNewContext();
  rendering_context->RegisterContextExtensions();

  return rendering_context;
}

void WebGLRenderingContext::Factory::OnError(HTMLCanvasElement* canvas,
                                             const String& error) {
  canvas->DispatchEvent(*WebGLContextEvent::Create(
      event_type_names::kWebglcontextcreationerror, error));
}

WebGLRenderingContext::WebGLRenderingContext(
    CanvasRenderingContextHost* host,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    bool using_gpu_compositing,
    const CanvasContextCreationAttributesCore& requested_attributes)
    : WebGLRenderingContextBase(host,
                                std::move(context_provider),
                                using_gpu_compositing,
                                requested_attributes,
                                Platform::kWebGL1ContextType) {}

void WebGLRenderingContext::SetCanvasGetContextResult(
    RenderingContext& result) {
  result.SetWebGLRenderingContext(this);
}

void WebGLRenderingContext::SetOffscreenCanvasGetContextResult(
    OffscreenRenderingContext& result) {
  result.SetWebGLRenderingContext(this);
}

ImageBitmap* WebGLRenderingContext::TransferToImageBitmap(
    ScriptState* script_state) {
  return TransferToImageBitmapBase(script_state);
}

void WebGLRenderingContext::RegisterContextExtensions() {
  // Register extensions.
  static const char* const kBothPrefixes[] = {
      "", "WEBKIT_", nullptr,
  };

  RegisterExtension<ANGLEInstancedArrays>(angle_instanced_arrays_);
  RegisterExtension<EXTBlendMinMax>(ext_blend_min_max_);
  RegisterExtension<EXTColorBufferHalfFloat>(ext_color_buffer_half_float_);
  RegisterExtension<EXTDisjointTimerQuery>(ext_disjoint_timer_query_);
  RegisterExtension<EXTFloatBlend>(ext_float_blend_);
  RegisterExtension<EXTFragDepth>(ext_frag_depth_);
  RegisterExtension<EXTShaderTextureLOD>(ext_shader_texture_lod_);
  RegisterExtension<EXTTextureFilterAnisotropic>(
      ext_texture_filter_anisotropic_, kApprovedExtension, kBothPrefixes);
  RegisterExtension<EXTsRGB>(exts_rgb_);
  RegisterExtension<KHRParallelShaderCompile>(khr_parallel_shader_compile_);
  RegisterExtension<OESElementIndexUint>(oes_element_index_uint_);
  RegisterExtension<OESFboRenderMipmap>(oes_fbo_render_mipmap_);
  RegisterExtension<OESStandardDerivatives>(oes_standard_derivatives_);
  RegisterExtension<OESTextureFloat>(oes_texture_float_);
  RegisterExtension<OESTextureFloatLinear>(oes_texture_float_linear_);
  RegisterExtension<OESTextureHalfFloat>(oes_texture_half_float_);
  RegisterExtension<OESTextureHalfFloatLinear>(oes_texture_half_float_linear_);
  RegisterExtension<OESVertexArrayObject>(oes_vertex_array_object_);
  RegisterExtension<WebGLColorBufferFloat>(webgl_color_buffer_float_);
  RegisterExtension<WebGLCompressedTextureASTC>(webgl_compressed_texture_astc_);
  RegisterExtension<WebGLCompressedTextureETC>(webgl_compressed_texture_etc_);
  RegisterExtension<WebGLCompressedTextureETC1>(webgl_compressed_texture_etc1_);
  RegisterExtension<WebGLCompressedTexturePVRTC>(
      webgl_compressed_texture_pvrtc_, kApprovedExtension, kBothPrefixes);
  RegisterExtension<WebGLCompressedTextureS3TC>(
      webgl_compressed_texture_s3tc_, kApprovedExtension, kBothPrefixes);
  RegisterExtension<WebGLCompressedTextureS3TCsRGB>(
      webgl_compressed_texture_s3tc_srgb_);
  RegisterExtension<WebGLDebugRendererInfo>(webgl_debug_renderer_info_);
  RegisterExtension<WebGLDebugShaders>(webgl_debug_shaders_);
  RegisterExtension<WebGLDepthTexture>(webgl_depth_texture_, kApprovedExtension,
                                       kBothPrefixes);
  RegisterExtension<WebGLDrawBuffers>(webgl_draw_buffers_);
  RegisterExtension<WebGLLoseContext>(webgl_lose_context_, kApprovedExtension,
                                      kBothPrefixes);
  RegisterExtension<WebGLMultiDraw>(webgl_multi_draw_, kDraftExtension);
  RegisterExtension<WebGLVideoTexture>(webgl_video_texture_, kDraftExtension);
}

void WebGLRenderingContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(angle_instanced_arrays_);
  visitor->Trace(ext_blend_min_max_);
  visitor->Trace(ext_color_buffer_half_float_);
  visitor->Trace(ext_disjoint_timer_query_);
  visitor->Trace(ext_float_blend_);
  visitor->Trace(ext_frag_depth_);
  visitor->Trace(ext_shader_texture_lod_);
  visitor->Trace(ext_texture_filter_anisotropic_);
  visitor->Trace(exts_rgb_);
  visitor->Trace(khr_parallel_shader_compile_);
  visitor->Trace(oes_element_index_uint_);
  visitor->Trace(oes_fbo_render_mipmap_);
  visitor->Trace(oes_standard_derivatives_);
  visitor->Trace(oes_texture_float_);
  visitor->Trace(oes_texture_float_linear_);
  visitor->Trace(oes_texture_half_float_);
  visitor->Trace(oes_texture_half_float_linear_);
  visitor->Trace(oes_vertex_array_object_);
  visitor->Trace(webgl_color_buffer_float_);
  visitor->Trace(webgl_compressed_texture_astc_);
  visitor->Trace(webgl_compressed_texture_etc_);
  visitor->Trace(webgl_compressed_texture_etc1_);
  visitor->Trace(webgl_compressed_texture_pvrtc_);
  visitor->Trace(webgl_compressed_texture_s3tc_);
  visitor->Trace(webgl_compressed_texture_s3tc_srgb_);
  visitor->Trace(webgl_debug_renderer_info_);
  visitor->Trace(webgl_debug_shaders_);
  visitor->Trace(webgl_depth_texture_);
  visitor->Trace(webgl_draw_buffers_);
  visitor->Trace(webgl_lose_context_);
  visitor->Trace(webgl_multi_draw_);
  visitor->Trace(webgl_video_texture_);
  WebGLRenderingContextBase::Trace(visitor);
}

}  // namespace blink
