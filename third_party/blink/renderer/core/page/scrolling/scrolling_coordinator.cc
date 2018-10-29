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
#include "cc/layers/layer_position_constraint.h"
#include "cc/layers/painted_overlay_scrollbar_layer.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/scrollbar_layer_interface.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/input/touch_action_util.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_geometry_map.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator_context.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_host.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/geometry/region.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/transforms/transform_state.h"
#if defined(OS_MACOSX)
#include "third_party/blink/renderer/core/scroll/scroll_animator_mac.h"
#endif
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_layer_delegate.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/scroll/main_thread_scrolling_reason.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

using blink::WebRect;
using blink::WebVector;

namespace {

cc::Layer* GraphicsLayerToCcLayer(blink::GraphicsLayer* layer) {
  return layer ? layer->CcLayer() : nullptr;
}

}  // namespace

namespace blink {

ScrollingCoordinator* ScrollingCoordinator::Create(Page* page) {
  return new ScrollingCoordinator(page);
}

ScrollingCoordinator::ScrollingCoordinator(Page* page) : page_(page) {}

ScrollingCoordinator::~ScrollingCoordinator() {
  DCHECK(!page_);
}

void ScrollingCoordinator::Trace(blink::Visitor* visitor) {
  visitor->Trace(page_);
  visitor->Trace(horizontal_scrollbars_);
  visitor->Trace(vertical_scrollbars_);
}

void ScrollingCoordinator::SetShouldHandleScrollGestureOnMainThreadRegion(
    const Region& region,
    LocalFrameView* frame_view) {
  if (cc::Layer* scroll_layer = GraphicsLayerToCcLayer(
          frame_view->LayoutViewport()->LayerForScrolling())) {
    scroll_layer->SetNonFastScrollableRegion(RegionToCCRegion(region));
  }
}

void ScrollingCoordinator::NotifyGeometryChanged(LocalFrameView* frame_view) {
  frame_view->GetScrollingContext()->SetScrollGestureRegionIsDirty(true);
  frame_view->GetScrollingContext()->SetTouchEventTargetRectsAreDirty(true);
  frame_view->GetScrollingContext()->SetShouldScrollOnMainThreadIsDirty(true);
}

void ScrollingCoordinator::NotifyTransformChanged(LocalFrame* frame,
                                                  const LayoutBox& box) {
  DCHECK(frame);
  if (!frame->View())
    return;

  if (frame->View()->NeedsLayout())
    return;

  if (RuntimeEnabledFeatures::PaintTouchActionRectsEnabled()) {
    // PaintTouchActionRects does not keep a list of layers with touch rects so
    // just do an update if transforms change.
    frame->View()->GetScrollingContext()->SetTouchEventTargetRectsAreDirty(
        true);
    return;
  }

  for (PaintLayer* layer = box.EnclosingLayer(); layer;
       layer = layer->Parent()) {
    if (frame->View()
            ->GetScrollingContext()
            ->GetLayersWithTouchRects()
            ->Contains(layer)) {
      frame->View()->GetScrollingContext()->SetTouchEventTargetRectsAreDirty(
          true);
      return;
    }
  }
}

void ScrollingCoordinator::DidScroll(const gfx::ScrollOffset& offset,
                                     const CompositorElementId& element_id) {
  for (auto* frame = page_->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    // Remote frames will receive DidScroll callbacks from their own compositor.
    if (!frame->IsLocalFrame())
      continue;

    // Find the associated scrollable area using the element id and notify it
    // of the compositor-side scroll. We explicitly do not check the
    // VisualViewport which handles scroll offset differently (see:
    // VisualViewport::didScroll).
    if (LocalFrameView* view = ToLocalFrame(frame)->View()) {
      if (auto* scrollable = view->ScrollableAreaWithElementId(element_id)) {
        scrollable->DidScroll(FloatPoint(offset.x(), offset.y()));
        return;
      }
    }
  }
  // The ScrollableArea with matching ElementId may have been deleted and we can
  // safely ignore the DidScroll callback.
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

  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.ScrollingCoordinator.UpdateTime");
  TRACE_EVENT0("input", "ScrollingCoordinator::UpdateAfterPaint");

  // TODO(pdr): Move the scroll gesture region logic to use touch action rects.
  // These features are similar and do not need independent implementations.
  if (scroll_gesture_region_dirty) {
    // Compute the region of the page where we can't handle scroll gestures and
    // mousewheel events
    // on the impl thread. This currently includes:
    // 1. All scrollable areas, such as subframes, overflow divs and list boxes,
    //    whose composited scrolling are not enabled. We need to do this even if
    //    the frame view whose layout was updated is not the main frame.
    // 2. Resize control areas, e.g. the small rect at the right bottom of
    //    div/textarea/iframe when CSS property "resize" is enabled.
    // 3. Plugin areas.
    Region should_handle_scroll_gesture_on_main_thread_region =
        ComputeShouldHandleScrollGestureOnMainThreadRegion(frame);
    SetShouldHandleScrollGestureOnMainThreadRegion(
        should_handle_scroll_gesture_on_main_thread_region, frame_view);
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

  // TODO(pdr): Move the should_scroll_on_main_thread logic to use touch action
  // rects. These features are similar and do not need independent
  // implementations.
  if (should_scroll_on_main_thread_dirty ||
      frame_view->FrameIsScrollableDidChange()) {
    SetShouldUpdateScrollLayerPositionOnMainThread(
        frame, frame_view->GetMainThreadScrollingReasons());

    // Need to update scroll on main thread reasons for subframe because
    // subframe (e.g. iframe with background-attachment:fixed) should
    // scroll on main thread while the main frame scrolls on impl.
    frame_view->UpdateSubFrameScrollOnMainReason(*frame, 0);
    frame_view->GetScrollingContext()->SetShouldScrollOnMainThreadIsDirty(
        false);
  }
  frame_view->ClearFrameIsScrollableDidChange();

  UpdateUserInputScrollable(&page_->GetVisualViewport());
}

template <typename Function>
static void ForAllGraphicsLayers(GraphicsLayer& layer,
                                 const Function& function) {
  function(layer);
  for (auto* child : layer.Children())
    ForAllGraphicsLayers(*child, function);
}

// Set the touch action rects on the cc layer from the touch action data stored
// on the GraphicsLayer's paint chunks.
static void UpdateLayerTouchActionRects(GraphicsLayer& layer) {
  DCHECK(RuntimeEnabledFeatures::PaintTouchActionRectsEnabled());

  // TODO(pdr): This will need to be moved to PaintArtifactCompositor (or later)
  // for SPV2 because composited layers are not known until then. The SPV2
  // implementation will iterate over the paint chunks in each composited layer
  // and will look almost the same as this function.
  DCHECK(!RuntimeEnabledFeatures::SlimmingPaintV2Enabled());

  if (!layer.DrawsContent())
    return;

  const auto& layer_state = layer.GetPropertyTreeState();
  Vector<HitTestRect> touch_action_rects_in_layer_space;
  if (layer.Client().ShouldThrottleRendering()) {
    layer.CcLayer()->SetTouchActionRegion(
        HitTestRect::BuildRegion(touch_action_rects_in_layer_space));
    return;
  }
  for (const auto& chunk : layer.GetPaintController().PaintChunks()) {
    const auto* hit_test_data = chunk.GetHitTestData();
    if (!hit_test_data || hit_test_data->touch_action_rects.IsEmpty())
      continue;

    const auto& chunk_state = chunk.properties.GetPropertyTreeState();
    for (auto touch_action_rect : hit_test_data->touch_action_rects) {
      auto rect =
          FloatClipRect(FloatRect(PixelSnappedIntRect(touch_action_rect.rect)));
      if (!GeometryMapper::LocalToAncestorVisualRect(chunk_state, layer_state,
                                                     rect)) {
        continue;
      }
      LayoutRect layout_rect = LayoutRect(rect.Rect());
      layout_rect.MoveBy(-layer.GetOffsetFromTransformNode());
      touch_action_rects_in_layer_space.emplace_back(
          HitTestRect(layout_rect, touch_action_rect.whitelisted_touch_action));
    }
  }
  layer.CcLayer()->SetTouchActionRegion(
      HitTestRect::BuildRegion(touch_action_rects_in_layer_space));
}

static void ClearPositionConstraintExceptForLayer(GraphicsLayer* layer,
                                                  GraphicsLayer* except) {
  if (layer && layer != except && GraphicsLayerToCcLayer(layer)) {
    GraphicsLayerToCcLayer(layer)->SetPositionConstraint(
        cc::LayerPositionConstraint());
  }
}

static cc::LayerPositionConstraint ComputePositionConstraint(
    const PaintLayer* layer) {
  DCHECK(layer->HasCompositedLayerMapping());
  do {
    if (layer->GetLayoutObject().Style()->GetPosition() == EPosition::kFixed) {
      const LayoutObject& fixed_position_object = layer->GetLayoutObject();
      bool fixed_to_right = !fixed_position_object.Style()->Right().IsAuto();
      bool fixed_to_bottom = fixed_position_object.Style()->IsFixedToBottom();
      cc::LayerPositionConstraint constraint;
      constraint.set_is_fixed_position(true);
      constraint.set_is_fixed_to_right_edge(fixed_to_right);
      constraint.set_is_fixed_to_bottom_edge(fixed_to_bottom);
      return constraint;
    }

    layer = layer->Parent();

    // Composited layers that inherit a fixed position state will be positioned
    // with respect to the nearest compositedLayerMapping's GraphicsLayer.
    // So, once we find a layer that has its own compositedLayerMapping, we can
    // stop searching for a fixed position LayoutObject.
  } while (layer && !layer->HasCompositedLayerMapping());
  return cc::LayerPositionConstraint();
}

void ScrollingCoordinator::UpdateLayerPositionConstraint(PaintLayer* layer) {
  DCHECK(layer->HasCompositedLayerMapping());
  CompositedLayerMapping* composited_layer_mapping =
      layer->GetCompositedLayerMapping();
  GraphicsLayer* main_layer = composited_layer_mapping->ChildForSuperlayers();

  // Avoid unnecessary commits
  ClearPositionConstraintExceptForLayer(
      composited_layer_mapping->SquashingContainmentLayer(), main_layer);
  ClearPositionConstraintExceptForLayer(
      composited_layer_mapping->AncestorClippingLayer(), main_layer);
  ClearPositionConstraintExceptForLayer(
      composited_layer_mapping->MainGraphicsLayer(), main_layer);

  if (cc::Layer* scrollable_layer = GraphicsLayerToCcLayer(main_layer))
    scrollable_layer->SetPositionConstraint(ComputePositionConstraint(layer));
}

void ScrollingCoordinator::WillDestroyScrollableArea(
    ScrollableArea* scrollable_area) {
  RemoveScrollbarLayerGroup(scrollable_area, kHorizontalScrollbar);
  RemoveScrollbarLayerGroup(scrollable_area, kVerticalScrollbar);
}

void ScrollingCoordinator::RemoveScrollbarLayerGroup(
    ScrollableArea* scrollable_area,
    ScrollbarOrientation orientation) {
  ScrollbarMap& scrollbars = orientation == kHorizontalScrollbar
                                 ? horizontal_scrollbars_
                                 : vertical_scrollbars_;
  if (std::unique_ptr<ScrollbarLayerGroup> scrollbar_layer_group =
          scrollbars.Take(scrollable_area)) {
    GraphicsLayer::UnregisterContentsLayer(scrollbar_layer_group->layer.get());
  }
}

static std::unique_ptr<ScrollingCoordinator::ScrollbarLayerGroup>
CreateScrollbarLayer(Scrollbar& scrollbar, float device_scale_factor) {
  ScrollbarTheme& theme = scrollbar.GetTheme();
  auto scrollbar_delegate =
      std::make_unique<ScrollbarLayerDelegate>(scrollbar, device_scale_factor);

  auto layer_group =
      std::make_unique<ScrollingCoordinator::ScrollbarLayerGroup>();
  if (theme.UsesOverlayScrollbars() && theme.UsesNinePatchThumbResource()) {
    auto scrollbar_layer = cc::PaintedOverlayScrollbarLayer::Create(
        std::move(scrollbar_delegate), /*scroll_element_id=*/cc::ElementId());
    scrollbar_layer->SetElementId(scrollbar.GetElementId());
    layer_group->scrollbar_layer = scrollbar_layer.get();
    layer_group->layer = std::move(scrollbar_layer);
  } else {
    auto scrollbar_layer = cc::PaintedScrollbarLayer::Create(
        std::move(scrollbar_delegate), /*scroll_element_id=*/cc::ElementId());
    scrollbar_layer->SetElementId(scrollbar.GetElementId());
    layer_group->scrollbar_layer = scrollbar_layer.get();
    layer_group->layer = std::move(scrollbar_layer);
  }

  GraphicsLayer::RegisterContentsLayer(layer_group->layer.get());

  return layer_group;
}

std::unique_ptr<ScrollingCoordinator::ScrollbarLayerGroup>
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
      is_left_side_vertical_scrollbar, cc::ElementId());
  scrollbar_layer->SetElementId(element_id);

