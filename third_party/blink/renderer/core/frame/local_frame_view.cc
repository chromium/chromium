/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/frame/local_frame_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"
#include "base/timer/lap_timer.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/picture_layer.h"
#include "cc/tiles/frame_viewer_instrumentation.h"
#include "cc/trees/layer_tree_host.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/editing/compute_layer_selection.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/find_in_page.h"
#include "third_party/blink/renderer/core/frame/frame_overlay.h"
#include "third_party/blink/renderer/core/frame/frame_view_auto_size_info.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html/portal/document_portals.h"
#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"
#include "third_party/blink/renderer/core/html/portal/portal_contents.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/style_retain_scope.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/layout/traced_layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator_context.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/paint/block_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_inputs_updater.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/frame_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_controller.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/geometry/double_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-blink.h"

// Used to check for dirty layouts violating document lifecycle rules.
// If arg evaluates to true, the program will continue. If arg evaluates to
// false, program will crash if DCHECK_IS_ON() or return false from the current
// function.
#define CHECK_FOR_DIRTY_LAYOUT(arg) \
  do {                              \
    if (!(arg)) {                   \
      NOTREACHED();                 \
      return false;                 \
    }                               \
  } while (false)

namespace blink {
namespace {

// Logs a UseCounter for the size of the cursor that will be set. This will be
// used for compatibility analysis to determine whether the maximum size can be
// reduced.
void LogCursorSizeCounter(LocalFrame* frame, const ui::Cursor& cursor) {
  DCHECK(frame);
  SkBitmap bitmap = cursor.custom_bitmap();
  if (cursor.type() != ui::mojom::blink::CursorType::kCustom || bitmap.isNull())
    return;
  // Should not overflow, this calculation is done elsewhere when determining
  // whether the cursor exceeds its maximum size (see event_handler.cc).
  auto scaled_size = IntSize(bitmap.width(), bitmap.height());
  scaled_size.Scale(1 / cursor.image_scale_factor());
  if (scaled_size.Width() > 64 || scaled_size.Height() > 64) {
    UseCounter::Count(frame->GetDocument(), WebFeature::kCursorImageGT64x64);
  } else if (scaled_size.Width() > 32 || scaled_size.Height() > 32) {
    UseCounter::Count(frame->GetDocument(), WebFeature::kCursorImageGT32x32);
  } else {
    UseCounter::Count(frame->GetDocument(), WebFeature::kCursorImageLE32x32);
  }
}

// Default value for how long we want to delay the
// compositor commit beyond the start of document lifecycle updates to avoid
// flash between navigations. The delay should be small enough so that it won't
// confuse users expecting a new page to appear after navigation and the omnibar
// has updated the url display.
constexpr int kCommitDelayDefaultInMs = 500;  // 30 frames @ 60hz

}  // namespace

// The maximum number of updatePlugins iterations that should be done before
// returning.
static const unsigned kMaxUpdatePluginsIterations = 2;

LocalFrameView::LocalFrameView(LocalFrame& frame)
    : LocalFrameView(frame, IntRect()) {
  Show();
}

LocalFrameView::LocalFrameView(LocalFrame& frame, const IntSize& initial_size)
    : LocalFrameView(frame, IntRect(IntPoint(), initial_size)) {
  SetLayoutSizeInternal(initial_size);
  Show();
}

LocalFrameView::LocalFrameView(LocalFrame& frame, IntRect frame_rect)
    : FrameView(frame_rect),
      frame_(frame),
      can_have_scrollbars_(true),
      has_pending_layout_(false),
      layout_scheduling_enabled_(true),
      layout_count_for_testing_(0),
      lifecycle_update_count_for_testing_(0),
      nested_layout_count_(0),
      // We want plugin updates to happen in FIFO order with loading tasks.
      update_plugins_timer_(frame.GetTaskRunner(TaskType::kInternalLoading),
                            this,
                            &LocalFrameView::UpdatePluginsTimerFired),
      first_layout_(true),
      base_background_color_(Color::kWhite),
      last_zoom_factor_(1.0f),
      media_type_(media_type_names::kScreen),
      safe_to_propagate_scroll_to_parent_(true),
      visually_non_empty_character_count_(0),
      visually_non_empty_pixel_count_(0),
      is_visually_non_empty_(false),
      sticky_position_object_count_(0),
      layout_size_fixed_to_frame_size_(true),
      needs_update_geometries_(false),
      root_layer_did_scroll_(false),
      frame_timing_requests_dirty_(true),
      // The compositor throttles the main frame using deferred begin main frame
      // updates. We can't throttle it here or it seems the root compositor
      // doesn't get setup properly.
      lifecycle_updates_throttled_(!GetFrame().IsMainFrame()),
      current_update_lifecycle_phases_target_state_(
          DocumentLifecycle::kUninitialized),
      past_layout_lifecycle_update_(false),
      suppress_adjust_view_size_(false),
      intersection_observation_state_(kNotNeeded),
      needs_forced_compositing_update_(false),
      needs_focus_on_fragment_(false),
      in_lifecycle_update_(false),
      main_thread_scrolling_reasons_(0),
      forced_layout_stack_depth_(0),
      forced_layout_start_time_(base::TimeTicks()),
      paint_frame_count_(0),
      unique_id_(NewUniqueObjectId()),
      layout_shift_tracker_(MakeGarbageCollected<LayoutShiftTracker>(this)),
      paint_timing_detector_(MakeGarbageCollected<PaintTimingDetector>(this)),
      mobile_friendliness_checker_(
          MakeGarbageCollected<MobileFriendlinessChecker>(*this))
#if DCHECK_IS_ON()
      ,
      is_updating_descendant_dependent_flags_(false)
#endif
{
  // Propagate the marginwidth/height and scrolling modes to the view.
  if (frame_->Owner() && frame_->Owner()->ScrollbarMode() ==
                             mojom::blink::ScrollbarMode::kAlwaysOff)
    SetCanHaveScrollbars(false);
}

LocalFrameView::~LocalFrameView() {
#if DCHECK_IS_ON()
  DCHECK(has_been_disposed_);
#endif
}

void LocalFrameView::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(fragment_anchor_);
  visitor->Trace(scrollable_areas_);
  visitor->Trace(animating_scrollable_areas_);
  visitor->Trace(auto_size_info_);
  visitor->Trace(plugins_);
  visitor->Trace(scrollbars_);
  visitor->Trace(viewport_scrollable_area_);
  visitor->Trace(anchoring_adjustment_queue_);
  visitor->Trace(scroll_event_queue_);
  visitor->Trace(layout_shift_tracker_);
  visitor->Trace(paint_timing_detector_);
  visitor->Trace(mobile_friendliness_checker_);
  visitor->Trace(lifecycle_observers_);
  visitor->Trace(fullscreen_video_elements_);
}

template <typename Function>
void LocalFrameView::ForAllChildViewsAndPlugins(const Function& function) {
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (child->View())
      function(*child->View());
  }

  for (const auto& plugin : plugins_) {
    function(*plugin);
  }

  if (Document* document = frame_->GetDocument()) {
    for (PortalContents* portal :
         DocumentPortals::From(*document).GetPortals()) {
      if (Frame* frame = portal->GetFrame())
        function(*frame->View());
    }
  }
}

template <typename Function>
void LocalFrameView::ForAllChildLocalFrameViews(const Function& function) {
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (!child_local_frame)
      continue;
    if (LocalFrameView* child_view = child_local_frame->View())
      function(*child_view);
  }
}

// Call function for each non-throttled frame view in pre-order. If this logic
// is updated, consider updating |ForAllThrottledLocalFrameViews| too.
template <typename Function>
void LocalFrameView::ForAllNonThrottledLocalFrameViews(
    const Function& function) {
  if (ShouldThrottleRendering())
    return;

  function(*this);

  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (!child_local_frame)
      continue;
    if (LocalFrameView* child_view = child_local_frame->View())
      child_view->ForAllNonThrottledLocalFrameViews(function);
  }
}

// Call function for each throttled frame view in pre-order. If this logic is
// updated, consider updating |ForAllNonThrottledLocalFrameViews| too.
template <typename Function>
void LocalFrameView::ForAllThrottledLocalFrameViews(const Function& function) {
  if (ShouldThrottleRendering())
    function(*this);

  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (!child_local_frame)
      continue;
    if (LocalFrameView* child_view = child_local_frame->View())
      child_view->ForAllThrottledLocalFrameViews(function);
  }
}

void LocalFrameView::ForAllThrottledLocalFrameViewsForTesting(
    base::RepeatingCallback<void(LocalFrameView&)> callback) {
  ForAllThrottledLocalFrameViews(
      [&callback](LocalFrameView& view) { callback.Run(view); });
}

template <typename Function>
void LocalFrameView::ForAllRemoteFrameViews(const Function& function) {
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (child->IsLocalFrame()) {
      To<LocalFrame>(child)->View()->ForAllRemoteFrameViews(function);
    } else {
      DCHECK(child->IsRemoteFrame());
      if (RemoteFrameView* view = To<RemoteFrame>(child)->View())
        function(*view);
    }
  }
  if (Document* document = frame_->GetDocument()) {
    for (PortalContents* portal :
         DocumentPortals::From(*document).GetPortals()) {
      if (RemoteFrame* frame = portal->GetFrame()) {
        if (RemoteFrameView* view = frame->View())
          function(*view);
      }
    }
  }
}

void LocalFrameView::Dispose() {
  CHECK(!IsInPerformLayout());

  // TODO(dcheng): It's wrong that the frame can be detached before the
  // LocalFrameView. Figure out what's going on and fix LocalFrameView to be
  // disposed with the correct timing.

  // We need to clear the RootFrameViewport's animator since it gets called
  // from non-GC'd objects and RootFrameViewport will still have a pointer to
  // this class.
  if (viewport_scrollable_area_)
    viewport_scrollable_area_->ClearScrollableArea();

  // Destroy |m_autoSizeInfo| as early as possible, to avoid dereferencing
  // partially destroyed |this| via |m_autoSizeInfo->m_frameView|.
  auto_size_info_.Clear();

  // FIXME: Do we need to do something here for OOPI?
  HTMLFrameOwnerElement* owner_element = frame_->DeprecatedLocalOwner();
  // TODO(dcheng): It seems buggy that we can have an owner element that points
  // to another EmbeddedContentView. This can happen when a plugin element loads
  // a frame (EmbeddedContentView A of type LocalFrameView) and then loads a
  // plugin (EmbeddedContentView B of type WebPluginContainerImpl). In this
  // case, the frame's view is A and the frame element's
  // OwnedEmbeddedContentView is B. See https://crbug.com/673170 for an example.
  if (owner_element && owner_element->OwnedEmbeddedContentView() == this)
    owner_element->SetEmbeddedContentView(nullptr);

  ukm_aggregator_.reset();
  layout_shift_tracker_->Dispose();

#if DCHECK_IS_ON()
  has_been_disposed_ = true;
#endif
}

void LocalFrameView::InvalidateAllCustomScrollbarsOnActiveChanged() {
  bool uses_window_inactive_selector =
      frame_->GetDocument()->GetStyleEngine().UsesWindowInactiveSelector();

  ForAllChildLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.InvalidateAllCustomScrollbarsOnActiveChanged();
  });

  for (const auto& scrollbar : scrollbars_) {
    if (uses_window_inactive_selector && scrollbar->IsCustomScrollbar())
      scrollbar->StyleChanged();
  }
}

bool LocalFrameView::DidFirstLayout() const {
  return !first_layout_;
}

bool LocalFrameView::LifecycleUpdatesActive() const {
  return !lifecycle_updates_throttled_;
}

void LocalFrameView::SetLifecycleUpdatesThrottledForTesting(bool throttled) {
  lifecycle_updates_throttled_ = throttled;
}

void LocalFrameView::InvalidateRect(const IntRect& rect) {
  auto* layout_object = GetLayoutEmbeddedContent();
  if (!layout_object)
    return;

  IntRect paint_invalidation_rect = rect;
  paint_invalidation_rect.Move(
      (layout_object->BorderLeft() + layout_object->PaddingLeft()).ToInt(),
      (layout_object->BorderTop() + layout_object->PaddingTop()).ToInt());
  layout_object->InvalidatePaintRectangle(
      PhysicalRect(paint_invalidation_rect));
}

void LocalFrameView::FrameRectsChanged(const IntRect& old_rect) {
  const bool width_changed = Size().Width() != old_rect.Width();
  const bool height_changed = Size().Height() != old_rect.Height();

  PropagateFrameRects();

  if (FrameRect() != old_rect) {
    if (auto* layout_view = GetLayoutView())
      layout_view->SetShouldCheckForPaintInvalidation();
  }

  if (width_changed || height_changed) {
    ViewportSizeChanged(width_changed, height_changed);
    if (frame_->IsMainFrame())
      frame_->GetPage()->GetVisualViewport().MainFrameDidChangeSize();
    GetFrame().Loader().RestoreScrollPositionAndViewState();
  }
}

Page* LocalFrameView::GetPage() const {
  return GetFrame().GetPage();
}

LayoutView* LocalFrameView::GetLayoutView() const {
  return GetFrame().ContentLayoutObject();
}

ScrollingCoordinator* LocalFrameView::GetScrollingCoordinator() const {
  Page* p = GetPage();
  return p ? p->GetScrollingCoordinator() : nullptr;
}

ScrollingCoordinatorContext* LocalFrameView::GetScrollingContext() const {
  LocalFrame* root = &GetFrame().LocalFrameRoot();
  if (GetFrame() != root)
    return root->View()->GetScrollingContext();

  if (!scrolling_context_)
    scrolling_context_.reset(new ScrollingCoordinatorContext());
  return scrolling_context_.get();
}

cc::AnimationHost* LocalFrameView::GetCompositorAnimationHost() const {
  if (GetScrollingContext()->GetCompositorAnimationHost())
    return GetScrollingContext()->GetCompositorAnimationHost();

  if (!GetFrame().LocalFrameRoot().IsMainFrame())
    return nullptr;

  // TODO(kenrb): Compositor animation host and timeline for the main frame
  // still live on ScrollingCoordinator. https://crbug.com/680606.
  ScrollingCoordinator* c = GetScrollingCoordinator();
  return c ? c->GetCompositorAnimationHost() : nullptr;
}

CompositorAnimationTimeline* LocalFrameView::GetCompositorAnimationTimeline()
    const {
  if (GetScrollingContext()->GetCompositorAnimationTimeline())
    return GetScrollingContext()->GetCompositorAnimationTimeline();

  if (!GetFrame().LocalFrameRoot().IsMainFrame())
    return nullptr;

  // TODO(kenrb): Compositor animation host and timeline for the main frame
  // still live on ScrollingCoordinator. https://crbug.com/680606.
  ScrollingCoordinator* c = GetScrollingCoordinator();
  return c ? c->GetCompositorAnimationTimeline() : nullptr;
}

void LocalFrameView::SetLayoutOverflowSize(const IntSize& size) {
  if (size == layout_overflow_size_)
    return;

  layout_overflow_size_ = size;

  Page* page = GetFrame().GetPage();
  if (!page)
    return;
  page->GetChromeClient().ContentsSizeChanged(frame_.Get(), size);
}

void LocalFrameView::AdjustViewSize() {
  if (suppress_adjust_view_size_)
    return;

  LayoutView* layout_view = GetLayoutView();
  if (!layout_view)
    return;

  DCHECK_EQ(frame_->View(), this);
  SetLayoutOverflowSize(
      PixelSnappedIntRect(layout_view->DocumentRect()).Size());
}

void LocalFrameView::AdjustViewSizeAndLayout() {
  AdjustViewSize();
  if (NeedsLayout()) {
    base::AutoReset<bool> suppress_adjust_view_size(&suppress_adjust_view_size_,
                                                    true);
    UpdateLayout();
  }
}

void LocalFrameView::UpdateCountersAfterStyleChange() {
  auto* layout_view = GetLayoutView();
  DCHECK(layout_view);
  layout_view->UpdateCounters();
}

void LocalFrameView::CountObjectsNeedingLayout(unsigned& needs_layout_objects,
                                               unsigned& total_objects,
                                               bool& is_subtree) {
  needs_layout_objects = 0;
  total_objects = 0;
  is_subtree = IsSubtreeLayout();
  if (is_subtree) {
    layout_subtree_root_list_.CountObjectsNeedingLayout(needs_layout_objects,
                                                        total_objects);
  } else {
    LayoutSubtreeRootList::CountObjectsNeedingLayoutInRoot(
        GetLayoutView(), needs_layout_objects, total_objects);
  }
}

void LocalFrameView::PerformPreLayoutTasks() {
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::performPreLayoutTasks");
  Lifecycle().AdvanceTo(DocumentLifecycle::kInPreLayout);

  // Don't schedule more layouts, we're in one.
  base::AutoReset<bool> change_scheduling_enabled(&layout_scheduling_enabled_,
                                                  false);

  bool was_resized = WasViewportResized();
  Document* document = frame_->GetDocument();
  if (was_resized)
    document->SetResizedForViewportUnits();

  // Viewport-dependent or device-dependent media queries may cause us to need
  // completely different style information.
  bool main_frame_rotation =
      frame_->IsMainFrame() && frame_->GetSettings() &&
      frame_->GetSettings()->GetMainFrameResizesAreOrientationChanges();
  if ((was_resized &&
       document->GetStyleEngine().MediaQueryAffectedByViewportChange()) ||
      (was_resized && main_frame_rotation &&
       document->GetStyleEngine().MediaQueryAffectedByDeviceChange())) {
    document->MediaQueryAffectingValueChanged(MediaValueChange::kSize);
  } else if (was_resized) {
    document->EvaluateMediaQueryList();
  }

  document->UpdateStyleAndLayoutTree();

  // Update style for all embedded SVG documents underneath this frame, so
  // that intrinsic size computation for any embedded objects has up-to-date
  // information before layout.
  ForAllChildLocalFrameViews([](LocalFrameView& view) {
    Document& document = *view.GetFrame().GetDocument();
    if (document.IsSVGDocument())
      document.UpdateStyleAndLayoutTree();
  });

  Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

  if (was_resized) {
    document->ClearResizedForViewportUnits();

    // Mark all of writing-mode roots for layout, as the ICB size has changed.
    MarkOrthogonalWritingModeRootsForLayout();
  }
}

bool LocalFrameView::LayoutFromRootObject(LayoutObject& root) {
  if (!root.NeedsLayout())
    return false;

  if (auto* locked_ancestor =
          DisplayLockUtilities::LockedAncestorPreventingLayout(root)) {
    // Note that since we're preventing the layout on a layout root, we have to
    // mark its ancestor chain for layout. The reason for this is that we will
    // clear the layout roots whether or not we have finished laying them out,
    // so the fact that this root still needs layout will be lost if we don't
    // mark its container chain.
    //
    // Also, since we know that this root has a layout-blocking ancestor, the
    // layout bit propagation will stop there.
    //
    // TODO(vmpstr): Note that an alternative to this approach is to keep `root`
    // as a layout root in `layout_subtree_root_list_`. It would mean that we
    // will keep it in the list while the display-lock prevents layout. We need
    // to investigate which of these approaches is better.
    root.MarkContainerChainForLayout();
    return false;
  }

  LayoutState layout_state(root);
  if (scrollable_areas_) {
    for (auto& scrollable_area : *scrollable_areas_) {
      if (scrollable_area->GetScrollAnchor() &&
          scrollable_area->ShouldPerformScrollAnchoring())
        scrollable_area->GetScrollAnchor()->NotifyBeforeLayout();
    }
  }

  ToLayoutBox(root).LayoutSubtreeRoot();
  return true;
}

void LocalFrameView::PrepareLayoutAnalyzer() {
  bool is_tracing = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("blink.debug.layout"), &is_tracing);
  if (!is_tracing) {
    analyzer_.reset();
    return;
  }
  if (!analyzer_)
    analyzer_ = std::make_unique<LayoutAnalyzer>();
  analyzer_->Reset();
}

std::unique_ptr<TracedValue> LocalFrameView::AnalyzerCounters() {
  if (!analyzer_)
    return std::make_unique<TracedValue>();
  std::unique_ptr<TracedValue> value = analyzer_->ToTracedValue();
  value->SetString("host", GetLayoutView()->GetDocument().location()->host());
  value->SetString(
      "frame",
      String::Format("0x%" PRIxPTR, reinterpret_cast<uintptr_t>(frame_.Get())));
  value->SetInteger(
      "contentsHeightAfterLayout",
      PixelSnappedIntRect(GetLayoutView()->DocumentRect()).Height());
  value->SetInteger("visibleHeight", Height());
  value->SetInteger("approximateBlankCharacterCount",
                    base::saturated_cast<int>(
                        FontFaceSetDocument::ApproximateBlankCharacterCount(
                            *frame_->GetDocument())));
  return value;
}

#define PERFORM_LAYOUT_TRACE_CATEGORIES \
  "blink,benchmark,rail," TRACE_DISABLED_BY_DEFAULT("blink.debug.layout")

