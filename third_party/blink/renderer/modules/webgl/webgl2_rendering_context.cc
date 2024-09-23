// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/webgl2_rendering_context.h"

#include <memory>

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_canvasrenderingcontext2d_gpucanvascontext_imagebitmaprenderingcontext_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpucanvascontext_imagebitmaprenderingcontext_offscreencanvasrenderingcontext2d_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/modules/webgl/ext_clip_control.h"
#include "third_party/blink/renderer/modules/webgl/ext_color_buffer_float.h"
#include "third_party/blink/renderer/modules/webgl/ext_color_buffer_half_float.h"
#include "third_party/blink/renderer/modules/webgl/ext_conservative_depth.h"
#include "third_party/blink/renderer/modules/webgl/ext_depth_clamp.h"
#include "third_party/blink/renderer/modules/webgl/ext_disjoint_timer_query_webgl2.h"
#include "third_party/blink/renderer/modules/webgl/ext_float_blend.h"
#include "third_party/blink/renderer/modules/webgl/ext_polygon_offset_clamp.h"
#include "third_party/blink/renderer/modules/webgl/ext_render_snorm.h"
#include "third_party/blink/renderer/modules/webgl/ext_texture_compression_bptc.h"
#include "third_party/blink/renderer/modules/webgl/ext_texture_compression_rgtc.h"
#include "third_party/blink/renderer/modules/webgl/ext_texture_filter_anisotropic.h"
#include "third_party/blink/renderer/modules/webgl/ext_texture_mirror_clamp_to_edge.h"
#include "third_party/blink/renderer/modules/webgl/ext_texture_norm_16.h"
#include "third_party/blink/renderer/modules/webgl/khr_parallel_shader_compile.h"
#include "third_party/blink/renderer/modules/webgl/nv_shader_noperspective_interpolation.h"
#include "third_party/blink/renderer/modules/webgl/oes_draw_buffers_indexed.h"
#include "third_party/blink/renderer/modules/webgl/oes_sample_variables.h"
#include "third_party/blink/renderer/modules/webgl/oes_shader_multisample_interpolation.h"
#include "third_party/blink/renderer/modules/webgl/oes_texture_float_linear.h"
#include "third_party/blink/renderer/modules/webgl/ovr_multiview_2.h"
#include "third_party/blink/renderer/modules/webgl/webgl_blend_func_extended.h"
#include "third_party/blink/renderer/modules/webgl/webgl_clip_cull_distance.h"
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
#include "third_party/blink/renderer/modules/webgl/webgl_polygon_mode.h"
#include "third_party/blink/renderer/modules/webgl/webgl_provoking_vertex.h"
#include "third_party/blink/renderer/modules/webgl/webgl_render_shared_exponent.h"
#include "third_party/blink/renderer/modules/webgl/webgl_shader_pixel_local_storage.h"
#include "third_party/blink/renderer/modules/webgl/webgl_stencil_texturing.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer.h"

namespace blink {

class ExceptionState;

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
  // Create a copy of attrs so flags can be modified if needed before passing
  // into the WebGL2RenderingContext constructor.
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
          host, attribs, Platform::kWebGL2ContextType, &graphics_info));
  if (!ShouldCreateContext(context_provider.get(), host))
    return nullptr;
  WebGL2RenderingContext* rendering_context =
      MakeGarbageCollected<WebGL2RenderingContext>(
          host, std::move(context_provider), graphics_info, attribs);

  if (!rendering_context->GetDrawingBuffer()) {
    host->HostDispatchEvent(
        WebGLContextEvent::Create(event_type_names::kWebglcontextcreationerror,
                                  "Could not create a WebGL2 context."));
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

void WebGL2RenderingContext::Factory::OnError(HTMLCanvasElement* canvas,
                                              const String& error) {
  canvas->DispatchEvent(*WebGLContextEvent::Create(
      event_type_names::kWebglcontextcreationerror, error));
}

WebGL2RenderingContext::WebGL2RenderingContext(
    CanvasRenderingContextHost* host,
    std::unique_ptr<WebGraphicsContext3DProvider> context_provider,
    const Platform::GraphicsInfo& graphics_info,
    const CanvasContextCreationAttributesCore& requested_attributes)
    : WebGL2RenderingContextBase(host,
                                 std::move(context_provider),
                                 graphics_info,
                                 requested_attributes,
                                 Platform::kWebGL2ContextType) {}

V8RenderingContext* WebGL2RenderingContext::AsV8RenderingContext() {
  return MakeGarbageCollected<V8RenderingContext>(this);
}

V8OffscreenRenderingContext*
WebGL2RenderingContext::AsV8OffscreenRenderingContext() {
  return MakeGarbageCollected<V8OffscreenRenderingContext>(this);
}

ImageBitmap* WebGL2RenderingContext::TransferToImageBitmap(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return TransferToImageBitmapBase(script_state);
}

void WebGL2RenderingContext::RegisterContextExtensions() {
  // Register extensions.
  RegisterExtension<EXTClipControl>();
  RegisterExtension<EXTColorBufferFloat>();
  RegisterExtension<EXTColorBufferHalfFloat>();
  RegisterExtension<EXTConservativeDepth>();
  RegisterExtension<EXTDepthClamp>();
  RegisterExtension<EXTDisjointTimerQueryWebGL2>(
      TimerQueryExtensionsEnabled() ? kApprovedExtension : kDeveloperExtension);
  RegisterExtension<EXTFloatBlend>();
  RegisterExtension<EXTPolygonOffsetClamp>();
  RegisterExtension<EXTRenderSnorm>();
  RegisterExtension<EXTTextureCompressionBPTC>();
  RegisterExtension<EXTTextureCompressionRGTC>();
  RegisterExtension<EXTTextureFilterAnisotropic>();
  RegisterExtension<EXTTextureMirrorClampToEdge>();
  RegisterExtension<EXTTextureNorm16>();
  RegisterExtension<KHRParallelShaderCompile>();
  RegisterExtension<NVShaderNoperspectiveInterpolation>();
  RegisterExtension<OESDrawBuffersIndexed>();
  RegisterExtension<OESSampleVariables>();
  RegisterExtension<OESShaderMultisampleInterpolation>();
  RegisterExtension<OESTextureFloatLinear>();
  RegisterExtension<OVRMultiview2>();
  RegisterExtension<WebGLBlendFuncExtended>();
  RegisterExtension<WebGLClipCullDistance>();
  RegisterExtension<WebGLCompressedTextureASTC>();
  RegisterExtension<WebGLCompressedTextureETC>();
  RegisterExtension<WebGLCompressedTextureETC1>();
  RegisterExtension<WebGLCompressedTexturePVRTC>();
  RegisterExtension<WebGLCompressedTextureS3TC>();
  RegisterExtension<WebGLCompressedTextureS3TCsRGB>();
  RegisterExtension<WebGLDebugRendererInfo>();
  RegisterExtension<WebGLDebugShaders>();
  RegisterExtension<WebGLDrawInstancedBaseVertexBaseInstance>(kDraftExtension);
  RegisterExtension<WebGLLoseContext>();
  RegisterExtension<WebGLMultiDraw>();
  RegisterExtension<WebGLMultiDrawInstancedBaseVertexBaseInstance>(
      kDraftExtension);
  RegisterExtension<WebGLPolygonMode>();
  RegisterExtension<WebGLProvokingVertex>();
  RegisterExtension<WebGLRenderSharedExponent>();
  RegisterExtension<WebGLShaderPixelLocalStorage>(kDraftExtension);
  RegisterExtension<WebGLStencilTexturing>();
}

}  // namespace blink
