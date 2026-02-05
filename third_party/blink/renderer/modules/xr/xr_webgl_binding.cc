// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_binding.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_union_webgl2renderingcontext_webglrenderingcontext.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_cube_layer_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_cylinder_layer_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_equirect_layer_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_eye.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_projection_layer_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_quad_layer_init.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/webgl/webgl_texture.h"
#include "third_party/blink/renderer/modules/webgl/webgl_unowned_texture.h"
#include "third_party/blink/renderer/modules/xr/xr_camera.h"
#include "third_party/blink/renderer/modules/xr/xr_cube_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_cube_map.h"
#include "third_party/blink/renderer/modules/xr/xr_cylinder_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_equirect_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_light_probe.h"
#include "third_party/blink/renderer/modules/xr/xr_projection_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_quad_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_render_state.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/modules/xr/xr_viewer_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_cubemap_swap_chain.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_drawing_buffer_swap_chain.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_drawing_context.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_frame_transport_context_impl.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_sub_image.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_swap_chain.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_texture_array_swap_chain.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/extensions_3d_util.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

const double kMinScaleFactor = 0.2;

}  // namespace

XRWebGLBinding* XRWebGLBinding::Create(XRSession* session,
                                       const V8XRWebGLRenderingContext* context,
                                       ExceptionState& exception_state) {
  if (session->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRWebGLBinding for an "
                                      "XRSession which has already ended.");
    return nullptr;
  }

  if (!session->immersive()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRWebGLBinding for an "
                                      "inline XRSession.");
    return nullptr;
  }

  WebGLRenderingContextBase* webgl_context =
      webglRenderingContextBaseFromUnion(context);

  if (webgl_context->isContextLost()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRWebGLBinding with a "
                                      "lost WebGL context.");
    return nullptr;
  }

  if (!webgl_context->IsXRCompatible()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "WebGL context must be marked as XR compatible in order to "
        "use with an immersive XRSession");
    return nullptr;
  }

  if (session->GraphicsApi() != XRGraphicsBinding::Api::kWebGL) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot create an XRWebGLBinding with a WebGPU-based XRSession.");
    return nullptr;
  }

  return MakeGarbageCollected<XRWebGLBinding>(
      session, webgl_context, context->IsWebGL2RenderingContext());
}

XRWebGLBinding::XRWebGLBinding(XRSession* session,
                               WebGLRenderingContextBase* webgl_context,
                               bool webgl2)
    : XRGraphicsBinding(session),
      webgl_context_(webgl_context),
      webgl2_(webgl2),
      transport_delegate_(MakeGarbageCollected<XRWebGLFrameTransportDelegate>(
          MakeGarbageCollected<XRWebGLFrameTransportContextImpl>(
              webgl_context_))) {}

bool XRWebGLBinding::usesDepthValues() const {
  return false;
}

XRWebGLSwapChain* XRWebGLBinding::CreateColorSwapchain(
    GLenum layer_format,
    gfx::Size texture_size,
    V8XRTextureType texture_type,
    V8XRLayerLayout::Enum final_layout,
    bool clear_on_access) {
  XRWebGLSwapChain::Descriptor color_desc = {};
  color_desc.format = FormatForLayerFormat(layer_format);
  color_desc.internal_format = InternalFormatForLayerFormat(layer_format);
  color_desc.type = TypeForLayerFormat(layer_format);
  color_desc.attachment_target = GL_COLOR_ATTACHMENT0;
  color_desc.width = static_cast<uint32_t>(texture_size.width());
  color_desc.height = static_cast<uint32_t>(texture_size.height());
  color_desc.layers = 1;
  color_desc.is_texture_array = false;
  // If we use XRWebGLTextureArraySwapChain as a wrapper, we don't need to
  // clear the old buffer of the wrapped swapchain because the wrapper
  // will always overwrite the entire texture. The value of "clear_on_access"
  // will be passed and used by the wrapper.
  color_desc.clear_on_access =
      clear_on_access &&
      texture_type.AsEnum() != V8XRTextureType::Enum::kTextureArray;

  XRWebGLSwapChain* color_swap_chain;
  if (session()->xr()->frameProvider()->DrawingIntoSharedBuffer()) {
    color_swap_chain = MakeGarbageCollected<XRWebGLSharedImageSwapChain>(
        webgl_context_, color_desc, webgl2_);
  } else {
    color_swap_chain = MakeGarbageCollected<XRWebGLDrawingBufferSwapChain>(
        webgl_context_, color_desc, webgl2_);
  }

  if (texture_type.AsEnum() == V8XRTextureType::Enum::kTextureArray) {
    // If a texture-array was requested, create a texture array wrapper for the
    // side-by-side swap chain.
    // TODO(crbug.com/359418629): Remove once array SharedImages are available.
    const size_t layers = final_layout == V8XRLayerLayout::Enum::kStereo
                              ? session()->array_texture_layers()
                              : 1;
    color_swap_chain = MakeGarbageCollected<XRWebGLTextureArraySwapChain>(
        color_swap_chain, layers, clear_on_access);
  }

  return color_swap_chain;
}