void LocalFrameView::PerformLayout(bool in_subtree_layout) {
  DCHECK(in_subtree_layout || layout_subtree_root_list_.IsEmpty());

  double contents_height_before_layout =
      GetLayoutView()->DocumentRect().Height();
  TRACE_EVENT_BEGIN1(
      PERFORM_LAYOUT_TRACE_CATEGORIES, "LocalFrameView::performLayout",
      "contentsHeightBeforeLayout", contents_height_before_layout);
  PrepareLayoutAnalyzer();

  ScriptForbiddenScope forbid_script;

  if (in_subtree_layout && HasOrthogonalWritingModeRoots()) {
    // If we're going to lay out from each subtree root, rather than once from
    // LayoutView, we need to merge the depth-ordered orthogonal writing mode
    // root list into the depth-ordered list of subtrees scheduled for
    // layout. Otherwise, during layout of one such subtree, we'd risk skipping
    // over a subtree of objects needing layout.
    DCHECK(!layout_subtree_root_list_.IsEmpty());
    ScheduleOrthogonalWritingModeRootsForLayout();
  }

  DCHECK(!IsInPerformLayout());
  Lifecycle().AdvanceTo(DocumentLifecycle::kInPerformLayout);

  // performLayout is the actual guts of layout().
  // FIXME: The 300 other lines in layout() probably belong in other helper
  // functions so that a single human could understand what layout() is actually
  // doing.

  {
    // TODO(szager): Remove this after diagnosing crash.
    DocumentLifecycle::CheckNoTransitionScope check_no_transition(Lifecycle());
    if (in_subtree_layout) {
      if (analyzer_) {
        analyzer_->Increment(LayoutAnalyzer::kPerformLayoutRootLayoutObjects,
                             layout_subtree_root_list_.size());
      }
      for (auto& root : layout_subtree_root_list_.Ordered()) {
        if (!LayoutFromRootObject(*root))
          continue;

        root->PaintingLayer()->UpdateLayerPositionsAfterLayout();

        // We need to ensure that we mark up all layoutObjects up to the
        // LayoutView for paint invalidation. This simplifies our code as we
        // just always do a full tree walk.
        if (LayoutObject* container = root->Container())
          container->SetShouldCheckForPaintInvalidation();
      }
      layout_subtree_root_list_.Clear();
    } else {
      if (HasOrthogonalWritingModeRoots())
        LayoutOrthogonalWritingModeRoots();
      GetLayoutView()->UpdateLayout();
    }
  }

  frame_->GetDocument()->Fetcher()->UpdateAllImageResourcePriorities();

  Lifecycle().AdvanceTo(DocumentLifecycle::kAfterPerformLayout);

  TRACE_EVENT_END1(PERFORM_LAYOUT_TRACE_CATEGORIES,
                   "LocalFrameView::performLayout", "counters",
                   AnalyzerCounters());
  FirstMeaningfulPaintDetector::From(*frame_->GetDocument())
      .MarkNextPaintAsMeaningfulIfNeeded(
          layout_object_counter_, contents_height_before_layout,
          GetLayoutView()->DocumentRect().Height(), Height());
}

void LocalFrameView::UpdateLayout() {
  // We should never layout a Document which is not in a LocalFrame.
  DCHECK(frame_);
  DCHECK_EQ(frame_->View(), this);
  DCHECK(frame_->GetPage());

  ScriptForbiddenScope forbid_script;

  if (IsInPerformLayout() || ShouldThrottleRendering() ||
      !frame_->GetDocument()->IsActive() || frame_->IsProvisional())
    return;

  TRACE_EVENT0("blink,benchmark", "LocalFrameView::layout");

  RUNTIME_CALL_TIMER_SCOPE(V8PerIsolateData::MainThreadIsolate(),
                           RuntimeCallStats::CounterId::kUpdateLayout);

  // The actual call to UpdateGeometries is in PerformPostLayoutTasks.
  SetNeedsUpdateGeometries();

  if (auto_size_info_)
    auto_size_info_->AutoSizeIfNeeded();

  has_pending_layout_ = false;

  Document* document = frame_->GetDocument();
  TRACE_EVENT_BEGIN1("devtools.timeline", "Layout", "beginData",
                     inspector_layout_event::BeginData(this));
  probe::UpdateLayout probe(document);

  PerformPreLayoutTasks();

  VisualViewport& visual_viewport = frame_->GetPage()->GetVisualViewport();
  DoubleSize viewport_size(visual_viewport.VisibleWidthCSSPx(),
                           visual_viewport.VisibleHeightCSSPx());

  // TODO(crbug.com/460956): The notion of a single root for layout is no
  // longer applicable. Remove or update this code.
  LayoutObject* root_for_this_layout = GetLayoutView();

  FontCachePurgePreventer font_cache_purge_preventer;
  StyleRetainScope style_retain_scope;
  bool in_subtree_layout = false;
  {
    base::AutoReset<bool> change_scheduling_enabled(&layout_scheduling_enabled_,
                                                    false);
    nested_layout_count_++;

    // If the layout view was marked as needing layout after we added items in
    // the subtree roots we need to clear the roots and do the layout from the
    // layoutView.
    if (GetLayoutView()->NeedsLayout())
      ClearLayoutSubtreeRootsAndMarkContainingBlocks();
    GetLayoutView()->ClearHitTestCache();

    in_subtree_layout = IsSubtreeLayout();

    // TODO(crbug.com/460956): The notion of a single root for layout is no
    // longer applicable. Remove or update this code.
    if (in_subtree_layout)
      root_for_this_layout = layout_subtree_root_list_.RandomRoot();

    if (!root_for_this_layout) {
      // FIXME: Do we need to set m_size here?
      NOTREACHED();
      return;
    }

    if (!in_subtree_layout) {
      ClearLayoutSubtreeRootsAndMarkContainingBlocks();
      Node* body = document->body();
      if (body && body->GetLayoutObject()) {
        if (IsA<HTMLFrameSetElement>(*body)) {
          body->GetLayoutObject()->SetChildNeedsLayout();
        } else if (IsA<HTMLBodyElement>(*body)) {
          if (!first_layout_ && size_.Height() != GetLayoutSize().Height() &&
              body->GetLayoutObject()->EnclosingBox()->StretchesToViewport())
            body->GetLayoutObject()->SetChildNeedsLayout();
        }
      }

      if (first_layout_) {
        first_layout_ = false;
        last_viewport_size_ = GetLayoutSize();
        last_zoom_factor_ = GetLayoutView()->StyleRef().Zoom();

        mojom::blink::ScrollbarMode h_mode;
        mojom::blink::ScrollbarMode v_mode;
        GetLayoutView()->CalculateScrollbarModes(h_mode, v_mode);
        if (v_mode == mojom::blink::ScrollbarMode::kAuto) {
          if (auto* scrollable_area = GetLayoutView()->GetScrollableArea())
            scrollable_area->ForceVerticalScrollbarForFirstLayout();
        }
      }

      LayoutSize old_size = size_;

      size_ = LayoutSize(GetLayoutSize());

      if (old_size != size_ && !first_layout_) {
        LayoutBox* root_layout_object =
            document->documentElement()
                ? document->documentElement()->GetLayoutBox()
                : nullptr;
        LayoutBox* body_layout_object = root_layout_object && document->body()
                                            ? document->body()->GetLayoutBox()
                                            : nullptr;
        if (body_layout_object && body_layout_object->StretchesToViewport())
          body_layout_object->SetChildNeedsLayout();
        else if (root_layout_object &&
                 root_layout_object->StretchesToViewport())
          root_layout_object->SetChildNeedsLayout();
      }
    }

    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.trees"), "LayoutTree",
        this, TracedLayoutObject::Create(*GetLayoutView(), false));

    IntSize old_size(Size());

    PerformLayout(in_subtree_layout);

    IntSize new_size(Size());
    if (old_size != new_size) {
      MarkViewportConstrainedObjectsForLayout(
          old_size.Width() != new_size.Width(),
          old_size.Height() != new_size.Height());
    }

    if (frame_->IsMainFrame()) {
      if (auto* text_autosizer = frame_->GetDocument()->GetTextAutosizer()) {
        if (text_autosizer->HasLayoutInlineSizeChanged())
          text_autosizer->UpdatePageInfoInAllFrames(frame_);
      }
    }

    if (NeedsLayout()) {
      base::AutoReset<bool> suppress(&suppress_adjust_view_size_, true);
      UpdateLayout();
    }

    DCHECK(layout_subtree_root_list_.IsEmpty());
  }  // Reset m_layoutSchedulingEnabled to its previous value.
  CheckDoesNotNeedLayout();

  DocumentLifecycle::Scope lifecycle_scope(Lifecycle(),
                                           DocumentLifecycle::kLayoutClean);

  frame_timing_requests_dirty_ = true;

  if (!in_subtree_layout)
    GetLayoutView()->EnclosingLayer()->UpdateLayerPositionsAfterLayout();

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.trees"), "LayoutTree", this,
      TracedLayoutObject::Create(*GetLayoutView(), true));

  layout_count_for_testing_++;

  if (AXObjectCache* cache = document->ExistingAXObjectCache()) {
    const KURL& url = document->Url();
    if (url.IsValid() && !url.IsAboutBlankURL()) {
      // TODO(kschmi) move HandleLayoutComplete to the accessibility lifecycle
      // stage. crbug.com/1062122
      cache->HandleLayoutComplete(document);
    }
  }

  UpdateDocumentAnnotatedRegions();
  CheckDoesNotNeedLayout();

  if (nested_layout_count_ == 1) {
    PerformPostLayoutTasks();
    CheckDoesNotNeedLayout();
  }

  // FIXME: The notion of a single root for layout is no longer applicable.
  // Remove or update this code. crbug.com/460596
  TRACE_EVENT_END1("devtools.timeline", "Layout", "endData",
                   inspector_layout_event::EndData(root_for_this_layout));
  probe::DidChangeViewport(frame_.Get());

  nested_layout_count_--;
  if (nested_layout_count_)
    return;

#if DCHECK_IS_ON()
  // Post-layout assert that nobody was re-marked as needing layout during
  // layout.
  GetLayoutView()->AssertSubtreeIsLaidOut();
#endif

  if (frame_->IsMainFrame()) {
    // Scrollbars changing state can cause a visual viewport size change.
    DoubleSize new_viewport_size(visual_viewport.VisibleWidthCSSPx(),
                                 visual_viewport.VisibleHeightCSSPx());
    if (new_viewport_size != viewport_size)
      frame_->GetDocument()->EnqueueVisualViewportResizeEvent();
  }

  GetFrame().GetDocument()->LayoutUpdated();
  CheckDoesNotNeedLayout();
}

void LocalFrameView::WillStartForcedLayout() {
  // UpdateLayout is re-entrant for auto-sizing and plugins. So keep
  // track of stack depth to include all the time in the top-level call.
  forced_layout_stack_depth_++;
  if (forced_layout_stack_depth_ > 1)
    return;
  forced_layout_start_time_ = base::TimeTicks::Now();
}

void LocalFrameView::DidFinishForcedLayout(DocumentUpdateReason reason) {
  CHECK_GT(forced_layout_stack_depth_, (unsigned)0);
  forced_layout_stack_depth_--;
  if (!forced_layout_stack_depth_ && base::TimeTicks::IsHighResolution()) {
    LocalFrameUkmAggregator& aggregator = EnsureUkmAggregator();
    aggregator.RecordSample(
        static_cast<size_t>(LocalFrameUkmAggregator::kForcedStyleAndLayout),
        forced_layout_start_time_, base::TimeTicks::Now());
    if (reason == DocumentUpdateReason::kHitTest) {
      aggregator.RecordSample(
          static_cast<size_t>(LocalFrameUkmAggregator::kHitTestDocumentUpdate),
          forced_layout_start_time_, base::TimeTicks::Now());
    }
  }
}

void LocalFrameView::MarkFirstEligibleToPaint() {
  if (frame_ && frame_->GetDocument()) {
    PaintTiming& timing = PaintTiming::From(*frame_->GetDocument());
    timing.MarkFirstEligibleToPaint();
  }
}

void LocalFrameView::MarkIneligibleToPaint() {
  if (frame_ && frame_->GetDocument()) {
    PaintTiming& timing = PaintTiming::From(*frame_->GetDocument());
    timing.MarkIneligibleToPaint();
  }
}

void LocalFrameView::SetNeedsPaintPropertyUpdate() {
  if (auto* layout_view = GetLayoutView())
    layout_view->SetNeedsPaintPropertyUpdate();
}

FloatSize LocalFrameView::ViewportSizeForViewportUnits() const {
  float zoom = 1;
  if (!frame_->GetDocument() || !frame_->GetDocument()->Printing())
    zoom = GetFrame().PageZoomFactor();

  auto* layout_view = GetLayoutView();
  if (!layout_view)
    return FloatSize();

  FloatSize layout_size;
  layout_size.SetWidth(layout_view->ViewWidth(kIncludeScrollbars) / zoom);
  layout_size.SetHeight(layout_view->ViewHeight(kIncludeScrollbars) / zoom);

  BrowserControls& browser_controls = frame_->GetPage()->GetBrowserControls();
  if (browser_controls.PermittedState() != cc::BrowserControlsState::kHidden) {
    // We use the layoutSize rather than frameRect to calculate viewport units
    // so that we get correct results on mobile where the page is laid out into
    // a rect that may be larger than the viewport (e.g. the 980px fallback
    // width for desktop pages). Since the layout height is statically set to
    // be the viewport with browser controls showing, we add the browser
    // controls height, compensating for page scale as well, since we want to
    // use the viewport with browser controls hidden for vh (to match Safari).
    int viewport_width = frame_->GetPage()->GetVisualViewport().Size().Width();
    if (frame_->IsMainFrame() && layout_size.Width() && viewport_width) {
      // TODO(bokan/eirage): BrowserControl height may need to account for the
      // zoom factor when use-zoom-for-dsf is enabled on Android. Confirm this
      // works correctly when that's turned on. https://crbug.com/737777.
      float page_scale_at_layout_width = viewport_width / layout_size.Width();
      layout_size.Expand(0, (browser_controls.TotalHeight() -
                             browser_controls.TotalMinHeight()) /
                                page_scale_at_layout_width);
    }
  }

  return layout_size;
}

FloatSize LocalFrameView::ViewportSizeForMediaQueries() const {
  FloatSize viewport_size(GetLayoutSize());
  if (!frame_->GetDocument() || !frame_->GetDocument()->Printing())
    viewport_size.Scale(1 / GetFrame().PageZoomFactor());
  return viewport_size;
}

DocumentLifecycle& LocalFrameView::Lifecycle() const {
  DCHECK(frame_);
  DCHECK(frame_->GetDocument());
  return frame_->GetDocument()->Lifecycle();
}

void LocalFrameView::RunPostLifecycleSteps() {
  RunIntersectionObserverSteps();
}

void LocalFrameView::RunIntersectionObserverSteps() {
#if DCHECK_IS_ON()
  bool was_dirty = NeedsLayout();
#endif
  if ((intersection_observation_state_ < kRequired &&
       ShouldThrottleRendering()) ||
      Lifecycle().LifecyclePostponed() || !frame_->GetDocument()->IsActive()) {
    return;
  }

  if (frame_->IsMainFrame()) {
    EnsureOverlayInterstitialAdDetector().MaybeFireDetection(frame_.Get());
    EnsureStickyAdDetector().MaybeFireDetection(frame_.Get());

    // Report the main frame's document intersection with itself.
    LayoutObject* layout_object = GetLayoutView();
    IntRect main_frame_dimensions =
        ToLayoutBox(layout_object)->PixelSnappedLayoutOverflowRect();
    GetFrame().Client()->OnMainFrameIntersectionChanged(WebRect(
        0, 0, main_frame_dimensions.Width(), main_frame_dimensions.Height()));
  }

  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::UpdateViewportIntersectionsForSubtree");
  SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                           LocalFrameUkmAggregator::kIntersectionObservation);

  bool needs_occlusion_tracking = UpdateViewportIntersectionsForSubtree(0);
  if (FrameOwner* owner = frame_->Owner())
    owner->SetNeedsOcclusionTracking(needs_occlusion_tracking);
#if DCHECK_IS_ON()
  DCHECK(was_dirty || !NeedsLayout());
#endif
  DeliverSynchronousIntersectionObservations();
}

void LocalFrameView::ForceUpdateViewportIntersections() {
  // IntersectionObserver targets in this frame (and its frame tree) need to
  // update; but we can't wait for a lifecycle update to run them, because a
  // hidden frame won't run lifecycle updates. Force layout and run them now.
  DocumentLifecycle::DisallowThrottlingScope disallow_throttling(Lifecycle());
  UpdateLifecycleToCompositingCleanPlusScrolling(
      DocumentUpdateReason::kIntersectionObservation);
  UpdateViewportIntersectionsForSubtree(
      IntersectionObservation::kImplicitRootObserversNeedUpdate |
      IntersectionObservation::kIgnoreDelay);
}

LayoutSVGRoot* LocalFrameView::EmbeddedReplacedContent() const {
  auto* layout_view = this->GetLayoutView();
  if (!layout_view)
    return nullptr;

  LayoutObject* first_child = layout_view->FirstChild();
  if (!first_child || !first_child->IsBox())
    return nullptr;

  // Currently only embedded SVG documents participate in the size-negotiation
  // logic.
  return ToLayoutSVGRootOrNull(first_child);
}

bool LocalFrameView::GetIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  if (LayoutSVGRoot* content_layout_object = EmbeddedReplacedContent()) {
    content_layout_object->UnscaledIntrinsicSizingInfo(intrinsic_sizing_info);
    return true;
  }
  return false;
}

bool LocalFrameView::HasIntrinsicSizingInfo() const {
  return EmbeddedReplacedContent();
}

void LocalFrameView::UpdateGeometry() {
  LayoutEmbeddedContent* layout = GetLayoutEmbeddedContent();
  if (!layout)
    return;

  bool did_need_layout = NeedsLayout();

  PhysicalRect new_frame = layout->ReplacedContentRect();
#if DCHECK_IS_ON()
  if (new_frame.Width() != LayoutUnit::Max().RawValue() &&
      new_frame.Height() != LayoutUnit::Max().RawValue())
    DCHECK(!new_frame.size.HasFraction());
#endif
  bool bounds_will_change = PhysicalSize(Size()) != new_frame.size;

  // If frame bounds are changing mark the view for layout. Also check the
  // frame's page to make sure that the frame isn't in the process of being
  // destroyed. If iframe scrollbars needs reconstruction from native to custom
  // scrollbar, then also we need to layout the frameview.
  if (bounds_will_change)
    SetNeedsLayout();

  layout->UpdateGeometry(*this);
  // If view needs layout, either because bounds have changed or possibly
  // indicating content size is wrong, we have to do a layout to set the right
  // LocalFrameView size.
  if (NeedsLayout())
    UpdateLayout();

  if (!did_need_layout && !ShouldThrottleRendering())
    CheckDoesNotNeedLayout();
}

void LocalFrameView::AddPartToUpdate(LayoutEmbeddedObject& object) {
  // This is typically called during layout to ensure we update plugins.
  // However, if layout is blocked (e.g. by content-visibility), we can add the
  // part to update during layout tree attachment (which is a part of style
  // recalc).
  DCHECK(IsInPerformLayout() ||
         (DisplayLockUtilities::NearestLockedExclusiveAncestor(object) &&
          frame_->GetDocument()->InStyleRecalc()));

  // Tell the DOM element that it needs a Plugin update.
  Node* node = object.GetNode();
  DCHECK(node);
  if (IsA<HTMLObjectElement>(*node) || IsA<HTMLEmbedElement>(*node))
    To<HTMLPlugInElement>(node)->SetNeedsPluginUpdate(true);

  part_update_set_.insert(&object);
}

void LocalFrameView::SetMediaType(const AtomicString& media_type) {
  DCHECK(frame_->GetDocument());
  media_type_ = media_type;
  frame_->GetDocument()->MediaQueryAffectingValueChanged(
      MediaValueChange::kOther);
}

AtomicString LocalFrameView::MediaType() const {
  // See if we have an override type.
  if (frame_->GetSettings() &&
      !frame_->GetSettings()->GetMediaTypeOverride().IsEmpty())
    return AtomicString(frame_->GetSettings()->GetMediaTypeOverride());
  return media_type_;
}

void LocalFrameView::AdjustMediaTypeForPrinting(bool printing) {
  if (printing) {
    if (media_type_when_not_printing_.IsNull())
      media_type_when_not_printing_ = MediaType();
    SetMediaType(media_type_names::kPrint);
  } else {
    if (!media_type_when_not_printing_.IsNull())
      SetMediaType(media_type_when_not_printing_);
    media_type_when_not_printing_ = g_null_atom;
  }
}

void LocalFrameView::AddBackgroundAttachmentFixedObject(LayoutObject* object) {
  DCHECK(!background_attachment_fixed_objects_.Contains(object));

  background_attachment_fixed_objects_.insert(object);

  // Ensure main thread scrolling reasons are recomputed.
  SetNeedsPaintPropertyUpdate();
  // The object's scroll properties are not affected by its own background.
  object->SetAncestorsNeedPaintPropertyUpdateForMainThreadScrolling();
}

void LocalFrameView::RemoveBackgroundAttachmentFixedObject(
    LayoutObject* object) {
  DCHECK(background_attachment_fixed_objects_.Contains(object));

  background_attachment_fixed_objects_.erase(object);

  // Ensure main thread scrolling reasons are recomputed.
  SetNeedsPaintPropertyUpdate();
  // The object's scroll properties are not affected by its own background.
  object->SetAncestorsNeedPaintPropertyUpdateForMainThreadScrolling();
}

bool LocalFrameView::RequiresMainThreadScrollingForBackgroundAttachmentFixed()
    const {
  if (background_attachment_fixed_objects_.IsEmpty())
    return false;
  if (background_attachment_fixed_objects_.size() > 1)
    return true;

  const auto* object =
      ToLayoutBoxModelObject(*background_attachment_fixed_objects_.begin());
  // We should not add such object in the set.
  DCHECK(!object->BackgroundTransfersToView());
  // If the background is viewport background and it paints onto the main
  // graphics layer only, then it doesn't need main thread scrolling.
  if (IsA<LayoutView>(object) &&
      object->GetBackgroundPaintLocation() == kBackgroundPaintInGraphicsLayer)
    return false;
  return true;
}

