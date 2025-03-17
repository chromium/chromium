// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_webgl_projection_layer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_gpu_projection_layer_init.h"
#include "third_party/blink/renderer/modules/webgl/webgl_rendering_context_base.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_binding.h"
#include "third_party/blink/renderer/modules/xr/xr_webgl_swap_chain.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"

namespace blink {

XRWebGLProjectionLayer::XRWebGLProjectionLayer(
    XRWebGLBinding* binding,
    XRWebGLSwapChain* color_swap_chain,
    XRWebGLSwapChain* depth_stencil_swap_chain)
    : XRProjectionLayer(binding),
      webgl_context_(binding->context()),
      color_swap_chain_(color_swap_chain),
      depth_stencil_swap_chain_(depth_stencil_swap_chain) {
  CHECK(color_swap_chain_);
  color_swap_chain_->SetLayer(this);
  if (depth_stencil_swap_chain_) {
    depth_stencil_swap_chain_->SetLayer(this);
  }
}

uint16_t XRWebGLProjectionLayer::textureWidth() const {
  return color_swap_chain_->descriptor().width;
}

uint16_t XRWebGLProjectionLayer::textureHeight() const {
  return color_swap_chain_->descriptor().height;
}

uint16_t XRWebGLProjectionLayer::textureArrayLength() const {
  return color_swap_chain_->descriptor().layers;
}

void XRWebGLProjectionLayer::OnFrameStart() {
  color_swap_chain_->OnFrameStart();
  if (depth_stencil_swap_chain_) {
    depth_stencil_swap_chain_->OnFrameStart();
  }
}

void XRWebGLProjectionLayer::OnFrameEnd() {
  color_swap_chain_->OnFrameEnd();
  if (depth_stencil_swap_chain_) {
    depth_stencil_swap_chain_->OnFrameEnd();
  }

  XRFrameProvider* frame_provider = session()->xr()->frameProvider();

  if (viewport_updated_) {
    frame_provider->UpdateLayerViewports(this);
    viewport_updated_ = false;
  }

  frame_provider->SubmitWebGLLayer(this,
                                   color_swap_chain_->texture_was_queried());
}

scoped_refptr<StaticBitmapImage>
XRWebGLProjectionLayer::TransferToStaticBitmapImage() {
  return color_swap_chain_->TransferToStaticBitmapImage();
}

void XRWebGLProjectionLayer::OnResize() {}

void XRWebGLProjectionLayer::Trace(Visitor* visitor) const {
  visitor->Trace(webgl_context_);
  visitor->Trace(color_swap_chain_);
  visitor->Trace(depth_stencil_swap_chain_);
  XRProjectionLayer::Trace(visitor);
}

}  // namespace blink