  auto layer_group = std::make_unique<ScrollbarLayerGroup>();
  layer_group->scrollbar_layer = scrollbar_layer.get();
  layer_group->layer = std::move(scrollbar_layer);
  GraphicsLayer::RegisterContentsLayer(layer_group->layer.get());

  return layer_group;
}

static void DetachScrollbarLayer(GraphicsLayer* scrollbar_graphics_layer) {
  DCHECK(scrollbar_graphics_layer);

  scrollbar_graphics_layer->SetContentsToCcLayer(nullptr, false);
  scrollbar_graphics_layer->SetDrawsContent(true);
}

static void SetupScrollbarLayer(
    GraphicsLayer* scrollbar_graphics_layer,
    const ScrollingCoordinator::ScrollbarLayerGroup* scrollbar_layer_group,
    cc::Layer* scrolling_layer) {
  DCHECK(scrollbar_graphics_layer);

  if (!scrolling_layer) {
    DetachScrollbarLayer(scrollbar_graphics_layer);
    return;
  }

  scrollbar_layer_group->scrollbar_layer->SetScrollElementId(
      scrolling_layer->element_id());
  scrollbar_graphics_layer->SetContentsToCcLayer(
      scrollbar_layer_group->layer.get(),
      /*prevent_contents_opaque_changes=*/false);
  scrollbar_graphics_layer->SetDrawsContent(false);
}