void LocalFrameView::AddViewportConstrainedObject(
    LayoutObject& object,
    ViewportConstrainedType constrained_reason) {
  if (!viewport_constrained_objects_)
    viewport_constrained_objects_ = std::make_unique<ObjectSet>();

  auto result = viewport_constrained_objects_->insert(&object);
  if (constrained_reason == ViewportConstrainedType::kSticky) {
    if (result.is_new_entry) {
      sticky_position_object_count_++;
    }
    DCHECK_LE(sticky_position_object_count_,
              viewport_constrained_objects_->size());
  }
}

void LocalFrameView::RemoveViewportConstrainedObject(
    LayoutObject& object,
    ViewportConstrainedType constrained_reason) {
  if (viewport_constrained_objects_) {
    auto it = viewport_constrained_objects_->find(&object);
    if (it != viewport_constrained_objects_->end()) {
      viewport_constrained_objects_->erase(it);
      if (constrained_reason == ViewportConstrainedType::kSticky) {
        DCHECK_GT(sticky_position_object_count_, 0U);
        sticky_position_object_count_--;
      }
    }
  }
}

void LocalFrameView::ViewportSizeChanged(bool width_changed,
                                         bool height_changed) {
  DCHECK(width_changed || height_changed);
  DCHECK(frame_->GetPage());
  if (frame_->GetDocument() &&
      frame_->GetDocument()->Lifecycle().LifecyclePostponed())
    return;

  if (frame_->IsMainFrame())
    layout_shift_tracker_->NotifyViewportSizeChanged();

  auto* layout_view = GetLayoutView();
  if (layout_view) {
    // If this is the main frame, we might have got here by hiding/showing the
    // top controls.  In that case, layout won't be triggered, so we need to
    // clamp the scroll offset here.
    if (GetFrame().IsMainFrame()) {
      layout_view->Layer()->UpdateSize();
      if (auto* scrollable_area = layout_view->GetScrollableArea())
        scrollable_area->ClampScrollOffsetAfterOverflowChange();
    }

    // TODO(pdr): |UsesCompositing()| will be false with CompositeAfterPaint but
    // do we need to do these updates?
    if (layout_view->UsesCompositing()) {
      layout_view->Layer()->SetNeedsCompositingInputsUpdate();
      SetNeedsPaintPropertyUpdate();
    }
  }

  if (GetFrame().GetDocument())
    GetFrame().GetDocument()->GetRootScrollerController().DidResizeFrameView();

  // Change of viewport size after browser controls showing/hiding may affect
  // painting of the background.
  if (layout_view && frame_->IsMainFrame() &&
      frame_->GetPage()->GetBrowserControls().TotalHeight())
    layout_view->SetShouldCheckForPaintInvalidation();

  if (GetFrame().GetDocument() && !IsInPerformLayout())
    MarkViewportConstrainedObjectsForLayout(width_changed, height_changed);

  if (GetPaintTimingDetector().Visualizer())
    GetPaintTimingDetector().Visualizer()->OnViewportChanged();
}

void LocalFrameView::MarkViewportConstrainedObjectsForLayout(
    bool width_changed,
    bool height_changed) {
  if (!HasViewportConstrainedObjects() || !(width_changed || height_changed))
    return;

  for (auto* const viewport_constrained_object :
       *viewport_constrained_objects_) {
    LayoutObject* layout_object = viewport_constrained_object;
    const ComputedStyle& style = layout_object->StyleRef();
    if (width_changed) {
      if (style.Width().IsFixed() &&
          (style.Left().IsAuto() || style.Right().IsAuto())) {
        layout_object->SetNeedsPositionedMovementLayout();
      } else {
        layout_object->SetNeedsLayoutAndFullPaintInvalidation(
            layout_invalidation_reason::kSizeChanged);
      }
    }
    if (height_changed) {
      if (style.Height().IsFixed() &&
          (style.Top().IsAuto() || style.Bottom().IsAuto())) {
        layout_object->SetNeedsPositionedMovementLayout();
      } else {
        layout_object->SetNeedsLayoutAndFullPaintInvalidation(
            layout_invalidation_reason::kSizeChanged);
      }
    }
  }
}

bool LocalFrameView::ShouldSetCursor() const {
  Page* page = GetFrame().GetPage();
  return page && page->IsPageVisible() &&
         !frame_->GetEventHandler().IsMousePositionUnknown() &&
         page->GetFocusController().IsActive();
}

void LocalFrameView::InvalidateBackgroundAttachmentFixedDescendantsOnScroll(
    const LayoutObject& scrolled_object) {
  for (auto* const layout_object : background_attachment_fixed_objects_) {
    if (scrolled_object != GetLayoutView() &&
        !layout_object->IsDescendantOf(&scrolled_object))
      continue;
    // An object needs to repaint the background on scroll when it has
    // background-attachment:fixed unless the object is the LayoutView and the
    // background is not painted on the scrolling contents.
    if (layout_object == GetLayoutView() &&
        !(GetLayoutView()->GetBackgroundPaintLocation() &
          kBackgroundPaintInScrollingContents))
      continue;
    layout_object->SetBackgroundNeedsFullPaintInvalidation();
  }
}

bool LocalFrameView::InvalidateViewportConstrainedObjects() {
  bool fast_path_allowed = true;
  for (auto* const viewport_constrained_object :
       *viewport_constrained_objects_) {
    LayoutObject* layout_object = viewport_constrained_object;
    DCHECK(layout_object->StyleRef().HasViewportConstrainedPosition() ||
           layout_object->StyleRef().HasStickyConstrainedPosition());
    DCHECK(layout_object->HasLayer());
    PaintLayer* layer = ToLayoutBoxModelObject(layout_object)->Layer();

    if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      DisableCompositingQueryAsserts disabler;
      if (layer->IsPaintInvalidationContainer())
        continue;
    }

    // If the layer has no visible content, then we shouldn't invalidate; but
    // if we're not compositing-inputs-clean, then we can't query
    // layer->SubtreeIsInvisible() here.
    layout_object->SetSubtreeShouldCheckForPaintInvalidation();
    if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
        !layer->SelfOrDescendantNeedsRepaint()) {
      DisableCompositingQueryAsserts disabler;
      // Paint properties of the layer relative to its containing graphics
      // layer may change if the paint properties escape the graphics layer's
      // property state. Need to check raster invalidation for relative paint
      // property changes.
      if (auto* paint_invalidation_layer =
              layer->EnclosingLayerForPaintInvalidation()) {
        auto* mapping = paint_invalidation_layer->GetCompositedLayerMapping();
        if (!mapping)
          mapping = paint_invalidation_layer->GroupedMapping();
        if (mapping)
          mapping->SetNeedsCheckRasterInvalidation();
      }
    }

    // If the fixed layer has a blur/drop-shadow filter applied on at least one
    // of its parents, we cannot scroll using the fast path, otherwise the
    // outsets of the filter will be moved around the page.
    if (layer->HasAncestorWithFilterThatMovesPixels())
      fast_path_allowed = false;
  }
  return fast_path_allowed;
}

void LocalFrameView::ProcessUrlFragment(const KURL& url,
                                        bool same_document_navigation,
                                        bool should_scroll) {
  // We want to create the anchor even if we don't need to scroll. This ensures
  // all the side effects like setting CSS :target are correctly set.
  FragmentAnchor* anchor =
      FragmentAnchor::TryCreate(url, *frame_, should_scroll);

  if (anchor) {
    fragment_anchor_ = anchor;
    fragment_anchor_->Installed();
    // Post-load, same-document navigations need to schedule a frame in which
    // the fragment anchor will be invoked. It will be done after layout as
    // part of the lifecycle.
    if (same_document_navigation)
      ScheduleAnimation();
  }
}

void LocalFrameView::SetLayoutSize(const IntSize& size) {
  DCHECK(!LayoutSizeFixedToFrameSize());
  if (frame_->GetDocument() &&
      frame_->GetDocument()->Lifecycle().LifecyclePostponed())
    return;

  SetLayoutSizeInternal(size);
}

void LocalFrameView::SetLayoutSizeFixedToFrameSize(bool is_fixed) {
  if (layout_size_fixed_to_frame_size_ == is_fixed)
    return;

  layout_size_fixed_to_frame_size_ = is_fixed;
  if (is_fixed)
    SetLayoutSizeInternal(Size());
}

static cc::LayerSelection ComputeLayerSelection(LocalFrame& frame) {
  if (!frame.View() || frame.View()->ShouldThrottleRendering())
    return {};

  return ComputeLayerSelection(frame.Selection());
}

void LocalFrameView::UpdateCompositedSelectionIfNeeded() {
  if (!RuntimeEnabledFeatures::CompositedSelectionUpdateEnabled())
    return;

  TRACE_EVENT0("blink", "LocalFrameView::updateCompositedSelectionIfNeeded");

  Page* page = GetFrame().GetPage();
  DCHECK(page);

  LocalFrame* focused_frame = page->GetFocusController().FocusedFrame();
  LocalFrame* local_frame =
      (focused_frame &&
       (focused_frame->LocalFrameRoot() == frame_->LocalFrameRoot()))
          ? focused_frame
          : nullptr;

  if (local_frame) {
    const cc::LayerSelection& selection = ComputeLayerSelection(*local_frame);
    if (selection != cc::LayerSelection()) {
      page->GetChromeClient().UpdateLayerSelection(local_frame, selection);
      return;
    }
  }

  if (!local_frame) {
    // Clearing the mainframe when there is no focused frame (and hence
    // no localFrame) is legacy behaviour, and implemented here to
    // satisfy WebFrameTest.CompositedSelectionBoundsCleared's
    // first check that the composited selection has been cleared even
    // though no frame has focus yet. If this is not desired, then the
    // expectation needs to be removed from the test.
    local_frame = &frame_->LocalFrameRoot();
  }
  DCHECK(local_frame);
  page->GetChromeClient().ClearLayerSelection(local_frame);
}

void LocalFrameView::SetNeedsCompositingUpdate(
    CompositingUpdateType update_type) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;
  if (auto* layout_view = GetLayoutView()) {
    if (frame_->GetDocument()->IsActive())
      layout_view->Compositor()->SetNeedsCompositingUpdate(update_type);
  }
}

ChromeClient* LocalFrameView::GetChromeClient() const {
  Page* page = GetFrame().GetPage();
  if (!page)
    return nullptr;
  return &page->GetChromeClient();
}

void LocalFrameView::HandleLoadCompleted() {
  // Once loading has completed, allow autoSize one last opportunity to
  // reduce the size of the frame.
  if (auto_size_info_)
    auto_size_info_->AutoSizeIfNeeded();
}

void LocalFrameView::ClearLayoutSubtreeRoot(const LayoutObject& root) {
  layout_subtree_root_list_.Remove(const_cast<LayoutObject&>(root));
}

void LocalFrameView::ClearLayoutSubtreeRootsAndMarkContainingBlocks() {
  layout_subtree_root_list_.ClearAndMarkContainingBlocksForLayout();
}

void LocalFrameView::AddOrthogonalWritingModeRoot(LayoutBox& root) {
  DCHECK(!root.IsLayoutCustomScrollbarPart());
  orthogonal_writing_mode_root_list_.Add(root);
}

void LocalFrameView::RemoveOrthogonalWritingModeRoot(LayoutBox& root) {
  orthogonal_writing_mode_root_list_.Remove(root);
}

bool LocalFrameView::HasOrthogonalWritingModeRoots() const {
  return !orthogonal_writing_mode_root_list_.IsEmpty();
}

static inline void RemoveFloatingObjectsForSubtreeRoot(LayoutObject& root) {
  // TODO(kojii): Under certain conditions, moveChildTo() defers
  // removeFloatingObjects() until the containing block layouts. For
  // instance, when descendants of the moving child is floating,
  // removeChildNode() does not clear them. In such cases, at this
  // point, FloatingObjects may contain old or even deleted objects.
  // Dealing this in markAllDescendantsWithFloatsForLayout() could
  // solve, but since that is likely to suffer the performance and
  // since the containing block of orthogonal writing mode roots
  // having floats is very rare, prefer to re-create
  // FloatingObjects.
  if (LayoutBlock* cb = root.ContainingBlock()) {
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(cb);
    if ((cb->NormalChildNeedsLayout() || cb->SelfNeedsLayout()) &&
        child_block_flow) {
      child_block_flow->RemoveFloatingObjectsFromDescendants();
    }
  }
}

static bool PrepareOrthogonalWritingModeRootForLayout(LayoutObject& root) {
  DCHECK(root.IsBox() && ToLayoutBox(root).IsOrthogonalWritingModeRoot());
  if (!root.NeedsLayout() || root.IsOutOfFlowPositioned() ||
      root.IsColumnSpanAll() ||
      !root.StyleRef().LogicalHeight().IsIntrinsicOrAuto() ||
      ToLayoutBox(root).IsGridItem() || root.IsTablePart())
    return false;

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    // Do not pre-layout objects that are fully managed by LayoutNG; it is not
    // necessary and may lead to double layouts. We do need to pre-layout
    // objects whose containing block is a legacy object so that it can
    // properly compute its intrinsic size.
    if (IsManagedByLayoutNG(root))
      return false;

    // If the root is legacy but has |CachedLayoutResult|, its parent is NG,
    // which called |RunLegacyLayout()|. This parent not only needs to run
    // pre-layout, but also clearing |NeedsLayout()| without updating
    // |CachedLayoutResult| is harmful.
    if (const auto* box = ToLayoutBoxOrNull(&root)) {
      if (box->GetCachedLayoutResult())
        return false;
    }
  }

  RemoveFloatingObjectsForSubtreeRoot(root);
  return true;
}

void LocalFrameView::LayoutOrthogonalWritingModeRoots() {
  for (auto& root : orthogonal_writing_mode_root_list_.Ordered()) {
    if (PrepareOrthogonalWritingModeRootForLayout(*root))
      LayoutFromRootObject(*root);
  }
}

void LocalFrameView::ScheduleOrthogonalWritingModeRootsForLayout() {
  for (auto& root : orthogonal_writing_mode_root_list_.Ordered()) {
    if (PrepareOrthogonalWritingModeRootForLayout(*root))
      layout_subtree_root_list_.Add(*root);
  }
}

void LocalFrameView::MarkOrthogonalWritingModeRootsForLayout() {
  for (auto& root : orthogonal_writing_mode_root_list_.Ordered()) {
    // OOF-positioned objects don't depend on the ICB size.
    if (root->NeedsLayout() || root->IsOutOfFlowPositioned())
      continue;

    root->SetNeedsLayoutAndIntrinsicWidthsRecalc(
        layout_invalidation_reason::kSizeChanged);
  }
}

bool LocalFrameView::CheckLayoutInvalidationIsAllowed() const {
#if DCHECK_IS_ON()
  if (allows_layout_invalidation_after_layout_clean_)
    return true;

  // If we are updating all lifecycle phases beyond LayoutClean, we don't expect
  // dirty layout after LayoutClean.
  CHECK_FOR_DIRTY_LAYOUT(Lifecycle().GetState() <
                         DocumentLifecycle::kLayoutClean);

#endif
  return true;
}

bool LocalFrameView::RunPostLayoutIntersectionObserverSteps() {
  DCHECK(frame_->IsLocalRoot());
  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean);

  ComputePostLayoutIntersections(0);

  bool needs_more_lifecycle_steps = false;
  ForAllNonThrottledLocalFrameViews(
      [&needs_more_lifecycle_steps](LocalFrameView& frame_view) {
        if (auto* controller = frame_view.GetFrame()
                                   .GetDocument()
                                   ->GetIntersectionObserverController()) {
          controller->DeliverNotifications(
              IntersectionObserver::kDeliverDuringPostLayoutSteps);
        }
        // If the lifecycle state changed as a result of the notifications, we
        // should run the lifecycle again.
        needs_more_lifecycle_steps |= frame_view.Lifecycle().GetState() <
                                      DocumentLifecycle::kPrePaintClean;
      });

  return needs_more_lifecycle_steps;
}

void LocalFrameView::ComputePostLayoutIntersections(unsigned parent_flags) {
  if (ShouldThrottleRendering())
    return;

  unsigned flags = GetIntersectionObservationFlags(parent_flags) |
                   IntersectionObservation::kPostLayoutDeliveryOnly;

  if (auto* controller =
          GetFrame().GetDocument()->GetIntersectionObserverController()) {
    controller->ComputeIntersections(flags);
  }

  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (!child_local_frame)
      continue;
    if (LocalFrameView* child_view = child_local_frame->View())
      child_view->ComputePostLayoutIntersections(flags);
  }
}

void LocalFrameView::ScheduleRelayout() {
  DCHECK(frame_->View() == this);

  if (!layout_scheduling_enabled_)
    return;
  // TODO(crbug.com/590856): It's still broken when we choose not to crash when
  // the check fails.
  if (!CheckLayoutInvalidationIsAllowed())
    return;
  if (!NeedsLayout())
    return;
  if (!frame_->GetDocument()->ShouldScheduleLayout())
    return;
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "InvalidateLayout", TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_invalidate_layout_event::Data(frame_.Get()));

  ClearLayoutSubtreeRootsAndMarkContainingBlocks();

  if (has_pending_layout_)
    return;
  has_pending_layout_ = true;

  if (!ShouldThrottleRendering())
    GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());
}

void LocalFrameView::ScheduleRelayoutOfSubtree(LayoutObject* relayout_root) {
  DCHECK(frame_->View() == this);
  DCHECK(relayout_root->IsBox());

  // TODO(crbug.com/590856): It's still broken when we choose not to crash when
  // the check fails.
  if (!CheckLayoutInvalidationIsAllowed())
    return;

  // FIXME: Should this call shouldScheduleLayout instead?
  if (!frame_->GetDocument()->IsActive())
    return;

  LayoutView* layout_view = this->GetLayoutView();
  if (layout_view && layout_view->NeedsLayout()) {
    if (relayout_root)
      relayout_root->MarkContainerChainForLayout(false);
    return;
  }

  if (relayout_root == layout_view)
    layout_subtree_root_list_.ClearAndMarkContainingBlocksForLayout();
  else
    layout_subtree_root_list_.Add(*relayout_root);

  if (layout_scheduling_enabled_) {
    has_pending_layout_ = true;

    if (!ShouldThrottleRendering())
      GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());

    if (GetPage()->Animator().IsServicingAnimations())
      Lifecycle().EnsureStateAtMost(DocumentLifecycle::kStyleClean);
  }
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "InvalidateLayout", TRACE_EVENT_SCOPE_THREAD, "data",
                       inspector_invalidate_layout_event::Data(frame_.Get()));
}

bool LocalFrameView::LayoutPending() const {
  // FIXME: This should check Document::lifecycle instead.
  return has_pending_layout_;
}

bool LocalFrameView::IsInPerformLayout() const {
  return Lifecycle().GetState() == DocumentLifecycle::kInPerformLayout;
}

bool LocalFrameView::NeedsLayout() const {
  // This can return true in cases where the document does not have a body yet.
  // Document::shouldScheduleLayout takes care of preventing us from scheduling
  // layout in that case.

  auto* layout_view = GetLayoutView();
  return LayoutPending() || (layout_view && layout_view->NeedsLayout()) ||
         IsSubtreeLayout();
}

NOINLINE bool LocalFrameView::CheckDoesNotNeedLayout() const {
  CHECK_FOR_DIRTY_LAYOUT(!LayoutPending());
  CHECK_FOR_DIRTY_LAYOUT(!GetLayoutView() || !GetLayoutView()->NeedsLayout());
  CHECK_FOR_DIRTY_LAYOUT(!IsSubtreeLayout());
  return true;
}

void LocalFrameView::SetNeedsLayout() {
  auto* layout_view = GetLayoutView();
  if (!layout_view)
    return;
  // TODO(crbug.com/590856): It's still broken if we choose not to crash when
  // the check fails.
  if (!CheckLayoutInvalidationIsAllowed())
    return;
  layout_view->SetNeedsLayout(layout_invalidation_reason::kUnknown);
}

bool LocalFrameView::ShouldUseColorAdjustBackground() const {
  return use_color_adjust_background_ == UseColorAdjustBackground::kYes ||
         (use_color_adjust_background_ ==
              UseColorAdjustBackground::kIfBaseNotTransparent &&
          base_background_color_ != Color::kTransparent);
}

Color LocalFrameView::BaseBackgroundColor() const {
  if (ShouldUseColorAdjustBackground()) {
    DCHECK(frame_->GetDocument());
    return frame_->GetDocument()->GetStyleEngine().ColorAdjustBackgroundColor();
  }
  return base_background_color_;
}

void LocalFrameView::SetBaseBackgroundColor(const Color& background_color) {
  if (base_background_color_ == background_color)
    return;

  base_background_color_ = background_color;

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    DisableCompositingQueryAsserts disabler;
    if (auto* layout_view = GetLayoutView()) {
      if (layout_view->Layer()->HasCompositedLayerMapping()) {
        CompositedLayerMapping* composited_layer_mapping =
            layout_view->Layer()->GetCompositedLayerMapping();
        composited_layer_mapping->UpdateContentsOpaque();
        if (composited_layer_mapping->MainGraphicsLayer())
          composited_layer_mapping->MainGraphicsLayer()->SetNeedsDisplay();
        if (composited_layer_mapping->ScrollingContentsLayer())
          composited_layer_mapping->ScrollingContentsLayer()->SetNeedsDisplay();
      }
    }
  }

  if (!ShouldThrottleRendering())
    GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());
}

