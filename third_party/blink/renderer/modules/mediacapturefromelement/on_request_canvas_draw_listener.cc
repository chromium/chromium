// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/on_request_canvas_draw_listener.h"

#include "third_party/skia/include/core/SkImage.h"

namespace blink {

OnRequestCanvasDrawListener::OnRequestCanvasDrawListener(
    std::unique_ptr<CanvasCaptureHandler> handler)
    : AutoCanvasDrawListener(std::move(handler)) {}

OnRequestCanvasDrawListener::~OnRequestCanvasDrawListener() = default;

CanvasDrawListener::NewFrameCallback
OnRequestCanvasDrawListener::GetNewFrameCallback() {
  frame_capture_requested_ = false;
  return AutoCanvasDrawListener::GetNewFrameCallback();
}

void OnRequestCanvasDrawListener::Trace(Visitor* visitor) const {
  AutoCanvasDrawListener::Trace(visitor);
}

}  // namespace blink
