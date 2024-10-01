// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_PROJECTION_LAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_PROJECTION_LAYER_H_

#include "third_party/blink/renderer/modules/xr/xr_projection_layer.h"

namespace blink {

class XRGPUBinding;
class XRGPULayerTextureSwapChain;

class XRGPUProjectionLayer final : public XRProjectionLayer {
 public:
  XRGPUProjectionLayer(XRGPUBinding*,
                       XRGPULayerTextureSwapChain* color_swap_chain,
                       XRGPULayerTextureSwapChain* depth_stencil_swap_chain_);
  ~XRGPUProjectionLayer() override = default;

  // TODO(crbug.com/359428629): Implement WebGPU layer submission
  void OnFrameStart(
      const std::optional<gpu::MailboxHolder>& buffer_mailbox_holder,
      const std::optional<gpu::MailboxHolder>& camera_image_mailbox_holder)
      override {}
  void OnFrameEnd() override {}
  void OnResize() override {}

  XRGPULayerTextureSwapChain* color_swap_chain() {
    return color_swap_chain_.Get();
  }
  XRGPULayerTextureSwapChain* depth_stencil_swap_chain() {
    return depth_stencil_swap_chain_.Get();
  }

  void Trace(Visitor*) const override;

 private:
  Member<XRGPULayerTextureSwapChain> color_swap_chain_;
  Member<XRGPULayerTextureSwapChain> depth_stencil_swap_chain_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_PROJECTION_LAYER_H_