void LocalFrameView::SetUseColorAdjustBackground(UseColorAdjustBackground use,
                                                 bool color_scheme_changed) {
  if (use_color_adjust_background_ == use && !color_scheme_changed)
    return;

  use_color_adjust_background_ = use;

  if (GetFrame().IsMainFrame() && ShouldUseColorAdjustBackground()) {
    // Pass the dark color-scheme background to the browser process to paint a
    // dark background in the browser tab while rendering is blocked in order to
    // avoid flashing the white background in between loading documents. If we
    // perform a navigation within the same renderer process, we keep the
    // content background from the previous page while rendering is blocked in
    // the new page, but for cross process navigations we would paint the
    // default background (typically white) while the rendering is blocked.
    GetFrame().DidChangeBackgroundColor(SkColor(BaseBackgroundColor()));
  }

  if (auto* layout_view = GetLayoutView())
    layout_view->SetBackgroundNeedsFullPaintInvalidation();
}

bool LocalFrameView::ShouldPaintBaseBackgroundColor() const {
  return ShouldUseColorAdjustBackground() ||
         frame_->GetDocument()->IsInMainFrame();
}

void LocalFrameView::UpdateBaseBackgroundColorRecursively(
    const Color& base_background_color) {
  ForAllNonThrottledLocalFrameViews(
      [base_background_color](LocalFrameView& frame_view) {
        frame_view.SetBaseBackgroundColor(base_background_color);
      });
}

void LocalFrameView::InvokeFragmentAnchor() {
  if (!fragment_anchor_)
    return;

  if (!fragment_anchor_->Invoke())
    fragment_anchor_ = nullptr;
}

void LocalFrameView::DismissFragmentAnchor() {
  if (!fragment_anchor_)
    return;

  if (fragment_anchor_->Dismiss())
    fragment_anchor_ = nullptr;
}

bool LocalFrameView::UpdatePlugins() {
  // This is always called from UpdatePluginsTimerFired.
  // update_plugins_timer should only be scheduled if we have FrameViews to
  // update. Thus I believe we can stop checking isEmpty here, and just ASSERT
  // isEmpty:
  // FIXME: This assert has been temporarily removed due to
  // https://crbug.com/430344
  if (nested_layout_count_ > 1 || part_update_set_.IsEmpty())
    return true;

  // Need to swap because script will run inside the below loop and invalidate
  // the iterator.
  EmbeddedObjectSet objects;
  objects.swap(part_update_set_);

  for (const auto& embedded_object : objects) {
    LayoutEmbeddedObject& object = *embedded_object;
    auto* element = To<HTMLPlugInElement>(object.GetNode());

    // The object may have already been destroyed (thus node cleared),
    // but LocalFrameView holds a manual ref, so it won't have been deleted.
    if (!element)
      continue;

    // No need to update if it's already crashed or known to be missing.
    if (object.ShowsUnavailablePluginIndicator())
      continue;

    if (element->NeedsPluginUpdate())
      element->UpdatePlugin();
    if (EmbeddedContentView* view = element->OwnedEmbeddedContentView())
      view->UpdateGeometry();

    // Prevent plugins from causing infinite updates of themselves.
    // FIXME: Do we really need to prevent this?
    part_update_set_.erase(&object);
  }

  return part_update_set_.IsEmpty();
}

void LocalFrameView::UpdatePluginsTimerFired(TimerBase*) {
  DCHECK(!IsInPerformLayout());
  for (unsigned i = 0; i < kMaxUpdatePluginsIterations; ++i) {
    if (UpdatePlugins())
      return;
  }
}

void LocalFrameView::FlushAnyPendingPostLayoutTasks() {
  DCHECK(!IsInPerformLayout());
  if (update_plugins_timer_.IsActive()) {
    update_plugins_timer_.Stop();
    UpdatePluginsTimerFired(nullptr);
  }
}

void LocalFrameView::ScheduleUpdatePluginsIfNecessary() {
  DCHECK(!IsInPerformLayout());
  if (update_plugins_timer_.IsActive() || part_update_set_.IsEmpty())
    return;
  update_plugins_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void LocalFrameView::PerformPostLayoutTasks() {
  // FIXME: We can reach here, even when the page is not active!
  // http/tests/inspector/elements/html-link-import.html and many other
  // tests hit that case.
  // We should DCHECK(isActive()); or at least return early if we can!

  // Always called before or after performLayout(), part of the highest-level
  // layout() call.
  DCHECK(!IsInPerformLayout());
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::performPostLayoutTasks");

  frame_->Selection().DidLayout();

  DCHECK(frame_->GetDocument());

  FontFaceSetDocument::DidLayout(*frame_->GetDocument());
  // Fire a fake a mouse move event to update hover state and mouse cursor, and
  // send the right mouse out/over events.
  // TODO(lanwei): we should check whether the mouse is inside the frame before
  // dirtying the hover state.
  frame_->LocalFrameRoot().GetEventHandler().MarkHoverStateDirty();

  UpdateGeometriesIfNeeded();

  // Plugins could have torn down the page inside updateGeometries().
  if (!GetLayoutView())
    return;

  ScheduleUpdatePluginsIfNecessary();
  SendResizeEventIfNeeded();
}

bool LocalFrameView::WasViewportResized() {
  DCHECK(frame_);
  auto* layout_view = GetLayoutView();
  if (!layout_view)
    return false;
  return (GetLayoutSize() != last_viewport_size_ ||
          layout_view->StyleRef().Zoom() != last_zoom_factor_);
}

void LocalFrameView::SendResizeEventIfNeeded() {
  DCHECK(frame_);

  auto* layout_view = GetLayoutView();
  if (!layout_view || layout_view->GetDocument().Printing())
    return;

  if (!WasViewportResized())
    return;

  last_viewport_size_ = GetLayoutSize();
  last_zoom_factor_ = layout_view->StyleRef().Zoom();

  frame_->GetDocument()->EnqueueVisualViewportResizeEvent();

  frame_->GetDocument()->EnqueueResizeEvent();

  if (frame_->IsMainFrame())
    probe::DidResizeMainFrame(frame_.Get());
}

float LocalFrameView::InputEventsScaleFactor() const {
  float page_scale = frame_->GetPage()->GetVisualViewport().Scale();
  return page_scale *
         frame_->GetPage()->GetChromeClient().InputEventsScaleForEmulation();
}

void LocalFrameView::NotifyPageThatContentAreaWillPaint() const {
  Page* page = frame_->GetPage();
  if (!page)
    return;

  if (!scrollable_areas_)
    return;

  for (const auto& scrollable_area : *scrollable_areas_) {
    if (!scrollable_area->ScrollbarsCanBeActive())
      continue;

    scrollable_area->ContentAreaWillPaint();
  }
}

void LocalFrameView::UpdateDocumentAnnotatedRegions() const {
  Document* document = frame_->GetDocument();
  if (!document->HasAnnotatedRegions())
    return;
  Vector<AnnotatedRegionValue> new_regions;
  CollectAnnotatedRegions(*(document->GetLayoutBox()), new_regions);
  if (new_regions == document->AnnotatedRegions())
    return;
  document->SetAnnotatedRegions(new_regions);

  DCHECK(frame_->Client());
  frame_->Client()->AnnotatedRegionsChanged();
}

void LocalFrameView::DidAttachDocument() {
  Page* page = frame_->GetPage();
  DCHECK(page);

  DCHECK(frame_->GetDocument());

  if (frame_->IsMainFrame()) {
    ScrollableArea& visual_viewport = frame_->GetPage()->GetVisualViewport();
    ScrollableArea* layout_viewport = LayoutViewport();
    DCHECK(layout_viewport);

    auto* root_frame_viewport = MakeGarbageCollected<RootFrameViewport>(
        visual_viewport, *layout_viewport);
    viewport_scrollable_area_ = root_frame_viewport;

    page->GlobalRootScrollerController().InitializeViewportScrollCallback(
        *root_frame_viewport, *frame_->GetDocument());

    // Allow for commits to be deferred because this is a new document.
    have_deferred_commits_ = false;
  }
}

Color LocalFrameView::DocumentBackgroundColor() const {
  // The LayoutView's background color is set in
  // Document::inheritHtmlAndBodyElementStyles.  Blend this with the base
  // background color of the LocalFrameView. This should match the color drawn
  // by ViewPainter::paintBoxDecorationBackground.
  Color result = BaseBackgroundColor();

  // If we have a fullscreen element grab the fullscreen color from the
  // backdrop.
  if (Document* doc = frame_->GetDocument()) {
    if (Element* element = Fullscreen::FullscreenElementFrom(*doc)) {
      if (doc->IsXrOverlay()) {
        // Use the fullscreened element's background directly. Don't bother
        // blending with the backdrop since that's transparent.
        if (LayoutObject* layout_object = element->GetLayoutObject()) {
          return layout_object->ResolveColor(GetCSSPropertyBackgroundColor());
        }
        if (LayoutObject* layout_object =
                element->PseudoElementLayoutObject(kPseudoIdBackdrop)) {
          return layout_object->ResolveColor(GetCSSPropertyBackgroundColor());
        }
      }
      if (LayoutObject* layout_object =
              element->PseudoElementLayoutObject(kPseudoIdBackdrop)) {
        return result.Blend(
            layout_object->ResolveColor(GetCSSPropertyBackgroundColor()));
      }
    }
  }
  auto* layout_view = GetLayoutView();
  if (layout_view) {
    result = result.Blend(
        layout_view->ResolveColor(GetCSSPropertyBackgroundColor()));
  }
  return result;
}

void LocalFrameView::WillBeRemovedFromFrame() {
  if (paint_artifact_compositor_)
    paint_artifact_compositor_->WillBeRemovedFromFrame();

  if (Settings* settings = frame_->GetSettings()) {
    DCHECK(frame_->GetPage());
    if (settings->GetSpatialNavigationEnabled()) {
      frame_->GetPage()->GetSpatialNavigationController().DidDetachFrameView(
          *this);
    }
  }
}

LocalFrameView* LocalFrameView::ParentFrameView() const {
  if (!IsAttached())
    return nullptr;

  Frame* parent_frame = frame_->Tree().Parent();
  if (auto* parent_local_frame = DynamicTo<LocalFrame>(parent_frame))
    return parent_local_frame->View();

  return nullptr;
}

LayoutEmbeddedContent* LocalFrameView::GetLayoutEmbeddedContent() const {
  return frame_->OwnerLayoutObject();
}

void LocalFrameView::VisualViewportScrollbarsChanged() {
  SetVisualViewportNeedsRepaint();
  if (LayoutView* layout_view = GetLayoutView())
    layout_view->Layer()->ClearClipRects();
}

void LocalFrameView::UpdateGeometriesIfNeeded() {
  if (!needs_update_geometries_)
    return;
  needs_update_geometries_ = false;
  HeapVector<Member<EmbeddedContentView>> views;
  ForAllChildViewsAndPlugins(
      [&](EmbeddedContentView& view) { views.push_back(view); });

  for (const auto& view : views) {
    // Script or plugins could detach the frame so abort processing if that
    // happens.
    if (!GetLayoutView())
      break;

    view->UpdateGeometry();
  }
  // Explicitly free the backing store to avoid memory regressions.
  // TODO(bikineev): Revisit after young generation is there.
  views.clear();
}

bool LocalFrameView::UpdateAllLifecyclePhases(DocumentUpdateReason reason) {
  bool updated = GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kPaintClean, reason);

#if DCHECK_IS_ON()
  if (updated) {
    // This function should return true iff all non-throttled frames are in the
    // kPaintClean lifecycle state.
    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      DCHECK_EQ(frame_view.Lifecycle().GetState(),
                DocumentLifecycle::kPaintClean);
    });

    // A required intersection observation should run throttled frames to
    // kLayoutClean.
    ForAllThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      DCHECK(frame_view.intersection_observation_state_ != kRequired ||
             frame_view.Lifecycle().GetState() >=
                 DocumentLifecycle::kLayoutClean);
    });
  }
#endif

  return updated;
}

// TODO(schenney): add a scrolling update lifecycle phase.
bool LocalFrameView::UpdateLifecycleToCompositingCleanPlusScrolling(
    DocumentUpdateReason reason) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return UpdateAllLifecyclePhasesExceptPaint(reason);
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kCompositingAssignmentsClean, reason);
}

bool LocalFrameView::UpdateLifecycleToPrePaintClean(
    DocumentUpdateReason reason) {
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kPrePaintClean, reason);
}

bool LocalFrameView::UpdateLifecycleToCompositingInputsClean(
    DocumentUpdateReason reason) {
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return UpdateAllLifecyclePhasesExceptPaint(reason);
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kCompositingInputsClean, reason);
}

bool LocalFrameView::UpdateAllLifecyclePhasesExceptPaint(
    DocumentUpdateReason reason) {
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kCompositingAssignmentsClean, reason);
}

void LocalFrameView::UpdateLifecyclePhasesForPrinting() {
  auto* local_frame_view_root = GetFrame().LocalFrameRoot().View();
  local_frame_view_root->UpdateLifecyclePhases(
      DocumentLifecycle::kCompositingAssignmentsClean,
      DocumentUpdateReason::kPrinting);

  auto* detached_frame_view = this;
  while (detached_frame_view->IsAttached() &&
         detached_frame_view != local_frame_view_root) {
    detached_frame_view = detached_frame_view->ParentFrameView();
    CHECK(detached_frame_view);
  }

  if (detached_frame_view == local_frame_view_root)
    return;
  DCHECK(!detached_frame_view->IsAttached());

  // We are printing a detached frame or a descendant of a detached frame which
  // was not reached in some phases during during |local_frame_view_root->
  // UpdateLifecyclePhasesnormal()|. We need the subtree to be ready for
  // painting.
  detached_frame_view->UpdateLifecyclePhases(
      DocumentLifecycle::kCompositingAssignmentsClean,
      DocumentUpdateReason::kPrinting);
}

bool LocalFrameView::UpdateLifecycleToLayoutClean(DocumentUpdateReason reason) {
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kLayoutClean, reason);
}

void LocalFrameView::ScheduleVisualUpdateForPaintInvalidationIfNeeded() {
  LocalFrame& local_frame_root = GetFrame().LocalFrameRoot();
  // We need a full lifecycle update to clear pending paint invalidations.
  if (local_frame_root.View()->current_update_lifecycle_phases_target_state_ <
          DocumentLifecycle::kPaintClean ||
      Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean) {
    // Schedule visual update to process the paint invalidation in the next
    // cycle.
    local_frame_root.ScheduleVisualUpdateUnlessThrottled();
  }
  // Otherwise the paint invalidation will be handled in the pre-paint and paint
  // phase of this full lifecycle update.
}

bool LocalFrameView::NotifyResizeObservers(
    DocumentLifecycle::LifecycleState target_state) {
  // Return true if lifecycles need to be re-run
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::NotifyResizeObservers");

  if (target_state < DocumentLifecycle::kPaintClean)
    return false;

  // Controller exists only if ResizeObserver was created.
  ResizeObserverController* resize_controller =
      ResizeObserverController::FromIfExists(*GetFrame().DomWindow());
  if (!resize_controller)
    return false;

  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean);

  size_t min_depth = resize_controller->GatherObservations();

  if (min_depth != ResizeObserverController::kDepthBottom) {
    resize_controller->DeliverObservations();
  } else {
    // Observation depth limit reached
    if (resize_controller->SkippedObservations() &&
        !resize_controller->IsLoopLimitErrorDispatched()) {
      resize_controller->ClearObservations();
      ErrorEvent* error = ErrorEvent::Create(
          "ResizeObserver loop limit exceeded",
          SourceLocation::Capture(frame_->DomWindow()), nullptr);
      // We're using |SanitizeScriptErrors::kDoNotSanitize| as the error is made
      // by blink itself.
      // TODO(yhirano): Reconsider this.
      frame_->DomWindow()->DispatchErrorEvent(
          error, SanitizeScriptErrors::kDoNotSanitize);
      // Ensure notifications will get delivered in next cycle.
      ScheduleAnimation();
      resize_controller->SetLoopLimitErrorDispatched(true);
    }
    if (Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean)
      return false;
  }

  // Lifecycle needs to be run again because Resize Observer affected layout
  return true;
}

// TODO(leviw): We don't assert lifecycle information from documents in child
// WebPluginContainerImpls.
bool LocalFrameView::UpdateLifecyclePhases(
    DocumentLifecycle::LifecycleState target_state,
    DocumentUpdateReason reason) {
  // If the lifecycle is postponed, which can happen if the inspector requests
  // it, then we shouldn't update any lifecycle phases.
  if (UNLIKELY(frame_->GetDocument() &&
               frame_->GetDocument()->Lifecycle().LifecyclePostponed())) {
    return false;
  }

  // Prevent reentrance.
  // TODO(vmpstr): Should we just have a DCHECK instead here?
  if (UNLIKELY(IsUpdatingLifecycle())) {
    NOTREACHED()
        << "LocalFrameView::updateLifecyclePhasesInternal() reentrance";
    return false;
  }

  // This must be called from the root frame, or a detached frame for printing,
  // since it recurses down, not up. Otherwise the lifecycles of the frames
  // might be out of sync.
  DCHECK(frame_->IsLocalRoot() || !IsAttached());

  // Only the following target states are supported.
  DCHECK(target_state == DocumentLifecycle::kLayoutClean ||
         target_state == DocumentLifecycle::kAccessibilityClean ||
         target_state == DocumentLifecycle::kCompositingInputsClean ||
         target_state == DocumentLifecycle::kCompositingAssignmentsClean ||
         target_state == DocumentLifecycle::kPrePaintClean ||
         target_state == DocumentLifecycle::kPaintClean);
  lifecycle_update_count_for_testing_++;

  // If the document is not active then it is either not yet initialized, or it
  // is stopping. In either case, we can't reach one of the supported target
  // states.
  if (!frame_->GetDocument()->IsActive())
    return false;

  if (frame_->IsLocalRoot())
    UpdateLayerDebugInfoEnabled();

  // This is used to guard against reentrance. It is also used in conjunction
  // with the current lifecycle state to determine which phases are yet to run
  // in this cycle.
  base::AutoReset<DocumentLifecycle::LifecycleState> target_state_scope(
      &current_update_lifecycle_phases_target_state_, target_state);
  // This is used to check if we're within a lifecycle update but have passed
  // the layout update phase. Note there is a bit of a subtlety here: it's not
  // sufficient for us to check the current lifecycle state, since it can be
  // past kLayoutClean but the function to run style and layout phase has not
  // actually been run yet. Since this bool affects throttling, and throttling,
  // in turn, determines whether style and layout function will run, we need a
  // separate bool.
  base::AutoReset<bool> past_layout_lifecycle_resetter(
      &past_layout_lifecycle_update_, false);
  base::AutoReset<bool> in_lifecycle_scope(&in_lifecycle_update_, true);

  // If we're throttling, then we don't need to update lifecycle phases. The
  // throttling status will get updated in RunPostLifecycleSteps().
  if (ShouldThrottleRendering()) {
    return Lifecycle().GetState() == target_state;
  }

  lifecycle_data_.start_time = base::TimeTicks::Now();
  ++lifecycle_data_.count;

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().EnsureStateAtMost(
        DocumentLifecycle::kVisualUpdatePending);
  });

  if (target_state == DocumentLifecycle::kPaintClean) {
    {
      TRACE_EVENT0("blink", "LocalFrameView::WillStartLifecycleUpdate");

      ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
        auto lifecycle_observers = frame_view.lifecycle_observers_;
        for (auto& observer : lifecycle_observers)
          observer->WillStartLifecycleUpdate(frame_view);
      });
    }

    {
      TRACE_EVENT0(
          "blink",
          "LocalFrameView::UpdateLifecyclePhases - start of lifecycle tasks");
      ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
        WTF::Vector<base::OnceClosure> tasks;
        frame_view.start_of_lifecycle_tasks_.swap(tasks);
        for (auto& task : tasks)
          std::move(task).Run();
      });
    }
  }

  // Run the lifecycle updates.
  UpdateLifecyclePhasesInternal(target_state);

  if (target_state == DocumentLifecycle::kPaintClean) {
    TRACE_EVENT0("blink", "LocalFrameView::DidFinishLifecycleUpdate");

    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      auto lifecycle_observers = frame_view.lifecycle_observers_;
      for (auto& observer : lifecycle_observers)
        observer->DidFinishLifecycleUpdate(frame_view);
    });
  }

  // Hit testing metrics include the entire time processing a document update
  // in preparation for a hit test.
  if (reason == DocumentUpdateReason::kHitTest) {
    LocalFrameUkmAggregator& aggregator = EnsureUkmAggregator();
    aggregator.RecordSample(
        static_cast<size_t>(LocalFrameUkmAggregator::kHitTestDocumentUpdate),
        lifecycle_data_.start_time, base::TimeTicks::Now());
  }

  return Lifecycle().GetState() == target_state;
}