void ScrollingCoordinator::AddScrollbarLayerGroup(
    ScrollableArea* scrollable_area,
    ScrollbarOrientation orientation,
    std::unique_ptr<ScrollbarLayerGroup> scrollbar_layer_group) {
  ScrollbarMap& scrollbars = orientation == kHorizontalScrollbar
                                 ? horizontal_scrollbars_
                                 : vertical_scrollbars_;
  scrollbars.insert(scrollable_area, std::move(scrollbar_layer_group));
}

ScrollingCoordinator::ScrollbarLayerGroup*
ScrollingCoordinator::GetScrollbarLayerGroup(ScrollableArea* scrollable_area,
                                             ScrollbarOrientation orientation) {
  ScrollbarMap& scrollbars = orientation == kHorizontalScrollbar
                                 ? horizontal_scrollbars_
                                 : vertical_scrollbars_;
  return scrollbars.at(scrollable_area);
}

void ScrollingCoordinator::ScrollableAreaScrollbarLayerDidChange(
    ScrollableArea* scrollable_area,
    ScrollbarOrientation orientation) {
  if (!page_ || !page_->MainFrame())
    return;

  GraphicsLayer* scrollbar_graphics_layer =
      orientation == kHorizontalScrollbar
          ? scrollable_area->LayerForHorizontalScrollbar()
          : scrollable_area->LayerForVerticalScrollbar();

  if (scrollbar_graphics_layer) {
    Scrollbar& scrollbar = orientation == kHorizontalScrollbar
                               ? *scrollable_area->HorizontalScrollbar()
                               : *scrollable_area->VerticalScrollbar();
    if (scrollbar.IsCustomScrollbar()) {
      DetachScrollbarLayer(scrollbar_graphics_layer);
      scrollbar_graphics_layer->CcLayer()->AddMainThreadScrollingReasons(
          MainThreadScrollingReason::kCustomScrollbarScrolling);
      scrollbar_graphics_layer->CcLayer()->SetIsScrollbar(true);
      return;
    }

    // Invalidate custom scrollbar scrolling reason in case a custom
    // scrollbar becomes a non-custom one.
    scrollbar_graphics_layer->CcLayer()->ClearMainThreadScrollingReasons(
        MainThreadScrollingReason::kCustomScrollbarScrolling);
    ScrollbarLayerGroup* scrollbar_layer_group =
        GetScrollbarLayerGroup(scrollable_area, orientation);
    if (!scrollbar_layer_group) {
      Settings* settings = page_->MainFrame()->GetSettings();

      std::unique_ptr<ScrollbarLayerGroup> group;
      if (settings->GetUseSolidColorScrollbars()) {
        DCHECK(RuntimeEnabledFeatures::OverlayScrollbarsEnabled());
        group = CreateSolidColorScrollbarLayer(
            orientation, scrollbar.GetTheme().ThumbThickness(scrollbar),
            scrollbar.GetTheme().TrackPosition(scrollbar),
            scrollable_area->ShouldPlaceVerticalScrollbarOnLeft(),
            scrollable_area->GetScrollbarElementId(orientation));
      } else {
        group = CreateScrollbarLayer(scrollbar,
                                     page_->DeviceScaleFactorDeprecated());
      }

      scrollbar_layer_group = group.get();
      AddScrollbarLayerGroup(scrollable_area, orientation, std::move(group));
    }

    cc::Layer* scroll_layer =
        GraphicsLayerToCcLayer(scrollable_area->LayerForScrolling());
    SetupScrollbarLayer(scrollbar_graphics_layer, scrollbar_layer_group,
                        scroll_layer);

    // Root layer non-overlay scrollbars should be marked opaque to disable
    // blending.
    bool is_opaque_scrollbar = !scrollbar.IsOverlayScrollbar();
    scrollbar_graphics_layer->SetContentsOpaque(
        IsForMainFrame(scrollable_area) && is_opaque_scrollbar);
  } else {
    RemoveScrollbarLayerGroup(scrollable_area, orientation);
  }
}

bool ScrollingCoordinator::UpdateCompositedScrollOffset(
    ScrollableArea* scrollable_area) {
  GraphicsLayer* scroll_layer = scrollable_area->LayerForScrolling();
  if (!scroll_layer)
    return false;

  cc::Layer* cc_layer =
      GraphicsLayerToCcLayer(scrollable_area->LayerForScrolling());
  if (!cc_layer)
    return false;

  cc_layer->SetScrollOffset(
      static_cast<gfx::ScrollOffset>(scrollable_area->ScrollPosition()));
  return true;
}