XRProjectionLayer* XRWebGLBinding::createProjectionLayer(
    const XRProjectionLayerInit* init,
    ExceptionState& exception_state) {
  if (!ValidateSessionAndContext(exception_state) ||
      !ValidateLayerColorFormat(init->colorFormat(), exception_state) ||
      !ValidateLayerDepthStencilFormat(init->depthFormat(), exception_state)) {
    return nullptr;
  }

  const bool is_texture_array =
      init->textureType().AsEnum() == V8XRTextureType::Enum::kTextureArray;

  if (is_texture_array && !webgl2_) {
    exception_state.ThrowTypeError(
        "textureType of 'texture-array' is only available with WebGL 2 "
        "contexts.");
    return nullptr;
  }

  // The max size will be either the native resolution or the default
  // if that happens to be larger than the native res. (That can happen on
  // desktop systems.)
  double max_scale = std::max(session()->NativeFramebufferScale(), 1.0);

  // Clamp the developer-requested framebuffer scale to ensure it's not too
  // small to see or unreasonably large.
  double scale_factor =
      std::clamp(init->scaleFactor(), kMinScaleFactor, max_scale);
  gfx::SizeF scaled_size =
      gfx::ScaleSize(session()->RecommendedArrayTextureSize(), scale_factor);

  V8XRLayerLayout::Enum final_layout =
      DetermineLayout(V8XRLayerLayout(V8XRLayerLayout::Enum::kDefault),
                      init->textureType().AsEnum());

  // We have only one layer unless the layout is "stereo".
  const size_t layers = final_layout == V8XRLayerLayout::Enum::kStereo
                            ? session()->array_texture_layers()
                            : 1;

  scaled_size.set_width(scaled_size.width() *
                        GetHorizontalViewCount(final_layout));
  scaled_size.set_height(scaled_size.height() *
                         GetVerticalViewCount(final_layout));

  // TODO(crbug.com/359418629): Remove once array Mailboxes are available.
  scaled_size.set_width(scaled_size.width() * layers);

  // If the scaled texture dimensions are larger than the max texture dimension
  // for the context scale it down till it fits.
  GLint max_texture_size = 0;
  webgl_context_->ContextGL()->GetIntegerv(GL_MAX_TEXTURE_SIZE,
                                           &max_texture_size);
  if (scaled_size.width() > max_texture_size ||
      scaled_size.height() > max_texture_size) {
    double max_dimension = std::max(scaled_size.width(), scaled_size.height());
    scaled_size = gfx::ScaleSize(scaled_size, max_texture_size / max_dimension);
  }

  gfx::Size texture_size = gfx::ToFlooredSize(scaled_size);

  XRWebGLSwapChain* color_swap_chain = CreateColorSwapchain(
      init->colorFormat(), texture_size, init->textureType(), final_layout,
      init->clearOnAccess());

  CHECK_EQ(color_swap_chain->descriptor().is_texture_array, is_texture_array);
  CHECK_EQ(color_swap_chain->descriptor().layers, layers);

  XRWebGLSwapChain* depth_stencil_swap_chain = nullptr;
  if (init->depthFormat() != GL_NONE) {
    XRWebGLSwapChain::Descriptor depth_stencil_desc = {};
    depth_stencil_desc.format = FormatForLayerFormat(init->depthFormat());
    depth_stencil_desc.internal_format =
        InternalFormatForLayerFormat(init->depthFormat());
    depth_stencil_desc.type = TypeForLayerFormat(init->depthFormat());
    depth_stencil_desc.attachment_target = GL_DEPTH_ATTACHMENT;
    depth_stencil_desc.is_texture_array = is_texture_array;
    depth_stencil_desc.clear_on_access = init->clearOnAccess();

    if (is_texture_array) {
      texture_size.set_width(texture_size.width() / layers);
      depth_stencil_desc.layers = layers;
    } else {
      depth_stencil_desc.layers = 1;
    }

    depth_stencil_desc.width = static_cast<uint32_t>(texture_size.width());
    depth_stencil_desc.height = static_cast<uint32_t>(texture_size.height());

    depth_stencil_swap_chain = MakeGarbageCollected<XRWebGLStaticSwapChain>(
        webgl_context_, depth_stencil_desc, webgl2_);
  }

  auto* drawing_context = MakeGarbageCollected<XRWebGLDrawingContext>(
      this, color_swap_chain, depth_stencil_swap_chain);

  return MakeGarbageCollected<XRProjectionLayer>(this, drawing_context,
                                                 final_layout);
}

