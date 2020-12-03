// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_CANVAS_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_CANVAS_CONTEXT_H_

#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_swap_chain.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

class GPUAdapter;
class GPUSwapChain;
class GPUSwapChainDescriptor;

// A GPUCanvasContext does little by itself and basically just binds a canvas
// and a GPUSwapChain together and forwards calls from one to the other.
class GPUCanvasContext : public CanvasRenderingContext {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
    DISALLOW_COPY_AND_ASSIGN(Factory);

   public:
    Factory();
    ~Factory() override;

    CanvasRenderingContext* Create(
        CanvasRenderingContextHost*,
        const CanvasContextCreationAttributesCore&) override;
    CanvasRenderingContext::ContextType GetContextType() const override;
  };

  GPUCanvasContext(CanvasRenderingContextHost*,
                   const CanvasContextCreationAttributesCore&);
  ~GPUCanvasContext() override;

  void Trace(Visitor*) const override;
  const IntSize& CanvasSize() const;

  // CanvasRenderingContext implementation
  ContextType GetContextType() const override;
  void SetCanvasGetContextResult(RenderingContext&) final;
  scoped_refptr<StaticBitmapImage> GetImage() final { return nullptr; }
  void SetIsInHiddenPage(bool) override {}
  void SetIsBeingDisplayed(bool) override {}
  bool isContextLost() const override { return false; }
  bool IsComposited() const final { return true; }
  bool IsAccelerated() const final { return true; }
  bool IsOriginTopLeft() const final { return true; }
  bool Is3d() const final { return true; }
  void SetFilterQuality(SkFilterQuality) override;
  bool IsPaintable() const final { return true; }
  int ExternallyAllocatedBufferCountPerPixel() final { return 1; }
  void Stop() final;
  cc::Layer* CcLayer() const final;

  // gpu_canvas_context.idl
  GPUSwapChain* configureSwapChain(const GPUSwapChainDescriptor* descriptor,
                                   ExceptionState&);
  ScriptPromise getSwapChainPreferredFormat(ScriptState* script_state,
                                            GPUDevice* device);
  String getSwapChainPreferredFormat(const GPUAdapter* adapter);

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUCanvasContext);
  SkFilterQuality filter_quality_ = kLow_SkFilterQuality;
  Member<GPUSwapChain> swapchain_;
  bool stopped_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_CANVAS_CONTEXT_H_