void LocalFrameView::UpdateLifecyclePhasesInternal(
    DocumentLifecycle::LifecycleState target_state) {
  // Run style, layout, compositing and prepaint lifecycle phases and deliver
  // resize observations if required. Resize observer callbacks/delegates have
  // the potential to dirty layout (until loop limit is reached) and therefore
  // the above lifecycle phases need to be re-run until the limit is reached
  // or no layout is pending.
  // Note that after ResizeObserver has settled, we also run intersection
  // observations that need to be delievered in post-layout. This process can
  // also dirty layout, which will run this loop again.
  while (true) {
    bool run_more_lifecycle_phases =
        RunStyleAndLayoutLifecyclePhases(target_state);
    if (!run_more_lifecycle_phases)
      return;
    DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean);

    if (!GetLayoutView())
      return;

    {
      // We need scoping braces here because this
      // DisallowLayoutInvalidationScope is meant to be in effect during
      // pre-paint, but not during ResizeObserver.
#if DCHECK_IS_ON()
      DisallowLayoutInvalidationScope disallow_layout_invalidation(this);
#endif

      DCHECK_GE(target_state, DocumentLifecycle::kAccessibilityClean);
      run_more_lifecycle_phases = RunAccessibilityLifecyclePhase(target_state);
      DCHECK(ShouldThrottleRendering() || !ExistingAXObjectCache() ||
             Lifecycle().GetState() == DocumentLifecycle::kAccessibilityClean);
      if (!run_more_lifecycle_phases)
        return;

      TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                           "SetLayerTreeId", TRACE_EVENT_SCOPE_THREAD, "data",
                           inspector_set_layer_tree_id::Data(frame_.Get()));
      TRACE_EVENT1("devtools.timeline", "UpdateLayerTree", "data",
                   inspector_update_layer_tree_event::Data(frame_.Get()));

      run_more_lifecycle_phases =
          RunCompositingInputsLifecyclePhase(target_state);
      if (!run_more_lifecycle_phases)
        return;

      // TODO(pdr): PrePaint should be under the "Paint" devtools timeline step
      // when CompositeAfterPaint is enabled.
      run_more_lifecycle_phases = RunPrePaintLifecyclePhase(target_state);
      DCHECK(ShouldThrottleRendering() ||
             Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean);
      if (!run_more_lifecycle_phases)
        return;

      run_more_lifecycle_phases =
          RunCompositingAssignmentsLifecyclePhase(target_state);
      if (!run_more_lifecycle_phases) {
        return;
      }
    }

    // ResizeObserver and post-layout IntersectionObserver observation
    // deliveries may dirty style and layout. RunResizeObserverSteps will return
    // true if any observer ran that may have dirtied style or layout;
    // RunPostLayoutIntersectionObserverSteps will return true if any
    // observations led to content-visibility intersection changing visibility
    // state synchronously (which happens on the first intersection
    // observeration of a context).
    bool needs_to_repeat_lifecycle = RunResizeObserverSteps(target_state);
    // Only run the rest of the steps here if resize observer is done.
    if (needs_to_repeat_lifecycle)
      continue;

    needs_to_repeat_lifecycle = RunPostLayoutIntersectionObserverSteps();
    if (!needs_to_repeat_lifecycle)
      break;
  }

  // Once we exit the ResizeObserver / IntersectionObserver loop above, we need
  // to clear the resize observer limits so that next time we run this, we can
  // deliver more observations.
  ClearResizeObserverLimit();

  // Layout invalidation scope was disabled for resize observer
  // re-enable it for subsequent steps
#if DCHECK_IS_ON()
  DisallowLayoutInvalidationScope disallow_layout_invalidation(this);
#endif
  // Now that we have run the lifecycle up to paint, we can reset
  // |need_paint_phase_after_throttling_| so that the paint phase will
  // properly see us as being throttled (if that was the only reason we remained
  // unthrottled), and clear the painted output.
  need_paint_phase_after_throttling_ = false;

  DCHECK_EQ(target_state, DocumentLifecycle::kPaintClean);
  RunPaintLifecyclePhase();
  DCHECK(ShouldThrottleRendering() ||
         frame_->GetDocument()->IsCapturingLayout() ||
         Lifecycle().GetState() == DocumentLifecycle::kPaintClean);

  ForAllRemoteFrameViews(
      [](RemoteFrameView& frame_view) { frame_view.UpdateCompositingRect(); });
}

bool LocalFrameView::RunResizeObserverSteps(
    DocumentLifecycle::LifecycleState target_state) {
  bool re_run_lifecycles = false;
  if (target_state == DocumentLifecycle::kPaintClean) {
    ForAllNonThrottledLocalFrameViews(
        [&re_run_lifecycles](LocalFrameView& frame_view) {
          bool result =
              frame_view.NotifyResizeObservers(DocumentLifecycle::kPaintClean);
          re_run_lifecycles = re_run_lifecycles || result;
        });
  }
  return re_run_lifecycles;
}

void LocalFrameView::ClearResizeObserverLimit() {
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    ResizeObserverController* resize_controller =
        ResizeObserverController::From(*frame_view.frame_->DomWindow());
    resize_controller->ClearMinDepth();
    resize_controller->SetLoopLimitErrorDispatched(false);
  });
}

bool LocalFrameView::RunStyleAndLayoutLifecyclePhases(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::RunStyleAndLayoutLifecyclePhases");
  UpdateStyleAndLayoutIfNeededRecursive();
  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean);

  frame_->GetDocument()
      ->GetRootScrollerController()
      .PerformRootScrollerSelection();

  // PerformRootScrollerSelection can dirty layout if an effective root
  // scroller is changed so make sure we get back to LayoutClean.
  if (RuntimeEnabledFeatures::ImplicitRootScrollerEnabled()) {
    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      if (frame_view.NeedsLayout())
        frame_view.UpdateLayout();

      DCHECK(frame_view.Lifecycle().GetState() >=
             DocumentLifecycle::kLayoutClean);
    });
  }

  if (target_state == DocumentLifecycle::kLayoutClean)
    return false;

  // This will be reset by AutoReset in the calling function
  // (UpdateLifecyclePhases()).
  past_layout_lifecycle_update_ = true;

  // After layout and the |past_layout_lifecycle_update_| update, the value of
  // ShouldThrottleRendering() can change. OOPIF local frame roots that are
  // throttled can return now that layout is clean. This situation happens if
  // the throttling was disabled due to required intersection observation, which
  // can now be run.
  if (ShouldThrottleRendering())
    return false;

  // Now we can run post layout steps in preparation for further phases.
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.PerformScrollAnchoringAdjustments();
  });

  frame_->GetDocument()->PerformScrollSnappingTasks();

  EnqueueScrollEvents();

  frame_->GetPage()->GetValidationMessageClient().LayoutOverlay();

  if (target_state == DocumentLifecycle::kPaintClean) {
    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      frame_view.NotifyFrameRectsChangedIfNeeded();
    });
  }

  return Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean;
}

bool LocalFrameView::RunCompositingInputsLifecyclePhase(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::RunCompositingInputsLifecyclePhase");
  auto* layout_view = GetLayoutView();
  DCHECK(layout_view);

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                             LocalFrameUkmAggregator::kCompositingInputs);
    layout_view->Compositor()->UpdateInputsIfNeededRecursive(target_state);
  } else {
    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      frame_view.Lifecycle().AdvanceTo(
          DocumentLifecycle::kCompositingInputsClean);
    });
  }

  return target_state > DocumentLifecycle::kCompositingInputsClean;
}

bool LocalFrameView::RunCompositingAssignmentsLifecyclePhase(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::RunCompositingAssignmentsLifecyclePhase");
  auto* layout_view = GetLayoutView();
  DCHECK(layout_view);

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                             LocalFrameUkmAggregator::kCompositingAssignments);
    layout_view->Compositor()->UpdateAssignmentsIfNeededRecursive(target_state);
  } else {
    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      frame_view.Lifecycle().AdvanceTo(
          DocumentLifecycle::kCompositingAssignmentsClean);
    });
  }

  UpdateCompositedSelectionIfNeeded();

  frame_->GetPage()->GetValidationMessageClient().UpdatePrePaint();
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& view) {
    view.frame_->UpdateFrameColorOverlayPrePaint();
  });
  if (auto* web_local_frame_impl = WebLocalFrameImpl::FromFrame(frame_))
    web_local_frame_impl->UpdateDevToolsOverlaysPrePaint();

  return target_state > DocumentLifecycle::kCompositingAssignmentsClean;
}

bool LocalFrameView::RunPrePaintLifecyclePhase(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::RunPrePaintLifecyclePhase");

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // TODO(pdr): This descendant dependent treewalk should be integrated into
    // the prepaint tree walk.
#if DCHECK_IS_ON()
    SetIsUpdatingDescendantDependentFlags(true);
#endif
    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      frame_view.GetLayoutView()->Layer()->UpdateDescendantDependentFlags();
      frame_view.GetLayoutView()->CommitPendingSelection();
    });
#if DCHECK_IS_ON()
    SetIsUpdatingDescendantDependentFlags(false);
#endif
  }

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPrePaint);
    if (frame_view.CanThrottleRendering()) {
      // This frame can be throttled but not throttled, meaning we are not in an
      // AllowThrottlingScope. Now this frame may contain dirty paint flags, and
      // we need to propagate the flags into the ancestor chain so that
      // PrePaintTreeWalk can reach this frame.
      frame_view.SetNeedsPaintPropertyUpdate();
      // We may record more pre-composited layers under the frame.
      if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
        frame_view.SetPaintArtifactCompositorNeedsUpdate();
      if (auto* owner = frame_view.GetLayoutEmbeddedContent())
        owner->SetShouldCheckForPaintInvalidation();
    }
  });

  {
    SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                             LocalFrameUkmAggregator::kPrePaint);

    GetPage()->GetLinkHighlight().UpdateBeforePrePaint();
    PrePaintTreeWalk().WalkTree(*this);
    GetPage()->GetLinkHighlight().UpdateAfterPrePaint();
  }

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPrePaintClean);
  });

  return target_state > DocumentLifecycle::kPrePaintClean;
}

void LocalFrameView::RunPaintLifecyclePhase(PaintBenchmarkMode benchmark_mode) {
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::RunPaintLifecyclePhase");
  // While printing or capturing a paint preview of a document, the paint walk
  // is done into a special canvas. There is no point doing a normal paint step
  // (or animations update) when in this mode.
  bool is_capturing_layout = frame_->GetDocument()->IsCapturingLayout();
  bool repainted = false;
  if (!is_capturing_layout)
    repainted = PaintTree(benchmark_mode);

  if (benchmark_mode ==
          PaintBenchmarkMode::kForcePaintArtifactCompositorUpdate ||
      // TODO(paint-dev): Separate requirement for update for repaint and full
      // PaintArtifactCompositor update.
      (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() &&
       benchmark_mode != PaintBenchmarkMode::kNormal)) {
    paint_artifact_compositor_->SetNeedsUpdate();
  }

  if (!is_capturing_layout) {
    bool needed_update = !paint_artifact_compositor_ ||
                         paint_artifact_compositor_->NeedsUpdate();
    PushPaintArtifactToCompositor(repainted);
    ForAllNonThrottledLocalFrameViews([this](LocalFrameView& frame_view) {
      if (auto* scrollable_area = frame_view.GetScrollableArea())
        scrollable_area->UpdateCompositorScrollAnimations();
      if (const auto* animating_scrollable_areas =
              frame_view.AnimatingScrollableAreas()) {
        for (PaintLayerScrollableArea* area : *animating_scrollable_areas)
          area->UpdateCompositorScrollAnimations();
      }
      frame_view.GetLayoutView()
          ->GetDocument()
          .GetDocumentAnimations()
          .UpdateAnimations(DocumentLifecycle::kPaintClean,
                            paint_artifact_compositor_.get());
    });

    // Initialize animation properties in the newly created paint property
    // nodes according to the current animation state. This is mainly for
    // the running composited animations which didn't change state during
    // above UpdateAnimations() but associated with new paint property nodes.
    if (needed_update) {
      auto* root_layer = RootCcLayer();
      if (root_layer && root_layer->layer_tree_host()) {
        root_layer->layer_tree_host()
            ->mutator_host()
            ->InitClientAnimationState();
      }
    }

    // Notify the controller that the artifact has been pushed and some
    // lifecycle state can be freed (such as raster invalidations).
    if (paint_controller_)
      paint_controller_->FinishCycle();

    if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      auto* root = GetLayoutView()->Compositor()->PaintRootGraphicsLayer();
      if (root) {
        ForAllPaintingGraphicsLayers(*root, [](GraphicsLayer& layer) {
          // Notify the paint controller that the artifact has been pushed and
          // some lifecycle state can be freed (such as raster invalidations).
          layer.GetPaintController().FinishCycle();
        });
      }
    }

    if (paint_artifact_compositor_)
      paint_artifact_compositor_->ClearPropertyTreeChangedState();
  }

  if (GetPage())
    GetPage()->Animator().ReportFrameAnimations(GetCompositorAnimationHost());
}

bool LocalFrameView::RunAccessibilityLifecyclePhase(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::RunAccessibilityLifecyclePhase");
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    if (AXObjectCache* cache = frame_view.ExistingAXObjectCache()) {
      frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInAccessibility);
      cache->ProcessDeferredAccessibilityEvents(
          *frame_view.GetFrame().GetDocument());
      frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kAccessibilityClean);
    }
  });

  return target_state > DocumentLifecycle::kAccessibilityClean;
}

void LocalFrameView::EnqueueScrollAnchoringAdjustment(
    ScrollableArea* scrollable_area) {
  anchoring_adjustment_queue_.insert(scrollable_area);
}

void LocalFrameView::DequeueScrollAnchoringAdjustment(
    ScrollableArea* scrollable_area) {
  anchoring_adjustment_queue_.erase(scrollable_area);
}

void LocalFrameView::SetNeedsEnqueueScrollEvent(
    PaintLayerScrollableArea* scrollable_area) {
  scroll_event_queue_.insert(scrollable_area);
  GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());
}

void LocalFrameView::PerformScrollAnchoringAdjustments() {
  // Adjust() will cause a scroll which could end up causing a layout and
  // reentering this method. Copy and clear the queue so we don't modify it
  // during iteration.
  AnchoringAdjustmentQueue queue_copy = anchoring_adjustment_queue_;
  anchoring_adjustment_queue_.clear();

  for (const WeakMember<ScrollableArea>& scroller : queue_copy) {
    if (scroller) {
      DCHECK(scroller->GetScrollAnchor());
      scroller->GetScrollAnchor()->Adjust();
    }
  }
}

void LocalFrameView::EnqueueScrollEvents() {
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    for (const WeakMember<PaintLayerScrollableArea>& scroller :
         frame_view.scroll_event_queue_) {
      if (scroller)
        scroller->EnqueueScrollEventIfNeeded();
    }
    frame_view.scroll_event_queue_.clear();
  });
}

bool LocalFrameView::PaintTree(PaintBenchmarkMode benchmark_mode) {
  SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                           LocalFrameUkmAggregator::kPaint);

  DCHECK(GetFrame().IsLocalRoot());

  auto* layout_view = GetLayoutView();
  DCHECK(layout_view);
  paint_frame_count_++;
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.MarkFirstEligibleToPaint();
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
  });
  ForAllThrottledLocalFrameViews(
      [](LocalFrameView& frame_view) { frame_view.MarkIneligibleToPaint(); });

  bool repainted = false;
  bool needs_clear_repaint_flags = false;

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    if (!paint_controller_)
      paint_controller_ = std::make_unique<PaintController>();

    PaintController::ScopedBenchmarkMode scoped_benchmark(*paint_controller_,
                                                          benchmark_mode);

    // TODO(crbug.com/917911): Painting of overlays should not force repainting
    // of the frame contents.
    auto* web_local_frame_impl = WebLocalFrameImpl::FromFrame(frame_);
    bool has_dev_tools_overlays =
        web_local_frame_impl && web_local_frame_impl->HasDevToolsOverlays();
    if (!paint_controller_->ShouldForcePaintForBenchmark() &&
        !GetLayoutView()->Layer()->SelfOrDescendantNeedsRepaint() &&
        !visual_viewport_needs_repaint_ && !has_dev_tools_overlays) {
      paint_controller_->UpdateUMACountsOnFullyCached();
    } else {
      GraphicsContext graphics_context(*paint_controller_);
      if (Settings* settings = frame_->GetSettings()) {
        graphics_context.SetDarkModeEnabled(
            settings->GetForceDarkModeEnabled() &&
            !GetLayoutView()->StyleRef().DarkColorScheme());
      }

      bool painted_full_screen_overlay = false;
      if (frame_->IsMainFrame()) {
        PaintLayer* full_screen_layer = GetFullScreenOverlayLayer();
        if (full_screen_layer) {
          PaintLayerPainter(*full_screen_layer)
              .Paint(graphics_context, CullRect::Infinite(),
                     kGlobalPaintNormalPhase, 0);
          painted_full_screen_overlay = true;
        }
      }

      if (!painted_full_screen_overlay) {
        PaintInternal(graphics_context, kGlobalPaintNormalPhase,
                      CullRect::Infinite());

        GetPage()->GetValidationMessageClient().PaintOverlay(graphics_context);
        ForAllNonThrottledLocalFrameViews(
            [&graphics_context](LocalFrameView& view) {
              view.frame_->PaintFrameColorOverlay(graphics_context);
            });

        // Devtools overlays query the inspected page's paint data so this
        // update needs to be after other paintings.
        if (has_dev_tools_overlays)
          web_local_frame_impl->PaintDevToolsOverlays(graphics_context);

        if (frame_->IsMainFrame())
          GetPage()->GetVisualViewport().Paint(graphics_context);
      }

      // Link highlights paint after all other paintings.
      GetPage()->GetLinkHighlight().Paint(graphics_context);

      paint_controller_->CommitNewDisplayItems();
      repainted = true;
      // TODO(paint-dev): Implement repaint-only update for CompositeAfterPaint.
      SetPaintArtifactCompositorNeedsUpdate();

      // As if we created a root layer containing all paintings which needs full
      // layerization.
      pre_composited_layers_ = {
          {PaintChunkSubset(paint_controller_->GetPaintArtifactShared())}};
    }
  } else {
    // A null graphics layer can occur for painting of SVG images that are not
    // parented into the main frame tree, or when the LocalFrameView is the main
    // frame view of a page overlay. The page overlay is in the layer tree of
    // the host page and will be painted during painting of the host page.
    paint_controller_ =
        std::make_unique<PaintController>(PaintController::kTransient);
    pre_composited_layers_.clear();
    GraphicsContext graphics_context(*paint_controller_);

    if (GraphicsLayer* root =
            layout_view->Compositor()->PaintRootGraphicsLayer()) {
      repainted = root->PaintRecursively(
          graphics_context, pre_composited_layers_, benchmark_mode);
      if (visual_viewport_needs_repaint_ && paint_artifact_compositor_)
        paint_artifact_compositor_->SetNeedsUpdate();

      {
        PaintChunkSubsetRecorder subset_recorder(*paint_controller_);
        if (root == GetLayoutView()->Compositor()->RootGraphicsLayer() &&
            frame_->IsMainFrame()) {
          GetPage()->GetVisualViewport().Paint(graphics_context);
        }
        // Link highlights paint after all other layers.
        GetPage()->GetLinkHighlight().Paint(graphics_context);
        pre_composited_layers_.push_back(
            PreCompositedLayerInfo{subset_recorder.Get()});
      }

      paint_controller_->CommitNewDisplayItems();
      paint_controller_ = nullptr;
    } else {
      needs_clear_repaint_flags = true;
    }
  }

  visual_viewport_needs_repaint_ = false;

  needs_clear_repaint_flags |= repainted;
  ForAllNonThrottledLocalFrameViews(
      [needs_clear_repaint_flags](LocalFrameView& frame_view) {
        frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
        if (needs_clear_repaint_flags) {
          if (auto* layout_view = frame_view.GetLayoutView())
            layout_view->Layer()->ClearNeedsRepaintRecursively();
        }
        frame_view.GetPaintTimingDetector().NotifyPaintFinished();
      });

  PaintController::ReportUMACounts();
  return repainted;
}

const cc::Layer* LocalFrameView::RootCcLayer() const {
  return paint_artifact_compositor_ ? paint_artifact_compositor_->RootLayer()
                                    : nullptr;
}

void LocalFrameView::PushPaintArtifactToCompositor(bool repainted) {
  TRACE_EVENT0("blink", "LocalFrameView::pushPaintArtifactToCompositor");
  if (!frame_->GetSettings()->GetAcceleratedCompositingEnabled()) {
    if (paint_artifact_compositor_) {
      paint_artifact_compositor_->WillBeRemovedFromFrame();
      paint_artifact_compositor_ = nullptr;
    }
    return;
  }

  Page* page = GetFrame().GetPage();
  if (!page)
    return;

  if (!paint_artifact_compositor_) {
    paint_artifact_compositor_ = std::make_unique<PaintArtifactCompositor>(
        page->GetScrollingCoordinator()->GetWeakPtr());
    page->GetChromeClient().AttachRootLayer(
        paint_artifact_compositor_->RootLayer(), &GetFrame());
  }

  SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                           LocalFrameUkmAggregator::kCompositingCommit);

  // Skip updating property trees, pushing cc::Layers, and issuing raster
  // invalidations if possible.
  // TODO(paint-dev): In CompositeAfterPaint mode, we always set
  // PaintArtifactCompositor::NeedsUpdate() when anything needs repaint.
  // We need an equivalent signal to indicate that PAC doesn't need to run the
  // layerization algorithm, but it does need to update properties on layers
  // that depend on painted output.
  if (!paint_artifact_compositor_->NeedsUpdate()) {
    if (repainted)
      paint_artifact_compositor_->UpdateRepaintedLayerProperties();
    return;
  }

  paint_artifact_compositor_->SetLayerDebugInfoEnabled(
      layer_debug_info_enabled_);

  PaintArtifactCompositor::ViewportProperties viewport_properties;
  if (GetFrame().IsMainFrame()) {
    const auto& viewport = page->GetVisualViewport();
    viewport_properties.overscroll_elasticity_transform =
        viewport.GetOverscrollElasticityTransformNode();
    viewport_properties.page_scale = viewport.GetPageScaleNode();

    if (const auto* root_scroller =
            GetPage()->GlobalRootScrollerController().GlobalRootScroller()) {
      if (const auto* layout_object = root_scroller->GetLayoutObject()) {
        if (const auto* paint_properties =
                layout_object->FirstFragment().PaintProperties()) {
          if (paint_properties->Scroll()) {
            viewport_properties.outer_clip = paint_properties->OverflowClip();
            viewport_properties.outer_scroll_translation =
                paint_properties->ScrollTranslation();
            viewport_properties.inner_scroll_translation =
                viewport.GetScrollTranslationNode();
          }
        }
      }
    }
  }

  WTF::Vector<const TransformPaintPropertyNode*> scroll_translation_nodes;
  if (RuntimeEnabledFeatures::ScrollUnificationEnabled()) {
    ForAllNonThrottledLocalFrameViews(
        [&scroll_translation_nodes](LocalFrameView& frame_view) {
          scroll_translation_nodes.AppendVector(
              frame_view.GetScrollTranslationNodes());
        });
  }

  paint_artifact_compositor_->Update(
      pre_composited_layers_, viewport_properties, scroll_translation_nodes);

  probe::LayerTreePainted(&GetFrame());
}