XRQuadLayer* XRWebGLBinding::createQuadLayer(const XRQuadLayerInit* init,
                                             ExceptionState& exception_state) {
  if (!CanCreateShapedLayer(init, exception_state) ||
      !ValidateShapedLayerTextureType(init->textureType(), exception_state)) {
    return nullptr;
  }

  if (init->layout().AsEnum() == V8XRLayerLayout::Enum::kDefault) {
    exception_state.ThrowTypeError("Invalid layout type.");
    return nullptr;
  }

  V8XRLayerLayout::Enum final_layout =
      DetermineLayout(init->layout(), init->textureType().AsEnum());
  if (!ValidateTextureSize(init, final_layout, exception_state)) {
    return nullptr;
  }

  // Check that width and height are greater than 0.f
  if (!init->hasWidth() ||
      init->width() <= std::numeric_limits<float>::epsilon()) {
    exception_state.ThrowTypeError(
        "width is required and must be greater than epsilon.");
    return nullptr;
  }
  if (!init->hasHeight() ||
      init->height() <= std::numeric_limits<float>::epsilon()) {
    exception_state.ThrowTypeError(
        "height is required and must be greater than epsilon.");
    return nullptr;
  }

  XRWebGLSwapChain* color_swap_chain = CreateColorSwapchain(
      init->colorFormat(), GetTextureSizeForLayer(init, final_layout),
      init->textureType(), final_layout, init->clearOnAccess());

  auto* drawing_context =
      MakeGarbageCollected<XRWebGLDrawingContext>(this, color_swap_chain);

  return MakeGarbageCollected<XRQuadLayer>(init, final_layout, this,
                                           drawing_context);
}

XRCylinderLayer* XRWebGLBinding::createCylinderLayer(
    const XRCylinderLayerInit* init,
    ExceptionState& exception_state) {
  if (!CanCreateShapedLayer(init, exception_state) ||
      !ValidateShapedLayerTextureType(init->textureType(), exception_state)) {
    return nullptr;
  }

  if (init->layout().AsEnum() == V8XRLayerLayout::Enum::kDefault) {
    exception_state.ThrowTypeError("Invalid layout type.");
    return nullptr;
  }

  V8XRLayerLayout::Enum final_layout =
      DetermineLayout(init->layout(), init->textureType().AsEnum());
  if (!ValidateTextureSize(init, final_layout, exception_state)) {
    return nullptr;
  }

  if (!init->hasRadius() || init->radius() < 0.f) {
    exception_state.ThrowTypeError(
        "radius is required and must be greater than or equal to zero.");
    return nullptr;
  }

  if (!init->hasAspectRatio() ||
      init->aspectRatio() < std::numeric_limits<float>::epsilon()) {
    exception_state.ThrowTypeError(
        "aspectRatio is required and must be greater than or equal to "
        "epsilon.");
    return nullptr;
  }

  if (!init->hasCentralAngle() || init->centralAngle() < 0.f ||
      init->centralAngle() > kTwoPiFloat) {
    exception_state.ThrowTypeError(
        "The central angle is required and must be in the range [0.f, "
        "2pi].");
    return nullptr;
  }

  XRWebGLSwapChain* color_swap_chain = CreateColorSwapchain(
      init->colorFormat(), GetTextureSizeForLayer(init, final_layout),
      init->textureType(), final_layout, init->clearOnAccess());

  auto* drawing_context =
      MakeGarbageCollected<XRWebGLDrawingContext>(this, color_swap_chain);

  return MakeGarbageCollected<XRCylinderLayer>(init, final_layout, this,
                                               drawing_context);
}

