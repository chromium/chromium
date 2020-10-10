/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator_context.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_layer_delegate.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

ScrollingCoordinator::ScrollingCoordinator(Page* page) : page_(page) {}

ScrollingCoordinator::~ScrollingCoordinator() {
  DCHECK(!page_);
}

void ScrollingCoordinator::Trace(Visitor* visitor) const {
  visitor->Trace(page_);
  visitor->Trace(horizontal_scrollbars_);
  visitor->Trace(vertical_scrollbars_);
}

ScrollableArea*
ScrollingCoordinator::ScrollableAreaWithElementIdInAllLocalFrames(
    const CompositorElementId& id) {
  for (auto* frame = page_->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;

    // Find the associated scrollable area using the element id.
    if (LocalFrameView* view = local_frame->View()) {
      if (auto* scrollable = view->ScrollableAreaWithElementId(id)) {
        return scrollable;
      }
    }
  }
  // The ScrollableArea with matching ElementId does not exist in local frames.
  return nullptr;
}

void ScrollingCoordinator::DidScroll(
    CompositorElementId element_id,
    const gfx::ScrollOffset& offset,
    const base::Optional<cc::TargetSnapAreaElementIds>& snap_target_ids) {
  // Find the associated scrollable area using the element id and notify it of
  // the compositor-side scroll. We explicitly do not check the VisualViewport
  // which handles scroll offset differently (see: VisualViewport::didScroll).
  // Remote frames will receive DidScroll callbacks from their own compositor.
  // The ScrollableArea with matching ElementId may have been deleted and we can
  // safely ignore the DidScroll callback.
  auto* scrollable = ScrollableAreaWithElementIdInAllLocalFrames(element_id);
  if (!scrollable)
    return;
  scrollable->DidScroll(FloatPoint(offset.x(), offset.y()));
  if (snap_target_ids)
    scrollable->SetTargetSnapAreaElementIds(snap_target_ids.value());
}

void ScrollingCoordinator::DidChangeScrollbarsHidden(
    CompositorElementId element_id,
    bool hidden) {
  // See the above function for the case of null scrollable area.
  if (auto* scrollable =
          ScrollableAreaWithElementIdInAllLocalFrames(element_id)) {
    // On Mac, we'll only receive these visibility changes if device emulation
    // is enabled and we're using the Android ScrollbarController. Make sure we
    // stop listening when device emulation is turned off since we might still
    // get a lagging message from the compositor before it finds out.
    if (scrollable->GetPageScrollbarTheme().BlinkControlsOverlayVisibility())
      scrollable->SetScrollbarsHiddenIfOverlay(hidden);
  }
}

void ScrollingCoordinator::WillDestroyScrollableArea(
    ScrollableArea* scrollable_area) {
  RemoveScrollbarLayer(scrollable_area, kHorizontalScrollbar);
  RemoveScrollbarLayer(scrollable_area, kVerticalScrollbar);
}

void ScrollingCoordinator::RemoveScrollbarLayer(
    ScrollableArea* scrollable_area,
    ScrollbarOrientation orientation) {
  ScrollbarMap& scrollbars = orientation == kHorizontalScrollbar
                                 ? horizontal_scrollbars_
                                 : vertical_scrollbars_;
  scrollbars.erase(scrollable_area);
}

static void DetachScrollbarLayerFromGraphicsLayer(
    GraphicsLayer* scrollbar_graphics_layer) {
  DCHECK(scrollbar_graphics_layer);

  scrollbar_graphics_layer->SetContentsToCcLayer(nullptr, false);
  scrollbar_graphics_layer->SetDrawsContent(true);
  scrollbar_graphics_layer->SetHitTestable(true);
}

static void SetupScrollbarLayer(GraphicsLayer* scrollbar_graphics_layer,
                                cc::ScrollbarLayerBase* scrollbar_layer,
                                cc::Layer* scrolling_layer) {
  DCHECK(scrollbar_graphics_layer);

  if (!scrolling_layer) {
    DetachScrollbarLayerFromGraphicsLayer(scrollbar_graphics_layer);
    return;
  }

  scrollbar_layer->SetScrollElementId(scrolling_layer->element_id());
  scrollbar_graphics_layer->SetContentsToCcLayer(
      scrollbar_layer,
      /*prevent_contents_opaque_changes=*/false);
  scrollbar_graphics_layer->SetDrawsContent(false);
  scrollbar_graphics_layer->SetHitTestable(false);
}

