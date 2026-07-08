// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_OFFSCREENCANVAS2D_OFFSCREEN_CANVAS_RENDERING_CONTEXT_2D_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_OFFSCREENCANVAS2D_OFFSCREEN_CANVAS_RENDERING_CONTEXT_2D_H_

#include <memory>

#include "base/notreached.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context_factory.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_color_params.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/flush_for_image_listener.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"

namespace blink {

class Canvas2DBitmapProvider;
class Canvas2DResourceProvider;
class ExceptionState;
class ExecutionContext;
class MemoryManagedPaintCanvas;

class MODULES_EXPORT OffscreenCanvasRenderingContext2D final
    : public ScriptWrappable,
      public BaseRenderingContext2D,
      public FlushForImageObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Factory : public CanvasRenderingContextFactory {
   public:
    Factory() = default;
    ~Factory() override = default;

    CanvasRenderingContext* Create(
        ExecutionContext*,
        CanvasRenderingContextHost* host,
        const CanvasContextCreationAttributesCore& attrs) override;

    CanvasRenderingContext::CanvasRenderingAPI GetRenderingAPI()
        const override {
      return CanvasRenderingContext::CanvasRenderingAPI::k2D;
    }
  };

  OffscreenCanvasRenderingContext2D(
      OffscreenCanvas*,
      const CanvasContextCreationAttributesCore& attrs);

  OffscreenCanvas* offscreenCanvasForBinding() const {
    DCHECK(!Host() || Host()->IsOffscreenCanvas());
    return static_cast<OffscreenCanvas*>(Host());
  }

  // CanvasRenderingContext implementation
  ~OffscreenCanvasRenderingContext2D() override;
  bool IsComposited() const override { return false; }
  V8RenderingContext* AsV8RenderingContext() final;
  V8OffscreenRenderingContext* AsV8OffscreenRenderingContext() final;
  void Stop() final { NOTREACHED(); }
  scoped_refptr<StaticBitmapImage> GetImage() final;
  scoped_refptr<StaticBitmapImage> PaintRenderingResultsToSnapshot(
      SourceDrawingBuffer source_buffer) override;
  void Reset() override;
  // CanvasRenderingContext - ActiveScriptWrappable
  // This method will avoid this class to be garbage collected, as soon as
  // HasPendingActivity returns true.
  bool HasPendingActivity() const final {
    if (!Host())
      return false;
    DCHECK(Host()->IsOffscreenCanvas());
    auto* offscreen_canvas = static_cast<OffscreenCanvas*>(Host());
    return offscreen_canvas->IsPendingFrame();
  }

  // BaseRenderingContext2D implementation
  bool OriginClean() const final;
  void SetOriginTainted() final;

  int Width() const final;
  int Height() const final;

  bool CanCreateResourceProvider() final;
  bool Is2DCanvasAccelerated() const override;

  // Offscreen canvas doesn't have any notion of image orientation.
  RespectImageOrientationEnum RespectImageOrientation() const final {
    return kRespectImageOrientation;
  }

  Color GetCurrentColor() const final;

  MemoryManagedPaintCanvas* GetOrCreatePaintCanvas() final;
  using BaseRenderingContext2D::GetPaintCanvas;  // Pull the non-const overload.
  const MemoryManagedPaintCanvas* GetPaintCanvas() const final;
  const MemoryManagedPaintRecorder* Recorder() const final;

  void WillDraw(const gfx::Rect& dirty_rect,
                CanvasPerformanceMonitor::DrawType) final;

  void FlushIfRecordingLimitExceeded();

  // FlushForImageObserver implementation
  void OnFlushForImage(cc::PaintImage::ContentId content_id) override;

  sk_sp<PaintFilter> StateGetFilter() final;

  bool HasAlpha() const final { return CreationAttributes().alpha; }
  bool IsDesynchronized() const final {
    return CreationAttributes().desynchronized;
  }
  void Dispose() override;
  void LoseContext(LostContextMode) override;

  ImageBitmap* TransferToImageBitmap(ScriptState* script_state,
                                     ExceptionState& exception_state) final;

  void Trace(Visitor*) const override;

  scoped_refptr<CanvasResource> GetResourceForPushFrame(
      bool& should_call_push_frame) override;

  CanvasRenderingContextHost* GetCanvasRenderingContextHost() const override;
  ExecutionContext* GetTopExecutionContext() const override;

  std::optional<cc::PaintRecord> FlushCanvas(FlushReason) override;
  base::ByteSize AllocatedBufferSize() const override;

 protected:
  OffscreenCanvas* HostAsOffscreenCanvas() const final;
  UniqueFontSelector* GetFontSelector() const final;

  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y) override;

  bool ResolveFont(const String& new_font) override;

 private:
  void FinalizeFrame(FlushReason) final;

  bool IsPaintable() const final;

  scoped_refptr<CanvasResource> ProduceCanvasResource(FlushReason);

  bool InitializeResourceProvider() override;
  bool IsResourceProviderValid() const;

  std::unique_ptr<Canvas2DResourceProvider> shared_image_provider_;
  std::unique_ptr<Canvas2DBitmapProvider> bitmap_provider_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_OFFSCREENCANVAS2D_OFFSCREEN_CANVAS_RENDERING_CONTEXT_2D_H_