XREquirectLayer* XRWebGLBinding::createEquirectLayer(
    const XREquirectLayerInit* init,
    ExceptionState& exception_state) {
  if (!CanCreateShapedLayer(init, exception_state) ||
      !ValidateShapedLayerTextureType(init->textureType(), exception_state)) {
    return nullptr;
  }

  if (init->layout().AsEnum() == V8XRLayerLayout::Enum::kDefault) {
    exception_state.ThrowTypeError("Invalid layout type.");
    return nullptr;
  }

  V8XRLayerLayout::Enum final_layout =
      DetermineLayout(init->layout(), init->textureType().AsEnum());
  if (!ValidateTextureSize(init, final_layout, exception_state)) {
    return nullptr;
  }

  // Validating parameters specific to XREquirectLayer.
  auto* space = DynamicTo<XRReferenceSpace>(init->space());
  if (!space) {
    exception_state.ThrowTypeError(
        "The 'space' parameter must be an XRReferenceSpace.");
    return nullptr;
  }

  if (!space->IsStationary()) {
    exception_state.ThrowTypeError(
        "The 'space' parameter cannot be of type 'viewer'.");
    return nullptr;
  }

  XRWebGLSwapChain* color_swap_chain = CreateColorSwapchain(
      init->colorFormat(), GetTextureSizeForLayer(init, final_layout),
      init->textureType(), final_layout, init->clearOnAccess());

  auto* drawing_context =
      MakeGarbageCollected<XRWebGLDrawingContext>(this, color_swap_chain);

  return MakeGarbageCollected<XREquirectLayer>(init, final_layout, this,
                                               drawing_context);
}

XRCubeLayer* XRWebGLBinding::createCubeLayer(const XRCubeLayerInit* init,
                                             ExceptionState& exception_state) {
  if (!CanCreateShapedLayer(init, exception_state)) {
    return nullptr;
  }

  // A cube layer can only use mono or stereo layout. For now, we only
  // support mono layout.
  if (init->layout().AsEnum() != V8XRLayerLayout::Enum::kMono) {
    exception_state.ThrowTypeError("Invalid layout type.");
    return nullptr;
  }

  // Validating parameters specific to XRCubeLayer.
  if (init->viewPixelWidth() != init->viewPixelHeight()) {
    exception_state.ThrowTypeError(
        "Cube face must be square (width == height).");
    return nullptr;
  }

  GLint max_texture_size = 0;
  webgl_context_->ContextGL()->GetIntegerv(GL_MAX_TEXTURE_SIZE,
                                           &max_texture_size);

  // We transfer 6 cube faces with a single texture 2D, where faces are placed
  // 3 per row, 2 rows in total.
  if (init->viewPixelHeight() > static_cast<uint32_t>(max_texture_size) / 2) {
    exception_state.ThrowTypeError(
        "ViewPixelHeight exceeds the maximum texture size.");
    return nullptr;
  }

  if (init->viewPixelWidth() > static_cast<uint32_t>(max_texture_size) / 3) {
    exception_state.ThrowTypeError(
        "ViewPixelWidth exceeds the maximum texture size.");
    return nullptr;
  }

  // We don't need to clear the buffer anyway because the wrapper
  // XRWebGLCubemapSwapChain will do it.
  XRWebGLSwapChain* texture_2d_swapchain = CreateColorSwapchain(
      init->colorFormat(),
      gfx::Size(init->viewPixelWidth(), init->viewPixelHeight()),
      V8XRTextureType(V8XRTextureType::Enum::kTexture),
      V8XRLayerLayout::Enum::kMono, false /*clear_on_access*/);

  XRWebGLSwapChain* cubemap_swap_chain =
      MakeGarbageCollected<XRWebGLCubemapSwapChain>(texture_2d_swapchain,
                                                    init->clearOnAccess());

  auto* drawing_context =
      MakeGarbageCollected<XRWebGLDrawingContext>(this, cubemap_swap_chain);

  return MakeGarbageCollected<XRCubeLayer>(init, V8XRLayerLayout::Enum::kMono,
                                           this, drawing_context);
}

gfx::Size XRWebGLBinding::GetTextureSizeForLayer(
    const XRLayerInit* init,
    V8XRLayerLayout::Enum final_layout) const {
  return gfx::Size(
      init->viewPixelWidth() * GetHorizontalViewCount(final_layout),
      init->viewPixelHeight() * GetVerticalViewCount(final_layout));
}

