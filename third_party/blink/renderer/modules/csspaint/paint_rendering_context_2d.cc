// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"

#include <memory>

#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_2d_recorder_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"

namespace blink {

class MemoryManagedPaintCanvas;

PaintRenderingContext2D::PaintRenderingContext2D(
    const gfx::Size& container_size,
    const PaintRenderingContext2DSettings* context_settings,
    float zoom,
    PaintWorkletGlobalScope* global_scope)
    : Canvas2DRecorderContext(zoom),
      paint_recorder_(container_size, this),
      container_size_(container_size),
      context_settings_(context_settings),
      global_scope_(global_scope) {
  scale(zoom, zoom);

  clip_antialiasing_ = kAntiAliased;
  GetState().SetShouldAntialias(true);

  GetPaintCanvas()->clear(context_settings->alpha() ? SkColors::kTransparent
                                                    : SkColors::kBlack);
}

void PaintRenderingContext2D::InitializeForRecording(
    cc::PaintCanvas* canvas) const {
  RestoreMatrixClipStack(canvas);
}

void PaintRenderingContext2D::RecordingCleared() {
  previous_frame_ = std::nullopt;
}

int PaintRenderingContext2D::Width() const {
  return container_size_.width();
}

int PaintRenderingContext2D::Height() const {
  return container_size_.height();
}

Color PaintRenderingContext2D::GetCurrentColor() const {
  // We ignore "currentColor" for PaintRenderingContext2D and just make it
  // "black". "currentColor" can be emulated by having "color" as an input
  // property for the css-paint-api.
  // https://github.com/w3c/css-houdini-drafts/issues/133
  return Color::kBlack;
}

const MemoryManagedPaintCanvas* PaintRenderingContext2D::GetPaintCanvas()
    const {
  return &paint_recorder_.getRecordingCanvas();
}

void PaintRenderingContext2D::WillDraw(const SkIRect&,
                                       CanvasPerformanceMonitor::DrawType) {}

sk_sp<PaintFilter> PaintRenderingContext2D::StateGetFilter() {
  return GetState().GetFilterForOffscreenCanvas(container_size_, this);
}

PredefinedColorSpace PaintRenderingContext2D::GetDefaultImageDataColorSpace()
    const {
  // PaintRenderingContext2D does not call getImageData or createImageData.
  NOTREACHED();
}

void PaintRenderingContext2D::reset() {
  Canvas2DRecorderContext::reset();
  Canvas2DRecorderContext::transform(effective_zoom_, 0, 0, effective_zoom_, 0,
                                     0);
}

PaintRecord PaintRenderingContext2D::GetRecord() {
  if (!paint_recorder_.HasRecordedDrawOps() && !!previous_frame_) {
    return *previous_frame_;  // Reuse the previous frame
  }

  return paint_recorder_.ReleaseMainRecording();
}

}  // namespace blink
