// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"

#include <memory>
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/offscreen_rendering_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/rendering_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/modules/webgl/ext_color_buffer_float.h"
#include "third_party/blink/renderer/modules/webgl/ext_disjoint_timer_query_webgl2.h"
#include "third_party/blink/renderer/modules/webgl/ext_float_blend.h"
#include "third_party/blink/renderer/modules/webgl/ext_texture_filter_anisotropic.h"
#include "third_party/blink/renderer/modules/webgl/khr_parallel_shader_compile.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_float_linear.h"
#include "third_party/blink/renderer/modules/webgl/ovr_multiview_2.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_astc.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_etc.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_etc1.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_pvrtc.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_s3tc.h"
#include "third_party/blink/renderer/modules/webgl/webgl_compressed_texture_s3tc_srgb.h"
#include "third_party/blink/renderer/modules/webgl/webgl_context_attribute_helpers.h"
#include "third_party/blink/renderer/modules/webgl/webgl_context_event.h"
#include "third_party/blink/renderer/modules/webgl/webgl_debug_renderer_info.h"
#include "third_party/blink/renderer/modules/webgl/webgl_debug_shaders.h"
#include "third_party/blink/renderer/modules/webgl/webgl_draw_instanced_base_vertex_base_instance.h"
#include "third_party/blink/renderer/modules/webgl/webgl_lose_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_multi_draw.h"
#include "third_party/blink/renderer/modules/webgl/webgl_multi_draw_instanced_base_vertex_base_instance.h"
#include "third_party/blink/renderer/modules/webgl/webgl_video_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"

namespace blink {

// An helper function for the two create() methods. The return value is an
// indicate of whether the create() should return nullptr or not.
static bool ShouldCreateContext(WebGraphicsContext3DProvider* context_provider,
                                CanvasRenderingContextHost* host) {
  if (!context_provider) {
    host->HostDispatchEvent(
        WebGLContextEvent::Create(event_type_names::kWebglcontextcreationerror,
                                  "Failed to create a WebGL2 context."));
    return false;
  }

  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();
  std::unique_ptr<Extensions3DUtil> extensions_util =
      Extensions3DUtil::Create(gl);
  if (!extensions_util)
    return false;
  if (extensions_util->SupportsExtension("GL_EXT_debug_marker")) {
    String context_label(
        String::Format("WebGL2RenderingContext-%p", context_provider));
    gl->PushGroupMarkerEXT(0, context_label.Ascii().c_str());
  }
  return true;
}

CanvasRenderingContext* WebGL2RenderingContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  bool using_gpu_compositing;
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider(
      CreateWebGraphicsContext3DProvider(
          host, attrs, Platform::kWebGL2ContextType, &using_gpu_compositing));
  if (!ShouldCreateContext(context_provider.get(), host))
    return nullptr;
  WebGL2RenderingContext* rendering_context =
      MakeGarbageCollected<WebGL2RenderingContext>(
          host, std::move(context_provider), using_gpu_compositing, attrs);

  if (!rendering_context->GetDrawingBuffer()) {
    host->HostDispatchEvent(
        WebGLContextEvent::Create(event_type_names::kWebglcontextcreationerror,
                                  "Could not create a WebGL2 context."));
    return nullptr;
  }

  rendering_context->InitializeNewContext();
  rendering_context->RegisterContextExtensions();

  return rendering_context;
}

void WebGL2RenderingContext::Factory::OnError(HTMLCanvasElement* canvas,
                                              const String& error) {
  canvas->DispatchEvent(*WebGLContextEvent::Create(
      event_type_names::kWebglcontextcreationerror, error));
}

WebGL2RenderingContext::WebGL2RenderingContext(
    CanvasRenderingContextHost* host,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    bool using_gpu_compositing,
    const CanvasContextCreationAttributesCore& requested_attributes)
    : WebGL2RenderingContextBase(host,
                                 std::move(context_provider),
                                 using_gpu_compositing,
                                 requested_attributes,
                                 Platform::kWebGL2ContextType) {}