gfx::Rect XRWebGLBinding::GetViewportForLayer(const XRCompositionLayer& layer,
                                              V8XREye eye) const {
  uint32_t width =
      layer.textureWidth() / GetHorizontalViewCount(layer.layout().AsEnum());
  uint32_t height =
      layer.textureHeight() / GetVerticalViewCount(layer.layout().AsEnum());

  if (eye == V8XREye::Enum::kRight &&
      (layer.layout() == V8XRLayerLayout::Enum::kStereoTopBottom ||
       layer.layout() == V8XRLayerLayout::Enum::kStereoLeftRight)) {
    return gfx::Rect(
        (layer.layout() == V8XRLayerLayout::Enum::kStereoTopBottom) ? 0 : width,
        (layer.layout() == V8XRLayerLayout::Enum::kStereoLeftRight) ? 0
                                                                    : height,
        width, height);
  }
  return gfx::Rect(0, 0, width, height);
}

XRWebGLSubImage* XRWebGLBinding::getViewSubImage(
    XRProjectionLayer* layer,
    XRView* view,
    ExceptionState& exception_state) {
  CHECK(layer);
  CHECK(view);

  if (!view || view->session() != session()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "View was not created with the same session as this binding.");
    return nullptr;
  }

  if (!session()->renderState()->HasLayer(layer)) {
    exception_state.ThrowTypeError(
        "Layer is not included in the active render state.");
    return nullptr;
  }

  XRViewData* viewData = view->ViewData();
  if (viewData->ApplyViewportScaleForFrame()) {
    layer->SetModified(true);
  }

  gfx::Rect viewport = GetViewportForView(layer, viewData);

  // The layer passed the session check, confirming it can only contain
  // a WebGL drawing context. This makes the static_cast safe.
  auto* drawing_context =
      static_cast<XRWebGLDrawingContext*>(layer->drawing_context());

  return MakeGarbageCollected<XRWebGLSubImage>(
      viewport, viewData->index(), drawing_context->color_swap_chain(),
      drawing_context->depth_stencil_swap_chain(),
      /*motion_vector_swap_chain=*/nullptr);
}

XRWebGLSubImage* XRWebGLBinding::getSubImage(XRCompositionLayer* layer,
                                             XRFrame* frame,
                                             V8XREye eye,
                                             ExceptionState& exception_state) {
  CHECK(layer);
  CHECK(frame);
  if (!ValidateSessionAndContext(exception_state)) {
    return nullptr;
  }

  if (layer->session() != session()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Layer does not belong to current session.");
    return nullptr;
  }

  // Check the frame state.
  if (frame->session() != session() || !frame->IsActive() ||
      !frame->IsAnimationFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid frame state.");
    return nullptr;
  }

  if (!session()->renderState()->HasLayer(layer)) {
    exception_state.ThrowTypeError(
        "Layer is not included in the active render state.");
    return nullptr;
  }

  if (layer->LayerType() == XRLayerType::kProjectionLayer) {
    exception_state.ThrowTypeError("Invalid layer type.");
    return nullptr;
  }

  if (layer->layout() == V8XRLayerLayout::Enum::kDefault) {
    exception_state.ThrowTypeError("Invalid layer's layout type.");
    return nullptr;
  }

  if (layer->layout() == V8XRLayerLayout::Enum::kStereo) {
    if (eye == V8XREye::Enum::kNone) {
      exception_state.ThrowTypeError(
          "The 'eye' parameter cannot be 'none' for the stereo layout.");
      return nullptr;
    }
  }

  // The layer passed the session check, confirming it can only contain
  // a WebGL drawing context. This makes the static_cast safe.
  auto* drawing_context =
      static_cast<XRWebGLDrawingContext*>(layer->drawing_context());

  return MakeGarbageCollected<XRWebGLSubImage>(
      GetViewportForLayer(*layer, eye), 0, drawing_context->color_swap_chain(),
      drawing_context->depth_stencil_swap_chain(),
      /*motion_vector_swap_chain=*/nullptr);
}

