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
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasrenderingcontext2d_gpucanvascontext_imagebitmaprenderingcontext_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpucanvascontext_imagebitmaprenderingcontext_offscreencanvasrenderingcontext2d_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/modules/webgl/angle_instanced_arrays.h"
#include "third_party/blink/renderer/modules/webgl/ext_blend_min_max.h"
#include "third_party/blink/renderer/modules/webgl/ext_clip_control.h"
#include "third_party/blink/renderer/modules/webgl/ext_color_buffer_half_float.h"
#include "third_party/blink/renderer/modules/webgl/ext_depth_clamp.h"
#include "third_party/blink/renderer/modules/webgl/ext_disjoint_timer_query.h"
#include "third_party/blink/renderer/modules/webgl/ext_float_blend.h"
#include "third_party/blink/renderer/modules/webgl/ext_frag_depth.h"
#include "third_party/blink/renderer/modules/webgl/ext_polygon_offset_clamp.h"
#include "third_party/blink/renderer/modules/webgl/ext_shader_texture_lod.h"
#include "third_party/blink/renderer/modules/webgl/ext_srgb.h"
#include "third_party/blink/renderer/modules/webgl/ext_texture_compression_bptc.h"
#include "third_party/blink/renderer/modules/webgl/ext_texture_compression_rgtc.h"
#include "third_party/blink/renderer/modules/webgl/ext_texture_filter_anisotropic.h"
#include "third_party/blink/renderer/modules/webgl/ext_texture_mirror_clamp_to_edge.h"
#include "third_party/blink/renderer/modules/webgl/khr_parallel_shader_compile.h"
#include "third_party/blink/renderer/modules/webgl/oes_element_index_uint.h"
#include "third_party/blink/renderer/modules/webgl/oes_fbo_render_mipmap.h"
#include "third_party/blink/renderer/modules/webgl/oes_standard_derivatives.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_float.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_float_linear.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_half_float.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_half_float_linear.h"
#include "third_party/blink/renderer/modules/webgl/oes_vertex_array_object.h"
#include "third_party/blink/renderer/modules/webgl/webgl_blend_func_extended.h"
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
#include "third_party/blink/renderer/modules/webgl/webgl_polygon_mode.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"

namespace blink {

class ExceptionState;

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
  // Create a copy of attrs so flags can be modified if needed before passing
  // into the WebGLRenderingContext constructor.
  CanvasContextCreationAttributesCore attribs = attrs;

  // The xr_compatible attribute needs to be handled before creating the context
  // because the GPU process may potentially be restarted in order to be XR
  // compatible. This scenario occurs if the GPU process is not using the GPU
  // that the VR headset is plugged into. If the GPU process is restarted, the
  // WebGraphicsContext3DProvider must be created using the new one.
  if (attribs.xr_compatible &&
      !WebGLRenderingContextBase::MakeXrCompatibleSync(host)) {
    // If xr compatibility is requested and we can't be xr compatible, return a
    // context with the flag set to false.
    attribs.xr_compatible = false;
  }

  Platform::GraphicsInfo graphics_info;
  std::unique_ptr<WebGraphicsContext3DProvider> context_provider(
      CreateWebGraphicsContext3DProvider(
          host, attribs, Platform::kWebGL1ContextType, &graphics_info));
  if (!ShouldCreateContext(context_provider.get()))
    return nullptr;

  WebGLRenderingContext* rendering_context =
      MakeGarbageCollected<WebGLRenderingContext>(
          host, std::move(context_provider), graphics_info, attribs);
  if (!rendering_context->GetDrawingBuffer()) {
    host->HostDispatchEvent(
        WebGLContextEvent::Create(event_type_names::kWebglcontextcreationerror,
                                  "Could not create a WebGL context."));
    // We must dispose immediately so that when rendering_context is
    // garbage-collected, it will not interfere with a subsequently created
    // rendering context.
    rendering_context->Dispose();
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
    const Platform::GraphicsInfo& graphics_info,
    const CanvasContextCreationAttributesCore& requested_attributes)
    : WebGLRenderingContextBase(host,
                                std::move(context_provider),
                                graphics_info,
                                requested_attributes,
                                Platform::kWebGL1ContextType) {}

V8RenderingContext* WebGLRenderingContext::AsV8RenderingContext() {
  return MakeGarbageCollected<V8RenderingContext>(this);
}

V8OffscreenRenderingContext*
WebGLRenderingContext::AsV8OffscreenRenderingContext() {
  return MakeGarbageCollected<V8OffscreenRenderingContext>(this);
}

ImageBitmap* WebGLRenderingContext::TransferToImageBitmap(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return TransferToImageBitmapBase(script_state);
}

void WebGLRenderingContext::RegisterContextExtensions() {
  RegisterExtension<ANGLEInstancedArrays>();
  RegisterExtension<EXTBlendMinMax>();
  RegisterExtension<EXTClipControl>();
  RegisterExtension<EXTColorBufferHalfFloat>();
  RegisterExtension<EXTDepthClamp>();
  RegisterExtension<EXTDisjointTimerQuery>(TimerQueryExtensionsEnabled()
                                           ? kApprovedExtension
                                           : kDeveloperExtension);
  RegisterExtension<EXTFloatBlend>();
  RegisterExtension<EXTFragDepth>();
  RegisterExtension<EXTPolygonOffsetClamp>();
  RegisterExtension<EXTShaderTextureLOD>();
  RegisterExtension<EXTTextureCompressionBPTC>();
  RegisterExtension<EXTTextureCompressionRGTC>();
  RegisterExtension<EXTTextureFilterAnisotropic>(kApprovedExtension);
  RegisterExtension<EXTTextureMirrorClampToEdge>();
  RegisterExtension<EXTsRGB>();
  RegisterExtension<KHRParallelShaderCompile>();
  RegisterExtension<OESElementIndexUint>();
  RegisterExtension<OESFboRenderMipmap>();
  RegisterExtension<OESStandardDerivatives>();
  RegisterExtension<OESTextureFloat>();
  RegisterExtension<OESTextureFloatLinear>();
  RegisterExtension<OESTextureHalfFloat>();
  RegisterExtension<OESTextureHalfFloatLinear>();
  RegisterExtension<OESVertexArrayObject>();
  RegisterExtension<WebGLBlendFuncExtended>();
  RegisterExtension<WebGLColorBufferFloat>();
  RegisterExtension<WebGLCompressedTextureASTC>();
  RegisterExtension<WebGLCompressedTextureETC>();
  RegisterExtension<WebGLCompressedTextureETC1>();
  RegisterExtension<WebGLCompressedTexturePVRTC>(kApprovedExtension);
  RegisterExtension<WebGLCompressedTextureS3TC>(kApprovedExtension);
  RegisterExtension<WebGLCompressedTextureS3TCsRGB>();
  RegisterExtension<WebGLDebugRendererInfo>();
  RegisterExtension<WebGLDebugShaders>();
  RegisterExtension<WebGLDepthTexture>(kApprovedExtension);
  RegisterExtension<WebGLDrawBuffers>();
  RegisterExtension<WebGLLoseContext>(kApprovedExtension);
  RegisterExtension<WebGLMultiDraw>();
  RegisterExtension<WebGLPolygonMode>();
}

}  // namespace blink
