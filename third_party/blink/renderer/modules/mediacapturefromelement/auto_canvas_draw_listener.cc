// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/auto_canvas_draw_listener.h"

#include <memory>

#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

AutoCanvasDrawListener::AutoCanvasDrawListener(
    std::unique_ptr<CanvasCaptureHandler> handler)
    : handler_(std::move(handler)), frame_capture_requested_(true) {}

CanvasDrawListener::NewFrameCallback
AutoCanvasDrawListener::GetNewFrameCallback() {
  return handler_->GetNewFrameCallback();
}

bool AutoCanvasDrawListener::CanDiscardAlpha() const {
  return handler_->CanDiscardAlpha();
}

bool AutoCanvasDrawListener::NeedsNewFrame() const {
  return frame_capture_requested_ && handler_->NeedsNewFrame();
}

void AutoCanvasDrawListener::RequestFrame() {
  frame_capture_requested_ = true;
}

}  // namespace blink
