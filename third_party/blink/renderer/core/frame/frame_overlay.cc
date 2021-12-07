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
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer_client.h"
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
  if (layer_)
    layer_.Release()->Destroy();

#if DCHECK_IS_ON()
  is_destroyed_ = true;
#endif
}

void FrameOverlay::UpdatePrePaint() {
  // Invalidate DisplayItemClient.
  Invalidate();

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    delegate_->Invalidate();
    return;
  }

  auto* parent_layer = frame_->LocalFrameRoot()
                           .View()
                           ->GetLayoutView()
                           ->Compositor()
                           ->PaintRootGraphicsLayer();
  if (!parent_layer) {
    layer_ = nullptr;
    return;
  }

  if (!layer_) {
    layer_ = MakeGarbageCollected<GraphicsLayer>(*this);
    layer_->SetDrawsContent(true);
    layer_->SetHitTestable(false);
  }

  DCHECK(parent_layer);
  if (layer_->Parent() != parent_layer ||
      // Keep the layer the last child of parent to make it topmost.
      parent_layer->Children().back() != layer_)
    parent_layer->AddChild(layer_);
  layer_->SetLayerState(DefaultPropertyTreeState(), gfx::Vector2d());
  layer_->SetSize(Size());
}

gfx::Size FrameOverlay::Size() const {
  gfx::Size size = frame_->GetPage()->GetVisualViewport().Size();
  if (!frame_->IsMainFrame())
    size.SetToMax(frame_->View()->Size());
  return size;
}

gfx::Rect FrameOverlay::ComputeInterestRect(const GraphicsLayer* graphics_layer,
                                            const gfx::Rect&) const {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  DCHECK(!RuntimeEnabledFeatures::CullRectUpdateEnabled());
  return gfx::Rect(gfx::Point(), Size());
}

gfx::Rect FrameOverlay::PaintableRegion(
    const GraphicsLayer* graphics_layer) const {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  DCHECK(RuntimeEnabledFeatures::CullRectUpdateEnabled());
  return gfx::Rect(gfx::Point(), Size());
}

void FrameOverlay::PaintContents(const GraphicsLayer* graphics_layer,
                                 GraphicsContext& context,
                                 GraphicsLayerPaintingPhase phase,
                                 const gfx::Rect& interest_rect) const {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  DCHECK_EQ(graphics_layer, layer_);
  DCHECK_EQ(DefaultPropertyTreeState(), layer_->GetPropertyTreeState());
  Paint(context);
}

void FrameOverlay::GraphicsLayersDidChange() {
  frame_->View()->SetPaintArtifactCompositorNeedsUpdate();
}

PaintArtifactCompositor* FrameOverlay::GetPaintArtifactCompositor() {
  return frame_->View()->GetPaintArtifactCompositor();
}

void FrameOverlay::ServiceScriptedAnimations(
    base::TimeTicks monotonic_frame_begin_time) {
  delegate_->ServiceScriptedAnimations(monotonic_frame_begin_time);
}

String FrameOverlay::DebugName(const GraphicsLayer*) const {
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  return "Frame Overlay Content Layer";
}

void FrameOverlay::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(layer_);
  GraphicsLayerClient::Trace(visitor);
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
  if (frame_->IsMainFrame()) {
    if (const auto* device_emulation = frame_->GetPage()
                                           ->GetVisualViewport()
                                           .GetDeviceEmulationTransformNode())
      state.SetTransform(*device_emulation);
  }
  return state;
}

}  // namespace blink