std::unique_ptr<JSONObject> LocalFrameView::CompositedLayersAsJSON(
    LayerTreeFlags flags) {
  auto* root_frame_view = GetFrame().LocalFrameRoot().View();
  if (root_frame_view->paint_artifact_compositor_)
    return root_frame_view->paint_artifact_compositor_->GetLayersAsJSON(flags);
  return std::make_unique<JSONObject>();
}

void LocalFrameView::UpdateStyleAndLayoutIfNeededRecursive() {
  if (ShouldThrottleRendering() || !frame_->GetDocument()->IsActive())
    return;

  ScopedFrameBlamer frame_blamer(frame_);
  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::updateStyleAndLayoutIfNeededRecursive");

  // We have to crawl our entire subtree looking for any FrameViews that need
  // layout and make sure they are up to date.
  // Mac actually tests for intersection with the dirty region and tries not to
  // update layout for frames that are outside the dirty region.  Not only does
  // this seem pointless (since those frames will have set a zero timer to
  // layout anyway), but it is also incorrect, since if two frames overlap, the
  // first could be excluded from the dirty region but then become included
  // later by the second frame adding rects to the dirty region when it lays
  // out.

  {
    SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                             LocalFrameUkmAggregator::kStyle);
    frame_->GetDocument()->UpdateStyleAndLayoutTree();

    // Update style for all embedded SVG documents underneath this frame, so
    // that intrinsic size computation for any embedded objects has up-to-date
    // information before layout.
    ForAllChildLocalFrameViews([](LocalFrameView& view) {
      Document& document = *view.GetFrame().GetDocument();
      if (document.IsSVGDocument())
        document.UpdateStyleAndLayoutTree();
    });
  }

  CHECK(!ShouldThrottleRendering());
  CHECK(frame_->GetDocument()->IsActive());
  CHECK(!nested_layout_count_);

  if (NeedsLayout()) {
    SCOPED_UMA_AND_UKM_TIMER(EnsureUkmAggregator(),
                             LocalFrameUkmAggregator::kLayout);
    UpdateLayout();
  } else {
    UpdateGeometriesIfNeeded();
  }

  CheckDoesNotNeedLayout();

  // WebView plugins need to update regardless of whether the
  // LayoutEmbeddedObject that owns them needed layout.
  // TODO(schenney): This currently runs the entire lifecycle on plugin
  // WebViews. We should have a way to only run these other Documents to the
  // same lifecycle stage as this frame.
  for (const auto& plugin : plugins_) {
    plugin->UpdateAllLifecyclePhases();
  }
  CheckDoesNotNeedLayout();

  // FIXME: Calling layout() shouldn't trigger script execution or have any
  // observable effects on the frame tree but we're not quite there yet.
  HeapVector<Member<LocalFrameView>> frame_views;
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (!child_local_frame)
      continue;
    if (LocalFrameView* view = child_local_frame->View())
      frame_views.push_back(view);
  }

  for (const auto& frame_view : frame_views)
    frame_view->UpdateStyleAndLayoutIfNeededRecursive();

  // These asserts ensure that parent frames are clean, when child frames
  // finished updating layout and style.
  // TODO(szager): this is the last call to CheckDoesNotNeedLayout during the
  // lifecycle code, but it can happen that NeedsLayout() becomes true after
  // this point, even while the document lifecycle proceeds to kLayoutClean
  // and beyond. Figure out how this happens, and do something sensible.
  CheckDoesNotNeedLayout();
#if DCHECK_IS_ON()
  frame_->GetDocument()->GetLayoutView()->AssertLaidOut();
#endif

  if (Lifecycle().GetState() < DocumentLifecycle::kLayoutClean)
    Lifecycle().AdvanceTo(DocumentLifecycle::kLayoutClean);

  // If we're restoring a scroll position from history, that takes precedence
  // over scrolling to the anchor in the URL.
  frame_->GetDocument()->ApplyScrollRestorationLogic();

  // Ensure that we become visually non-empty eventually.
  // TODO(esprehn): This should check isRenderingReady() instead.
  if (GetFrame().GetDocument()->HasFinishedParsing() &&
      !GetFrame().GetDocument()->IsInitialEmptyDocument())
    is_visually_non_empty_ = true;

  GetFrame().Selection().UpdateStyleAndLayoutIfNeeded();
  GetFrame().GetPage()->GetDragCaret().UpdateStyleAndLayoutIfNeeded();
}

void LocalFrameView::EnableAutoSizeMode(const IntSize& min_size,
                                        const IntSize& max_size) {
  if (!auto_size_info_)
    auto_size_info_ = MakeGarbageCollected<FrameViewAutoSizeInfo>(this);

  auto_size_info_->ConfigureAutoSizeMode(min_size, max_size);
  SetLayoutSizeFixedToFrameSize(true);
  SetNeedsLayout();
  ScheduleRelayout();
}

void LocalFrameView::DisableAutoSizeMode() {
  if (!auto_size_info_)
    return;

  SetLayoutSizeFixedToFrameSize(false);
  SetNeedsLayout();
  ScheduleRelayout();

  // Since autosize mode forces the scrollbar mode, change them to being auto.
  GetLayoutView()->SetAutosizeScrollbarModes(
      mojom::blink::ScrollbarMode::kAuto, mojom::blink::ScrollbarMode::kAuto);
  auto_size_info_.Clear();
}

void LocalFrameView::ForceLayoutForPagination(
    const FloatSize& page_size,
    const FloatSize& original_page_size,
    float maximum_shrink_factor) {
  // Dumping externalRepresentation(m_frame->layoutObject()).ascii() is a good
  // trick to see the state of things before and after the layout
  if (LayoutView* layout_view = this->GetLayoutView()) {
    float page_logical_width = layout_view->StyleRef().IsHorizontalWritingMode()
                                   ? page_size.Width()
                                   : page_size.Height();
    float page_logical_height =
        layout_view->StyleRef().IsHorizontalWritingMode() ? page_size.Height()
                                                          : page_size.Width();

    LayoutUnit floored_page_logical_width =
        static_cast<LayoutUnit>(page_logical_width);
    LayoutUnit floored_page_logical_height =
        static_cast<LayoutUnit>(page_logical_height);
    layout_view->SetLogicalWidth(floored_page_logical_width);
    layout_view->SetPageLogicalHeight(floored_page_logical_height);
    layout_view->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kPrintingChanged);
    UpdateLayout();

    // If we don't fit in the given page width, we'll lay out again. If we don't
    // fit in the page width when shrunk, we will lay out at maximum shrink and
    // clip extra content.
    // FIXME: We are assuming a shrink-to-fit printing implementation.  A
    // cropping implementation should not do this!
    bool horizontal_writing_mode =
        layout_view->StyleRef().IsHorizontalWritingMode();
    PhysicalRect document_rect(layout_view->DocumentRect());
    LayoutUnit doc_logical_width = horizontal_writing_mode
                                       ? document_rect.Width()
                                       : document_rect.Height();
    if (doc_logical_width > page_logical_width) {
      // ResizePageRectsKeepingRatio would truncate the expected page size,
      // while we want it rounded -- so make sure it's rounded here.
      FloatSize expected_page_size(
          std::min<float>(document_rect.Width().Round(),
                          page_size.Width() * maximum_shrink_factor),
          std::min<float>(document_rect.Height().Round(),
                          page_size.Height() * maximum_shrink_factor));
      FloatSize max_page_size = frame_->ResizePageRectsKeepingRatio(
          FloatSize(original_page_size.Width(), original_page_size.Height()),
          expected_page_size);
      page_logical_width = horizontal_writing_mode ? max_page_size.Width()
                                                   : max_page_size.Height();
      page_logical_height = horizontal_writing_mode ? max_page_size.Height()
                                                    : max_page_size.Width();

      floored_page_logical_width = static_cast<LayoutUnit>(page_logical_width);
      floored_page_logical_height =
          static_cast<LayoutUnit>(page_logical_height);
      layout_view->SetLogicalWidth(floored_page_logical_width);
      layout_view->SetPageLogicalHeight(floored_page_logical_height);
      layout_view
          ->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
              layout_invalidation_reason::kPrintingChanged);
      UpdateLayout();

      PhysicalRect updated_document_rect(layout_view->DocumentRect());
      LayoutUnit doc_logical_height = horizontal_writing_mode
                                          ? updated_document_rect.Height()
                                          : updated_document_rect.Width();
      LayoutUnit doc_logical_top = horizontal_writing_mode
                                       ? updated_document_rect.Y()
                                       : updated_document_rect.X();
      LayoutUnit doc_logical_right = horizontal_writing_mode
                                         ? updated_document_rect.Right()
                                         : updated_document_rect.Bottom();
      LayoutUnit clipped_logical_left;
      if (!layout_view->StyleRef().IsLeftToRightDirection()) {
        clipped_logical_left =
            LayoutUnit(doc_logical_right - page_logical_width);
      }
      LayoutRect overflow(clipped_logical_left, doc_logical_top,
                          LayoutUnit(page_logical_width), doc_logical_height);

      if (!horizontal_writing_mode)
        overflow = overflow.TransposedRect();
      AdjustViewSizeAndLayout();
      // This is how we clip in case we overflow again.
      layout_view->ClearLayoutOverflow();
      layout_view->AddLayoutOverflow(overflow);
      return;
    }
  }

  if (TextAutosizer* text_autosizer = frame_->GetDocument()->GetTextAutosizer())
    text_autosizer->UpdatePageInfo();
  AdjustViewSizeAndLayout();
}

IntRect LocalFrameView::RootFrameToDocument(const IntRect& rect_in_root_frame) {
  IntPoint offset = RootFrameToDocument(rect_in_root_frame.Location());
  IntRect local_rect = rect_in_root_frame;
  local_rect.SetLocation(offset);
  return local_rect;
}

IntPoint LocalFrameView::RootFrameToDocument(
    const IntPoint& point_in_root_frame) {
  return FlooredIntPoint(RootFrameToDocument(FloatPoint(point_in_root_frame)));
}

FloatPoint LocalFrameView::RootFrameToDocument(
    const FloatPoint& point_in_root_frame) {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return point_in_root_frame;

  FloatPoint local_frame = ConvertFromRootFrame(point_in_root_frame);
  return local_frame + layout_viewport->GetScrollOffset();
}

IntRect LocalFrameView::DocumentToFrame(const IntRect& rect_in_document) const {
  IntRect rect_in_frame = rect_in_document;
  rect_in_frame.SetLocation(DocumentToFrame(rect_in_document.Location()));
  return rect_in_frame;
}

DoublePoint LocalFrameView::DocumentToFrame(
    const DoublePoint& point_in_document) const {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return point_in_document;

  return point_in_document - layout_viewport->GetScrollOffset();
}

IntPoint LocalFrameView::DocumentToFrame(
    const IntPoint& point_in_document) const {
  return FlooredIntPoint(DocumentToFrame(DoublePoint(point_in_document)));
}

FloatPoint LocalFrameView::DocumentToFrame(
    const FloatPoint& point_in_document) const {
  return FloatPoint(DocumentToFrame(DoublePoint(point_in_document)));
}

PhysicalOffset LocalFrameView::DocumentToFrame(
    const PhysicalOffset& offset_in_document) const {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return offset_in_document;

  return offset_in_document -
         PhysicalOffset::FromFloatSizeRound(layout_viewport->GetScrollOffset());
}

PhysicalRect LocalFrameView::DocumentToFrame(
    const PhysicalRect& rect_in_document) const {
  return PhysicalRect(DocumentToFrame(rect_in_document.offset),
                      rect_in_document.size);
}

IntPoint LocalFrameView::FrameToDocument(const IntPoint& point_in_frame) const {
  return FlooredIntPoint(FrameToDocument(PhysicalOffset(point_in_frame)));
}

PhysicalOffset LocalFrameView::FrameToDocument(
    const PhysicalOffset& offset_in_frame) const {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return offset_in_frame;

  return offset_in_frame +
         PhysicalOffset::FromFloatSizeRound(layout_viewport->GetScrollOffset());
}

IntRect LocalFrameView::FrameToDocument(const IntRect& rect_in_frame) const {
  return IntRect(FrameToDocument(rect_in_frame.Location()),
                 rect_in_frame.Size());
}

PhysicalRect LocalFrameView::FrameToDocument(
    const PhysicalRect& rect_in_frame) const {
  return PhysicalRect(FrameToDocument(rect_in_frame.offset),
                      rect_in_frame.size);
}

IntRect LocalFrameView::ConvertToContainingEmbeddedContentView(
    const IntRect& local_rect) const {
  if (ParentFrameView()) {
    auto* layout_object = GetLayoutEmbeddedContent();
    if (!layout_object)
      return local_rect;

    IntRect rect(local_rect);
    // Add borders and padding
    rect.Move(
        (layout_object->BorderLeft() + layout_object->PaddingLeft()).ToInt(),
        (layout_object->BorderTop() + layout_object->PaddingTop()).ToInt());
    return PixelSnappedIntRect(
        layout_object->LocalToAbsoluteRect(PhysicalRect(rect)));
  }

  return local_rect;
}

IntRect LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const IntRect& parent_rect) const {
  if (ParentFrameView()) {
    IntRect local_rect = parent_rect;
    local_rect.MoveBy(-Location());
    return local_rect;
  }
  return parent_rect;
}

PhysicalOffset LocalFrameView::ConvertToContainingEmbeddedContentView(
    const PhysicalOffset& local_offset) const {
  if (ParentFrameView()) {
    auto* layout_object = GetLayoutEmbeddedContent();
    if (!layout_object)
      return local_offset;

    PhysicalOffset point(local_offset);

    // Add borders and padding
    point += PhysicalOffset(
        layout_object->BorderLeft() + layout_object->PaddingLeft(),
        layout_object->BorderTop() + layout_object->PaddingTop());
    return layout_object->LocalToAbsolutePoint(point);
  }

  return local_offset;
}

FloatPoint LocalFrameView::ConvertToContainingEmbeddedContentView(
    const FloatPoint& local_point) const {
  if (ParentFrameView()) {
    auto* layout_object = GetLayoutEmbeddedContent();
    if (!layout_object)
      return local_point;

    PhysicalOffset point = PhysicalOffset::FromFloatPointRound(local_point);

    // Add borders and padding
    point.left += layout_object->BorderLeft() + layout_object->PaddingLeft();
    point.top += layout_object->BorderTop() + layout_object->PaddingTop();
    return FloatPoint(layout_object->LocalToAbsolutePoint(point));
  }

  return local_point;
}

PhysicalOffset LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const PhysicalOffset& parent_offset) const {
  return PhysicalOffset::FromFloatPointRound(
      ConvertFromContainingEmbeddedContentView(FloatPoint(parent_offset)));
}

FloatPoint LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const FloatPoint& parent_point) const {
  return FloatPoint(
      ConvertFromContainingEmbeddedContentView(DoublePoint(parent_point)));
}

DoublePoint LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const DoublePoint& parent_point) const {
  if (ParentFrameView()) {
    // Get our layoutObject in the parent view
    auto* layout_object = GetLayoutEmbeddedContent();
    if (!layout_object)
      return parent_point;

    DoublePoint point(
        layout_object->AbsoluteToLocalFloatPoint(FloatPoint(parent_point)));
    // Subtract borders and padding
    point.Move(
        (-layout_object->BorderLeft() - layout_object->PaddingLeft())
            .ToDouble(),
        (-layout_object->BorderTop() - layout_object->PaddingTop()).ToDouble());
    return point;
  }

  return parent_point;
}

IntPoint LocalFrameView::ConvertToContainingEmbeddedContentView(
    const IntPoint& local_point) const {
  return RoundedIntPoint(
      ConvertToContainingEmbeddedContentView(PhysicalOffset(local_point)));
}

void LocalFrameView::SetTracksRasterInvalidations(
    bool track_raster_invalidations) {
  if (track_raster_invalidations == is_tracking_raster_invalidations_)
    return;

  // Ensure the document is up-to-date before tracking invalidations.
  UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);

  for (Frame* frame = &frame_->Tree().Top(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    if (auto* layout_view = local_frame->ContentLayoutObject()) {
      is_tracking_raster_invalidations_ = track_raster_invalidations;
      if (paint_artifact_compositor_) {
        paint_artifact_compositor_->SetTracksRasterInvalidations(
            track_raster_invalidations);
      }
      if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
        layout_view->Compositor()->UpdateTrackingRasterInvalidations();
    }
  }

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"),
                       "LocalFrameView::setTracksPaintInvalidations",
                       TRACE_EVENT_SCOPE_GLOBAL, "enabled",
                       track_raster_invalidations);
}

void LocalFrameView::ScheduleAnimation(base::TimeDelta delay) {
  if (auto* client = GetChromeClient())
    client->ScheduleAnimation(this, delay);
}

void LocalFrameView::AddScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  DCHECK(scrollable_area);
  if (!scrollable_areas_)
    scrollable_areas_ = MakeGarbageCollected<ScrollableAreaSet>();
  scrollable_areas_->insert(scrollable_area);
}

void LocalFrameView::RemoveScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  if (!scrollable_areas_)
    return;
  scrollable_areas_->erase(scrollable_area);
}

void LocalFrameView::AddAnimatingScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  DCHECK(scrollable_area);
  if (!animating_scrollable_areas_)
    animating_scrollable_areas_ = MakeGarbageCollected<ScrollableAreaSet>();
  animating_scrollable_areas_->insert(scrollable_area);
}

void LocalFrameView::RemoveAnimatingScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  if (!animating_scrollable_areas_)
    return;
  animating_scrollable_areas_->erase(scrollable_area);
}

void LocalFrameView::AttachToLayout() {
  CHECK(!IsAttached());
  if (frame_->GetDocument())
    CHECK_NE(Lifecycle().GetState(), DocumentLifecycle::kStopping);
  SetAttached(true);
  LocalFrameView* parent_view = ParentFrameView();
  CHECK(parent_view);
  if (parent_view->IsVisible())
    SetParentVisible(true);
  UpdateRenderThrottlingStatus(IsHiddenForThrottling(),
                               parent_view->CanThrottleRendering());

  // We may have updated paint properties in detached frame subtree for
  // printing (see UpdateLifecyclePhasesForPrinting()). The paint properties
  // may change after the frame is attached.
  if (auto* layout_view = GetLayoutView()) {
    layout_view->AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kPrinting);
  }
}

void LocalFrameView::DetachFromLayout() {
  CHECK(IsAttached());
  SetParentVisible(false);
  SetAttached(false);

  // We may need update paint properties in detached frame subtree for printing.
  // See UpdateLifecyclePhasesForPrinting().
  if (auto* layout_view = GetLayoutView()) {
    layout_view->AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kPrinting);
  }
}

void LocalFrameView::AddPlugin(WebPluginContainerImpl* plugin) {
  DCHECK(!plugins_.Contains(plugin));
  plugins_.insert(plugin);
}

void LocalFrameView::RemovePlugin(WebPluginContainerImpl* plugin) {
  DCHECK(plugins_.Contains(plugin));
  plugins_.erase(plugin);
}

void LocalFrameView::RemoveScrollbar(Scrollbar* scrollbar) {
  DCHECK(scrollbars_.Contains(scrollbar));
  scrollbars_.erase(scrollbar);
}

void LocalFrameView::AddScrollbar(Scrollbar* scrollbar) {
  DCHECK(!scrollbars_.Contains(scrollbar));
  scrollbars_.insert(scrollbar);
}

bool LocalFrameView::VisualViewportSuppliesScrollbars() {
  // On desktop, we always use the layout viewport's scrollbars.
  if (!frame_->GetSettings() || !frame_->GetSettings()->GetViewportEnabled() ||
      !frame_->GetDocument() || !frame_->GetPage())
    return false;

  if (!LayoutViewport())
    return false;

  const TopDocumentRootScrollerController& controller =
      frame_->GetPage()->GlobalRootScrollerController();
  return controller.RootScrollerArea() == LayoutViewport();
}

AXObjectCache* LocalFrameView::ExistingAXObjectCache() const {
  if (GetFrame().GetDocument())
    return GetFrame().GetDocument()->ExistingAXObjectCache();
  return nullptr;
}

void LocalFrameView::SetCursor(const ui::Cursor& cursor) {
  Page* page = GetFrame().GetPage();
  if (!page || frame_->GetEventHandler().IsMousePositionUnknown())
    return;
  LogCursorSizeCounter(&GetFrame(), cursor);
  page->GetChromeClient().SetCursor(cursor, frame_);
}

void LocalFrameView::PropagateFrameRects() {
  TRACE_EVENT0("blink", "LocalFrameView::PropagateFrameRects");
  if (LayoutSizeFixedToFrameSize())
    SetLayoutSizeInternal(Size());

  ForAllChildViewsAndPlugins([](EmbeddedContentView& view) {
    auto* local_frame_view = DynamicTo<LocalFrameView>(view);
    if (!local_frame_view || !local_frame_view->ShouldThrottleRendering()) {
      view.PropagateFrameRects();
    }
  });

  GetFrame().Client()->FrameRectsChanged(FrameRect());

  // It's possible for changing the frame rect to not generate a layout
  // or any other event tracked by accessibility, we've seen this with
  // Android WebView. Ensure that the root of the accessibility tree is
  // invalidated so that it gets the right bounding rect.
  if (AXObjectCache* cache = ExistingAXObjectCache())
    cache->HandleFrameRectsChanged(*GetFrame().GetDocument());
}

