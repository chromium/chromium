// Copyright 2015 The Chromium Authors. All rights reserved.
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

void AutoCanvasDrawListener::SendNewFrame(
    sk_sp<SkImage> image,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider) {
  handler_->SendNewFrame(
      image, context_provider ? context_provider->ContextProvider() : nullptr);
}

bool AutoCanvasDrawListener::NeedsNewFrame() const {
  return frame_capture_requested_ && handler_->NeedsNewFrame();
}

void AutoCanvasDrawListener::RequestFrame() {
  frame_capture_requested_ = true;
}

}  // namespace blink
