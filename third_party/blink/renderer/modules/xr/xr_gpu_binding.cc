// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_gpu_binding.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_gpu_projection_layer_init.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_enum_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/modules/webgpu/gpu.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_adapter.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_supported_limits.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_gpu_drawing_context.h"
#include "third_party/blink/renderer/modules/xr/xr_gpu_sub_image.h"
#include "third_party/blink/renderer/modules/xr/xr_gpu_swap_chain.h"
#include "third_party/blink/renderer/modules/xr/xr_gpu_texture_array_swap_chain.h"
#include "third_party/blink/renderer/modules/xr/xr_projection_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

const double kMinScaleFactor = 0.2;

}  // namespace

XRGPUBinding* XRGPUBinding::Create(XRSession* session,
                                   GPUDevice* device,
                                   ExceptionState& exception_state) {
  DCHECK(session);
  DCHECK(device);

  if (session->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRGPUBinding for an "
                                      "XRSession which has already ended.");
    return nullptr;
  }

  if (!session->immersive()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRGPUBinding for an "
                                      "inline XRSession.");
    return nullptr;
  }

  if (device->IsDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRGPUBinding with a "
                                      "destroyed WebGPU device.");
    return nullptr;
  }

  if (!device->adapter()->IsXRCompatible()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "WebGPU device must be created by an XR compatible adapter in order to "
        "use with an immersive XRSession");
    return nullptr;
  }

  if (session->GraphicsApi() != XRGraphicsBinding::Api::kWebGPU) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Cannot create an XRGPUBinding with a WebGL-based XRSession.");
    return nullptr;
  }

  return MakeGarbageCollected<XRGPUBinding>(session, device);
}

XRGPUBinding::XRGPUBinding(XRSession* session, GPUDevice* device)
    : XRGraphicsBinding(session), device_(device) {
  transport_delegate_ = MakeGarbageCollected<XrGpuFrameTransportDelegate>(this);
}

XRProjectionLayer* XRGPUBinding::createProjectionLayer(
    const XRGPUProjectionLayerInit* init,
    ExceptionState& exception_state) {
  if (!CanCreateLayer(exception_state) ||
      !ValidateFormats(init, exception_state)) {
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

  // TODO(crbug.com/359418629): Remove once array Mailboxes are available.
  scaled_size.set_width(scaled_size.width() *
                        session()->array_texture_layers());

  // If the scaled texture dimensions are larger than the max texture dimension
  // for the device scale it down till it fits.
  unsigned max_texture_size = device_->limits()->maxTextureDimension2D();
  if (scaled_size.width() > max_texture_size ||
      scaled_size.height() > max_texture_size) {
    double max_dimension = std::max(scaled_size.width(), scaled_size.height());
    scaled_size = gfx::ScaleSize(scaled_size, max_texture_size / max_dimension);
  }

  gfx::Size texture_size = gfx::ToFlooredSize(scaled_size);

  // Create the side-by-side color swap chain
  wgpu::TextureDescriptor color_desc = {};
  color_desc.label = "XRProjectionLayer Color";
  // TODO(crbug.com/359418629): Currently all backend images are RGBA8 format.
  color_desc.format = wgpu::TextureFormat::RGBA8Unorm;
  color_desc.usage = static_cast<wgpu::TextureUsage>(init->textureUsage());
  color_desc.size = {static_cast<uint32_t>(texture_size.width()),
                     static_cast<uint32_t>(texture_size.height()),
                     static_cast<uint32_t>(1)};
  color_desc.dimension = wgpu::TextureDimension::e2D;

  XRGPUSwapChain* color_swap_chain;
  if (session()->xr()->frameProvider()->DrawingIntoSharedBuffer()) {
    color_swap_chain =
        MakeGarbageCollected<XRGPUMailboxSwapChain>(device_, color_desc);
  } else {
    // TODO(crbug.com/359418629): Replace with a shared image swap chain.
    color_swap_chain =
        MakeGarbageCollected<XRGPUStaticSwapChain>(device_, color_desc);
  }

  // Create the texture array wrapper for the side-by-side swap chain.
  // TODO(crbug.com/359418629): Remove once array Mailboxes are available.
  XRGPUTextureArraySwapChain* wrapped_swap_chain =
      MakeGarbageCollected<XRGPUTextureArraySwapChain>(
          device_, color_swap_chain, AsDawnEnum(init->colorFormat()),
          session()->array_texture_layers());

  // Create the depth/stencil swap chain
  XRGPUStaticSwapChain* depth_stencil_swap_chain = nullptr;
  if (init->hasDepthStencilFormat()) {
    wgpu::TextureDescriptor depth_stencil_desc = {};
    depth_stencil_desc.label = "XRProjectionLayer Depth/Stencil";
    depth_stencil_desc.format = AsDawnEnum(*init->depthStencilFormat());
    depth_stencil_desc.usage =
        static_cast<wgpu::TextureUsage>(init->textureUsage());
    depth_stencil_desc.size = wrapped_swap_chain->descriptor().size;
    depth_stencil_desc.dimension = wgpu::TextureDimension::e2D;

    depth_stencil_swap_chain =
        MakeGarbageCollected<XRGPUStaticSwapChain>(device_, depth_stencil_desc);
  }

  auto* drawing_context = MakeGarbageCollected<XRGPUDrawingContext>(
      this, wrapped_swap_chain, depth_stencil_swap_chain);

  return MakeGarbageCollected<XRProjectionLayer>(this, drawing_context);
}

XRGPUSubImage* XRGPUBinding::getViewSubImage(XRProjectionLayer* layer,
                                             XRView* view,
                                             ExceptionState& exception_state) {
  if (!OwnsLayer(layer)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Layer was not created with this binding.");
    return nullptr;
  }

  if (!view || view->session() != session()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "View was not created with the same session as this binding.");
    return nullptr;
  }

  // The layer passed the OwnsLayer check, confirming it can only contain
  // a GPU drawing context. This makes the static_cast safe.
  XRGPUDrawingContext* drawing_context =
      static_cast<XRGPUDrawingContext*>(layer->drawing_context());

  XRGPUSwapChain* color_swap_chain = drawing_context->color_swap_chain();
  GPUTexture* color_texture = color_swap_chain->GetCurrentTexture();

  GPUTexture* depth_stencil_texture = nullptr;
  XRGPUSwapChain* depth_stencil_swap_chain =
      drawing_context->depth_stencil_swap_chain();
  if (depth_stencil_swap_chain) {
    depth_stencil_texture = depth_stencil_swap_chain->GetCurrentTexture();
  }

  XRViewData* viewData = view->ViewData();
  if (viewData->ApplyViewportScaleForFrame()) {
    layer->SetModified(true);
  }

  gfx::Rect viewport = GetViewportForView(layer, viewData);

  return MakeGarbageCollected<XRGPUSubImage>(
      viewport, view->ViewData()->index(), color_texture,
      depth_stencil_texture);
}

