// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl2_compute_rendering_context.h"

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
#include "third_party/blink/renderer/modules/webgl/oes_texture_float_linear.h"
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
#include "third_party/blink/renderer/modules/webgl/webgl_lose_context.h"
#include "third_party/blink/renderer/modules/webgl/webgl_video_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"

namespace blink {

// An helper function for the two create() methods. The return value is an
// indicate of whether the create() should return nullptr or not.
static bool ShouldCreateWebGL2ComputeContext(
    WebGraphicsContext3DProvider* context_provider,
    CanvasRenderingContextHost* host) {
  if (!context_provider) {
    host->HostDispatchEvent(WebGLContextEvent::Create(
        event_type_names::kWebglcontextcreationerror,
        "Failed to create a WebGL2 Compute context."));
    return false;
  }

  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();
  std::unique_ptr<Extensions3DUtil> extensions_util =
      Extensions3DUtil::Create(gl);
  if (!extensions_util)
    return false;
  if (extensions_util->SupportsExtension("GL_EXT_debug_marker")) {
    String context_label(
        String::Format("WebGL2ComputeRenderingContext-%p", context_provider));
    gl->PushGroupMarkerEXT(0, context_label.Ascii().c_str());
  }
  return true;
}

CanvasRenderingContext* WebGL2ComputeRenderingContext::Factory::Create(
    CanvasRenderingContextHost* host,
    const CanvasContextCreationAttributesCore& attrs) {
  bool using_gpu_compositing;
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider(
      CreateWebGraphicsContext3DProvider(host, attrs,
                                         Platform::kWebGL2ComputeContextType,
                                         &using_gpu_compositing));
  if (!ShouldCreateWebGL2ComputeContext(context_provider.get(), host))
    return nullptr;
  WebGL2ComputeRenderingContext* rendering_context =
      MakeGarbageCollected<WebGL2ComputeRenderingContext>(
          host, std::move(context_provider), using_gpu_compositing, attrs);

  if (!rendering_context->GetDrawingBuffer()) {
    host->HostDispatchEvent(WebGLContextEvent::Create(
        event_type_names::kWebglcontextcreationerror,
        "Could not create a WebGL2 Compute context."));
    return nullptr;
  }

  rendering_context->InitializeNewContext();
  rendering_context->RegisterContextExtensions();

  return rendering_context;
}

void WebGL2ComputeRenderingContext::Factory::OnError(HTMLCanvasElement* canvas,
                                                     const String& error) {
  canvas->DispatchEvent(*WebGLContextEvent::Create(
      event_type_names::kWebglcontextcreationerror, error));
}

WebGL2ComputeRenderingContext::WebGL2ComputeRenderingContext(
    CanvasRenderingContextHost* host,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    bool using_gpu_compositing,
    const CanvasContextCreationAttributesCore& requested_attributes)
    : WebGL2ComputeRenderingContextBase(host,
                                        std::move(context_provider),
                                        using_gpu_compositing,
                                        requested_attributes) {}

void WebGL2ComputeRenderingContext::SetCanvasGetContextResult(
    RenderingContext& result) {
  result.SetWebGL2ComputeRenderingContext(this);
}

void WebGL2ComputeRenderingContext::SetOffscreenCanvasGetContextResult(
    OffscreenRenderingContext& result) {
  result.SetWebGL2ComputeRenderingContext(this);
}

ImageBitmap* WebGL2ComputeRenderingContext::TransferToImageBitmap(
    ScriptState* script_state) {
  return TransferToImageBitmapBase(script_state);
}

void WebGL2ComputeRenderingContext::RegisterContextExtensions() {
  // Register extensions.
  RegisterExtension<EXTColorBufferFloat>(ext_color_buffer_float_);
  RegisterExtension<EXTDisjointTimerQueryWebGL2>(
      ext_disjoint_timer_query_web_gl2_);
  RegisterExtension<EXTFloatBlend>(ext_float_blend_);
  RegisterExtension<EXTTextureFilterAnisotropic>(
      ext_texture_filter_anisotropic_);
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
  RegisterExtension<WebGLLoseContext>(webgl_lose_context_);
  RegisterExtension<WebGLVideoTexture>(webgl_video_texture_, kDraftExtension);
}

void WebGL2ComputeRenderingContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(ext_color_buffer_float_);
  visitor->Trace(ext_disjoint_timer_query_web_gl2_);
  visitor->Trace(ext_float_blend_);
  visitor->Trace(ext_texture_filter_anisotropic_);
  visitor->Trace(oes_texture_float_linear_);
  visitor->Trace(webgl_compressed_texture_astc_);
  visitor->Trace(webgl_compressed_texture_etc_);
  visitor->Trace(webgl_compressed_texture_etc1_);
  visitor->Trace(webgl_compressed_texture_pvrtc_);
  visitor->Trace(webgl_compressed_texture_s3tc_);
  visitor->Trace(webgl_compressed_texture_s3tc_srgb_);
  visitor->Trace(webgl_debug_renderer_info_);
  visitor->Trace(webgl_debug_shaders_);
  visitor->Trace(webgl_lose_context_);
  visitor->Trace(webgl_video_texture_);
  WebGL2ComputeRenderingContextBase::Trace(visitor);
}

}  // namespace blink
