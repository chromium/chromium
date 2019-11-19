// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_RENDERING_CONTEXT_2D_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_RENDERING_CONTEXT_2D_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d_settings.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"

namespace blink {

class CanvasImageSource;
class Color;

// In our internal implementation, there are different kinds of canvas such as
// recording canvas, GPU canvas. The CSS Paint API uses the recording canvas and
// this class is specifically designed for the recording canvas.
//
// The main difference between this class and other contexts is that
// PaintRenderingContext2D operates on CSS pixels rather than physical pixels.
class MODULES_EXPORT PaintRenderingContext2D : public ScriptWrappable,
                                               public BaseRenderingContext2D {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(PaintRenderingContext2D);

 public:
  PaintRenderingContext2D(const IntSize& container_size,
                          const PaintRenderingContext2DSettings*,
                          float zoom,
                          float device_scale_factor);

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(context_settings_);
    ScriptWrappable::Trace(visitor);
    BaseRenderingContext2D::Trace(visitor);
  }

  // PaintRenderingContext2D doesn't have any pixel readback so the origin
  // is always clean, and unable to taint it.
  bool OriginClean() const final { return true; }
  void SetOriginTainted() final {}
  bool WouldTaintOrigin(CanvasImageSource*) final { return false; }

  int Width() const final;
  int Height() const final;

  bool ParseColorOrCurrentColor(Color&, const String& color_string) const final;

  cc::PaintCanvas* DrawingCanvas() const final;
  cc::PaintCanvas* ExistingDrawingCanvas() const final;

  void DidDraw(const SkIRect&) final;

  double shadowOffsetX() const final;
  void setShadowOffsetX(double) final;

  double shadowOffsetY() const final;
  void setShadowOffsetY(double) final;

  double shadowBlur() const final;
  void setShadowBlur(double) final;

  bool StateHasFilter() final;
  sk_sp<PaintFilter> StateGetFilter() final;
  void SnapshotStateForFilter() final {}

  void ValidateStateStack() const final;

  bool HasAlpha() const final { return context_settings_->alpha(); }

  // PaintRenderingContext2D cannot lose it's context.
  bool isContextLost() const final { return false; }

  // PaintRenderingContext2D uses a recording canvas, so it should never
  // allocate a pixel buffer and is not accelerated.
  bool CanCreateCanvas2dResourceProvider() const final { return false; }
  bool IsAccelerated() const final { return false; }

  void setTransform(double m11,
                    double m12,
                    double m21,
                    double m22,
                    double dx,
                    double dy) final;
  void setTransform(DOMMatrix2DInit*, ExceptionState&) final;

  sk_sp<PaintRecord> GetRecord();

 protected:
  bool IsPaint2D() const override { return true; }
  void WillOverwriteCanvas() override;

 private:
  void InitializePaintRecorder();
  cc::PaintCanvas* Canvas() const;

  std::unique_ptr<PaintRecorder> paint_recorder_;
  sk_sp<PaintRecord> previous_frame_;
  IntSize container_size_;
  Member<const PaintRenderingContext2DSettings> context_settings_;
  bool did_record_draw_commands_in_paint_recorder_;
  // The paint worklet canvas operates on CSS pixels, and that's different than
  // the HTML canvas which operates on physical pixels. In other words, the
  // paint worklet canvas needs to handle device scale factor and browser zoom,
  // and this is designed for that purpose.
  const float effective_zoom_;
  // On platforms where zoom_for_dsf is enabled, the |effective_zoom_|
  // accounts for the device scale factor. For platforms where the feature is
  // not enabled (currently Mac only), we need this extra variable.
  const float device_scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(PaintRenderingContext2D);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_PAINT_RENDERING_CONTEXT_2D_H_
