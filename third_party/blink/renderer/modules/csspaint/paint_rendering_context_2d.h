// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_RENDERING_CONTEXT_2D_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_RENDERING_CONTEXT_2D_H_

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_paint_rendering_context_2d_settings.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"

namespace blink {

class Color;

// In our internal implementation, there are different kinds of canvas such as
// recording canvas, GPU canvas. The CSS Paint API uses the recording canvas and
// this class is specifically designed for the recording canvas.
//
// The main difference between this class and other contexts is that
// PaintRenderingContext2D operates on CSS pixels rather than physical pixels.
class MODULES_EXPORT PaintRenderingContext2D
    : public ScriptWrappable,
      public BaseRenderingContext2D,
      public MemoryManagedPaintRecorder::Client {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PaintRenderingContext2D(
      const gfx::Size& container_size,
      const PaintRenderingContext2DSettings*,
      float zoom,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      PaintWorkletGlobalScope* global_scope = nullptr);

  PaintRenderingContext2D(const PaintRenderingContext2D&) = delete;
  PaintRenderingContext2D& operator=(const PaintRenderingContext2D&) = delete;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(context_settings_);
    visitor->Trace(global_scope_);
    ScriptWrappable::Trace(visitor);
    BaseRenderingContext2D::Trace(visitor);
  }

  // PaintRenderingContext2D doesn't have any pixel readback so the origin
  // is always clean, and unable to taint it.
  bool OriginClean() const final { return true; }
  void SetOriginTainted() final {}

  int Width() const final;
  int Height() const final;

  Color GetCurrentColor() const final;

  cc::PaintCanvas* GetOrCreatePaintCanvas() final { return GetPaintCanvas(); }
  using BaseRenderingContext2D::GetPaintCanvas;  // Pull the non-const overload.
  const cc::PaintCanvas* GetPaintCanvas() const final;
  const MemoryManagedPaintRecorder* Recorder() const override {
    return &paint_recorder_;
  }

  void WillDraw(const SkIRect&, CanvasPerformanceMonitor::DrawType) final;

  double shadowOffsetX() const final;
  void setShadowOffsetX(double) final;

  double shadowOffsetY() const final;
  void setShadowOffsetY(double) final;

  double shadowBlur() const final;
  void setShadowBlur(double) final;

  sk_sp<PaintFilter> StateGetFilter() final;

  bool HasAlpha() const final { return context_settings_->alpha(); }

  // PaintRenderingContext2D cannot lose it's context.
  bool isContextLost() const final { return false; }

  // PaintRenderingContext2D uses a recording canvas, so it should never
  // allocate a pixel buffer and is not accelerated.
  bool CanCreateCanvas2dResourceProvider() const final { return false; }

  // CSS Paint doesn't have any notion of image orientation.
  RespectImageOrientationEnum RespectImageOrientation() const final {
    return kRespectImageOrientation;
  }

  DOMMatrix* getTransform() final;
  void resetTransform() final;
  void reset() final;

  std::optional<cc::PaintRecord> FlushCanvas(FlushReason) final {
    return std::nullopt;
  }

  PaintRecord GetRecord();

  ExecutionContext* GetTopExecutionContext() const override {
    return global_scope_.Get();
  }

 protected:
  PredefinedColorSpace GetDefaultImageDataColorSpace() const final;
  bool IsPaint2D() const override { return true; }

  // PaintRenderingContext2D is unable to resolve fonts.
  bool ResolveFont(const String& new_font) final { return false; }

 private:
  void InitializeForRecording(cc::PaintCanvas* canvas) const override;
  void RecordingCleared() override;

  MemoryManagedPaintRecorder paint_recorder_;
  std::optional<PaintRecord> previous_frame_;
  gfx::Size container_size_;
  Member<const PaintRenderingContext2DSettings> context_settings_;
  // The paint worklet canvas operates on CSS pixels, and that's different than
  // the HTML canvas which operates on physical pixels. In other words, the
  // paint worklet canvas needs to handle device scale factor and browser zoom,
  // and this is designed for that purpose.
  const float effective_zoom_;
  WeakMember<PaintWorkletGlobalScope> global_scope_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_RENDERING_CONTEXT_2D_H_