gfx::Rect XRGPUBinding::GetViewportForView(XRProjectionLayer* layer,
                                           XRViewData* view) {
  CHECK(OwnsLayer(layer));

  return gfx::Rect(0, 0, layer->textureWidth() * view->CurrentViewportScale(),
                   layer->textureHeight() * view->CurrentViewportScale());
}

V8GPUTextureFormat XRGPUBinding::getPreferredColorFormat() {
  // TODO(crbug.com/5818595): Ensure the backend swap chain format matches this.
  // Till then the copy between formats is done in XRGPUTextureArraySwapChain.
  return FromDawnEnum(GPU::GetPreferredCanvasFormat());
}

XrGpuFrameTransportDelegate* XRGPUBinding::GetTransportDelegate() {
  return transport_delegate_;
}

scoped_refptr<DawnControlClientHolder> XRGPUBinding::GetDawnControlClient()
    const {
  if (!device_) {
    return nullptr;
  }
  return device_->GetDawnControlClient();
}

bool XRGPUBinding::CanCreateLayer(ExceptionState& exception_state) {
  if (session()->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create a new layer for an "
                                      "XRSession which has already ended.");
    return false;
  }

  if (device_->IsDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create a new layer with a "
                                      "destroyed WebGPU device.");
    return false;
  }

  return true;
}

bool XRGPUBinding::ValidateFormats(const XRGPUProjectionLayerInit* init,
                                   ExceptionState& exception_state) {
  // Is the color format supported?
  switch (init->colorFormat().AsEnum()) {
    case V8GPUTextureFormat::Enum::kBgra8Unorm:
    case V8GPUTextureFormat::Enum::kRgba8Unorm:
      // TODO(crbug.com/5818595): Support 'rgba16float'
      break;
    default:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Only 'rgba8unorm' or 'bgra8unorm' colorFormats are supported.");
      return false;
  }

  if (init->hasDepthStencilFormat()) {
    switch (init->depthStencilFormat().value().AsEnum()) {
      case V8GPUTextureFormat::Enum::kStencil8:
      case V8GPUTextureFormat::Enum::kDepth16Unorm:
      case V8GPUTextureFormat::Enum::kDepth24Plus:
      case V8GPUTextureFormat::Enum::kDepth24PlusStencil8:
      case V8GPUTextureFormat::Enum::kDepth32Float:
      case V8GPUTextureFormat::Enum::kDepth32FloatStencil8:
        break;
      default:
        exception_state.ThrowDOMException(
            DOMExceptionCode::kInvalidStateError,
            "depthStencilFormat must be a depth and/or stencil format.");
        return false;
    }
  }

  return true;
}

void XRGPUBinding::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(transport_delegate_);
  XRGraphicsBinding::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