void ScrollingCoordinator::ScrollableAreaScrollLayerDidChange(
    ScrollableArea* scrollable_area) {
  if (!page_ || !page_->MainFrame())
    return;

  UpdateUserInputScrollable(scrollable_area);

  cc::Layer* cc_layer =
      GraphicsLayerToCcLayer(scrollable_area->LayerForScrolling());
  cc::Layer* container_layer =
      GraphicsLayerToCcLayer(scrollable_area->LayerForContainer());
  if (cc_layer) {
    cc_layer->SetScrollable(container_layer->bounds());
    FloatPoint scroll_position(scrollable_area->ScrollPosition());
    cc_layer->SetScrollOffset(static_cast<gfx::ScrollOffset>(scroll_position));
    // TODO(bokan): This method shouldn't be resizing the layer geometry. That
    // happens in CompositedLayerMapping::UpdateScrollingLayerGeometry.
    LayoutSize subpixel_accumulation =
        scrollable_area->Layer()
            ? scrollable_area->Layer()->SubpixelAccumulation()
            : LayoutSize();
    LayoutSize contents_size =
        scrollable_area->GetLayoutBox()
            ? LayoutSize(scrollable_area->GetLayoutBox()->ScrollWidth(),
                         scrollable_area->GetLayoutBox()->ScrollHeight())
            : LayoutSize(scrollable_area->ContentsSize());
    IntSize scroll_contents_size =
        PixelSnappedIntRect(
            LayoutRect(LayoutPoint(subpixel_accumulation), contents_size))
            .Size();

    if (scrollable_area != &page_->GetVisualViewport()) {
      // The scrolling contents layer must be at least as large as its clip.
      // The visual viewport is special because the size of its scrolling
      // content depends on the page scale factor. Its scrollable content is
      // the layout viewport which is sized based on the minimum allowed page
      // scale so it actually can be smaller than its clip.
      scroll_contents_size =
          scroll_contents_size.ExpandedTo(IntSize(container_layer->bounds()));

      // VisualViewport scrolling may involve pinch zoom and gets routed through
      // WebViewImpl explicitly rather than via ScrollingCoordinator::DidScroll
      // since it needs to be set in tandem with the page scale delta.
      cc_layer->set_did_scroll_callback(WTF::BindRepeating(
          &ScrollingCoordinator::DidScroll, WrapWeakPersistent(this)));
    }

    // This call has to go through the GraphicsLayer method to preserve
    // invalidation code there.
    scrollable_area->LayerForScrolling()->SetSize(
        static_cast<gfx::Size>(scroll_contents_size));
  }
  if (ScrollbarLayerGroup* scrollbar_layer_group =
          GetScrollbarLayerGroup(scrollable_area, kHorizontalScrollbar)) {
    GraphicsLayer* horizontal_scrollbar_layer =
        scrollable_area->LayerForHorizontalScrollbar();
    if (horizontal_scrollbar_layer) {
      SetupScrollbarLayer(horizontal_scrollbar_layer, scrollbar_layer_group,
                          cc_layer);
    }
  }
  if (ScrollbarLayerGroup* scrollbar_layer_group =
          GetScrollbarLayerGroup(scrollable_area, kVerticalScrollbar)) {
    GraphicsLayer* vertical_scrollbar_layer =
        scrollable_area->LayerForVerticalScrollbar();

    if (vertical_scrollbar_layer) {
      SetupScrollbarLayer(vertical_scrollbar_layer, scrollbar_layer_group,
                          cc_layer);
    }
  }

  // Update the viewport layer registration if the outer viewport may have
  // changed.
  if (IsForRootLayer(scrollable_area))
    page_->GetChromeClient().RegisterViewportLayers();

  CompositorAnimationTimeline* timeline;
  // LocalFrameView::CompositorAnimationTimeline() can indirectly return
  // m_programmaticScrollAnimatorTimeline if it does not have its own
  // timeline.
  if (scrollable_area->IsPaintLayerScrollableArea()) {
    timeline = ToPaintLayerScrollableArea(scrollable_area)
                   ->GetCompositorAnimationTimeline();
  } else {
    timeline = programmatic_scroll_animator_timeline_.get();
  }
  scrollable_area->LayerForScrollingDidChange(timeline);

  return;
}

using GraphicsLayerHitTestRects =
    WTF::HashMap<const GraphicsLayer*, Vector<HitTestRect>>;

// In order to do a DFS cross-frame walk of the Layer tree, we need to know
// which Layers have child frames inside of them. This computes a mapping for
// the current frame which we can consult while walking the layers of that
// frame.  Whenever we descend into a new frame, a new map will be created.
using LayerFrameMap =
    HeapHashMap<const PaintLayer*, HeapVector<Member<const LocalFrame>>>;
static void MakeLayerChildFrameMap(const LocalFrame* current_frame,
                                   LayerFrameMap* map) {
  map->clear();
  const FrameTree& tree = current_frame->Tree();
  for (const Frame* child = tree.FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (!child->IsLocalFrame())
      continue;
    auto* owner_layout_object = ToLocalFrame(child)->OwnerLayoutObject();
    if (!owner_layout_object)
      continue;
    const PaintLayer* containing_layer = owner_layout_object->EnclosingLayer();
    LayerFrameMap::iterator iter = map->find(containing_layer);
    if (iter == map->end())
      map->insert(containing_layer, HeapVector<Member<const LocalFrame>>())
          .stored_value->value.push_back(ToLocalFrame(child));
    else
      iter->value.push_back(ToLocalFrame(child));
  }
}

