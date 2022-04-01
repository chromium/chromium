/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/frame_overlay.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

FrameOverlay::FrameOverlay(LocalFrame* local_frame,
                           std::unique_ptr<FrameOverlay::Delegate> delegate)
    : frame_(local_frame), delegate_(std::move(delegate)) {
  DCHECK(frame_);
  frame_->View()->SetVisualViewportOrOverlayNeedsRepaint();
}

FrameOverlay::~FrameOverlay() {
#if DCHECK_IS_ON()
  DCHECK(is_destroyed_);
#endif
}

void FrameOverlay::Destroy() {
  frame_->View()->SetVisualViewportOrOverlayNeedsRepaint();

  delegate_.reset();
#if DCHECK_IS_ON()
  is_destroyed_ = true;
#endif
}

void FrameOverlay::UpdatePrePaint() {
  // Invalidate DisplayItemClient.
  Invalidate();
  delegate_->Invalidate();
}

gfx::Size FrameOverlay::Size() const {
  gfx::Size size = frame_->GetPage()->GetVisualViewport().Size();
  if (!frame_->IsMainFrame() || frame_->IsInFencedFrameTree())
    size.SetToMax(frame_->View()->Size());
  return size;
}

void FrameOverlay::ServiceScriptedAnimations(
    base::TimeTicks monotonic_frame_begin_time) {
  delegate_->ServiceScriptedAnimations(monotonic_frame_begin_time);
}

void FrameOverlay::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  DisplayItemClient::Trace(visitor);
}

void FrameOverlay::Paint(GraphicsContext& context) const {
  ScopedPaintChunkProperties properties(context.GetPaintController(),
                                        DefaultPropertyTreeState(), *this,
                                        DisplayItem::kFrameOverlay);
  delegate_->PaintFrameOverlay(*this, context, Size());
}

PropertyTreeState FrameOverlay::DefaultPropertyTreeState() const {
  auto state = PropertyTreeState::Root();
  if (frame_->IsMainFrame() && !frame_->IsInFencedFrameTree()) {
    if (const auto* device_emulation = frame_->GetPage()
                                           ->GetVisualViewport()
                                           .GetDeviceEmulationTransformNode())
      state.SetTransform(*device_emulation);
  }
  return state;
}

}  // namespace blink
