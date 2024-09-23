// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_gpu_projection_layer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_gpu_projection_layer_init.h"
#include "third_party/blink/renderer/modules/xr/xr_gpu_binding.h"

namespace blink {

XRGPUProjectionLayer::XRGPUProjectionLayer(
    XRGPUBinding* binding,
    XRGPULayerTextureSwapChain* color_swap_chain,
    XRGPULayerTextureSwapChain* depth_stencil_swap_chain)
    : XRProjectionLayer(binding),
      color_swap_chain_(color_swap_chain),
      depth_stencil_swap_chain_(depth_stencil_swap_chain) {}

void XRGPUProjectionLayer::Trace(Visitor* visitor) const {
  visitor->Trace(color_swap_chain_);
  visitor->Trace(depth_stencil_swap_chain_);
  XRProjectionLayer::Trace(visitor);
}

}  // namespace blink