void ScrollingCoordinator::SetScrollbarLayer(
    ScrollableArea* scrollable_area,
    ScrollbarOrientation orientation,
    scoped_refptr<cc::ScrollbarLayerBase> scrollbar_layer) {
  ScrollbarMap& scrollbars = orientation == kHorizontalScrollbar
                                 ? horizontal_scrollbars_
                                 : vertical_scrollbars_;
  scrollbars.Set(scrollable_area, std::move(scrollbar_layer));
}

cc::ScrollbarLayerBase* ScrollingCoordinator::GetScrollbarLayer(
    ScrollableArea* scrollable_area,
    ScrollbarOrientation orientation) {
  ScrollbarMap& scrollbars = orientation == kHorizontalScrollbar
                                 ? horizontal_scrollbars_
                                 : vertical_scrollbars_;
  return scrollbars.at(scrollable_area);
}

void ScrollingCoordinator::ScrollableAreaScrollbarLayerDidChange(
    PaintLayerScrollableArea* scrollable_area,
    ScrollbarOrientation orientation) {
  if (!page_ || !page_->MainFrame())
    return;

  GraphicsLayer* scrollbar_graphics_layer =
      orientation == kHorizontalScrollbar
          ? scrollable_area->GraphicsLayerForHorizontalScrollbar()
          : scrollable_area->GraphicsLayerForVerticalScrollbar();

  if (scrollbar_graphics_layer) {
    Scrollbar& scrollbar = orientation == kHorizontalScrollbar
                               ? *scrollable_area->HorizontalScrollbar()
                               : *scrollable_area->VerticalScrollbar();
    if (scrollbar.IsCustomScrollbar()) {
      // |scrollbar_graphics_layer| and the cc::PictureLayer in it will be used
      // for the custom scrollbar, without any special cc scrollbar layer.
      DetachScrollbarLayerFromGraphicsLayer(scrollbar_graphics_layer);
      return;
    }

    cc::ScrollbarLayerBase* scrollbar_layer =
        GetScrollbarLayer(scrollable_area, orientation);
    auto scrollbar_delegate = base::MakeRefCounted<ScrollbarLayerDelegate>(
        scrollbar, page_->DeviceScaleFactorDeprecated());
    scoped_refptr<cc::ScrollbarLayerBase> new_scrollbar_layer =
        cc::ScrollbarLayerBase::CreateOrReuse(std::move(scrollbar_delegate),
                                              scrollbar_layer);
    new_scrollbar_layer->SetElementId(scrollbar.GetElementId());
    SetupScrollbarLayer(scrollbar_graphics_layer, new_scrollbar_layer.get(),
                        scrollable_area->LayerForScrolling());
    SetScrollbarLayer(scrollable_area, orientation,
                      std::move(new_scrollbar_layer));

    // Root layer non-overlay scrollbars should be marked opaque to disable
    // blending.
    bool is_opaque_scrollbar = !scrollbar.IsOverlayScrollbar();
    scrollbar_graphics_layer->SetContentsOpaque(
        IsForMainFrame(scrollable_area) && is_opaque_scrollbar);
  } else {
    RemoveScrollbarLayer(scrollable_area, orientation);
  }
}

bool ScrollingCoordinator::UpdateCompositorScrollOffset(
    const LocalFrame& frame,
    const ScrollableArea& scrollable_area) {
  auto* paint_artifact_compositor =
      frame.LocalFrameRoot().View()->GetPaintArtifactCompositor();
  if (!paint_artifact_compositor)
    return false;
  return paint_artifact_compositor->DirectlySetScrollOffset(
      scrollable_area.GetScrollElementId(), scrollable_area.ScrollPosition());
}