void WebGL2RenderingContext::SetCanvasGetContextResult(
    RenderingContext& result) {
  result.SetWebGL2RenderingContext(this);
}

void WebGL2RenderingContext::SetOffscreenCanvasGetContextResult(
    OffscreenRenderingContext& result) {
  result.SetWebGL2RenderingContext(this);
}

ImageBitmap* WebGL2RenderingContext::TransferToImageBitmap(
    ScriptState* script_state) {
  return TransferToImageBitmapBase(script_state);
}

void WebGL2RenderingContext::RegisterContextExtensions() {
  // Register extensions.
  RegisterExtension<EXTColorBufferFloat>(ext_color_buffer_float_);
  RegisterExtension<EXTDisjointTimerQueryWebGL2>(
      ext_disjoint_timer_query_web_gl2_);
  RegisterExtension<EXTFloatBlend>(ext_float_blend_);
  RegisterExtension<EXTTextureFilterAnisotropic>(
      ext_texture_filter_anisotropic_);
  RegisterExtension<KHRParallelShaderCompile>(khr_parallel_shader_compile_);
  RegisterExtension<OESTextureFloatLinear>(oes_texture_float_linear_);
  RegisterExtension<WebGLCompressedTextureASTC>(webgl_compressed_texture_astc_);
  RegisterExtension<WebGLCompressedTextureETC>(webgl_compressed_texture_etc_);
  RegisterExtension<WebGLCompressedTextureETC1>(webgl_compressed_texture_etc1_);
  RegisterExtension<WebGLCompressedTexturePVRTC>(
      webgl_compressed_texture_pvrtc_);
  RegisterExtension<WebGLCompressedTextureS3TC>(webgl_compressed_texture_s3tc_);
  RegisterExtension<WebGLCompressedTextureS3TCsRGB>(
      webgl_compressed_texture_s3tc_srgb_);
  RegisterExtension<WebGLDebugRendererInfo>(webgl_debug_renderer_info_);
  RegisterExtension<WebGLDebugShaders>(webgl_debug_shaders_);
  RegisterExtension<WebGLDrawInstancedBaseVertexBaseInstance>(
      webgl_draw_instanced_base_vertex_base_instance_, kDraftExtension);
  RegisterExtension<WebGLLoseContext>(webgl_lose_context_);
  RegisterExtension<WebGLMultiDraw>(webgl_multi_draw_, kDraftExtension);
  RegisterExtension<WebGLMultiDrawInstancedBaseVertexBaseInstance>(
      webgl_multi_draw_instanced_base_vertex_base_instance_, kDraftExtension);
  RegisterExtension<WebGLVideoTexture>(webgl_video_texture_, kDraftExtension);
  RegisterExtension<OVRMultiview2>(ovr_multiview2_);
}

void WebGL2RenderingContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(ext_color_buffer_float_);
  visitor->Trace(ext_disjoint_timer_query_web_gl2_);
  visitor->Trace(ext_float_blend_);
  visitor->Trace(ext_texture_filter_anisotropic_);
  visitor->Trace(khr_parallel_shader_compile_);
  visitor->Trace(oes_texture_float_linear_);
  visitor->Trace(ovr_multiview2_);
  visitor->Trace(webgl_compressed_texture_astc_);
  visitor->Trace(webgl_compressed_texture_etc_);
  visitor->Trace(webgl_compressed_texture_etc1_);
  visitor->Trace(webgl_compressed_texture_pvrtc_);
  visitor->Trace(webgl_compressed_texture_s3tc_);
  visitor->Trace(webgl_compressed_texture_s3tc_srgb_);
  visitor->Trace(webgl_debug_renderer_info_);
  visitor->Trace(webgl_debug_shaders_);
  visitor->Trace(webgl_draw_instanced_base_vertex_base_instance_);
  visitor->Trace(webgl_lose_context_);
  visitor->Trace(webgl_multi_draw_);
  visitor->Trace(webgl_multi_draw_instanced_base_vertex_base_instance_);
  visitor->Trace(webgl_video_texture_);
  WebGL2RenderingContextBase::Trace(visitor);
}

}  // namespace blink