static void ProjectRectsToGraphicsLayerSpaceRecursive(
    const PaintLayer* cur_layer,
    const LayerHitTestRects& layer_rects,
    GraphicsLayerHitTestRects& graphics_rects,
    LayoutGeometryMap& geometry_map,
    HashSet<const PaintLayer*>& layers_with_rects,
    LayerFrameMap& layer_child_frame_map) {
  // Project any rects for the current layer
  LayerHitTestRects::const_iterator layer_iter = layer_rects.find(cur_layer);
  if (layer_iter != layer_rects.end()) {
    // Find the enclosing composited layer when it's in another document (for
    // non-composited iframes).
    const PaintLayer* composited_layer =
        layer_iter->key
            ->EnclosingLayerForPaintInvalidationCrossingFrameBoundaries();
    // https://crbug.com/751768. |composited_layer| can be null, don't just
    // DCHECK it.
    if (!composited_layer)
      return;

    // Find the appropriate GraphicsLayer for the composited Layer.
    GraphicsLayer* graphics_layer =
        composited_layer->GraphicsLayerBacking(&cur_layer->GetLayoutObject());

    GraphicsLayerHitTestRects::iterator gl_iter =
        graphics_rects.find(graphics_layer);
    Vector<HitTestRect>* gl_rects;
    if (gl_iter == graphics_rects.end()) {
      gl_rects = &graphics_rects.insert(graphics_layer, Vector<HitTestRect>())
                      .stored_value->value;
    } else {
      gl_rects = &gl_iter->value;
    }

    // Transform each rect to the co-ordinate space of the graphicsLayer.
    for (wtf_size_t i = 0; i < layer_iter->value.size(); ++i) {
      HitTestRect rect = layer_iter->value[i];
      if (composited_layer != cur_layer) {
        FloatQuad compositor_quad = geometry_map.MapToAncestor(
            FloatRect(rect.rect), &composited_layer->GetLayoutObject());
        rect.rect = LayoutRect(compositor_quad.BoundingBox());
        // If the enclosing composited layer itself is scrolled, we have to undo
        // the subtraction of its scroll offset since we want the offset
        // relative to the scrolling content, not the element itself.
        if (composited_layer->GetLayoutObject().HasOverflowClip()) {
          rect.rect.Move(
              composited_layer->GetLayoutBox()->ScrolledContentOffset());
        }
      }
      PaintLayer::MapRectInPaintInvalidationContainerToBacking(
          composited_layer->GetLayoutObject(), rect.rect);
      rect.rect.Move(-graphics_layer->OffsetFromLayoutObject());

      gl_rects->push_back(rect);
    }
  }

  // Walk child layers of interest
  for (const PaintLayer* child_layer = cur_layer->FirstChild(); child_layer;
       child_layer = child_layer->NextSibling()) {
    if (layers_with_rects.Contains(child_layer)) {
      geometry_map.PushMappingsToAncestor(child_layer, cur_layer);
      ProjectRectsToGraphicsLayerSpaceRecursive(
          child_layer, layer_rects, graphics_rects, geometry_map,
          layers_with_rects, layer_child_frame_map);
      geometry_map.PopMappingsToAncestor(cur_layer);
    }
  }

  // If this layer has any frames of interest as a child of it, walk those (with
  // an updated frame map).
  LayerFrameMap::iterator map_iter = layer_child_frame_map.find(cur_layer);
  if (map_iter != layer_child_frame_map.end()) {
    for (wtf_size_t i = 0; i < map_iter->value.size(); i++) {
      const LocalFrame* child_frame = map_iter->value[i];
      if (child_frame->ShouldThrottleRendering())
        continue;

      const PaintLayer* child_layer =
          child_frame->View()->GetLayoutView()->Layer();
      if (layers_with_rects.Contains(child_layer)) {
        LayerFrameMap new_layer_child_frame_map;
        MakeLayerChildFrameMap(child_frame, &new_layer_child_frame_map);
        geometry_map.PushMappingsToAncestor(child_layer, cur_layer);
        ProjectRectsToGraphicsLayerSpaceRecursive(
            child_layer, layer_rects, graphics_rects, geometry_map,
            layers_with_rects, new_layer_child_frame_map);
        geometry_map.PopMappingsToAncestor(cur_layer);
      }
    }
  }
}

static void ProjectRectsToGraphicsLayerSpace(
    LocalFrame* main_frame,
    const LayerHitTestRects& layer_rects,
    GraphicsLayerHitTestRects& graphics_rects) {
  TRACE_EVENT0("input",
               "ScrollingCoordinator::projectRectsToGraphicsLayerSpace");

  if (main_frame->ShouldThrottleRendering())
    return;

  bool touch_handler_in_child_frame = false;

  // We have a set of rects per Layer, we need to map them to their bounding
  // boxes in their enclosing composited layer. To do this most efficiently
  // we'll walk the Layer tree using LayoutGeometryMap. First record all the
  // branches we should traverse in the tree (including all documents on the
  // page).
  HashSet<const PaintLayer*> layers_with_rects;
  for (const auto& layer_rect : layer_rects) {
    const PaintLayer* layer = layer_rect.key;
    do {
      if (!layers_with_rects.insert(layer).is_new_entry)
        break;

      if (layer->Parent()) {
        layer = layer->Parent();
      } else {
        auto* parent_doc_layout_object =
            layer->GetLayoutObject().GetFrame()->OwnerLayoutObject();
        if (parent_doc_layout_object) {
          layer = parent_doc_layout_object->EnclosingLayer();
          touch_handler_in_child_frame = true;
        }
      }
    } while (layer);
  }

  // Now walk the layer projecting rects while maintaining a LayoutGeometryMap
  MapCoordinatesFlags flags = kUseTransforms;
  if (touch_handler_in_child_frame)
    flags |= kTraverseDocumentBoundaries;
  PaintLayer* root_layer = main_frame->ContentLayoutObject()->Layer();
  LayoutGeometryMap geometry_map(flags);
  geometry_map.PushMappingsToAncestor(root_layer, nullptr);
  LayerFrameMap layer_child_frame_map;
  MakeLayerChildFrameMap(main_frame, &layer_child_frame_map);
  ProjectRectsToGraphicsLayerSpaceRecursive(
      root_layer, layer_rects, graphics_rects, geometry_map, layers_with_rects,
      layer_child_frame_map);
}

void ScrollingCoordinator::UpdateTouchEventTargetRectsIfNeeded(
    LocalFrame* frame) {
  TRACE_EVENT0("input",
               "ScrollingCoordinator::updateTouchEventTargetRectsIfNeeded");

  // TODO(chrishtr): implement touch event target rects for SPv2.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  if (RuntimeEnabledFeatures::PaintTouchActionRectsEnabled()) {
    auto* view_layer = frame->View()->GetLayoutView()->Layer();
    if (auto* root = view_layer->Compositor()->PaintRootGraphicsLayer())
      ForAllGraphicsLayers(*root, UpdateLayerTouchActionRects);
  } else {
    LayerHitTestRects touch_event_target_rects;
    ComputeTouchEventTargetRects(frame, touch_event_target_rects);
    SetTouchEventTargetRects(frame, touch_event_target_rects);
  }
}

