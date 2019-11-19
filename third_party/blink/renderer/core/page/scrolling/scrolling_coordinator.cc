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
#include "cc/layers/painted_overlay_scrollbar_layer.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
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

void ScrollingCoordinator::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
  visitor->Trace(horizontal_scrollbars_);
  visitor->Trace(vertical_scrollbars_);
}

void ScrollingCoordinator::NotifyGeometryChanged(LocalFrameView* frame_view) {
  frame_view->GetScrollingContext()->SetScrollGestureRegionIsDirty(true);
  frame_view->GetScrollingContext()->SetTouchEventTargetRectsAreDirty(true);
  frame_view->GetScrollingContext()->SetShouldScrollOnMainThreadIsDirty(true);
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

void ScrollingCoordinator::DidScroll(CompositorElementId element_id,
                                     const gfx::ScrollOffset& offset) {
  // Find the associated scrollable area using the element id and notify it of
  // the compositor-side scroll. We explicitly do not check the VisualViewport
  // which handles scroll offset differently (see: VisualViewport::didScroll).
  // Remote frames will receive DidScroll callbacks from their own compositor.
  // The ScrollableArea with matching ElementId may have been deleted and we can
  // safely ignore the DidScroll callback.
  if (auto* scrollable =
          ScrollableAreaWithElementIdInAllLocalFrames(element_id)) {
    scrollable->DidScroll(FloatPoint(offset.x(), offset.y()));
  }
}

void ScrollingCoordinator::DidChangeScrollbarsHidden(
    CompositorElementId element_id,
    bool hidden) {
  // See the above function for the case of null scrollable area.
  if (auto* scrollable =
          ScrollableAreaWithElementIdInAllLocalFrames(element_id)) {
    scrollable->SetScrollbarsHiddenIfOverlay(hidden);
  }
}

void ScrollingCoordinator::UpdateAfterPaint(LocalFrameView* frame_view) {
  LocalFrame* frame = &frame_view->GetFrame();
  DCHECK(frame->IsLocalRoot());

  bool scroll_gesture_region_dirty =
      frame_view->GetScrollingContext()->ScrollGestureRegionIsDirty();
  bool touch_event_rects_dirty =
      frame_view->GetScrollingContext()->TouchEventTargetRectsAreDirty();
  bool should_scroll_on_main_thread_dirty =
      frame_view->GetScrollingContext()->ShouldScrollOnMainThreadIsDirty();
  bool frame_scroller_dirty = FrameScrollerIsDirty(frame_view);

  if (!(scroll_gesture_region_dirty || touch_event_rects_dirty ||
        should_scroll_on_main_thread_dirty || frame_scroller_dirty)) {
    return;
  }

  SCOPED_UMA_AND_UKM_TIMER(frame_view->EnsureUkmAggregator(),
                           LocalFrameUkmAggregator::kScrollingCoordinator);
  TRACE_EVENT0("input", "ScrollingCoordinator::UpdateAfterPaint");

  if (scroll_gesture_region_dirty) {
    UpdateNonFastScrollableRegions(frame);
    frame_view->GetScrollingContext()->SetScrollGestureRegionIsDirty(false);
  }

  if (!(touch_event_rects_dirty || should_scroll_on_main_thread_dirty ||
        frame_scroller_dirty)) {
    return;
  }

  if (touch_event_rects_dirty) {
    UpdateTouchEventTargetRectsIfNeeded(frame);
    frame_view->GetScrollingContext()->SetTouchEventTargetRectsAreDirty(false);
  }

  if (should_scroll_on_main_thread_dirty ||
      frame_view->FrameIsScrollableDidChange()) {
    // TODO(pdr): Now that BlinkGenPropertyTrees has launched, we should remove
    // FrameIsScrollableDidChange.
    frame_view->GetScrollingContext()->SetShouldScrollOnMainThreadIsDirty(
        false);
  }
  frame_view->ClearFrameIsScrollableDidChange();
}

template <typename Function>
static void ForAllPaintingGraphicsLayers(GraphicsLayer& layer,
                                         const Function& function) {
  // Don't recurse into display-locked elements.
  if (layer.Client().PaintBlockedByDisplayLockIncludingAncestors(
          DisplayLockContextLifecycleTarget::kSelf)) {
    return;
  }

  if (layer.PaintsContentOrHitTest())
    function(layer);

  for (auto* child : layer.Children())
    ForAllPaintingGraphicsLayers(*child, function);
}

// Set the non-fast scrollable regions on |layer|'s cc layer.
static void UpdateLayerNonFastScrollableRegions(GraphicsLayer& layer) {
  // CompositeAfterPaint does this update in PaintArtifactCompositor.
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  DCHECK(layer.PaintsContentOrHitTest());

  if (layer.Client().ShouldThrottleRendering()) {
    layer.CcLayer()->SetNonFastScrollableRegion(cc::Region());
    return;
  }

  auto offset = layer.GetOffsetFromTransformNode();
  gfx::Vector2dF layer_offset = gfx::Vector2dF(offset.X(), offset.Y());
  PaintChunkSubset paint_chunks =
      PaintChunkSubset(layer.GetPaintController().PaintChunks());
  PaintArtifactCompositor::UpdateNonFastScrollableRegions(
      layer.CcLayer(), layer_offset, layer.GetPropertyTreeState(),
      paint_chunks);
}

// Compute the regions of the page where we can't handle scroll gestures on
// the impl thread. This currently includes:
// 1. All scrollable areas, such as subframes, overflow divs and list boxes,
//    whose composited scrolling are not enabled. We need to do this even if
//    the frame view whose layout was updated is not the main frame.
// 2. Resize control areas, e.g. the small rect at the right bottom of
//    div/textarea/iframe when CSS property "resize" is enabled.
// 3. Plugin areas.
void ScrollingCoordinator::UpdateNonFastScrollableRegions(LocalFrame* frame) {
  auto* view_layer = frame->View()->GetLayoutView()->Layer();
  if (auto* root = view_layer->Compositor()->PaintRootGraphicsLayer())
    ForAllPaintingGraphicsLayers(*root, UpdateLayerNonFastScrollableRegions);
}

// Set the touch action rects on the cc layer from the touch action data stored
// on the GraphicsLayer's paint chunks.
static void UpdateLayerTouchActionRects(GraphicsLayer& layer) {
  if (layer.Client().ShouldThrottleRendering()) {
    layer.CcLayer()->SetTouchActionRegion(cc::TouchActionRegion());
    return;
  }

  auto offset = layer.GetOffsetFromTransformNode();
  gfx::Vector2dF layer_offset = gfx::Vector2dF(offset.X(), offset.Y());
  PaintChunkSubset paint_chunks =
      PaintChunkSubset(layer.GetPaintController().PaintChunks());
  PaintArtifactCompositor::UpdateTouchActionRects(layer.CcLayer(), layer_offset,
                                                  layer.GetPropertyTreeState(),
                                                  paint_chunks);
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
  if (scoped_refptr<cc::ScrollbarLayerBase> scrollbar_layer =
          scrollbars.Take(scrollable_area)) {
    GraphicsLayer::UnregisterContentsLayer(scrollbar_layer.get());
  }
}

static scoped_refptr<cc::ScrollbarLayerBase> CreateScrollbarLayer(
    Scrollbar& scrollbar,
    float device_scale_factor) {
  ScrollbarTheme& theme = scrollbar.GetTheme();
  auto scrollbar_delegate = base::MakeRefCounted<ScrollbarLayerDelegate>(
      scrollbar, device_scale_factor);
  scoped_refptr<cc::ScrollbarLayerBase> scrollbar_layer;
  if (theme.UsesOverlayScrollbars() && theme.UsesNinePatchThumbResource()) {
    scrollbar_layer =
        cc::PaintedOverlayScrollbarLayer::Create(std::move(scrollbar_delegate));
  } else {
    scrollbar_layer =
        cc::PaintedScrollbarLayer::Create(std::move(scrollbar_delegate));
  }
  scrollbar_layer->SetElementId(scrollbar.GetElementId());
  GraphicsLayer::RegisterContentsLayer(scrollbar_layer.get());
  return scrollbar_layer;
}

scoped_refptr<cc::ScrollbarLayerBase>
ScrollingCoordinator::CreateSolidColorScrollbarLayer(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar,
    cc::ElementId element_id) {
  cc::ScrollbarOrientation cc_orientation =
      orientation == kHorizontalScrollbar ? cc::HORIZONTAL : cc::VERTICAL;
  auto scrollbar_layer = cc::SolidColorScrollbarLayer::Create(
      cc_orientation, thumb_thickness, track_start,
      is_left_side_vertical_scrollbar);
  scrollbar_layer->SetElementId(element_id);
  GraphicsLayer::RegisterContentsLayer(scrollbar_layer.get());
  return scrollbar_layer;
}

static void DetachScrollbarLayer(GraphicsLayer* scrollbar_graphics_layer) {
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
    DetachScrollbarLayer(scrollbar_graphics_layer);
    return;
  }

  scrollbar_layer->SetScrollElementId(scrolling_layer->element_id());
  scrollbar_graphics_layer->SetContentsToCcLayer(
      scrollbar_layer,
      /*prevent_contents_opaque_changes=*/false);
  scrollbar_graphics_layer->SetDrawsContent(false);
  scrollbar_graphics_layer->SetHitTestable(false);
}

