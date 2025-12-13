// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_gpu_drawing_context.h"

#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/xr/xr_gpu_binding.h"
#include "third_party/blink/renderer/modules/xr/xr_gpu_swap_chain.h"
#include "third_party/blink/renderer/platform/graphics/gpu/xr_gpu_frame_transport_delegate.h"

namespace blink {

XRGPUDrawingContext::XRGPUDrawingContext(
    XRGPUBinding* binding,
    XRGPUSwapChain* color_swap_chain,
    XRGPUSwapChain* depth_stencil_swap_chain)
    : device_(binding->device()),
      color_swap_chain_(color_swap_chain),
      depth_stencil_swap_chain_(depth_stencil_swap_chain),
      binding_(binding) {
  CHECK(color_swap_chain_);
  CHECK(binding_);
}

uint16_t XRGPUDrawingContext::TextureWidth() const {
  return color_swap_chain_->descriptor().size.width;
}

uint16_t XRGPUDrawingContext::TextureHeight() const {
  return color_swap_chain_->descriptor().size.height;
}

uint16_t XRGPUDrawingContext::TextureArrayLength() const {
  return color_swap_chain_->descriptor().size.depthOrArrayLayers;
}

bool XRGPUDrawingContext::TextureWasQueried() const {
  return color_swap_chain_->texture_was_queried();
}

void XRGPUDrawingContext::SetCompositionLayer(XRCompositionLayer* layer) {
  color_swap_chain_->SetLayer(layer);
  if (depth_stencil_swap_chain_) {
    depth_stencil_swap_chain_->SetLayer(layer);
  }
}

void XRGPUDrawingContext::OnFrameStart() {
  color_swap_chain_->OnFrameStart();
  if (depth_stencil_swap_chain_) {
    depth_stencil_swap_chain_->OnFrameStart();
  }
}

void XRGPUDrawingContext::OnFrameEnd() {
  color_swap_chain_->OnFrameEnd();
  if (depth_stencil_swap_chain_) {
    depth_stencil_swap_chain_->OnFrameEnd();
  }
}

XRSession* XRGPUDrawingContext::session() const {
  return color_swap_chain_->layer()->session();
}

scoped_refptr<StaticBitmapImage>
XRGPUDrawingContext::TransferToStaticBitmapImage() {
  // TransferToStaticBitmapImage is only used with SUBMIT_AS_TEXTURE_HANDLE,
  // which we don't support.
  NOTREACHED();
}

XRFrameTransportDelegate* XRGPUDrawingContext::GetTransportDelegate() {
  return binding_->GetTransportDelegate();
}

void XRGPUDrawingContext::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(color_swap_chain_);
  visitor->Trace(depth_stencil_swap_chain_);
  visitor->Trace(binding_);
  XRLayerDrawingContext::Trace(visitor);
}

}  // namespace blink