void ScrollingCoordinator::UpdateUserInputScrollable(
    ScrollableArea* scrollable_area) {
  cc::Layer* cc_layer =
      GraphicsLayerToCcLayer(scrollable_area->LayerForScrolling());
  if (cc_layer) {
    bool can_scroll_x =
        scrollable_area->UserInputScrollable(kHorizontalScrollbar);
    bool can_scroll_y =
        scrollable_area->UserInputScrollable(kVerticalScrollbar);
    cc_layer->SetUserScrollable(can_scroll_x, can_scroll_y);
  }
}

void ScrollingCoordinator::Reset(LocalFrame* frame) {
  for (const auto& scrollbar : horizontal_scrollbars_)
    GraphicsLayer::UnregisterContentsLayer(scrollbar.value->layer.get());
  for (const auto& scrollbar : vertical_scrollbars_)
    GraphicsLayer::UnregisterContentsLayer(scrollbar.value->layer.get());

  horizontal_scrollbars_.clear();
  vertical_scrollbars_.clear();
  frame->View()->GetScrollingContext()->GetLayersWithTouchRects()->clear();
  frame->View()->ClearFrameIsScrollableDidChange();
}

// Note that in principle this could be called more often than
// computeTouchEventTargetRects, for example during a non-composited scroll
// (although that's not yet implemented - crbug.com/261307).
void ScrollingCoordinator::SetTouchEventTargetRects(
    LocalFrame* frame,
    const LayerHitTestRects& layer_rects) {
  TRACE_EVENT0("input", "ScrollingCoordinator::setTouchEventTargetRects");

  DCHECK(!RuntimeEnabledFeatures::PaintTouchActionRectsEnabled());

  // Ensure we have an entry for each composited layer that previously had rects
  // (so that old ones will get cleared out). Note that ideally we'd track this
  // on GraphicsLayer instead of Layer, but we have no good hook into the
  // lifetime of a GraphicsLayer.
  GraphicsLayerHitTestRects graphics_layer_rects;
  for (const PaintLayer* layer :
       *frame->View()->GetScrollingContext()->GetLayersWithTouchRects()) {
    if (layer->GetLayoutObject().GetFrameView() &&
        layer->GetLayoutObject().GetFrameView()->ShouldThrottleRendering()) {
      continue;
    }
    GraphicsLayer* main_graphics_layer =
        layer->GraphicsLayerBacking(&layer->GetLayoutObject());
    if (main_graphics_layer) {
      graphics_layer_rects.insert(main_graphics_layer, Vector<HitTestRect>());
    }
    GraphicsLayer* scrolling_contents_layer = layer->GraphicsLayerBacking();
    if (scrolling_contents_layer &&
        scrolling_contents_layer != main_graphics_layer) {
      graphics_layer_rects.insert(scrolling_contents_layer,
                                  Vector<HitTestRect>());
    }
  }

  frame->View()->GetScrollingContext()->GetLayersWithTouchRects()->clear();
  for (const auto& layer_rect : layer_rects) {
    if (!layer_rect.value.IsEmpty()) {
      DCHECK(layer_rect.key->IsRootLayer() || layer_rect.key->Parent());
      const PaintLayer* composited_layer =
          layer_rect.key
              ->EnclosingLayerForPaintInvalidationCrossingFrameBoundaries();
      // https://crbug.com/751768. |composited_layer| can be null, don't just
      // DCHECK it.
      if (!composited_layer)
        continue;
      frame->View()->GetScrollingContext()->GetLayersWithTouchRects()->insert(
          composited_layer);
    }
  }

  ProjectRectsToGraphicsLayerSpace(frame, layer_rects, graphics_layer_rects);

  for (const auto& layer_rect : graphics_layer_rects) {
    const GraphicsLayer* graphics_layer = layer_rect.key;
    graphics_layer->CcLayer()->SetTouchActionRegion(
        HitTestRect::BuildRegion(layer_rect.value));
  }
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

void ScrollingCoordinator::UpdateScrollParentForGraphicsLayer(
    GraphicsLayer* child,
    const PaintLayer* parent) {
  cc::Layer* scroll_parent_cc_layer = nullptr;
  if (parent && parent->HasCompositedLayerMapping())
    scroll_parent_cc_layer = GraphicsLayerToCcLayer(
        parent->GetCompositedLayerMapping()->ScrollingContentsLayer());

  child->SetScrollParent(scroll_parent_cc_layer);
}

void ScrollingCoordinator::UpdateClipParentForGraphicsLayer(
    GraphicsLayer* child,
    const PaintLayer* parent) {
  cc::Layer* clip_parent_cc_layer = nullptr;
  if (parent && parent->HasCompositedLayerMapping()) {
    clip_parent_cc_layer = GraphicsLayerToCcLayer(
        parent->GetCompositedLayerMapping()->ParentForSublayers());
  }

  child->SetClipParent(clip_parent_cc_layer);
}

void ScrollingCoordinator::WillDestroyLayer(PaintLayer* layer) {
  layer->GetLayoutObject()
      .GetFrame()
      ->View()
      ->GetScrollingContext()
      ->GetLayersWithTouchRects()
      ->erase(layer);
}

void ScrollingCoordinator::SetShouldUpdateScrollLayerPositionOnMainThread(
    LocalFrame* frame,
    MainThreadScrollingReasons main_thread_scrolling_reasons) {
  VisualViewport& visual_viewport = frame->GetPage()->GetVisualViewport();
  GraphicsLayer* visual_viewport_layer = visual_viewport.ScrollLayer();
  cc::Layer* visual_viewport_scroll_layer =
      GraphicsLayerToCcLayer(visual_viewport_layer);
  ScrollableArea* scrollable_area = frame->View()->LayoutViewport();
  GraphicsLayer* layer = scrollable_area->LayerForScrolling();
  if (cc::Layer* scroll_layer = GraphicsLayerToCcLayer(layer)) {
    if (main_thread_scrolling_reasons) {
      if (ScrollAnimatorBase* scroll_animator =
              scrollable_area->ExistingScrollAnimator()) {
        DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
               frame->GetDocument()->Lifecycle().GetState() >=
                   DocumentLifecycle::kCompositingClean);
        scroll_animator->TakeOverCompositorAnimation();
      }
      scroll_layer->AddMainThreadScrollingReasons(
          main_thread_scrolling_reasons);
      if (visual_viewport_scroll_layer) {
        if (ScrollAnimatorBase* scroll_animator =
                visual_viewport.ExistingScrollAnimator()) {
          DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
                 frame->GetDocument()->Lifecycle().GetState() >=
                     DocumentLifecycle::kCompositingClean);
          scroll_animator->TakeOverCompositorAnimation();
        }
        visual_viewport_scroll_layer->AddMainThreadScrollingReasons(
            main_thread_scrolling_reasons);
      }
    } else {
      // Clear all main thread scrolling reasons except the one that's set
      // if there is a running scroll animation.
      uint32_t main_thread_scrolling_reasons_to_clear = ~0u;
      main_thread_scrolling_reasons_to_clear &=
          ~MainThreadScrollingReason::kHandlingScrollFromMainThread;
      scroll_layer->ClearMainThreadScrollingReasons(
          main_thread_scrolling_reasons_to_clear);
      if (visual_viewport_scroll_layer)
        visual_viewport_scroll_layer->ClearMainThreadScrollingReasons(
            main_thread_scrolling_reasons_to_clear);
    }
  }
}

