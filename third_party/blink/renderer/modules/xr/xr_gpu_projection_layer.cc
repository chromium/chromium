// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_gpu_projection_layer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_gpu_projection_layer_init.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/xr/xr_frame_provider.h"
#include "third_party/blink/renderer/modules/xr/xr_gpu_binding.h"
#include "third_party/blink/renderer/modules/xr/xr_gpu_swap_chain.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_system.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_mailbox_texture.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

XRGPUProjectionLayer::XRGPUProjectionLayer(
    XRGPUBinding* binding,
    XRGPUSwapChain* color_swap_chain,
    XRGPUSwapChain* depth_stencil_swap_chain)
    : XRProjectionLayer(binding),
      device_(binding->device()),
      color_swap_chain_(color_swap_chain),
      depth_stencil_swap_chain_(depth_stencil_swap_chain) {
  CHECK(color_swap_chain_);
  color_swap_chain_->SetLayer(this);
  if (depth_stencil_swap_chain_) {
    depth_stencil_swap_chain_->SetLayer(this);
  }
}

uint16_t XRGPUProjectionLayer::textureWidth() const {
  return color_swap_chain_->descriptor().size.width;
}

uint16_t XRGPUProjectionLayer::textureHeight() const {
  return color_swap_chain_->descriptor().size.height;
}

uint16_t XRGPUProjectionLayer::textureArrayLength() const {
  return color_swap_chain_->descriptor().size.depthOrArrayLayers;
}

void XRGPUProjectionLayer::OnFrameStart() {
  color_swap_chain_->OnFrameStart();
  if (depth_stencil_swap_chain_) {
    depth_stencil_swap_chain_->OnFrameStart();
  }
}

void XRGPUProjectionLayer::OnFrameEnd() {
  color_swap_chain_->OnFrameEnd();
  if (depth_stencil_swap_chain_) {
    depth_stencil_swap_chain_->OnFrameEnd();
  }

  session()->xr()->frameProvider()->SubmitWebGPULayer(
      this, color_swap_chain_->texture_was_queried());
}

void XRGPUProjectionLayer::OnResize() {}

void XRGPUProjectionLayer::Trace(Visitor* visitor) const {
  visitor->Trace(device_);
  visitor->Trace(color_swap_chain_);
  visitor->Trace(depth_stencil_swap_chain_);
  XRProjectionLayer::Trace(visitor);
}

}  // namespace blink
