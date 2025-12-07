// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_DRAWING_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_DRAWING_CONTEXT_H_

#include "third_party/blink/renderer/modules/xr/xr_layer_drawing_context.h"

namespace blink {

class GPUDevice;
class XRCompositionLayer;
class XRGPUBinding;
class XRGPUSwapChain;
class XRSession;

class XRGPUDrawingContext final : public XRLayerDrawingContext {
 public:
  XRGPUDrawingContext(XRGPUBinding*,
                      XRGPUSwapChain* color_swap_chain,
                      XRGPUSwapChain* depth_stencil_swap_chain);
  ~XRGPUDrawingContext() override = default;

  uint16_t TextureWidth() const override;
  uint16_t TextureHeight() const override;
  uint16_t TextureArrayLength() const override;

  void SetCompositionLayer(XRCompositionLayer* layer) override;

  void OnFrameStart() override;
  void OnFrameEnd() override;

  bool TextureWasQueried() const override;

  // XrLayerClient overrides.
  XRSession* session() const override;
  scoped_refptr<StaticBitmapImage> TransferToStaticBitmapImage() override;
  XRFrameTransportDelegate* GetTransportDelegate() override;

  GPUDevice* device() { return device_; }

  XRGPUSwapChain* color_swap_chain() { return color_swap_chain_.Get(); }
  XRGPUSwapChain* depth_stencil_swap_chain() {
    return depth_stencil_swap_chain_.Get();
  }

  void Trace(Visitor*) const override;

 private:
  Member<GPUDevice> device_;
  Member<XRGPUSwapChain> color_swap_chain_;
  Member<XRGPUSwapChain> depth_stencil_swap_chain_;
  Member<XRGPUBinding> binding_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_DRAWING_CONTEXT_H_