void ScrollingCoordinator::LayerTreeViewInitialized(
    WebLayerTreeView& layer_tree_view,
    LocalFrameView* view) {
  if (Platform::Current()->IsThreadedAnimationEnabled()) {
    std::unique_ptr<CompositorAnimationTimeline> timeline =
        CompositorAnimationTimeline::Create();
    auto host = std::make_unique<CompositorAnimationHost>(
        layer_tree_view.CompositorAnimationHost());
    if (view && view->GetFrame().LocalFrameRoot() != page_->MainFrame()) {
      view->GetScrollingContext()->SetAnimationHost(std::move(host));
      view->GetScrollingContext()->SetAnimationTimeline(std::move(timeline));
      view->GetCompositorAnimationHost()->AddTimeline(
          *view->GetCompositorAnimationTimeline());
    } else {
      animation_host_ = std::move(host);
      programmatic_scroll_animator_timeline_ = std::move(timeline);
      animation_host_->AddTimeline(
          *programmatic_scroll_animator_timeline_.get());
    }
  }
}

void ScrollingCoordinator::WillCloseLayerTreeView(
    WebLayerTreeView& layer_tree_view,
    LocalFrameView* view) {
  if (view && view->GetFrame().LocalFrameRoot() != page_->MainFrame()) {
    view->GetCompositorAnimationHost()->RemoveTimeline(
        *view->GetCompositorAnimationTimeline());
    view->GetScrollingContext()->SetAnimationTimeline(nullptr);
    view->GetScrollingContext()->SetAnimationHost(nullptr);
  } else if (programmatic_scroll_animator_timeline_) {
    animation_host_->RemoveTimeline(
        *programmatic_scroll_animator_timeline_.get());
    programmatic_scroll_animator_timeline_ = nullptr;
    animation_host_ = nullptr;
  }
}