void ScrollingCoordinator::ScrollableAreaScrollLayerDidChange(
    PaintLayerScrollableArea* scrollable_area) {
  if (!page_ || !page_->MainFrame())
    return;

  cc::Layer* cc_layer = scrollable_area->LayerForScrolling();
  if (cc_layer) {
    auto* graphics_layer = scrollable_area->GraphicsLayerForScrolling();
    DCHECK(graphics_layer);
    // TODO(bokan): This method shouldn't be resizing the layer geometry. That
    // happens in CompositedLayerMapping::UpdateScrollingLayerGeometry.
    DCHECK(scrollable_area->Layer());
    DCHECK(scrollable_area->GetLayoutBox());
    PhysicalOffset subpixel_accumulation =
        scrollable_area->Layer()->SubpixelAccumulation();
    PhysicalSize contents_size(scrollable_area->GetLayoutBox()->ScrollWidth(),
                               scrollable_area->GetLayoutBox()->ScrollHeight());
    IntSize scroll_contents_size =
        PhysicalRect(subpixel_accumulation, contents_size).PixelSnappedSize();

    // The scrolling contents layer must be at least as large as its clip.
    // The visual viewport is special because the size of its scrolling
    // content depends on the page scale factor. Its scrollable content is
    // the layout viewport which is sized based on the minimum allowed page
    // scale so it actually can be smaller than its clip.
    IntSize container_size = scrollable_area->VisibleContentRect().Size();
    scroll_contents_size = scroll_contents_size.ExpandedTo(container_size);

    // This call has to go through the GraphicsLayer method to preserve
    // invalidation code there.
    graphics_layer->SetSize(gfx::Size(scroll_contents_size));
  }
  if (cc::ScrollbarLayerBase* scrollbar_layer =
          GetScrollbarLayer(scrollable_area, kHorizontalScrollbar)) {
    if (GraphicsLayer* horizontal_scrollbar_layer =
            scrollable_area->GraphicsLayerForHorizontalScrollbar()) {
      SetupScrollbarLayer(horizontal_scrollbar_layer, scrollbar_layer,
                          cc_layer);
    }
  }
  if (cc::ScrollbarLayerBase* scrollbar_layer =
          GetScrollbarLayer(scrollable_area, kVerticalScrollbar)) {
    if (GraphicsLayer* vertical_scrollbar_layer =
            scrollable_area->GraphicsLayerForVerticalScrollbar()) {
      SetupScrollbarLayer(vertical_scrollbar_layer, scrollbar_layer, cc_layer);
    }
  }

  scrollable_area->LayerForScrollingDidChange(
      scrollable_area->GetCompositorAnimationTimeline());
}

void ScrollingCoordinator::Reset(LocalFrame* frame) {
  horizontal_scrollbars_.clear();
  vertical_scrollbars_.clear();
}

void ScrollingCoordinator::AnimationHostInitialized(
    cc::AnimationHost& animation_host,
    LocalFrameView* view) {
  if (!Platform::Current()->IsThreadedAnimationEnabled())
    return;

  auto timeline = std::make_unique<CompositorAnimationTimeline>();
  if (view && view->GetFrame().LocalFrameRoot() != page_->MainFrame()) {
    view->GetScrollingContext()->SetAnimationHost(&animation_host);
    view->GetScrollingContext()->SetAnimationTimeline(std::move(timeline));
    view->GetCompositorAnimationHost()->AddAnimationTimeline(
        view->GetCompositorAnimationTimeline()->GetAnimationTimeline());
  } else {
    animation_host_ = &animation_host;
    programmatic_scroll_animator_timeline_ = std::move(timeline);
    animation_host_->AddAnimationTimeline(
        programmatic_scroll_animator_timeline_->GetAnimationTimeline());
  }
}

void ScrollingCoordinator::WillCloseAnimationHost(LocalFrameView* view) {
  if (view && view->GetFrame().LocalFrameRoot() != page_->MainFrame()) {
    view->GetCompositorAnimationHost()->RemoveAnimationTimeline(
        view->GetCompositorAnimationTimeline()->GetAnimationTimeline());
    view->GetScrollingContext()->SetAnimationTimeline(nullptr);
    view->GetScrollingContext()->SetAnimationHost(nullptr);
  } else if (programmatic_scroll_animator_timeline_) {
    animation_host_->RemoveAnimationTimeline(
        programmatic_scroll_animator_timeline_->GetAnimationTimeline());
    programmatic_scroll_animator_timeline_ = nullptr;
    animation_host_ = nullptr;
  }
}

void ScrollingCoordinator::WillBeDestroyed() {
  DCHECK(page_);
  page_ = nullptr;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool ScrollingCoordinator::IsForMainFrame(
    ScrollableArea* scrollable_area) const {
  if (!IsA<LocalFrame>(page_->MainFrame()))
    return false;

  // FIXME(305811): Refactor for OOPI.
  return scrollable_area ==
         page_->DeprecatedLocalMainFrame()->View()->LayoutViewport();
}

}  // namespace blink