void ScrollingCoordinator::AddScrollbarLayer(
    ScrollableArea* scrollable_area,
    ScrollbarOrientation orientation,
    scoped_refptr<cc::ScrollbarLayerBase> scrollbar_layer_group) {
  ScrollbarMap& scrollbars = orientation == kHorizontalScrollbar
                                 ? horizontal_scrollbars_
                                 : vertical_scrollbars_;
  scrollbars.insert(scrollable_area, std::move(scrollbar_layer_group));
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
      DetachScrollbarLayer(scrollbar_graphics_layer);
      scrollbar_graphics_layer->CcLayer()->SetIsScrollbar(true);
      return;
    }

    cc::ScrollbarLayerBase* scrollbar_layer =
        GetScrollbarLayer(scrollable_area, orientation);
    if (!scrollbar_layer) {
      scoped_refptr<cc::ScrollbarLayerBase> new_scrollbar_layer;
      if (scrollbar.IsSolidColor()) {
        DCHECK(scrollbar.IsOverlayScrollbar());
        new_scrollbar_layer = CreateSolidColorScrollbarLayer(
            orientation, scrollbar.GetTheme().ThumbThickness(scrollbar),
            scrollbar.GetTheme().TrackPosition(scrollbar),
            scrollable_area->ShouldPlaceVerticalScrollbarOnLeft(),
            scrollable_area->GetScrollbarElementId(orientation));
      } else {
        new_scrollbar_layer = CreateScrollbarLayer(
            scrollbar, page_->DeviceScaleFactorDeprecated());
      }

      scrollbar_layer = new_scrollbar_layer.get();
      AddScrollbarLayer(scrollable_area, orientation,
                        std::move(new_scrollbar_layer));
    }

    cc::Layer* scroll_layer = scrollable_area->LayerForScrolling();
    SetupScrollbarLayer(scrollbar_graphics_layer, scrollbar_layer,
                        scroll_layer);

    // Root layer non-overlay scrollbars should be marked opaque to disable
    // blending.
    bool is_opaque_scrollbar = !scrollbar.IsOverlayScrollbar();
    scrollbar_graphics_layer->SetContentsOpaque(
        IsForMainFrame(scrollable_area) && is_opaque_scrollbar);
  } else {
    RemoveScrollbarLayer(scrollable_area, orientation);
  }
}