void ScrollingCoordinator::WillBeDestroyed() {
  DCHECK(page_);

  page_ = nullptr;
  for (const auto& scrollbar : horizontal_scrollbars_)
    GraphicsLayer::UnregisterContentsLayer(scrollbar.value->layer.get());
  for (const auto& scrollbar : vertical_scrollbars_)
    GraphicsLayer::UnregisterContentsLayer(scrollbar.value->layer.get());
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

Region ScrollingCoordinator::ComputeShouldHandleScrollGestureOnMainThreadRegion(
    const LocalFrame* frame) const {
  Region should_handle_scroll_gesture_on_main_thread_region;
  LocalFrameView* frame_view = frame->View();

  if (!frame_view || frame_view->ShouldThrottleRendering() ||
      !frame_view->IsVisible()) {
    return should_handle_scroll_gesture_on_main_thread_region;
  }

  if (const LocalFrameView::ScrollableAreaSet* scrollable_areas =
          frame_view->ScrollableAreas()) {
    for (const ScrollableArea* scrollable_area : *scrollable_areas) {
      // Composited scrollable areas can be scrolled off the main thread.
      if (scrollable_area->UsesCompositedScrolling())
        continue;

      IntRect box = scrollable_area->ScrollableAreaBoundingBox();
      should_handle_scroll_gesture_on_main_thread_region.Unite(box);
    }
  }

  // We use GestureScrollBegin/Update/End for moving the resizer handle. So we
  // mark these small resizer areas as non-fast-scrollable to allow the scroll
  // gestures to be passed to main thread if they are targeting the resizer
  // area. (Resizing is done in EventHandler.cpp on main thread).
  if (const LocalFrameView::ResizerAreaSet* resizer_areas =
          frame_view->ResizerAreas()) {
    for (const LayoutBox* box : *resizer_areas) {
      PaintLayerScrollableArea* scrollable_area =
          box->Layer()->GetScrollableArea();
      IntRect bounds = box->AbsoluteBoundingBoxRect();
      // Get the corner in local coords.
      IntRect corner =
          scrollable_area->ResizerCornerRect(bounds, kResizerForTouch);
      // Map corner to top-frame coords.
      corner = scrollable_area->GetLayoutBox()
                   ->LocalToAbsoluteQuad(FloatRect(corner),
                                         kTraverseDocumentBoundaries)
                   .EnclosingBoundingBox();
      should_handle_scroll_gesture_on_main_thread_region.Unite(corner);
    }
  }

  for (const auto& plugin : frame_view->Plugins()) {
    if (plugin->WantsWheelEvents()) {
      IntRect box = frame_view->ConvertToRootFrame(plugin->FrameRect());
      should_handle_scroll_gesture_on_main_thread_region.Unite(box);
    }
  }

  const FrameTree& tree = frame->Tree();
  for (Frame* sub_frame = tree.FirstChild(); sub_frame;
       sub_frame = sub_frame->Tree().NextSibling()) {
    if (sub_frame->IsLocalFrame()) {
      should_handle_scroll_gesture_on_main_thread_region.Unite(
          ComputeShouldHandleScrollGestureOnMainThreadRegion(
              ToLocalFrame(sub_frame)));
    }
  }

  return should_handle_scroll_gesture_on_main_thread_region;
}

static void AccumulateDocumentTouchEventTargetRects(
    LayerHitTestRects& rects,
    EventHandlerRegistry::EventHandlerClass event_class,
    Document* document,
    TouchAction supported_fast_actions) {
  DCHECK(document);
  const EventTargetSet* targets =
      document->GetFrame()->GetEventHandlerRegistry().EventHandlerTargets(
          event_class);
  if (!targets)
    return;

  // If there's a handler on the window, document, html or body element (fairly
  // common in practice), then we can quickly mark the entire document and skip
  // looking at any other handlers.  Note that technically a handler on the body
  // doesn't cover the whole document, but it's reasonable to be conservative
  // and report the whole document anyway.
  //
  // Fullscreen HTML5 video when OverlayFullscreenVideo is enabled is
  // implemented by replacing the root cc::layer with the video layer so doing
  // this optimization causes the compositor to think that there are no
  // handlers, therefore skip it.
  if (!document->GetLayoutView()->Compositor()->InOverlayFullscreenVideo() &&
      (!document->View() || !document->View()->ShouldThrottleRendering())) {
    if (targets->Contains(document) ||
        (document->documentElement() &&
         targets->Contains(document->documentElement())) ||
        (document->body() && targets->Contains(document->body())) ||
        targets->Contains(document->GetFrame()->DomWindow())) {
      document->GetLayoutView()->ComputeLayerHitTestRects(
          rects, supported_fast_actions);
      return;
    }
  }

  for (const auto& event_target : *targets) {
    EventTarget* target = event_target.key;
    Node* node = target->ToNode();
    LocalDOMWindow* window = target->ToLocalDOMWindow();
    if (!window && (!node || !node->isConnected()))
      continue;

    Document& document_node =
        window ? *window->document() : node->GetDocument();

    // If the document belongs to an invisible subframe it does not have a
    // composited layer and should be skipped.
    if (document_node.IsInInvisibleSubframe())
      continue;

    // If the node belongs to a throttled frame, skip it.
    if (document_node.View() && document_node.View()->ShouldThrottleRendering())
      continue;

    // Only event targets belonging to the same local root as |document| should
    // be processed here.
    DCHECK_EQ(&document->GetFrame()->LocalFrameRoot(),
              &document_node.GetFrame()->LocalFrameRoot());

    if ((window || node->IsDocumentNode()) && &document_node != document) {
      AccumulateDocumentTouchEventTargetRects(
          rects, event_class, &document_node, supported_fast_actions);
    } else if (node) {
      LayoutObject* layout_object = node->GetLayoutObject();
      if (!layout_object)
        continue;
      // If the set also contains one of our ancestor nodes then processing
      // this node would be redundant.
      bool has_touch_event_target_ancestor = false;
      for (Node& ancestor : NodeTraversal::AncestorsOf(*node)) {
        if (has_touch_event_target_ancestor)
          break;
        if (targets->Contains(&ancestor))
          has_touch_event_target_ancestor = true;
      }
      if (!has_touch_event_target_ancestor) {
        // Walk up the tree to the outermost non-composited scrollable layer.
        PaintLayer* enclosing_non_composited_scroll_layer = nullptr;
        for (PaintLayer* parent = layout_object->EnclosingLayer();
             parent && parent->GetCompositingState() == kNotComposited;
             parent = parent->Parent()) {
          if (parent->ScrollsOverflow())
            enclosing_non_composited_scroll_layer = parent;
        }

        // Report the whole non-composited scroll layer as a touch hit rect
        // because any rects inside of it may move around relative to their
        // enclosing composited layer without causing the rects to be
        // recomputed. Non-composited scrolling occurs on the main thread, so
        // we're not getting much benefit from compositor touch hit testing in
        // this case anyway.
        if (enclosing_non_composited_scroll_layer) {
          enclosing_non_composited_scroll_layer->ComputeSelfHitTestRects(
              rects, supported_fast_actions);
        }

        layout_object->ComputeLayerHitTestRects(rects, supported_fast_actions);
      }
    }
  }
}

void ScrollingCoordinator::ComputeTouchEventTargetRects(
    LocalFrame* frame,
    LayerHitTestRects& rects) {
  TRACE_EVENT0("input", "ScrollingCoordinator::computeTouchEventTargetRects");

  DCHECK(!RuntimeEnabledFeatures::PaintTouchActionRectsEnabled());

  Document* document = frame->GetDocument();
  if (!document || !document->View() || !document->GetFrame())
    return;

  AccumulateDocumentTouchEventTargetRects(
      rects, EventHandlerRegistry::kTouchAction, document,
      TouchAction::kTouchActionAuto);
  AccumulateDocumentTouchEventTargetRects(
      rects, EventHandlerRegistry::kTouchStartOrMoveEventBlocking, document,
      TouchAction::kTouchActionNone);
  AccumulateDocumentTouchEventTargetRects(
      rects, EventHandlerRegistry::kTouchStartOrMoveEventBlockingLowLatency,
      document, TouchAction::kTouchActionNone);
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

bool ScrollingCoordinator::IsForRootLayer(
    ScrollableArea* scrollable_area) const {
  if (!page_->MainFrame()->IsLocalFrame())
    return false;

  // FIXME(305811): Refactor for OOPI.
  if (auto* layout_view =
          page_->DeprecatedLocalMainFrame()->View()->GetLayoutView())
    return scrollable_area == layout_view->Layer()->GetScrollableArea();
  return false;
}

bool ScrollingCoordinator::IsForMainFrame(
    ScrollableArea* scrollable_area) const {
  if (!page_->MainFrame()->IsLocalFrame())
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
  if (frame_view->FrameIsScrollableDidChange())
    return true;

  if (cc::Layer* scroll_layer =
          frame_view ? GraphicsLayerToCcLayer(
                           frame_view->LayoutViewport()->LayerForScrolling())
                     : nullptr) {
    return static_cast<gfx::Size>(
               frame_view->LayoutViewport()->ContentsSize()) !=
           scroll_layer->bounds();
  }
  return false;
}

}  // namespace blink