void LocalFrameView::SetLayoutSizeInternal(const IntSize& size) {
  if (layout_size_ == size)
    return;
  layout_size_ = size;

  if (frame_->IsMainFrame() && frame_->GetDocument())
    TextAutosizer::UpdatePageInfoInAllFrames(frame_);

  SetNeedsLayout();
}

void LocalFrameView::ClipPaintRect(FloatRect* paint_rect) const {
  // TODO(wangxianzhu): Support ChromeClient::VisibleContentRectForPainting()
  // with CompositeAfterPaint.
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());

  // Paint the whole rect if ClipsContent is false, meaning that the whole
  // document should be recorded. This occurs if:
  // - A paint preview is being captured.
  // - WebPreferences::record_whole_document is true.
  if (!frame_->ClipsContent())
    return;

  // By default we consider the bounds of the FrameView to be what is considered
  // visible for the Frame.
  IntRect visible_rect = IntRect(IntPoint(), Size());
  // Non-main frames always clip to their FrameView bounds. Main frames can
  // have this behaviour modified by devtools.
  if (frame_->IsMainFrame()) {
    // If devtools is overriding the viewport, then the FrameView's bounds are
    // not what we should paint, instead we should paint inside the bounds
    // specified by devtools.
    GetPage()->GetChromeClient().OverrideVisibleRectForMainFrame(*frame_,
                                                                 &visible_rect);
  }
  paint_rect->Intersect(visible_rect);
}

void LocalFrameView::DidChangeScrollOffset() {
  GetFrame().Client()->DidChangeScrollOffset();
  if (GetFrame().IsMainFrame()) {
    GetFrame().GetPage()->GetChromeClient().MainFrameScrollOffsetChanged(
        GetFrame());
  }
}

ScrollableArea* LocalFrameView::ScrollableAreaWithElementId(
    const CompositorElementId& id) {
  // Check for the layout viewport, which may not be in scrollable_areas_ if it
  // is styled overflow: hidden.  (Other overflow: hidden elements won't have
  // composited scrolling layers per crbug.com/784053, so we don't have to worry
  // about them.)
  ScrollableArea* viewport = LayoutViewport();
  if (id == viewport->GetScrollElementId())
    return viewport;

  if (scrollable_areas_) {
    // This requires iterating over all scrollable areas. We may want to store a
    // map of ElementId to ScrollableArea if this is an issue for performance.
    for (ScrollableArea* scrollable_area : *scrollable_areas_) {
      if (id == scrollable_area->GetScrollElementId())
        return scrollable_area;
    }
  }
  return nullptr;
}

void LocalFrameView::ScrollRectToVisibleInRemoteParent(
    const PhysicalRect& rect_to_scroll,
    mojom::blink::ScrollIntoViewParamsPtr params) {
  DCHECK(GetFrame().IsLocalRoot() && !GetFrame().IsMainFrame() &&
         safe_to_propagate_scroll_to_parent_);
  PhysicalRect new_rect = ConvertToRootFrame(rect_to_scroll);
  frame_->GetLocalFrameHostRemote().ScrollRectToVisibleInParentFrame(
      gfx::Rect(new_rect.X().ToInt(), new_rect.Y().ToInt(),
                new_rect.Width().ToInt(), new_rect.Height().ToInt()),
      std::move(params));
}

void LocalFrameView::NotifyFrameRectsChangedIfNeeded() {
  if (root_layer_did_scroll_) {
    root_layer_did_scroll_ = false;
    PropagateFrameRects();
  }
}

void LocalFrameView::SetViewportIntersection(
    const mojom::blink::ViewportIntersectionState& intersection_state) {
  // The viewport intersection of the main frame is not tracked.
  DCHECK(!GetFrame().IsMainFrame());

  if (auto* document_resource_coordinator =
          frame_->GetDocument()->GetResourceCoordinator()) {
    gfx::RectF transform_rect =
        gfx::RectF(gfx::Rect(intersection_state.viewport_intersection));

    // The viewport intersection in the child frame's coordinate system is
    // transformed into the intersection in the viewport's coordinate system.
    intersection_state.main_frame_transform.TransformRect(&transform_rect);

    // Get rid of the possible floating point values for x and y of the
    // resulting rectangle.
    IntRect rect = EnclosingIntRect(
        FloatRect(transform_rect.x(), transform_rect.y(),
                  transform_rect.width(), transform_rect.height()));

    // Return <0, 0, 0, 0> if there is no area.
    if (rect.IsEmpty())
      rect.SetLocation(IntPoint(0, 0));

    document_resource_coordinator->SetViewportIntersection(rect);
  }
}

PhysicalOffset LocalFrameView::ViewportToFrame(
    const PhysicalOffset& point_in_viewport) const {
  PhysicalOffset point_in_root_frame = PhysicalOffset::FromFloatPointRound(
      frame_->GetPage()->GetVisualViewport().ViewportToRootFrame(
          FloatPoint(point_in_viewport)));
  return ConvertFromRootFrame(point_in_root_frame);
}

FloatPoint LocalFrameView::ViewportToFrame(
    const FloatPoint& point_in_viewport) const {
  FloatPoint point_in_root_frame(
      frame_->GetPage()->GetVisualViewport().ViewportToRootFrame(
          point_in_viewport));
  return ConvertFromRootFrame(point_in_root_frame);
}

IntRect LocalFrameView::ViewportToFrame(const IntRect& rect_in_viewport) const {
  IntRect rect_in_root_frame =
      frame_->GetPage()->GetVisualViewport().ViewportToRootFrame(
          rect_in_viewport);
  return ConvertFromRootFrame(rect_in_root_frame);
}

IntPoint LocalFrameView::ViewportToFrame(
    const IntPoint& point_in_viewport) const {
  return RoundedIntPoint(ViewportToFrame(PhysicalOffset(point_in_viewport)));
}

IntRect LocalFrameView::FrameToViewport(const IntRect& rect_in_frame) const {
  IntRect rect_in_root_frame = ConvertToRootFrame(rect_in_frame);
  return frame_->GetPage()->GetVisualViewport().RootFrameToViewport(
      rect_in_root_frame);
}

IntPoint LocalFrameView::FrameToViewport(const IntPoint& point_in_frame) const {
  IntPoint point_in_root_frame = ConvertToRootFrame(point_in_frame);
  return frame_->GetPage()->GetVisualViewport().RootFrameToViewport(
      point_in_root_frame);
}

IntRect LocalFrameView::FrameToScreen(const IntRect& rect) const {
  if (auto* client = GetChromeClient())
    return client->ViewportToScreen(FrameToViewport(rect), this);
  return IntRect();
}

IntPoint LocalFrameView::SoonToBeRemovedUnscaledViewportToContents(
    const IntPoint& point_in_viewport) const {
  IntPoint point_in_root_frame = FlooredIntPoint(
      frame_->GetPage()->GetVisualViewport().ViewportCSSPixelsToRootFrame(
          FloatPoint(point_in_viewport)));
  return ConvertFromRootFrame(point_in_root_frame);
}

bool LocalFrameView::CapturePaintPreview(GraphicsContext& context,
                                         const IntSize& paint_offset) const {
  base::Optional<base::UnguessableToken> maybe_embedding_token =
      GetFrame().GetEmbeddingToken();

  // Avoid crashing if a local frame doesn't have an embedding token.
  // e.g. it was unloaded or hasn't finished loading (crbug/1103157).
  if (!maybe_embedding_token.has_value())
    return false;

  // Ensure a recording canvas is properly created.
  DrawingRecorder recorder(context, *GetFrame().OwnerLayoutObject(),
                           DisplayItem::kDocumentBackground);
  context.Save();
  context.Translate(paint_offset.Width(), paint_offset.Height());
  DCHECK(context.Canvas());

  auto* tracker = context.Canvas()->GetPaintPreviewTracker();
  DCHECK(tracker);  // |tracker| must exist or there is a bug upstream.

  // Create a placeholder ID that maps to an embedding token.
  context.Canvas()->recordCustomData(tracker->CreateContentForRemoteFrame(
      FrameRect(), maybe_embedding_token.value()));
  context.Restore();

  // Send a request to the browser to trigger a capture of the frame.
  GetFrame().GetLocalFrameHostRemote().CapturePaintPreviewOfSubframe(
      FrameRect(), tracker->Guid());
  return true;
}

void LocalFrameView::Paint(GraphicsContext& context,
                           const GlobalPaintFlags global_paint_flags,
                           const CullRect& cull_rect,
                           const IntSize& paint_offset) const {
  // When capturing a Paint Preview we want to capture scrollable embedded
  // content separately. Paint should stop here and ask the browser to
  // coordinate painting such frames as a separate task.
  if (context.IsPaintingPreview() && LayoutViewport()->ScrollsOverflow()) {
    // If capture fails we should fallback to capturing inline if possible.
    if (CapturePaintPreview(context, paint_offset))
      return;
  }

  // |paint_offset| is not used because paint properties of the contents will
  // ensure the correct location.
  PaintInternal(context, global_paint_flags, cull_rect);
}

void LocalFrameView::PaintInternal(GraphicsContext& context,
                                   const GlobalPaintFlags global_paint_flags,
                                   const CullRect& cull_rect) const {
  FramePainter(*this).Paint(context, global_paint_flags, cull_rect);
}

static bool PaintOutsideOfLifecycleIsAllowed(GraphicsContext& context,
                                             const LocalFrameView& frame_view) {
  // A paint outside of lifecycle should not conflict about paint controller
  // caching with the default painting executed during lifecycle update,
  // otherwise the caller should either use a transient paint controller or
  // explicitly skip cache.
  if (context.GetPaintController().IsSkippingCache())
    return true;
  // For CompositeAfterPaint, they always conflict because we always paint into
  // paint_controller_ during lifecycle update.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return false;
  // For pre-CompositeAfterPaint, they conflict if the local frame root has a
  // a root graphics layer.
  return !frame_view.GetFrame()
              .LocalFrameRoot()
              .View()
              ->GetLayoutView()
              ->Compositor()
              ->PaintRootGraphicsLayer();
}

void LocalFrameView::PaintOutsideOfLifecycle(
    GraphicsContext& context,
    const GlobalPaintFlags global_paint_flags,
    const CullRect& cull_rect) {
  DCHECK(PaintOutsideOfLifecycleIsAllowed(context, *this));

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
  });

  PaintInternal(context, global_paint_flags, cull_rect);

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
  });
}

void LocalFrameView::PaintContentsOutsideOfLifecycle(
    GraphicsContext& context,
    const GlobalPaintFlags global_paint_flags,
    const CullRect& cull_rect) {
  DCHECK(PaintOutsideOfLifecycleIsAllowed(context, *this));

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
  });

  FramePainter(*this).PaintContents(context, global_paint_flags, cull_rect);

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
  });
}

sk_sp<PaintRecord> LocalFrameView::GetPaintRecord() const {
  DCHECK(RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  DCHECK_EQ(DocumentLifecycle::kPaintClean, Lifecycle().GetState());
  DCHECK(frame_->IsLocalRoot());
  DCHECK(paint_controller_);
  return paint_controller_->GetPaintArtifact().GetPaintRecord(
      PropertyTreeState::Root());
}

IntRect LocalFrameView::ConvertToRootFrame(const IntRect& local_rect) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    IntRect parent_rect = ConvertToContainingEmbeddedContentView(local_rect);
    return parent->ConvertToRootFrame(parent_rect);
  }
  return local_rect;
}

IntPoint LocalFrameView::ConvertToRootFrame(const IntPoint& local_point) const {
  return RoundedIntPoint(ConvertToRootFrame(PhysicalOffset(local_point)));
}

PhysicalOffset LocalFrameView::ConvertToRootFrame(
    const PhysicalOffset& local_offset) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    PhysicalOffset parent_offset =
        ConvertToContainingEmbeddedContentView(local_offset);
    return parent->ConvertToRootFrame(parent_offset);
  }
  return local_offset;
}

FloatPoint LocalFrameView::ConvertToRootFrame(
    const FloatPoint& local_point) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    FloatPoint parent_point =
        ConvertToContainingEmbeddedContentView(local_point);
    return parent->ConvertToRootFrame(parent_point);
  }
  return local_point;
}

PhysicalRect LocalFrameView::ConvertToRootFrame(
    const PhysicalRect& local_rect) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    PhysicalOffset parent_offset =
        ConvertToContainingEmbeddedContentView(local_rect.offset);
    PhysicalRect parent_rect(parent_offset, local_rect.size);
    return parent->ConvertToRootFrame(parent_rect);
  }
  return local_rect;
}

IntRect LocalFrameView::ConvertFromRootFrame(
    const IntRect& rect_in_root_frame) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    IntRect parent_rect = parent->ConvertFromRootFrame(rect_in_root_frame);
    return ConvertFromContainingEmbeddedContentView(parent_rect);
  }
  return rect_in_root_frame;
}

IntPoint LocalFrameView::ConvertFromRootFrame(
    const IntPoint& point_in_root_frame) const {
  return RoundedIntPoint(
      ConvertFromRootFrame(PhysicalOffset(point_in_root_frame)));
}

PhysicalOffset LocalFrameView::ConvertFromRootFrame(
    const PhysicalOffset& offset_in_root_frame) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    PhysicalOffset parent_point =
        parent->ConvertFromRootFrame(offset_in_root_frame);
    return ConvertFromContainingEmbeddedContentView(parent_point);
  }
  return offset_in_root_frame;
}

FloatPoint LocalFrameView::ConvertFromRootFrame(
    const FloatPoint& point_in_root_frame) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    FloatPoint parent_point = parent->ConvertFromRootFrame(point_in_root_frame);
    return ConvertFromContainingEmbeddedContentView(parent_point);
  }
  return point_in_root_frame;
}

void LocalFrameView::ParentVisibleChanged() {
  // As parent visibility changes, we may need to recomposite this frame view
  // and potentially child frame views.
  SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);

  if (!IsSelfVisible())
    return;

  bool visible = IsParentVisible();
  ForAllChildViewsAndPlugins(
      [visible](EmbeddedContentView& embedded_content_view) {
        embedded_content_view.SetParentVisible(visible);
      });
}

void LocalFrameView::SelfVisibleChanged() {
  // FrameView visibility affects PLC::CanBeComposited, which in turn affects
  // compositing inputs.
  if (LayoutView* view = GetLayoutView())
    view->Layer()->SetNeedsCompositingInputsUpdate();
}

void LocalFrameView::Show() {
  if (!IsSelfVisible()) {
    SetSelfVisible(true);
    SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
    if (IsParentVisible()) {
      ForAllChildViewsAndPlugins(
          [](EmbeddedContentView& embedded_content_view) {
            embedded_content_view.SetParentVisible(true);
          });
    }
  }
}

void LocalFrameView::Hide() {
  if (IsSelfVisible()) {
    if (IsParentVisible()) {
      ForAllChildViewsAndPlugins(
          [](EmbeddedContentView& embedded_content_view) {
            embedded_content_view.SetParentVisible(false);
          });
    }
    SetSelfVisible(false);
    SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
  }
}

int LocalFrameView::ViewportWidth() const {
  int viewport_width = GetLayoutSize().Width();
  return AdjustForAbsoluteZoom::AdjustInt(viewport_width, GetLayoutView());
}

ScrollableArea* LocalFrameView::GetScrollableArea() {
  if (viewport_scrollable_area_)
    return viewport_scrollable_area_.Get();

  return LayoutViewport();
}

PaintLayerScrollableArea* LocalFrameView::LayoutViewport() const {
  auto* layout_view = GetLayoutView();
  return layout_view ? layout_view->GetScrollableArea() : nullptr;
}

RootFrameViewport* LocalFrameView::GetRootFrameViewport() {
  return viewport_scrollable_area_.Get();
}

void LocalFrameView::CollectAnnotatedRegions(
    LayoutObject& layout_object,
    Vector<AnnotatedRegionValue>& regions) const {
  // LayoutTexts don't have their own style, they just use their parent's style,
  // so we don't want to include them.
  if (layout_object.IsText())
    return;

  layout_object.AddAnnotatedRegions(regions);
  for (LayoutObject* curr = layout_object.SlowFirstChild(); curr;
       curr = curr->NextSibling())
    CollectAnnotatedRegions(*curr, regions);
}

bool LocalFrameView::UpdateViewportIntersectionsForSubtree(
    unsigned parent_flags) {
  // TODO(dcheng): Since LocalFrameView tree updates are deferred, FrameViews
  // might still be in the LocalFrameView hierarchy even though the associated
  // Document is already detached. Investigate if this check and a similar check
  // in lifecycle updates are still needed when there are no more deferred
  // LocalFrameView updates: https://crbug.com/561683
  if (!GetFrame().GetDocument()->IsActive())
    return false;

  unsigned flags = GetIntersectionObservationFlags(parent_flags);
  bool needs_occlusion_tracking = false;

  if (!NeedsLayout()) {
    // Notify javascript IntersectionObservers
    if (IntersectionObserverController* controller =
            GetFrame().GetDocument()->GetIntersectionObserverController()) {
      needs_occlusion_tracking |= controller->ComputeIntersections(flags);
    }
    intersection_observation_state_ = kNotNeeded;
  }

  UpdateViewportIntersection(flags, needs_occlusion_tracking);

  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    needs_occlusion_tracking |=
        child->View()->UpdateViewportIntersectionsForSubtree(flags);
  }

  for (PortalContents* portal :
       DocumentPortals::From(*frame_->GetDocument()).GetPortals()) {
    if (Frame* frame = portal->GetFrame()) {
      needs_occlusion_tracking |=
          frame->View()->UpdateViewportIntersectionsForSubtree(flags);
    }
  }

  return needs_occlusion_tracking;
}

void LocalFrameView::DeliverSynchronousIntersectionObservations() {
  if (IntersectionObserverController* controller =
          GetFrame().GetDocument()->GetIntersectionObserverController()) {
    controller->DeliverNotifications(
        IntersectionObserver::kDeliverDuringPostLifecycleSteps);
  }
  ForAllChildLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.DeliverSynchronousIntersectionObservations();
  });
}

void LocalFrameView::CrossOriginToMainFrameChanged() {
  // If any of these conditions hold, then a change in cross-origin status does
  // not affect throttling.
  if (lifecycle_updates_throttled_ || IsSubtreeThrottled() ||
      !IsHiddenForThrottling()) {
    return;
  }
  RenderThrottlingStatusChanged();
  // We need to invalidate unconditionally, so if it didn't happen during
  // RenderThrottlingStatusChanged, do it now.
  if (CanThrottleRendering())
    InvalidateForThrottlingChange();
  // Immediately propagate changes to children.
  UpdateRenderThrottlingStatus(IsHiddenForThrottling(), IsSubtreeThrottled(),
                               true);
}

void LocalFrameView::CrossOriginToParentFrameChanged() {
  if (base::FeatureList::IsEnabled(
          blink::features::kCompositeCrossOriginIframes)) {
    if (LayoutView* layout_view = GetLayoutView()) {
      if (PaintLayer* root_layer = layout_view->Layer())
        root_layer->SetNeedsCompositingInputsUpdate();
    }
  }
}

void LocalFrameView::VisibilityForThrottlingChanged() {
  if (FrameScheduler* frame_scheduler = frame_->GetFrameScheduler()) {
    // TODO(szager): Per crbug.com/994443, maybe this should be:
    //   SetFrameVisible(IsHiddenForThrottling() || IsSubtreeThrottled());
    frame_scheduler->SetFrameVisible(!IsHiddenForThrottling());
  }
}

void LocalFrameView::VisibilityChanged(
    blink::mojom::FrameVisibility visibility) {
  frame_->GetLocalFrameHostRemote().VisibilityChanged(visibility);
}

void LocalFrameView::RenderThrottlingStatusChanged() {
  TRACE_EVENT0("blink", "LocalFrameView::RenderThrottlingStatusChanged");
  DCHECK(!IsInPerformLayout());
  DCHECK(!frame_->GetDocument() || !frame_->GetDocument()->InStyleRecalc());

  // We may record more/less pre-composited layers under the frame.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    SetPaintArtifactCompositorNeedsUpdate();

  if (!CanThrottleRendering())
    InvalidateForThrottlingChange();

  // If we have become unthrottled, this is essentially a no-op since we're
  // going to paint anyway. If we have become throttled, then this will force
  // one lifecycle update to clear the painted output.
  if (GetFrame().IsLocalRoot())
    need_paint_phase_after_throttling_ = true;

#if DCHECK_IS_ON()
  // Make sure we never have an unthrottled frame inside a throttled one.
  LocalFrameView* parent = ParentFrameView();
  while (parent) {
    DCHECK(CanThrottleRendering() || !parent->CanThrottleRendering());
    parent = parent->ParentFrameView();
  }
#endif
}

void LocalFrameView::InvalidateForThrottlingChange() {
  // Start ticking animation frames again if necessary.
  if (GetPage())
    GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());
  // Force a full repaint of this frame to ensure we are not left with a
  // partially painted version of this frame's contents if we skipped
  // painting them while the frame was throttled.
  LayoutView* layout_view = GetLayoutView();
  if (layout_view) {
    layout_view->InvalidatePaintForViewAndDescendants();
    // Also need to update all paint properties that might be skipped while
    // the frame was throttled.
    layout_view->AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kPreviouslySkipped);

    if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      // PaintLayerCompositor::CanBeComposited returns an incorrect visibility
      // value for throttled frames, and it needs to be recomputed when the
      // frame becomes unthrottled again.
      layout_view->Layer()->SetNeedsCompositingInputsUpdate();
    }
  }
  // Ensure we'll recompute viewport intersection for the frame subtree during
  // the scheduled visual update.
  SetIntersectionObservationState(kRequired);
}