bool ScrollingCoordinator::UpdateCompositedScrollOffset(
    ScrollableArea* scrollable_area) {
  cc::Layer* scroll_layer = scrollable_area->LayerForScrolling();
  scroll_layer->SetScrollOffset(
      static_cast<gfx::ScrollOffset>(scrollable_area->ScrollPosition()));
  return true;
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

    IntSize container_size = scrollable_area->VisibleContentRect().Size();
    cc_layer->SetScrollable(gfx::Size(container_size));

    // The scrolling contents layer must be at least as large as its clip.
    // The visual viewport is special because the size of its scrolling
    // content depends on the page scale factor. Its scrollable content is
    // the layout viewport which is sized based on the minimum allowed page
    // scale so it actually can be smaller than its clip.
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

void ScrollingCoordinator::UpdateTouchEventTargetRectsIfNeeded(
    LocalFrame* frame) {
  TRACE_EVENT0("input",
               "ScrollingCoordinator::updateTouchEventTargetRectsIfNeeded");

  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  auto* view_layer = frame->View()->GetLayoutView()->Layer();
  if (auto* root = view_layer->Compositor()->PaintRootGraphicsLayer())
    ForAllPaintingGraphicsLayers(*root, UpdateLayerTouchActionRects);
}

void ScrollingCoordinator::Reset(LocalFrame* frame) {
  for (const auto& scrollbar : horizontal_scrollbars_)
    GraphicsLayer::UnregisterContentsLayer(scrollbar.value.get());
  for (const auto& scrollbar : vertical_scrollbars_)
    GraphicsLayer::UnregisterContentsLayer(scrollbar.value.get());

  horizontal_scrollbars_.clear();
  vertical_scrollbars_.clear();
  frame->View()->ClearFrameIsScrollableDidChange();
}

void ScrollingCoordinator::TouchEventTargetRectsDidChange(LocalFrame* frame) {
  if (!frame)
    return;

  // If frame is not a local root, then the call to StaleInCompositingMode()
  // below may unexpectedly fail.
  DCHECK(frame->IsLocalRoot());
  LocalFrameView* frame_view = frame->View();
  if (!frame_view)
    return;

  // Wait until after layout to update.
  // TODO(pdr): This check is wrong as we need to mark the rects as dirty
  // regardless of whether the frame view needs layout. Remove this check.
  if (frame_view->NeedsLayout())
    return;

  // FIXME: scheduleAnimation() is just a method of forcing the compositor to
  // realize that it needs to commit here. We should expose a cleaner API for
  // this.
  auto* layout_view = frame->ContentLayoutObject();
  if (layout_view && layout_view->Compositor() &&
      layout_view->Compositor()->StaleInCompositingMode()) {
    frame_view->ScheduleAnimation();
  }

  frame_view->GetScrollingContext()->SetTouchEventTargetRectsAreDirty(true);
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
  for (const auto& scrollbar : horizontal_scrollbars_)
    GraphicsLayer::UnregisterContentsLayer(scrollbar.value.get());
  for (const auto& scrollbar : vertical_scrollbars_)
    GraphicsLayer::UnregisterContentsLayer(scrollbar.value.get());
}

bool ScrollingCoordinator::CoordinatesScrollingForFrameView(
    LocalFrameView* frame_view) const {
  DCHECK(IsMainThread());

  // We currently only support composited mode.
  auto* layout_view = frame_view->GetFrame().ContentLayoutObject();
  if (!layout_view)
    return false;
  return layout_view->UsesCompositing();
}

void ScrollingCoordinator::
    FrameViewHasBackgroundAttachmentFixedObjectsDidChange(
        LocalFrameView* frame_view) {
  DCHECK(IsMainThread());
  DCHECK(frame_view);

  if (!CoordinatesScrollingForFrameView(frame_view))
    return;

  frame_view->GetScrollingContext()->SetShouldScrollOnMainThreadIsDirty(true);
}

void ScrollingCoordinator::FrameViewFixedObjectsDidChange(
    LocalFrameView* frame_view) {
  DCHECK(IsMainThread());
  DCHECK(frame_view);

  if (!CoordinatesScrollingForFrameView(frame_view))
    return;

  frame_view->GetScrollingContext()->SetShouldScrollOnMainThreadIsDirty(true);
}

bool ScrollingCoordinator::IsForMainFrame(
    ScrollableArea* scrollable_area) const {
  if (!IsA<LocalFrame>(page_->MainFrame()))
    return false;

  // FIXME(305811): Refactor for OOPI.
  return scrollable_area ==
         page_->DeprecatedLocalMainFrame()->View()->LayoutViewport();
}

void ScrollingCoordinator::FrameViewRootLayerDidChange(
    LocalFrameView* frame_view) {
  DCHECK(IsMainThread());
  DCHECK(page_);

  if (!CoordinatesScrollingForFrameView(frame_view))
    return;

  NotifyGeometryChanged(frame_view);
}

bool ScrollingCoordinator::FrameScrollerIsDirty(
    LocalFrameView* frame_view) const {
  DCHECK(frame_view);
  // TODO(bokan): This should probably be checking the root scroller in the
  // FrameView, rather than the frame_view.
  if (frame_view->FrameIsScrollableDidChange())
    return true;

  if (cc::Layer* scroll_layer =
          frame_view->LayoutViewport()->LayerForScrolling()) {
    return static_cast<gfx::Size>(
               frame_view->LayoutViewport()->ContentsSize()) !=
           scroll_layer->bounds();
  }
  return false;
}

}  // namespace blink
