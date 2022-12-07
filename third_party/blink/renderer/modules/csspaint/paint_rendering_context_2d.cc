// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"

#include <memory>
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"

namespace blink {

PaintRenderingContext2D::PaintRenderingContext2D(
    const gfx::Size& container_size,
    const PaintRenderingContext2DSettings* context_settings,
    float zoom,
    float device_scale_factor,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    PaintWorkletGlobalScope* global_scope)
    : BaseRenderingContext2D(std::move(task_runner)),
      container_size_(container_size),
      context_settings_(context_settings),
      effective_zoom_(zoom),
      global_scope_(global_scope) {
  InitializePaintRecorder();

  clip_antialiasing_ = kAntiAliased;
  GetState().SetShouldAntialias(true);

  GetPaintCanvas()->clear(context_settings->alpha() ? SkColors::kTransparent
                                                    : SkColors::kBlack);
  did_record_draw_commands_in_paint_recorder_ = true;
}

void PaintRenderingContext2D::InitializePaintRecorder() {
  cc::PaintCanvas* canvas = paint_recorder_.beginRecording(container_size_);

  // Always save an initial frame, to support resetting the top level matrix
  // and clip.
  canvas->save();

  scale(effective_zoom_, effective_zoom_);

  did_record_draw_commands_in_paint_recorder_ = false;
}

int PaintRenderingContext2D::Width() const {
  return container_size_.width();
}

int PaintRenderingContext2D::Height() const {
  return container_size_.height();
}

bool PaintRenderingContext2D::ParseColorOrCurrentColor(
    Color& color,
    const String& color_string) const {
  // We ignore "currentColor" for PaintRenderingContext2D and just make it
  // "black". "currentColor" can be emulated by having "color" as an input
  // property for the css-paint-api.
  // https://github.com/w3c/css-houdini-drafts/issues/133
  return ::blink::ParseColorOrCurrentColor(color, color_string, nullptr);
}

// We need to account for the |effective_zoom_| for shadow effects only, and not
// for line width. This is because the line width is affected by skia's current
// transform matrix (CTM) while shadows are not. The skia's CTM combines both
// the canvas context transform and the CSS layout transform. That means, the
// |effective_zoom_| is implictly applied to line width through CTM.
double PaintRenderingContext2D::shadowBlur() const {
  return BaseRenderingContext2D::shadowBlur() / effective_zoom_;
}

void PaintRenderingContext2D::setShadowBlur(double blur) {
  BaseRenderingContext2D::setShadowBlur(blur * effective_zoom_);
}

double PaintRenderingContext2D::shadowOffsetX() const {
  return BaseRenderingContext2D::shadowOffsetX() / effective_zoom_;
}

void PaintRenderingContext2D::setShadowOffsetX(double x) {
  BaseRenderingContext2D::setShadowOffsetX(x * effective_zoom_);
}

double PaintRenderingContext2D::shadowOffsetY() const {
  return BaseRenderingContext2D::shadowOffsetY() / effective_zoom_;
}

void PaintRenderingContext2D::setShadowOffsetY(double y) {
  BaseRenderingContext2D::setShadowOffsetY(y * effective_zoom_);
}

cc::PaintCanvas* PaintRenderingContext2D::GetPaintCanvas() const {
  DCHECK(paint_recorder_.getRecordingCanvas());
  return paint_recorder_.getRecordingCanvas();
}

cc::PaintCanvas* PaintRenderingContext2D::GetDrawingPaintCanvas() {
  DCHECK(paint_recorder_.getRecordingCanvas());
  did_record_draw_commands_in_paint_recorder_ = true;
  return paint_recorder_.getRecordingCanvas();
}

cc::PaintCanvas* PaintRenderingContext2D::GetPaintCanvasForDraw(
    const SkIRect&,
    CanvasPerformanceMonitor::DrawType) {
  return GetDrawingPaintCanvas();
}

void PaintRenderingContext2D::ValidateStateStackWithCanvas(
    const cc::PaintCanvas* canvas) const {
#if DCHECK_IS_ON()
  if (canvas) {
    DCHECK_EQ(static_cast<size_t>(canvas->getSaveCount()),
              state_stack_.size() + 1);
  }
#endif
}

sk_sp<PaintFilter> PaintRenderingContext2D::StateGetFilter() {
  return GetState().GetFilterForOffscreenCanvas(container_size_, this);
}

PredefinedColorSpace PaintRenderingContext2D::GetDefaultImageDataColorSpace()
    const {
  // PaintRenderingContext2D does not call getImageData or createImageData.
  NOTREACHED();
  return PredefinedColorSpace::kSRGB;
}

void PaintRenderingContext2D::WillOverwriteCanvas() {
  previous_frame_.reset();
  if (did_record_draw_commands_in_paint_recorder_) {
    // Discard previous draw commands
    paint_recorder_.finishRecordingAsPicture();
    InitializePaintRecorder();
  }
}

DOMMatrix* PaintRenderingContext2D::getTransform() {
  const AffineTransform& t = GetState().GetTransform();
  DOMMatrix* m = DOMMatrix::Create();
  m->setA(t.A() / effective_zoom_);
  m->setB(t.B() / effective_zoom_);
  m->setC(t.C() / effective_zoom_);
  m->setD(t.D() / effective_zoom_);
  m->setE(t.E() / effective_zoom_);
  m->setF(t.F() / effective_zoom_);
  return m;
}

// On a platform where zoom_for_dsf is not enabled, the recording canvas has its
// logic to account for the device scale factor. Therefore, when the transform
// of the canvas happen, we must account for the effective_zoom_ such that the
// recording canvas would have the correct behavior.
//
// The BaseRenderingContext2D::setTransform calls resetTransform, so integrating
// the effective_zoom_ in here instead of setTransform, to avoid integrating it
// twice if we have resetTransform and setTransform API calls.
void PaintRenderingContext2D::resetTransform() {
  BaseRenderingContext2D::resetTransform();
  BaseRenderingContext2D::transform(effective_zoom_, 0, 0, effective_zoom_, 0,
                                    0);
}

sk_sp<PaintRecord> PaintRenderingContext2D::GetRecord() {
  if (!did_record_draw_commands_in_paint_recorder_ && !!previous_frame_) {
    return previous_frame_;  // Reuse the previous frame
  }

  DCHECK(paint_recorder_.getRecordingCanvas());
  previous_frame_ = paint_recorder_.finishRecordingAsPicture();
  InitializePaintRecorder();
  return previous_frame_;
}

}  // namespace blink