WebGLTexture* XRWebGLBinding::getReflectionCubeMap(
    XRLightProbe* light_probe,
    ExceptionState& exception_state) {
  GLenum internal_format, format, type;

  if (webgl_context_->isContextLost()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot get reflection cube map with a lost context.");
    return nullptr;
  }

  if (session()->ended()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot get a reflection cube map for a session which has ended.");
    return nullptr;
  }

  if (session() != light_probe->session()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "LightProbe comes from a different session than this binding");
    return nullptr;
  }

  // Determine the internal_format, format, and type that will be passed to
  // glTexImage2D for each possible light probe reflection format. The formats
  // will differ depending on whether we're using WebGL 2 or WebGL 1 with
  // extensions.
  // Note that at this point, since we know we have a valid lightProbe, we also
  // know that we support whatever reflectionFormat it was created with, as it
  // would not have been created otherwise.
  switch (light_probe->ReflectionFormat()) {
    case XRLightProbe::kReflectionFormatRGBA16F:
      if (!webgl2_ && !webgl_context_->ExtensionsUtil()->IsExtensionEnabled(
                          "GL_OES_texture_half_float")) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "WebGL contexts must have the OES_texture_half_float extension "
            "enabled "
            "prior to calling getReflectionCubeMap with a format of "
            "\"rgba16f\". "
            "This restriction does not apply to WebGL 2.0 contexts.");
        return nullptr;
      }

      internal_format = webgl2_ ? GL_RGBA16F : GL_RGBA;
      format = GL_RGBA;
      // Surprisingly GL_HALF_FLOAT and GL_HALF_FLOAT_OES have different values.
      type = webgl2_ ? GL_HALF_FLOAT : GL_HALF_FLOAT_OES;
      break;

    case XRLightProbe::kReflectionFormatSRGBA8:
      bool use_srgb =
          webgl2_ ||
          webgl_context_->ExtensionsUtil()->IsExtensionEnabled("GL_EXT_sRGB");

      if (use_srgb) {
        internal_format = webgl2_ ? GL_SRGB8_ALPHA8 : GL_SRGB_ALPHA_EXT;
      } else {
        internal_format = GL_RGBA;
      }

      format = webgl2_ ? GL_RGBA : internal_format;
      type = GL_UNSIGNED_BYTE;
      break;
  }

  XRCubeMap* cube_map = light_probe->getReflectionCubeMap();
  if (!cube_map) {
    return nullptr;
  }

  WebGLTexture* texture = MakeGarbageCollected<WebGLTexture>(webgl_context_);
  cube_map->updateWebGLEnvironmentCube(webgl_context_, texture, internal_format,
                                       format, type);

  return texture;
}

WebGLTexture* XRWebGLBinding::getCameraImage(XRCamera* camera,
                                             ExceptionState& exception_state) {
  DVLOG(3) << __func__;

  XRFrame* frame = camera->Frame();
  DCHECK(frame);

  XRSession* frame_session = frame->session();
  DCHECK(frame_session);

  if (!frame_session->IsFeatureEnabled(
          device::mojom::XRSessionFeature::CAMERA_ACCESS)) {
    DVLOG(2) << __func__ << ": raw camera access is not enabled on a session";
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        XRSession::kRawCameraAccessFeatureNotSupported);
    return nullptr;
  }

  if (!frame->IsActive()) {
    DVLOG(2) << __func__ << ": frame is not active";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRFrame::kInactiveFrame);
    return nullptr;
  }

  if (!frame->IsAnimationFrame()) {
    DVLOG(2) << __func__ << ": frame is not animating";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRFrame::kNonAnimationFrame);
    return nullptr;
  }

  if (session() != frame_session) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Camera comes from a different session than this binding");
    return nullptr;
  }

  XRWebGLLayer* base_layer = frame_session->renderState()->baseLayer();
  DCHECK(base_layer);

  // This resource is owned by the XRWebGLLayer, and is freed in OnFrameEnd();
  return base_layer->GetCameraTexture();
}

XRWebGLDepthInformation* XRWebGLBinding::getDepthInformation(
    XRView* view,
    ExceptionState& exception_state) {
  DVLOG(1) << __func__;

  XRFrame* frame = view->frame();

  if (session() != frame->session()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "View comes from a different session than this binding");
    return nullptr;
  }

  if (!session()->IsFeatureEnabled(device::mojom::XRSessionFeature::DEPTH)) {
    DVLOG(2) << __func__ << ": depth sensing is not enabled on a session";
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        XRSession::kDepthSensingFeatureNotSupported);
    return nullptr;
  }

  if (!frame->IsActive()) {
    DVLOG(2) << __func__ << ": frame is not active";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRFrame::kInactiveFrame);
    return nullptr;
  }

  if (!frame->IsAnimationFrame()) {
    DVLOG(2) << __func__ << ": frame is not animating";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRFrame::kNonAnimationFrame);
    return nullptr;
  }

  return view->GetWebGLDepthInformation(exception_state);
}

