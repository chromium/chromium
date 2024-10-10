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
#include "third_party/blink/renderer/modules/xr/xr_gpu_projection_layer.h"
#include "third_party/blink/renderer/modules/xr/xr_gpu_sub_image.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

const double kMinScaleFactor = 0.2;

// A texture swap chain that is not communicated back to the compositor, used
// for things like depth/stencil attachments that don't assist reprojection.
class XRGPUStaticTextureLayerSwapChain : public XRGPULayerTextureSwapChain {
 public:
  XRGPUStaticTextureLayerSwapChain(GPUDevice* device,
                                   const wgpu::TextureDescriptor* desc) {
    texture_ = GPUTexture::Create(device, desc);
  }

  GPUTexture* GetCurrentTexture() override { return texture_; }

  void OnFrameEnd() override {
    // TODO(crbug.com/5818595): Prior to shipping the spec needs to determine
    // if texture re-use is appropriate or not. If re-use is not specified then
    // it should at the very least be detached from the JavaScript wrapper and
    // reattached to a new one here. In both cases the texture should be
    // cleared.
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(texture_);
    XRGPULayerTextureSwapChain::Trace(visitor);
  }

 private:
  Member<GPUTexture> texture_;
};

}  // namespace

void XRGPULayerTextureSwapChain::OnFrameStart() {}
void XRGPULayerTextureSwapChain::OnFrameEnd() {}
void XRGPULayerTextureSwapChain::Trace(Visitor* visitor) const {}

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

  if (device->destroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create an XRGPUBinding with a "
                                      "destroyed WebGPU device.");
    return nullptr;
  }

  if (!device->adapter()->isXRCompatible()) {
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
    : XRGraphicsBinding(session), device_(device) {}

XRProjectionLayer* XRGPUBinding::createProjectionLayer(
    const XRGPUProjectionLayerInit* init,
    ExceptionState& exception_state) {
  // TODO(crbug.com/5818595): Validate the colorFormat and depthStencilFormat.

  if (!CanCreateLayer(exception_state)) {
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

  // If the scaled texture dimensions are larger than the max texture dimension
  // for the device scale it down till it fits.
  unsigned max_texture_size = device_->limits()->maxTextureDimension2D();
  if (scaled_size.width() > max_texture_size ||
      scaled_size.height() > max_texture_size) {
    double max_dimension = std::max(scaled_size.width(), scaled_size.height());
    scaled_size = gfx::ScaleSize(scaled_size, max_texture_size / max_dimension);
  }

  gfx::Size texture_size = gfx::ToFlooredSize(scaled_size);

  // Create the color swap chain
  wgpu::TextureDescriptor color_desc = {};
  color_desc.label = "XRProjectionLayer Color";
  color_desc.format = AsDawnEnum(init->colorFormat());
  color_desc.usage = static_cast<wgpu::TextureUsage>(init->textureUsage());
  color_desc.size = {static_cast<uint32_t>(texture_size.width()),
                     static_cast<uint32_t>(texture_size.height()),
                     static_cast<uint32_t>(session()->array_texture_layers())};
  color_desc.dimension = wgpu::TextureDimension::e2D;

  XRGPUStaticTextureLayerSwapChain* color_swap_chain =
      MakeGarbageCollected<XRGPUStaticTextureLayerSwapChain>(device_,
                                                             &color_desc);

  // Create the depth/stencil swap chain
  XRGPUStaticTextureLayerSwapChain* depth_stencil_swap_chain = nullptr;
  if (init->hasDepthStencilFormat()) {
    wgpu::TextureDescriptor depth_stencil_desc = {};
    depth_stencil_desc.label = "XRProjectionLayer Depth/Stencil";
    depth_stencil_desc.format = AsDawnEnum(*init->depthStencilFormat());
    depth_stencil_desc.usage =
        static_cast<wgpu::TextureUsage>(init->textureUsage());
    depth_stencil_desc.size = {
        static_cast<uint32_t>(texture_size.width()),
        static_cast<uint32_t>(texture_size.height()),
        static_cast<uint32_t>(session()->array_texture_layers())};
    depth_stencil_desc.dimension = wgpu::TextureDimension::e2D;

    depth_stencil_swap_chain =
        MakeGarbageCollected<XRGPUStaticTextureLayerSwapChain>(
            device_, &depth_stencil_desc);
  }

  return MakeGarbageCollected<XRGPUProjectionLayer>(this, color_swap_chain,
                                                    depth_stencil_swap_chain);
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

  XRGPUProjectionLayer* gpu_layer = static_cast<XRGPUProjectionLayer*>(layer);

  GPUTexture* color_texture =
      gpu_layer->color_swap_chain()->GetCurrentTexture();

  GPUTexture* depth_stencil_texture = nullptr;
  XRGPULayerTextureSwapChain* depth_stencil_swap_chain =
      gpu_layer->depth_stencil_swap_chain();
  if (depth_stencil_swap_chain) {
    depth_stencil_texture = depth_stencil_swap_chain->GetCurrentTexture();
  }

  gfx::Rect viewport =
      gfx::Rect(0, 0, color_texture->width(), color_texture->height());

  return MakeGarbageCollected<XRGPUSubImage>(
      viewport, view->ViewData()->index(), color_texture,
      depth_stencil_texture);
}

V8GPUTextureFormat XRGPUBinding::getPreferredColorFormat() {
  return FromDawnEnum(GPU::preferred_canvas_format());
}

bool XRGPUBinding::CanCreateLayer(ExceptionState& exception_state) {
  if (session()->ended()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create a new layer for an "
                                      "XRSession which has already ended.");
    return false;
  }

  if (device_->destroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot create a new layer with a "
                                      "destroyed WebGPU device.");
    return false;
  }

  return true;
}

void XRGPUBinding::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  XRGraphicsBinding::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