void LocalFrameView::SetNeedsForcedCompositingUpdate() {
  needs_forced_compositing_update_ = true;
  if (LocalFrameView* parent = ParentFrameView())
    parent->SetNeedsForcedCompositingUpdate();
}

void LocalFrameView::SetIntersectionObservationState(
    IntersectionObservationState state) {
  if (intersection_observation_state_ >= state)
    return;
  intersection_observation_state_ = state;

  // If an intersection observation is required, force all ancestors to update.
  // Otherwise, an update could stop at a throttled frame before reaching this.
  if (state == kRequired) {
    Frame* parent_frame = frame_->Tree().Parent();
    if (auto* parent_local_frame = DynamicTo<LocalFrame>(parent_frame)) {
      if (parent_local_frame->View())
        parent_local_frame->View()->SetIntersectionObservationState(kRequired);
    }
  }
}

void LocalFrameView::SetPaintArtifactCompositorNeedsUpdate() {
  LocalFrameView* root = GetFrame().LocalFrameRoot().View();
  if (root && root->paint_artifact_compositor_)
    root->paint_artifact_compositor_->SetNeedsUpdate();
}

PaintArtifactCompositor* LocalFrameView::GetPaintArtifactCompositor() const {
  LocalFrameView* root = GetFrame().LocalFrameRoot().View();
  return root ? root->paint_artifact_compositor_.get() : nullptr;
}

unsigned LocalFrameView::GetIntersectionObservationFlags(
    unsigned parent_flags) const {
  unsigned flags = 0;

  const LocalFrame& target_frame = GetFrame();
  const Frame& root_frame = target_frame.Tree().Top();
  if (&root_frame == &target_frame ||
      target_frame.GetSecurityContext()->GetSecurityOrigin()->CanAccess(
          root_frame.GetSecurityContext()->GetSecurityOrigin())) {
    flags |= IntersectionObservation::kReportImplicitRootBounds;
  }

  // Observers with explicit roots only need to be checked on the same frame,
  // since in this case target and root must be in the same document.
  if (intersection_observation_state_ != kNotNeeded) {
    flags |= (IntersectionObservation::kExplicitRootObserversNeedUpdate |
              IntersectionObservation::kImplicitRootObserversNeedUpdate);
  }

  // For observers with implicit roots, we need to check state on the whole
  // local frame tree, as passed down from the parent.
  flags |= (parent_flags &
            IntersectionObservation::kImplicitRootObserversNeedUpdate);

  // The kIgnoreDelay parameter is used to force computation in an OOPIF which
  // is hidden in the parent document, thus not running lifecycle updates. It
  // applies to the entire frame tree.
  flags |= (parent_flags & IntersectionObservation::kIgnoreDelay);

  return flags;
}

bool LocalFrameView::ShouldThrottleRendering() const {
  bool throttled_for_global_reasons = CanThrottleRendering() &&
                                      frame_->GetDocument() &&
                                      Lifecycle().ThrottlingAllowed();
  if (!throttled_for_global_reasons || needs_forced_compositing_update_ ||
      need_paint_phase_after_throttling_) {
    return false;
  }

  if (intersection_observation_state_ == kRequired) {
    auto* local_frame_root_view = GetFrame().LocalFrameRoot().View();
    // When doing a lifecycle update required by intersection observer, we can
    // throttle lifecycle states after layout. Outside of lifecycle updates,
    // the frame should be considered throttled because it is not fully updating
    // the lifecycle.
    return !local_frame_root_view->in_lifecycle_update_ ||
           local_frame_root_view->past_layout_lifecycle_update_;
  }

  return true;
}

bool LocalFrameView::CanThrottleRendering() const {
  if (lifecycle_updates_throttled_)
    return true;
  if (IsSubtreeThrottled())
    return true;
  // We only throttle hidden cross-origin frames. This is to avoid a situation
  // where an ancestor frame directly depends on the pipeline timing of a
  // descendant and breaks as a result of throttling. The rationale is that
  // cross-origin frames must already communicate with asynchronous messages,
  // so they should be able to tolerate some delay in receiving replies from a
  // throttled peer.
  return IsHiddenForThrottling() && frame_->IsCrossOriginToMainFrame();
}

void LocalFrameView::UpdateRenderThrottlingStatus(bool hidden_for_throttling,
                                                  bool subtree_throttled,
                                                  bool recurse) {
  bool was_throttled = CanThrottleRendering();
  FrameView::UpdateRenderThrottlingStatus(hidden_for_throttling,
                                          subtree_throttled, recurse);
  if (was_throttled != CanThrottleRendering())
    RenderThrottlingStatusChanged();
}

void LocalFrameView::BeginLifecycleUpdates() {
  // Avoid pumping frames for the initially empty document.
  // TODO(schenney): This seems pointless because main frame updates do occur
  // for pages like about:blank, at least according to log messages.
  if (GetFrame().GetDocument()->IsInitialEmptyDocument())
    return;
  lifecycle_updates_throttled_ = false;
  if (auto* owner = GetLayoutEmbeddedContent())
    owner->SetShouldCheckForPaintInvalidation();

  LayoutView* layout_view = GetLayoutView();
  bool layout_view_is_empty = layout_view && !layout_view->FirstChild();
  if (layout_view_is_empty && !DidFirstLayout() && !NeedsLayout()) {
    // Make sure a display:none iframe gets an initial layout pass.
    layout_view->SetNeedsLayout(layout_invalidation_reason::kAddedToLayout,
                                kMarkOnlyThis);
  }

  ScheduleAnimation();
  SetIntersectionObservationState(kRequired);

  // Non-main-frame lifecycle and commit deferral are controlled by their
  // main frame.
  if (!GetFrame().IsMainFrame())
    return;

  Document* document = GetFrame().GetDocument();
  ChromeClient& chrome_client = GetFrame().GetPage()->GetChromeClient();

  // Determine if we want to defer commits to the compositor once lifecycle
  // updates start. Doing so allows us to update the page lifecycle but not
  // present the results to screen until we see first contentful paint is
  // available or until a timer expires.
  // This is enabled only when the document loading is regular HTML served
  // over HTTP/HTTPs. And only defer commits once. This method gets called
  // multiple times, and we do not want to defer a second time if we have
  // already done so once and resumed commits already.
  if (document &&
      document->DeferredCompositorCommitIsAllowed() &&
      !have_deferred_commits_) {
    chrome_client.StartDeferringCommits(
        GetFrame(), base::TimeDelta::FromMilliseconds(kCommitDelayDefaultInMs));
    have_deferred_commits_ = true;
  }

  chrome_client.BeginLifecycleUpdates(GetFrame());
}

void LocalFrameView::SetInitialViewportSize(const IntSize& viewport_size) {
  if (viewport_size == initial_viewport_size_)
    return;

  initial_viewport_size_ = viewport_size;
  if (Document* document = frame_->GetDocument())
    document->GetStyleEngine().InitialViewportChanged();
}

int LocalFrameView::InitialViewportWidth() const {
  DCHECK(frame_->IsMainFrame());
  return initial_viewport_size_.Width();
}

int LocalFrameView::InitialViewportHeight() const {
  DCHECK(frame_->IsMainFrame());
  return initial_viewport_size_.Height();
}

bool LocalFrameView::HasVisibleSlowRepaintViewportConstrainedObjects() const {
  if (!ViewportConstrainedObjects())
    return false;
  for (const LayoutObject* layout_object : *ViewportConstrainedObjects()) {
    DCHECK(layout_object->IsBoxModelObject() && layout_object->HasLayer());
    DCHECK(layout_object->StyleRef().GetPosition() == EPosition::kFixed ||
           layout_object->StyleRef().GetPosition() == EPosition::kSticky);
    if (ToLayoutBoxModelObject(layout_object)->IsSlowRepaintConstrainedObject())
      return true;
  }
  return false;
}

MainThreadScrollingReasons LocalFrameView::MainThreadScrollingReasonsPerFrame()
    const {
  MainThreadScrollingReasons reasons =
      static_cast<MainThreadScrollingReasons>(0);

  if (ShouldThrottleRendering())
    return reasons;

  if (RequiresMainThreadScrollingForBackgroundAttachmentFixed()) {
    reasons |=
        cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  }

  // Force main-thread scrolling if the frame has uncomposited position: fixed
  // elements.  Note: we care about this not only for input-scrollable frames
  // but also for overflow: hidden frames, because script can run composited
  // smooth-scroll animations.  For this reason, we use HasOverflow instead of
  // ScrollsOverflow (which is false for overflow: hidden).
  if (LayoutViewport()->HasOverflow() &&
      GetLayoutView()->StyleRef().VisibleToHitTesting() &&
      HasVisibleSlowRepaintViewportConstrainedObjects()) {
    reasons |=
        cc::MainThreadScrollingReason::kHasNonLayerViewportConstrainedObjects;
  }
  return reasons;
}

MainThreadScrollingReasons LocalFrameView::GetMainThreadScrollingReasons()
    const {
  MainThreadScrollingReasons reasons =
      static_cast<MainThreadScrollingReasons>(0);

  if (!GetPage()->GetSettings().GetThreadedScrollingEnabled())
    reasons |= cc::MainThreadScrollingReason::kThreadedScrollingDisabled;

  if (!GetPage()->MainFrame()->IsLocalFrame())
    return reasons;

  // TODO(alexmos,kenrb): For OOPIF, local roots that are different from
  // the main frame can't be used in the calculation, since they use
  // different compositors with unrelated state, which breaks some of the
  // calculations below.
  if (&frame_->LocalFrameRoot() != GetPage()->MainFrame())
    return reasons;

  // Walk the tree to the root. Use the gathered reasons to determine
  // whether the target frame should be scrolled on main thread regardless
  // other subframes on the same page.
  for (Frame* frame = frame_; frame; frame = frame->Tree().Parent()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    reasons |= local_frame->View()->MainThreadScrollingReasonsPerFrame();
  }

  DCHECK(
      !cc::MainThreadScrollingReason::HasNonCompositedScrollReasons(reasons));
  return reasons;
}

String LocalFrameView::MainThreadScrollingReasonsAsText() {
  MainThreadScrollingReasons reasons = 0;
  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean);
  const auto* properties = GetLayoutView()->FirstFragment().PaintProperties();
  if (properties && properties->Scroll())
    reasons = properties->Scroll()->GetMainThreadScrollingReasons();
  return String(cc::MainThreadScrollingReason::AsText(reasons).c_str());
}

bool LocalFrameView::MapToVisualRectInRemoteRootFrame(
    PhysicalRect& rect,
    bool apply_overflow_clip) {
  DCHECK(frame_->IsLocalRoot());
  // This is the top-level frame, so no mapping necessary.
  if (frame_->IsMainFrame())
    return true;
  bool result = rect.InclusiveIntersect(PhysicalRect(
      apply_overflow_clip ? frame_->RemoteViewportIntersection()
                          : frame_->RemoteMainFrameIntersection()));
  if (result) {
    if (LayoutView* layout_view = GetLayoutView()) {
      rect = layout_view->LocalToAncestorRect(
          rect, nullptr,
          kTraverseDocumentBoundaries | kApplyRemoteMainFrameTransform);
    }
  }
  return result;
}

void LocalFrameView::MapLocalToRemoteMainFrame(
    TransformState& transform_state) {
  DCHECK(frame_->IsLocalRoot());
  // This is the top-level frame, so no mapping necessary.
  if (frame_->IsMainFrame())
    return;
  transform_state.ApplyTransform(
      TransformationMatrix(GetFrame().RemoteMainFrameTransform().matrix()),
      TransformState::kAccumulateTransform);
}

LayoutUnit LocalFrameView::CaretWidth() const {
  return LayoutUnit(std::max<float>(
      1.0f, GetChromeClient()->WindowToViewportScalar(&GetFrame(), 1.0f)));
}

void LocalFrameView::DidChangeMobileFriendliness(
    const blink::MobileFriendliness& mf) {
  GetFrame().Client()->DidChangeMobileFriendliness(mf);
}

LocalFrameUkmAggregator& LocalFrameView::EnsureUkmAggregator() {
  if (!ukm_aggregator_) {
    ukm_aggregator_ = base::MakeRefCounted<LocalFrameUkmAggregator>(
        frame_->GetDocument()->UkmSourceID(),
        frame_->GetDocument()->UkmRecorder());
  }
  return *ukm_aggregator_;
}

void LocalFrameView::OnFirstContentfulPaint() {
  GetPage()->GetChromeClient().StopDeferringCommits(
      *frame_, cc::PaintHoldingCommitTrigger::kFirstContentfulPaint);
  EnsureUkmAggregator().DidReachFirstContentfulPaint(frame_->IsMainFrame());
}

void LocalFrameView::RegisterForLifecycleNotifications(
    LifecycleNotificationObserver* observer) {
  lifecycle_observers_.insert(observer);
}

void LocalFrameView::UnregisterFromLifecycleNotifications(
    LifecycleNotificationObserver* observer) {
  lifecycle_observers_.erase(observer);
}

void LocalFrameView::EnqueueStartOfLifecycleTask(base::OnceClosure closure) {
  start_of_lifecycle_tasks_.push_back(std::move(closure));
}

void LocalFrameView::NotifyVideoIsDominantVisibleStatus(
    HTMLVideoElement* element,
    bool is_dominant) {
  if (is_dominant) {
    fullscreen_video_elements_.insert(element);
    return;
  }

  fullscreen_video_elements_.erase(element);
}

bool LocalFrameView::HasDominantVideoElement() const {
  return !fullscreen_video_elements_.IsEmpty();
}

#if DCHECK_IS_ON()
LocalFrameView::DisallowLayoutInvalidationScope::
    DisallowLayoutInvalidationScope(LocalFrameView* view)
    : local_frame_view_(view) {
  local_frame_view_->allows_layout_invalidation_after_layout_clean_ = false;
  local_frame_view_->ForAllChildLocalFrameViews([](LocalFrameView& frame_view) {
    if (!frame_view.ShouldThrottleRendering())
      frame_view.CheckDoesNotNeedLayout();
    frame_view.allows_layout_invalidation_after_layout_clean_ = false;
  });
}

LocalFrameView::DisallowLayoutInvalidationScope::
    ~DisallowLayoutInvalidationScope() {
  local_frame_view_->allows_layout_invalidation_after_layout_clean_ = true;
  local_frame_view_->ForAllChildLocalFrameViews([](LocalFrameView& frame_view) {
    if (!frame_view.ShouldThrottleRendering())
      frame_view.CheckDoesNotNeedLayout();
    frame_view.allows_layout_invalidation_after_layout_clean_ = true;
  });
}

#endif

void LocalFrameView::UpdateLayerDebugInfoEnabled() {
  DCHECK(frame_->IsLocalRoot());
#if DCHECK_IS_ON()
  DCHECK(layer_debug_info_enabled_);
#else
  bool should_enable =
      cc::frame_viewer_instrumentation::IsTracingLayerTreeSnapshots() ||
      WebTestSupport::IsRunningWebTest() ||
      CoreProbeSink::HasAgentsGlobal(CoreProbeSink::kInspectorLayerTreeAgent);
  if (should_enable != layer_debug_info_enabled_) {
    layer_debug_info_enabled_ = should_enable;
    SetPaintArtifactCompositorNeedsUpdate();
  }
#endif
}

OverlayInterstitialAdDetector&
LocalFrameView::EnsureOverlayInterstitialAdDetector() {
  if (!overlay_interstitial_ad_detector_) {
    overlay_interstitial_ad_detector_ =
        std::make_unique<OverlayInterstitialAdDetector>();
  }
  return *overlay_interstitial_ad_detector_.get();
}

WTF::Vector<const TransformPaintPropertyNode*>
LocalFrameView::GetScrollTranslationNodes() {
  WTF::Vector<const TransformPaintPropertyNode*> scroll_translation_nodes;
  for (auto area : *ScrollableAreas()) {
    const auto* paint_properties =
        area->GetLayoutBox()->FirstFragment().PaintProperties();
    if (paint_properties && paint_properties->Scroll()) {
      scroll_translation_nodes.push_back(paint_properties->ScrollTranslation());
    }
  }
  return scroll_translation_nodes;
}

StickyAdDetector& LocalFrameView::EnsureStickyAdDetector() {
  if (!sticky_ad_detector_) {
    sticky_ad_detector_ = std::make_unique<StickyAdDetector>();
  }
  return *sticky_ad_detector_.get();
}

static PaintLayer* GetFullScreenOverlayVideoLayer(Document& document) {
  // Recursively find the document that is in fullscreen.
  Document* content_document = &document;
  Element* fullscreen_element =
      Fullscreen::FullscreenElementFrom(*content_document);
  while (auto* frame_owner =
             DynamicTo<HTMLFrameOwnerElement>(fullscreen_element)) {
    content_document = frame_owner->contentDocument();
    if (!content_document)
      return nullptr;
    fullscreen_element = Fullscreen::FullscreenElementFrom(*content_document);
  }
  auto* video_element = DynamicTo<HTMLVideoElement>(fullscreen_element);
  if (!video_element || !video_element->UsesOverlayFullscreenVideo())
    return nullptr;
  return video_element->GetLayoutBoxModelObject()->Layer();
}

static PaintLayer* GetXrOverlayLayer(Document& document) {
  // immersive-ar DOM overlay mode is very similar to fullscreen video, using
  // the AR camera image instead of a video element as a background that's
  // separately composited in the browser. The fullscreened DOM content is shown
  // on top of that, same as HTML video controls.
  if (!document.IsXrOverlay())
    return nullptr;

  // When DOM overlay mode is active in iframe content, the parent frame's
  // document will also be marked as being in DOM overlay mode, with the iframe
  // element being in fullscreen mode. Find the innermost reachable fullscreen
  // element to use as the XR overlay layer. This is the overlay element for
  // same-process iframes, or an iframe element for OOPIF if the overlay element
  // is in another process.
  Document* content_document = &document;
  Element* fullscreen_element =
      Fullscreen::FullscreenElementFrom(*content_document);
  while (auto* frame_owner =
             DynamicTo<HTMLFrameOwnerElement>(fullscreen_element)) {
    content_document = frame_owner->contentDocument();
    if (!content_document) {
      // This is an OOPIF iframe, treat it as the fullscreen element.
      break;
    }
    fullscreen_element = Fullscreen::FullscreenElementFrom(*content_document);
  }

  if (!fullscreen_element)
    return nullptr;

  const auto* object = fullscreen_element->GetLayoutBoxModelObject();
  if (!object) {
    // Currently, only HTML fullscreen elements are supported for this mode,
    // not others such as SVG or MathML.
    DVLOG(1) << "no LayoutBoxModelObject for element " << fullscreen_element;
    return nullptr;
  }

  return object->Layer();
}

PaintLayer* LocalFrameView::GetFullScreenOverlayLayer() const {
  Document* doc = frame_->GetDocument();
  DCHECK(doc);

  // For WebXR DOM Overlay, the fullscreen overlay layer comes from either the
  // overlay element itself, or from an iframe element if the overlay element is
  // in an OOPIF. This layer is needed even for non-main-frame scenarios to
  // ensure the background remains transparent.
  if (doc->IsXrOverlay())
    return GetXrOverlayLayer(*doc);

  // Fullscreen overlay video layers are only used for the main frame.
  DCHECK(frame_->IsMainFrame());
  return GetFullScreenOverlayVideoLayer(*doc);
}

void LocalFrameView::RunPaintBenchmark(int repeat_count,
                                       cc::PaintBenchmarkResult& result) {
  DCHECK_EQ(Lifecycle().GetState(), DocumentLifecycle::kPaintClean);

  auto run_benchmark = [&](PaintBenchmarkMode mode) -> double {
    constexpr int kTimeCheckInterval = 1;
    constexpr int kWarmupRuns = 0;
    constexpr base::TimeDelta kTimeLimit = base::TimeDelta::FromMilliseconds(1);

    base::TimeDelta min_time = base::TimeDelta::Max();
    for (int i = 0; i < repeat_count; i++) {
      // Run for a minimum amount of time to avoid problems with timer
      // quantization when the time is very small.
      base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
      do {
        RunPaintLifecyclePhase(mode);
        timer.NextLap();
      } while (!timer.HasTimeLimitExpired());

      base::TimeDelta duration = timer.TimePerLap();
      if (duration < min_time)
        min_time = duration;
    }
    return min_time.InMillisecondsF();
  };

  result.record_time_ms = run_benchmark(PaintBenchmarkMode::kForcePaint);
  result.record_time_caching_disabled_ms =
      run_benchmark(PaintBenchmarkMode::kCachingDisabled);
  result.record_time_subsequence_caching_disabled_ms =
      run_benchmark(PaintBenchmarkMode::kSubsequenceCachingDisabled);
  result.record_time_partial_invalidation_ms =
      run_benchmark(PaintBenchmarkMode::kPartialInvalidation);
  result.raster_invalidation_and_convert_time_ms =
      run_benchmark(PaintBenchmarkMode::kForceRasterInvalidationAndConvert);
  result.paint_artifact_compositor_update_time_ms =
      run_benchmark(PaintBenchmarkMode::kForcePaintArtifactCompositorUpdate);

  result.painter_memory_usage = 0;
  if (paint_controller_) {
    result.painter_memory_usage +=
        paint_controller_->ApproximateUnsharedMemoryUsage();
  }
  if (paint_artifact_compositor_) {
    result.painter_memory_usage +=
        paint_artifact_compositor_->ApproximateUnsharedMemoryUsage();
  }
  if (auto* root = GetLayoutView()->Compositor()->PaintRootGraphicsLayer()) {
    result.painter_memory_usage +=
        root->ApproximateUnsharedMemoryUsageRecursive();
  }
}

}  // namespace blink