gfx::Rect XRWebGLBinding::GetViewportForView(XRProjectionLayer* layer,
                                             XRViewData* view) {
  // If the layer is not side-by-side return the full texture size adjusted by
  // the viewport scale.
  if (layer->textureArrayLength() > 1) {
    return gfx::Rect(0, 0, layer->textureWidth() * view->CurrentViewportScale(),
                     layer->textureHeight() * view->CurrentViewportScale());
  }

  // Otherwise the layer is side-by-side, so the viewports should be distributed
  // across the texture width.
  uint32_t viewport_width =
      layer->textureWidth() / session()->array_texture_layers();
  uint32_t viewport_offset = viewport_width * view->index();
  return gfx::Rect(viewport_offset, 0,
                   viewport_width * view->CurrentViewportScale(),
                   layer->textureHeight() * view->CurrentViewportScale());
}

XRFrameTransportDelegate* XRWebGLBinding::GetTransportDelegate() {
  return transport_delegate_;
}

bool XRWebGLBinding::ValidateSessionAndContext(
    ExceptionState& exception_state) {
  if (session()->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create a new layer for an "
                                      "XRSession which has already ended.");
    return false;
  }

  if (webgl_context_->isContextLost()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create a new layer with a lost "
                                      "WebGL context.");
    return false;
  }

  return true;
}

bool XRWebGLBinding::ValidateLayerColorFormat(GLenum color_format,
                                              ExceptionState& exception_state) {
  switch (color_format) {
    case GL_RGBA:
    case GL_RGB:
    case GL_SRGB_EXT:
    case GL_SRGB_ALPHA_EXT:
      return true;
    case GL_RGBA8:
    case GL_RGB8:
    case GL_SRGB8:
    case GL_SRGB8_ALPHA8:
      if (!webgl2_) {
        exception_state.ThrowTypeError(
            "Specified colorFormat only available with WebGL 2 contexts.");
        return false;
      }
      return true;
    default:
      exception_state.ThrowTypeError("Invalid colorFormat.");
      return false;
  }
}

bool XRWebGLBinding::ValidateLayerDepthStencilFormat(
    GLenum depth_stencil_format,
    ExceptionState& exception_state) {
  switch (depth_stencil_format) {
    case GL_NONE:
      return true;
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_STENCIL:
      if (!webgl2_ && !webgl_context_->ExtensionsUtil()->IsExtensionEnabled(
                          "WEBGL_depth_texture")) {
        exception_state.ThrowTypeError(
            "depthFormat can only be set with with WebGL 2 contexts or when "
            "WEBGL_depth_texture is enabled.");
        return false;
      }
      return true;
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH24_STENCIL8:
      if (!webgl2_) {
        exception_state.ThrowTypeError(
            "Specified depthFormat only available with WebGL 2 contexts.");
        return false;
      }
      return true;
    default:
      exception_state.ThrowTypeError(
          "depthFormat must be a valid depth format or GL_NONE.");
      return false;
  }
}

GLenum XRWebGLBinding::FormatForLayerFormat(GLenum layer_format) {
  switch (layer_format) {
    case GL_RGBA:
    case GL_RGBA8:
      return GL_RGBA;

    case GL_RGB:
    case GL_RGB8:
      return GL_RGB;

    case GL_SRGB_EXT:
    case GL_SRGB8:
      return GL_SRGB_EXT;

    case GL_SRGB_ALPHA_EXT:
    case GL_SRGB8_ALPHA8:
      return GL_SRGB_ALPHA_EXT;

    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT24:
      return GL_DEPTH_COMPONENT;

    case GL_DEPTH_STENCIL:
    case GL_DEPTH24_STENCIL8:
      return GL_DEPTH_STENCIL;

    default:
      NOTREACHED();
  }
}

GLenum XRWebGLBinding::InternalFormatForLayerFormat(GLenum layer_format) {
  switch (layer_format) {
    case GL_RGBA:
    case GL_RGBA8:
      return GL_RGBA8;

    case GL_RGB:
    case GL_RGB8:
      return GL_RGB8;

    case GL_SRGB_EXT:
    case GL_SRGB8:
      return GL_SRGB8;

    case GL_SRGB_ALPHA_EXT:
    case GL_SRGB8_ALPHA8:
      return GL_SRGB8_ALPHA8;

    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT24:
      return GL_DEPTH_COMPONENT24;

    case GL_DEPTH_STENCIL:
    case GL_DEPTH24_STENCIL8:
      return GL_DEPTH24_STENCIL8;

    default:
      NOTREACHED();
  }
}

