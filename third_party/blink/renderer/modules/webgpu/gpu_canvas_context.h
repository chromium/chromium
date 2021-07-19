// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_CANVAS_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_CANVAS_CONTEXT_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_swap_chain.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

class GPUAdapter;
class GPUCanvasConfiguration;
class GPUSwapChain;
class GPUTexture;
class V8UnionHTMLCanvasElementOrOffscreenCanvas;

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
  V8RenderingContext* AsV8RenderingContext() final;
  V8OffscreenRenderingContext* AsV8OffscreenRenderingContext() final;
  scoped_refptr<StaticBitmapImage> GetImage() final;
  bool PaintRenderingResultsToCanvas(SourceDrawingBuffer) final;
  bool CopyRenderingResultsFromDrawingBuffer(CanvasResourceProvider*,
                                             SourceDrawingBuffer) final;
  void SetIsInHiddenPage(bool) override {}
  void SetIsBeingDisplayed(bool) override {}
  bool isContextLost() const override { return false; }
  bool IsComposited() const final { return true; }
  bool IsAccelerated() const final { return true; }
  bool IsOriginTopLeft() const final { return true; }
  void SetFilterQuality(cc::PaintFlags::FilterQuality) override;
  bool IsPaintable() const final { return true; }
  int ExternallyAllocatedBufferCountPerPixel() final { return 1; }
  void Stop() final;
  cc::Layer* CcLayer() const final;

  // OffscreenCanvas-specific methods
  bool PushFrame() final;
  ImageBitmap* TransferToImageBitmap(ScriptState*) final;

  bool IsOffscreenCanvas() const {
    if (Host())
      return Host()->IsOffscreenCanvas();
    return false;
  }

  // gpu_presentation_context.idl
  V8UnionHTMLCanvasElementOrOffscreenCanvas* getHTMLOrOffscreenCanvas() const;

  void configure(const GPUCanvasConfiguration* descriptor, ExceptionState&);
  void unconfigure();
  String getPreferredFormat(const GPUAdapter* adapter);
  GPUTexture* getCurrentTexture(ExceptionState&);

  // gpu_canvas_context.idl (Deprecated)
  GPUSwapChain* configureSwapChain(const GPUCanvasConfiguration* descriptor,
                                   ExceptionState&);
  String getSwapChainPreferredFormat(ExecutionContext* execution_context,
                                     GPUAdapter* adapter);

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUCanvasContext);

  void ConfigureInternal(const GPUCanvasConfiguration* descriptor,
                         ExceptionState&,
                         bool deprecated_resize_behavior = false);

  cc::PaintFlags::FilterQuality filter_quality_ =
      cc::PaintFlags::FilterQuality::kLow;
  Member<GPUSwapChain> swapchain_;
  Member<GPUDevice> configured_device_;
  bool stopped_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_CANVAS_CONTEXT_H_