GLenum XRWebGLBinding::TypeForLayerFormat(GLenum layer_format) {
  switch (layer_format) {
    case GL_RGBA:
    case GL_RGBA8:
    case GL_RGB:
    case GL_RGB8:
    case GL_SRGB_EXT:
    case GL_SRGB8:
    case GL_SRGB_ALPHA_EXT:
    case GL_SRGB8_ALPHA8:
      return GL_BYTE;

    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_STENCIL:
      return GL_UNSIGNED_SHORT;

    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH24_STENCIL8:
      return GL_UNSIGNED_INT;

    default:
      NOTREACHED();
  }
}

V8XRLayerLayout::Enum XRWebGLBinding::DetermineLayout(
    V8XRLayerLayout layout,
    V8XRTextureType::Enum texture_type) {
  V8XRLayerLayout::Enum final_layout = layout.AsEnum();

  if (final_layout == V8XRLayerLayout::Enum::kDefault) {
    if (session()->StereoscopicViews()) {
      final_layout = V8XRLayerLayout::Enum::kStereo;
    } else {
      final_layout = V8XRLayerLayout::Enum::kMono;
    }
  }

  // We don't support 2-texture solution for "stereo". So if the texture type is
  // not "texture-array", we fall back to "stereo-left-right".
  if (final_layout == V8XRLayerLayout::Enum::kStereo &&
      texture_type == V8XRTextureType::Enum::kTexture) {
    final_layout = V8XRLayerLayout::Enum::kStereoLeftRight;
  }

  // "default" must be resolved.
  CHECK(final_layout != V8XRLayerLayout::Enum::kDefault);
  return final_layout;
}

bool XRWebGLBinding::CanCreateShapedLayer(const XRLayerInit* init,
                                          ExceptionState& exception_state) {
  // Check that 'layers' feature was requested for session
  if (!session()->IsFeatureEnabled(device::mojom::XRSessionFeature::LAYERS)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "'layers' feature was not requested on session start.");
    return false;
  }

  if (!ValidateSessionAndContext(exception_state) ||
      !ValidateShapedLayerData(init, exception_state)) {
    return false;
  }
  return true;
}

bool XRWebGLBinding::ValidateShapedLayerTextureType(
    const V8XRTextureType texture_type,
    ExceptionState& exception_state) {
  if (texture_type.AsEnum() == V8XRTextureType::Enum::kTextureArray &&
      !webgl2_) {
    exception_state.ThrowTypeError(
        "textureType of 'texture-array' is only available with WebGL 2 "
        "contexts.");
    return false;
  }
  return true;
}

bool XRWebGLBinding::ValidateShapedLayerData(const XRLayerInit* init,
                                             ExceptionState& exception_state) {
  if (!ValidateLayerColorFormat(init->colorFormat(), exception_state)) {
    return false;
  }

  if (!init->hasSpace() || init->space() == nullptr) {
    exception_state.ThrowTypeError("XRSpace can't be null for Layer.");
    return false;
  }

  // XRSpace should belong to the same session
  if (init->space()->session() != session()) {
    exception_state.ThrowTypeError(
        "XRSpace is associated with a different XRSession.");
    return false;
  }

  if (!init->hasViewPixelHeight() || init->viewPixelHeight() == 0) {
    exception_state.ThrowTypeError("viewPixelHeight is required.");
    return false;
  }

  if (!init->hasViewPixelWidth() || init->viewPixelWidth() == 0) {
    exception_state.ThrowTypeError("viewPixelWidth is required.");
    return false;
  }

  return true;
}

bool XRWebGLBinding::ValidateTextureSize(const XRLayerInit* init,
                                         V8XRLayerLayout::Enum final_layout,
                                         ExceptionState& exception_state) {
  GLint max_texture_size = 0;
  webgl_context_->ContextGL()->GetIntegerv(GL_MAX_TEXTURE_SIZE,
                                           &max_texture_size);

  if (init->viewPixelHeight() > static_cast<uint32_t>(max_texture_size) /
                                    GetVerticalViewCount(final_layout)) {
    exception_state.ThrowTypeError(
        "ViewPixelHeight exceeds the maximum texture size.");
    return false;
  }

  if (init->viewPixelWidth() > static_cast<uint32_t>(max_texture_size) /
                                   GetHorizontalViewCount(final_layout)) {
    exception_state.ThrowTypeError(
        "ViewPixelWidth exceeds the maximum texture size.");
    return false;
  }

  return true;
}

void XRWebGLBinding::Trace(Visitor* visitor) const {
  visitor->Trace(webgl_context_);
  visitor->Trace(transport_delegate_);
  XRGraphicsBinding::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
