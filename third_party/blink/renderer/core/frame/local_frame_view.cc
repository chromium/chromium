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

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"
#include "base/timer/lap_timer.h"
#include "base/trace_event/typed_macros.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/features.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/picture_layer.h"
#include "cc/tiles/frame_viewer_instrumentation.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/view_transition/view_transition_request.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/css/post_style_update_scope.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_document_state.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/fragment_directive/fragment_directive_utils.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_handler.h"
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
#include "third_party/blink/renderer/core/frame/pagination_state.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/highlight/highlight_registry.h"
#include "third_party/blink/renderer/core/html/fenced_frame/document_fenced_frames.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element.h"
#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"
#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/pagination_utils.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/layout/traced_layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/mobile_metrics/mobile_friendliness_checker.h"
#include "third_party/blink/renderer/core/mobile_metrics/tap_friendliness_checker.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/link_highlight.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/paint/cull_rect_updater.h"
#include "third_party/blink/renderer/core/paint/frame_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_controller.h"
#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/scroll/smooth_scroll_sequencer.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/position_try_fallbacks.h"
#include "third_party/blink/renderer/core/svg/svg_document_extensions.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_request.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_performance.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
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
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-blink.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_f.h"

// Used to check for dirty layouts violating document lifecycle rules.
// If arg evaluates to true, the program will continue. If arg evaluates to
// false, program will crash if DCHECK_IS_ON() or return false from the current
// function.
#define CHECK_FOR_DIRTY_LAYOUT(arg) \
  do {                              \
    DCHECK(arg);                    \
    if (!(arg)) {                   \
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
  if (cursor.type() != ui::mojom::blink::CursorType::kCustom) {
    return;
  }

  const SkBitmap& bitmap = cursor.custom_bitmap();
  if (bitmap.isNull()) {
    return;
  }

  // Should not overflow, this calculation is done elsewhere when determining
  // whether the cursor exceeds its maximum size (see event_handler.cc).
  auto scaled_size =
      gfx::ScaleToFlooredSize(gfx::Size(bitmap.width(), bitmap.height()),
                              1 / cursor.image_scale_factor());
  if (scaled_size.width() > 64 || scaled_size.height() > 64) {
    UseCounter::Count(frame->GetDocument(), WebFeature::kCursorImageGT64x64);
  } else if (scaled_size.width() > 32 || scaled_size.height() > 32) {
    UseCounter::Count(frame->GetDocument(), WebFeature::kCursorImageGT32x32);
  } else {
    UseCounter::Count(frame->GetDocument(), WebFeature::kCursorImageLE32x32);
  }
}

gfx::QuadF GetQuadForTimelinePaintEvent(const scoped_refptr<cc::Layer>& layer) {
  gfx::RectF rect(layer->update_rect());
  if (layer->transform_tree_index() != -1)
    rect = layer->ScreenSpaceTransform().MapRect(rect);
  return gfx::QuadF(rect);
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

// The number of |InvalidationDisallowedScope| class instances. Used to ensure
// that no more than one instance of this class exists at any given time.
int LocalFrameView::InvalidationDisallowedScope::instance_count_ = 0;

LocalFrameView::LocalFrameView(LocalFrame& frame)
    : LocalFrameView(frame, gfx::Rect()) {
  Show();
}

LocalFrameView::LocalFrameView(LocalFrame& frame, const gfx::Size& initial_size)
    : LocalFrameView(frame, gfx::Rect(gfx::Point(), initial_size)) {
  SetLayoutSizeInternal(initial_size);
  Show();
}

LocalFrameView::LocalFrameView(LocalFrame& frame, gfx::Rect frame_rect)
    : FrameView(frame_rect),
      frame_(frame),
      can_have_scrollbars_(true),
      has_pending_layout_(false),
      layout_scheduling_enabled_(true),
      layout_count_for_testing_(0),
      // We want plugin updates to happen in FIFO order with loading tasks.
      update_plugins_timer_(frame.GetTaskRunner(TaskType::kInternalLoading),
                            this,
                            &LocalFrameView::UpdatePluginsTimerFired),
      base_background_color_(Color::kWhite),
      media_type_(media_type_names::kScreen),
      visually_non_empty_character_count_(0),
      visually_non_empty_pixel_count_(0),
      is_visually_non_empty_(false),
      layout_size_fixed_to_frame_size_(true),
      needs_update_geometries_(false),
      root_layer_did_scroll_(false),
      // The compositor throttles the main frame using deferred begin main frame
      // updates. We can't throttle it here or it seems the root compositor
      // doesn't get setup properly.
      lifecycle_updates_throttled_(!GetFrame().IsMainFrame()),
      target_state_(DocumentLifecycle::kUninitialized),
      suppress_adjust_view_size_(false),
      intersection_observation_state_(kNotNeeded),
      main_thread_scrolling_reasons_(0),
      forced_layout_stack_depth_(0),
      paint_frame_count_(0),
      unique_id_(NewUniqueObjectId()),
      layout_shift_tracker_(MakeGarbageCollected<LayoutShiftTracker>(this)),
      paint_timing_detector_(MakeGarbageCollected<PaintTimingDetector>(this)),
      mobile_friendliness_checker_(MobileFriendlinessChecker::Create(*this)),
      tap_friendliness_checker_(TapFriendlinessChecker::CreateIfMobile(*this))
#if DCHECK_IS_ON()
      ,
      is_updating_descendant_dependent_flags_(false),
      is_updating_layout_(false)
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
  visitor->Trace(part_update_set_);
  visitor->Trace(frame_);
  visitor->Trace(update_plugins_timer_);
  visitor->Trace(layout_subtree_root_list_);
  visitor->Trace(fragment_anchor_);
  visitor->Trace(scroll_anchoring_scrollable_areas_);
  visitor->Trace(animating_scrollable_areas_);
  visitor->Trace(user_scrollable_areas_);
  visitor->Trace(background_attachment_fixed_objects_);
  visitor->Trace(auto_size_info_);
  visitor->Trace(pagination_state_);
  visitor->Trace(plugins_);
  visitor->Trace(scrollbars_);
  visitor->Trace(viewport_scrollable_area_);
  visitor->Trace(anchoring_adjustment_queue_);
  visitor->Trace(scroll_event_queue_);
  visitor->Trace(paint_controller_persistent_data_);
  visitor->Trace(paint_artifact_compositor_);
  visitor->Trace(layout_shift_tracker_);
  visitor->Trace(paint_timing_detector_);
  visitor->Trace(mobile_friendliness_checker_);
  visitor->Trace(tap_friendliness_checker_);
  visitor->Trace(lifecycle_observers_);
  visitor->Trace(fullscreen_video_elements_);
  visitor->Trace(pending_transform_updates_);
  visitor->Trace(pending_opacity_updates_);
  visitor->Trace(pending_sticky_updates_);
  visitor->Trace(pending_snap_updates_);
  visitor->Trace(pending_perform_snap_);
  visitor->Trace(disconnected_elements_with_remembered_size_);
}

void LocalFrameView::ForAllChildViewsAndPlugins(
    base::FunctionRef<void(EmbeddedContentView&)> function) {
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (child->View())
      function(*child->View());
  }

  for (const auto& plugin : plugins_) {
    function(*plugin);
  }

  if (Document* document = frame_->GetDocument()) {
    if (DocumentFencedFrames* fenced_frames =
            DocumentFencedFrames::Get(*document)) {
      for (HTMLFencedFrameElement* fenced_frame :
           fenced_frames->GetFencedFrames()) {
        if (Frame* frame = fenced_frame->ContentFrame())
          function(*frame->View());
      }
    }
  }
}

void LocalFrameView::ForAllChildLocalFrameViews(
    base::FunctionRef<void(LocalFrameView&)> function) {
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (!child_local_frame)
      continue;
    if (LocalFrameView* child_view = child_local_frame->View())
      function(*child_view);
  }
}

// Note: if this logic is updated, `ForAllThrottledLocalFrameViews()` may
// need to be updated as well.
void LocalFrameView::ForAllNonThrottledLocalFrameViews(
    base::FunctionRef<void(LocalFrameView&)> function,
    TraversalOrder order) {
  if (ShouldThrottleRendering())
    return;

  if (order == kPreOrder)
    function(*this);

  ForAllChildLocalFrameViews([&function, order](LocalFrameView& child_view) {
    child_view.ForAllNonThrottledLocalFrameViews(function, order);
  });

  if (order == kPostOrder)
    function(*this);
}

// Note: if this logic is updated, `ForAllNonThrottledLocalFrameViews()` may
// need to be updated as well.
void LocalFrameView::ForAllThrottledLocalFrameViews(
    base::FunctionRef<void(LocalFrameView&)> function) {
  if (ShouldThrottleRendering())
    function(*this);

  ForAllChildLocalFrameViews([&function](LocalFrameView& child_view) {
    child_view.ForAllThrottledLocalFrameViews(function);
  });
}

void LocalFrameView::ForAllRemoteFrameViews(
    base::FunctionRef<void(RemoteFrameView&)> function) {
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
    if (DocumentFencedFrames* fenced_frames =
            DocumentFencedFrames::Get(*document)) {
      for (HTMLFencedFrameElement* fenced_frame :
           fenced_frames->GetFencedFrames()) {
        if (RemoteFrame* frame =
                To<RemoteFrame>(fenced_frame->ContentFrame())) {
          if (RemoteFrameView* view = frame->View())
            function(*view);
        }
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
  if (viewport_scrollable_area_) {
    DCHECK(frame_->IsMainFrame());
    DCHECK(frame_->GetPage());

    viewport_scrollable_area_->ClearScrollableArea();
    viewport_scrollable_area_.Clear();
    frame_->GetPage()->GlobalRootScrollerController().Reset();
  }

  // If we have scheduled plugins to be updated, cancel it. They will still be
  // notified before they are destroyed.
  if (update_plugins_timer_.IsActive())
    update_plugins_timer_.Stop();
  part_update_set_.clear();

  // These are LayoutObjects whose layout has been deferred to a subsequent
  // lifecycle update. Not gonna happen.
  layout_subtree_root_list_.Clear();

  // TODO(szager): LayoutObjects are supposed to remove themselves from these
  // tracking groups when they update style or are destroyed, but sometimes they
  // are missed. It would be good to understand how/why that happens, but in the
  // mean time, it's not safe to keep pointers around to defunct LayoutObjects.
  background_attachment_fixed_objects_.clear();

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

  if (ukm_aggregator_) {
    LocalFrame& root_frame = GetFrame().LocalFrameRoot();
    Document* root_document = root_frame.GetDocument();
    if (root_document) {
      ukm_aggregator_->TransmitFinalSample(root_document->UkmSourceID(),
                                           root_document->UkmRecorder(),
                                           root_frame.IsMainFrame());
    }
    ukm_aggregator_.reset();
  }
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

void LocalFrameView::UsesOverlayScrollbarsChanged() {
  if (!user_scrollable_areas_)
    return;
  for (const auto& scrollable_area : user_scrollable_areas_->Values()) {
    if (scrollable_area->ScrollsOverflow() || scrollable_area->HasScrollbar()) {
      scrollable_area->RemoveScrollbarsForReconstruction();
      if (auto* layout_box = scrollable_area->GetLayoutBox()) {
        layout_box->SetNeedsLayout(
            layout_invalidation_reason::kScrollbarChanged);
      }
    }
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

void LocalFrameView::FrameRectsChanged(const gfx::Rect& old_rect) {
  PropagateFrameRects();

  if (FrameRect() != old_rect) {
    if (auto* layout_view = GetLayoutView())
      layout_view->SetShouldCheckForPaintInvalidation();
  }

  if (Size() != old_rect.size()) {
    ViewportSizeChanged();
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

cc::AnimationHost* LocalFrameView::GetCompositorAnimationHost() const {
  if (!GetChromeClient())
    return nullptr;

  return GetChromeClient()->GetCompositorAnimationHost(*frame_);
}

cc::AnimationTimeline* LocalFrameView::GetScrollAnimationTimeline() const {
  if (!GetChromeClient())
    return nullptr;

  return GetChromeClient()->GetScrollAnimationTimeline(*frame_);
}

void LocalFrameView::SetLayoutOverflowSize(const gfx::Size& size) {
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
  SetLayoutOverflowSize(ToPixelSnappedRect(layout_view->DocumentRect()).size());
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

bool LocalFrameView::LayoutFromRootObject(LayoutObject& root) {
  if (!root.NeedsLayout())
    return false;

  if (DisplayLockUtilities::LockedAncestorPreventingLayout(root)) {
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

  if (scroll_anchoring_scrollable_areas_) {
    for (auto& scrollable_area : *scroll_anchoring_scrollable_areas_) {
      if (scrollable_area->GetScrollAnchor() &&
          scrollable_area->ShouldPerformScrollAnchoring())
        scrollable_area->GetScrollAnchor()->NotifyBeforeLayout();
    }
  }

  To<LayoutBox>(root).LayoutSubtreeRoot();
  return true;
}

#define PERFORM_LAYOUT_TRACE_CATEGORIES \
  "blink,benchmark,rail," TRACE_DISABLED_BY_DEFAULT("blink.debug.layout")

void LocalFrameView::PerformLayout() {
  ScriptForbiddenScope forbid_script;

  has_pending_layout_ = false;

  FontCachePurgePreventer font_cache_purge_preventer;
  base::AutoReset<bool> change_scheduling_enabled(&layout_scheduling_enabled_,
                                                  false);
  // If the layout view was marked as needing layout after we added items in
  // the subtree roots we need to clear the roots and do the layout from the
  // layoutView.
  if (GetLayoutView()->NeedsLayout())
    ClearLayoutSubtreeRootsAndMarkContainingBlocks();
  GetLayoutView()->ClearHitTestCache();

  const bool in_subtree_layout = IsSubtreeLayout();

  Document* document = GetFrame().GetDocument();
  if (!in_subtree_layout) {
    ClearLayoutSubtreeRootsAndMarkContainingBlocks();
    Node* body = document->body();
    if (IsA<HTMLFrameSetElement>(body) && body->GetLayoutObject()) {
      body->GetLayoutObject()->SetChildNeedsLayout();
    }

    first_layout_ = false;

    if (first_layout_with_body_ && body) {
      first_layout_with_body_ = false;
      mojom::blink::ScrollbarMode h_mode;
      mojom::blink::ScrollbarMode v_mode;
      GetLayoutView()->CalculateScrollbarModes(h_mode, v_mode);
      if (v_mode == mojom::blink::ScrollbarMode::kAuto) {
        if (auto* scrollable_area = GetLayoutView()->GetScrollableArea())
          scrollable_area->ForceVerticalScrollbarForFirstLayout();
      }
    }
  }

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.trees"), "LayoutTree", this,
      TracedLayoutObject::Create(*GetLayoutView(), false));

  gfx::Size old_size(Size());

  DCHECK(in_subtree_layout || layout_subtree_root_list_.IsEmpty());

  double contents_height_before_layout =
      GetLayoutView()->DocumentRect().Height();
  TRACE_EVENT_BEGIN1(
      PERFORM_LAYOUT_TRACE_CATEGORIES, "LocalFrameView::performLayout",
      "contentsHeightBeforeLayout", contents_height_before_layout);

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
      // This map will be used to avoid rebuilding several times the fragment
      // tree spine of a common ancestor.
      HeapHashMap<Member<const LayoutBox>, unsigned> fragment_tree_spines;
      for (auto& root : layout_subtree_root_list_.Unordered()) {
        const LayoutBox* container_box = root->ContainingNGBox();
        if (container_box && container_box->PhysicalFragmentCount()) {
          auto add_result = fragment_tree_spines.insert(container_box, 0);
          ++add_result.stored_value->value;
        }
      }
      for (auto& root : layout_subtree_root_list_.Ordered()) {
        bool should_rebuild_fragments = false;
        LayoutObject& root_layout_object = *root;
        LayoutBox* container_box = root->ContainingNGBox();
        if (container_box) {
          auto it = fragment_tree_spines.find(container_box);
          DCHECK(it == fragment_tree_spines.end() || it->value > 0);
          // Ensure fragment-tree consistency just after all the cb's
          // descendants have completed their subtree layout.
          should_rebuild_fragments =
              it != fragment_tree_spines.end() && --it->value == 0;
        }

        if (!LayoutFromRootObject(*root))
          continue;

        if (should_rebuild_fragments)
          container_box->RebuildFragmentTreeSpine();

        // We need to ensure that we mark up all layoutObjects up to the
        // LayoutView for paint invalidation. This simplifies our code as we
        // just always do a full tree walk.
        if (LayoutObject* container = root_layout_object.Container())
          container->SetShouldCheckForPaintInvalidation();
      }
      layout_subtree_root_list_.Clear();
#if DCHECK_IS_ON()
      // Ensure fragment-tree consistency after a subtree layout.
      for (const auto& p : fragment_tree_spines) {
        p.key->AssertFragmentTree();
        DCHECK_EQ(p.value, 0u);
      }
#endif
      fragment_tree_spines.clear();
    } else {
      GetLayoutView()->LayoutRoot();
    }
  }

  document->Fetcher()->UpdateAllImageResourcePriorities();

  Lifecycle().AdvanceTo(DocumentLifecycle::kAfterPerformLayout);

  TRACE_EVENT_END0(PERFORM_LAYOUT_TRACE_CATEGORIES,
                   "LocalFrameView::performLayout");
  FirstMeaningfulPaintDetector::From(*document)
      .MarkNextPaintAsMeaningfulIfNeeded(
          layout_object_counter_, contents_height_before_layout,
          GetLayoutView()->DocumentRect().Height(), Height());

  if (old_size != Size()) {
    InvalidateLayoutForViewportConstrainedObjects();
  }

  if (frame_->IsMainFrame()) {
    if (auto* text_autosizer = document->GetTextAutosizer()) {
      if (text_autosizer->HasLayoutInlineSizeChanged())
        text_autosizer->UpdatePageInfoInAllFrames(frame_);
    }
  }
#if EXPENSIVE_DCHECKS_ARE_ON()
  DCHECK(!Lifecycle().LifecyclePostponed() && !ShouldThrottleRendering());
  document->AssertLayoutTreeUpdatedAfterLayout();
#endif
}

void LocalFrameView::UpdateLayout() {
  // We should never layout a Document which is not in a LocalFrame.
  DCHECK(frame_);
  DCHECK_EQ(frame_->View(), this);
  DCHECK(frame_->GetPage());

  Lifecycle().EnsureStateAtMost(DocumentLifecycle::kStyleClean);

  std::optional<RuntimeCallTimerScope> rcs_scope;
  probe::UpdateLayout probe(frame_->GetDocument());
  HeapVector<LayoutObjectWithDepth> layout_roots;

  v8::Isolate* isolate = frame_->GetPage()->GetAgentGroupScheduler().Isolate();
  ENTER_EMBEDDER_STATE(isolate, frame_, BlinkState::LAYOUT);
  TRACE_EVENT_BEGIN0("blink,benchmark", "LocalFrameView::layout");
  if (RuntimeEnabledFeatures::BlinkRuntimeCallStatsEnabled()) [[unlikely]] {
    rcs_scope.emplace(RuntimeCallStats::From(isolate),
                      RuntimeCallStats::CounterId::kUpdateLayout);
  }
  layout_roots = layout_subtree_root_list_.Ordered();
  if (layout_roots.empty())
    layout_roots.push_back(LayoutObjectWithDepth(GetLayoutView()));
  TRACE_EVENT_BEGIN1("devtools.timeline", "Layout", "beginData",
                     [&](perfetto::TracedValue context) {
                       inspector_layout_event::BeginData(std::move(context),
                                                         this);
                     });

  PerformLayout();
  Lifecycle().AdvanceTo(DocumentLifecycle::kLayoutClean);

  TRACE_EVENT_END0("blink,benchmark", "LocalFrameView::layout");

  TRACE_EVENT_END1("devtools.timeline", "Layout", "endData",
                   [&](perfetto::TracedValue context) {
                     inspector_layout_event::EndData(std::move(context),
                                                     layout_roots);
                   });
  probe::DidChangeViewport(frame_.Get());
}

void LocalFrameView::WillStartForcedLayout(DocumentUpdateReason reason) {
  if (!base::TimeTicks::IsHighResolution()) {
    return;
  }

  // UpdateLayout is re-entrant for auto-sizing and plugins. So keep
  // track of stack depth to include all the time in the top-level call.
  forced_layout_stack_depth_++;
  if (forced_layout_stack_depth_ > 1)
    return;
  if (auto* metrics_aggregator = GetUkmAggregator()) {
    DCHECK(!forced_layout_timer_.has_value());
    forced_layout_timer_ =
        metrics_aggregator->GetScopedForcedLayoutTimer(reason);
  }
}

void LocalFrameView::DidFinishForcedLayout() {
  if (!base::TimeTicks::IsHighResolution()) {
    return;
  }

  CHECK_GT(forced_layout_stack_depth_, (unsigned)0);
  forced_layout_stack_depth_--;
  if (!forced_layout_stack_depth_) {
    forced_layout_timer_.reset();
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

gfx::SizeF LocalFrameView::SmallViewportSizeForViewportUnits() const {
  float zoom = 1;
  if (!frame_->GetDocument() || !frame_->GetDocument()->Printing())
    zoom = GetFrame().LayoutZoomFactor();

  auto* layout_view = GetLayoutView();
  if (!layout_view)
    return gfx::SizeF();

  gfx::SizeF layout_size;
  layout_size.set_width(layout_view->ViewWidth(kIncludeScrollbars) / zoom);
  layout_size.set_height(layout_view->ViewHeight(kIncludeScrollbars) / zoom);

  return layout_size;
}

gfx::SizeF LocalFrameView::LargeViewportSizeForViewportUnits() const {
  auto* layout_view = GetLayoutView();
  if (!layout_view)
    return gfx::SizeF();

  gfx::SizeF layout_size = SmallViewportSizeForViewportUnits();

  BrowserControls& browser_controls = frame_->GetPage()->GetBrowserControls();
  if (browser_controls.PermittedState() != cc::BrowserControlsState::kHidden) {
    // We use the layoutSize rather than frameRect to calculate viewport units
    // so that we get correct results on mobile where the page is laid out into
    // a rect that may be larger than the viewport (e.g. the 980px fallback
    // width for desktop pages). Since the layout height is statically set to
    // be the viewport with browser controls showing, we add the browser
    // controls height, compensating for page scale as well, since we want to
    // use the viewport with browser controls hidden for vh (to match Safari).
    int viewport_width = frame_->GetPage()->GetVisualViewport().Size().width();
    if (frame_->IsOutermostMainFrame() && layout_size.width() &&
        viewport_width) {
      float layout_to_viewport_width_scale_factor =
          viewport_width / layout_size.width();
      layout_size.Enlarge(0, (browser_controls.TotalHeight() -
                              browser_controls.TotalMinHeight()) /
                                 layout_to_viewport_width_scale_factor);
    }
  }

  return layout_size;
}

gfx::SizeF LocalFrameView::ViewportSizeForMediaQueries() const {
  if (!frame_->GetDocument()) {
    return gfx::SizeF(layout_size_);
  }
  if (frame_->ShouldUsePaginatedLayout()) {
    if (const LayoutView* layout_view = GetLayoutView()) {
      return layout_view->DefaultPageAreaSize();
    }
  }
  gfx::SizeF viewport_size(layout_size_);
  if (!frame_->GetDocument()->Printing()) {
    viewport_size.Scale(1 / GetFrame().LayoutZoomFactor());
  }
  return viewport_size;
}

gfx::SizeF LocalFrameView::DynamicViewportSizeForViewportUnits() const {
  BrowserControls& browser_controls = frame_->GetPage()->GetBrowserControls();
  return browser_controls.ShrinkViewport()
             ? SmallViewportSizeForViewportUnits()
             : LargeViewportSizeForViewportUnits();
}

DocumentLifecycle& LocalFrameView::Lifecycle() const {
  DCHECK(frame_);
  DCHECK(frame_->GetDocument());
  return frame_->GetDocument()->Lifecycle();
}

bool LocalFrameView::InvalidationDisallowed() const {
  return GetFrame().LocalFrameRoot().View()->invalidation_disallowed_;
}

void LocalFrameView::RunPostLifecycleSteps() {
  InvalidationDisallowedScope invalidation_disallowed(*this);
  AllowThrottlingScope allow_throttling(*this);
  RunAccessibilitySteps();
  RunIntersectionObserverSteps();
  if (mobile_friendliness_checker_)
    mobile_friendliness_checker_->MaybeRecompute();

  ForAllRemoteFrameViews([](RemoteFrameView& frame_view) {
    frame_view.UpdateCompositingScaleFactor();
  });
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

  if (frame_->IsOutermostMainFrame()) {
    EnsureOverlayInterstitialAdDetector().MaybeFireDetection(frame_.Get());
    EnsureStickyAdDetector().MaybeFireDetection(frame_.Get());

    // Report the main frame's document intersection with itself.
    LayoutObject* layout_object = GetLayoutView();
    gfx::Rect main_frame_dimensions(ToRoundedSize(
        To<LayoutBox>(layout_object)->ScrollableOverflowRect().size));
    GetFrame().Client()->OnMainFrameIntersectionChanged(main_frame_dimensions);
    GetFrame().Client()->OnMainFrameViewportRectangleChanged(
        gfx::Rect(frame_->GetOutermostMainFrameScrollPosition(),
                  frame_->GetOutermostMainFrameSize()));
  }

  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::UpdateViewportIntersectionsForSubtree");
  SCOPED_UMA_AND_UKM_TIMER(GetUkmAggregator(),
                           LocalFrameUkmAggregator::kIntersectionObservation);

  ComputeIntersectionsContext context;
  bool needs_occlusion_tracking =
      UpdateViewportIntersectionsForSubtree(0, context);
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
  DisallowThrottlingScope disallow_throttling(*this);
  UpdateLifecycleToPrePaintClean(
      DocumentUpdateReason::kIntersectionObservation);
  ComputeIntersectionsContext context;
  UpdateViewportIntersectionsForSubtree(
      IntersectionObservation::kImplicitRootObserversNeedUpdate |
          IntersectionObservation::kIgnoreDelay,
      context);
}

LayoutSVGRoot* LocalFrameView::EmbeddedReplacedContent() const {
  auto* layout_view = GetLayoutView();
  if (!layout_view)
    return nullptr;

  LayoutObject* first_child = layout_view->FirstChild();
  if (!first_child || !first_child->IsBox())
    return nullptr;

  // Currently only embedded SVG documents participate in the size-negotiation
  // logic.
  return DynamicTo<LayoutSVGRoot>(first_child);
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
}

void LocalFrameView::AddPartToUpdate(LayoutEmbeddedObject& object) {
  // This is typically called during layout to ensure we update plugins.
  // However, if layout is blocked (e.g. by content-visibility), we can add the
  // part to update during layout tree attachment (which is a part of style
  // recalc).
  DCHECK(IsInPerformLayout() ||
         (DisplayLockUtilities::LockedAncestorPreventingLayout(object) &&
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
      !frame_->GetSettings()->GetMediaTypeOverride().empty())
    return AtomicString(frame_->GetSettings()->GetMediaTypeOverride());
  return media_type_;
}

void LocalFrameView::AdjustMediaTypeForPrinting(bool printing) {
  if (printing) {
    if (media_type_when_not_printing_.IsNull())
      media_type_when_not_printing_ = media_type_;
    SetMediaType(media_type_names::kPrint);
  } else {
    if (!media_type_when_not_printing_.IsNull())
      SetMediaType(media_type_when_not_printing_);
    media_type_when_not_printing_ = g_null_atom;
  }
}

void LocalFrameView::AddBackgroundAttachmentFixedObject(
    LayoutBoxModelObject& object) {
  DCHECK(!background_attachment_fixed_objects_.Contains(&object));
  background_attachment_fixed_objects_.insert(&object);
  SetNeedsPaintPropertyUpdate();
}

void LocalFrameView::RemoveBackgroundAttachmentFixedObject(
    LayoutBoxModelObject& object) {
  background_attachment_fixed_objects_.erase(&object);
  SetNeedsPaintPropertyUpdate();
}

static bool BackgroundAttachmentFixedNeedsRepaintOnScroll(
    const LayoutObject& object) {
  // We should not add such object in the background_attachment_fixed_objects_.
  DCHECK(!To<LayoutBoxModelObject>(object).BackgroundTransfersToView());
  // The background doesn't need repaint if it's the viewport background and it
  // paints onto the border box space only.
  if (const auto* view = DynamicTo<LayoutView>(object)) {
    if (view->GetBackgroundPaintLocation() ==
        kBackgroundPaintInBorderBoxSpace) {
      return false;
    }
  }
  return !object.CanCompositeBackgroundAttachmentFixed();
}

bool LocalFrameView::RequiresMainThreadScrollingForBackgroundAttachmentFixed()
    const {
  for (const auto& object : background_attachment_fixed_objects_) {
    if (BackgroundAttachmentFixedNeedsRepaintOnScroll(*object)) {
      return true;
    }
  }
  return false;
}

void LocalFrameView::ViewportSizeChanged() {
  DCHECK(frame_->GetPage());
  if (frame_->GetDocument() &&
      frame_->GetDocument()->Lifecycle().LifecyclePostponed())
    return;

  if (frame_->IsOutermostMainFrame())
    layout_shift_tracker_->NotifyViewportSizeChanged();

  auto* layout_view = GetLayoutView();
  if (layout_view) {
    // If this is the outermost main frame, we might have got here by
    // hiding/showing the top controls. In that case, layout might not be
    // triggered, so some things that normally hook into layout need to be
    // specially notified.
    if (GetFrame().IsOutermostMainFrame()) {
      if (auto* scrollable_area = layout_view->GetScrollableArea()) {
        scrollable_area->ClampScrollOffsetAfterOverflowChange();
        scrollable_area->EnqueueForSnapUpdateIfNeeded();
      }
    }

    layout_view->Layer()->SetNeedsCompositingInputsUpdate();
  }

  if (GetFrame().GetDocument())
    GetFrame().GetDocument()->GetRootScrollerController().DidResizeFrameView();

  // Change of viewport size after browser controls showing/hiding may affect
  // painting of the background.
  if (layout_view && frame_->IsMainFrame() &&
      frame_->GetPage()->GetBrowserControls().TotalHeight())
    layout_view->SetShouldCheckForPaintInvalidation();

  if (GetFrame().GetDocument() && !IsInPerformLayout()) {
    InvalidateLayoutForViewportConstrainedObjects();
  }

  if (GetPaintTimingDetector().Visualizer())
    GetPaintTimingDetector().Visualizer()->OnViewportChanged();
}

void LocalFrameView::InvalidateLayoutForViewportConstrainedObjects() {
  auto* layout_view = GetLayoutView();
  if (layout_view && !layout_view->NeedsLayout()) {
    for (const auto& fragment : layout_view->PhysicalFragments()) {
      if (fragment.StickyDescendants()) {
        layout_view->SetNeedsSimplifiedLayout();
        return;
      }
      if (!fragment.HasOutOfFlowFragmentChild()) {
        continue;
      }
      for (const auto& fragment_child : fragment.Children()) {
        if (fragment_child->IsFixedPositioned()) {
          layout_view->SetNeedsSimplifiedLayout();
          return;
        }
      }
    }
  }
}

void LocalFrameView::DynamicViewportUnitsChanged() {
  if (GetFrame().GetDocument())
    GetFrame().GetDocument()->DynamicViewportUnitsChanged();
}

bool LocalFrameView::ShouldSetCursor() const {
  Page* page = GetFrame().GetPage();
  return page && page->IsPageVisible() &&
         !frame_->GetEventHandler().IsMousePositionUnknown() &&
         page->GetFocusController().IsActive();
}

void LocalFrameView::UpdateCanCompositeBackgroundAttachmentFixed() {
  // Too many composited background-attachment:fixed hurt performance, so we
  // want to avoid that with this heuristic (which doesn't need to be accurate
  // so we simply check the number of all background-attachment:fixed objects).
  constexpr wtf_size_t kMaxCompositedBackgroundAttachmentFixed = 8;
  bool enable_composited_background_attachment_fixed =
      background_attachment_fixed_objects_.size() <=
      kMaxCompositedBackgroundAttachmentFixed;
  for (const auto& object : background_attachment_fixed_objects_) {
    object->UpdateCanCompositeBackgroundAttachmentFixed(
        enable_composited_background_attachment_fixed);
  }
}

void LocalFrameView::InvalidateBackgroundAttachmentFixedDescendantsOnScroll(
    const LayoutBox& scroller) {
  for (const auto& layout_object : background_attachment_fixed_objects_) {
    if (scroller != GetLayoutView() &&
        !layout_object->IsDescendantOf(&scroller)) {
      continue;
    }
    if (BackgroundAttachmentFixedNeedsRepaintOnScroll(*layout_object)) {
      layout_object->SetBackgroundNeedsFullPaintInvalidation();
    }
  }
}

HitTestResult LocalFrameView::HitTestWithThrottlingAllowed(
    const HitTestLocation& location,
    HitTestRequest::HitTestRequestType request_type) const {
  AllowThrottlingScope allow_throttling(*this);
  return GetFrame().GetEventHandler().HitTestResultAtLocation(location,
                                                              request_type);
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

void LocalFrameView::SetLayoutSize(const gfx::Size& size) {
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

ChromeClient* LocalFrameView::GetChromeClient() const {
  Page* page = GetFrame().GetPage();
  if (!page)
    return nullptr;
  return &page->GetChromeClient();
}

void LocalFrameView::HandleLoadCompleted() {
  TRACE_EVENT1("blink", "LocalFrameView::HandleLoadCompleted",
               "has_auto_size_info", !!auto_size_info_);

  // Once loading has completed, allow autoSize one last opportunity to
  // reduce the size of the frame.
  if (auto_size_info_)
    UpdateStyleAndLayout();
}

void LocalFrameView::ClearLayoutSubtreeRoot(const LayoutObject& root) {
  layout_subtree_root_list_.Remove(const_cast<LayoutObject&>(root));
}

void LocalFrameView::ClearLayoutSubtreeRootsAndMarkContainingBlocks() {
  layout_subtree_root_list_.ClearAndMarkContainingBlocksForLayout();
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

  ComputeIntersectionsContext context;
  ComputePostLayoutIntersections(0, context);

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
                                          DocumentLifecycle::kPrePaintClean ||
                                      frame_view.NeedsLayout();
      });

  return needs_more_lifecycle_steps;
}

void LocalFrameView::ComputePostLayoutIntersections(
    unsigned parent_flags,
    ComputeIntersectionsContext& context) {
  if (ShouldThrottleRendering())
    return;

  unsigned flags = GetIntersectionObservationFlags(parent_flags) |
                   IntersectionObservation::kPostLayoutDeliveryOnly;

  if (auto* controller =
          GetFrame().GetDocument()->GetIntersectionObserverController()) {
    controller->ComputeIntersections(
        flags, *this, accumulated_scroll_delta_since_last_intersection_update_,
        context);
    accumulated_scroll_delta_since_last_intersection_update_ = gfx::Vector2dF();
  }

  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    auto* child_local_frame = DynamicTo<LocalFrame>(child);
    if (!child_local_frame)
      continue;
    if (LocalFrameView* child_view = child_local_frame->View())
      child_view->ComputePostLayoutIntersections(flags, context);
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
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT_WITH_CATEGORIES(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "InvalidateLayout",
      inspector_invalidate_layout_event::Data, frame_.Get(),
      GetLayoutView()->OwnerNodeId());

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

  LayoutView* layout_view = GetLayoutView();
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
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT_WITH_CATEGORIES(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "InvalidateLayout",
      inspector_invalidate_layout_event::Data, frame_.Get(),
      relayout_root->OwnerNodeId());
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

  if (auto* layout_view = GetLayoutView())
    layout_view->SetBackgroundNeedsFullPaintInvalidation();

  if (!ShouldThrottleRendering())
    GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());
}

void LocalFrameView::SetUseColorAdjustBackground(UseColorAdjustBackground use,
                                                 bool color_scheme_changed) {
  if (use_color_adjust_background_ == use && !color_scheme_changed)
    return;

  if (!frame_->GetDocument())
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
    GetFrame().DidChangeBackgroundColor(BaseBackgroundColor().toSkColor4f(),
                                        true /* color_adjust */);
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

void LocalFrameView::ClearFragmentAnchor() {
  fragment_anchor_ = nullptr;
}

bool LocalFrameView::UpdatePlugins() {
  // This is always called from UpdatePluginsTimerFired.
  // update_plugins_timer should only be scheduled if we have FrameViews to
  // update. Thus I believe we can stop checking isEmpty here, and just ASSERT
  // isEmpty:
  // FIXME: This assert has been temporarily removed due to
  // https://crbug.com/430344
  if (part_update_set_.empty())
    return true;

  // Need to swap because script will run inside the below loop and invalidate
  // the iterator.
  EmbeddedObjectSet objects;
  objects.swap(part_update_set_);

  for (const auto& embedded_object : objects) {
    LayoutEmbeddedObject& object = *embedded_object;

#if DCHECK_IS_ON()
    if (object.is_destroyed_)
      continue;
#endif

    auto* element = To<HTMLPlugInElement>(object.GetNode());

    // The object may have already been destroyed (thus node cleared).
    if (!element)
      continue;

    // No need to update if it's already crashed or known to be missing.
    if (object.ShowsUnavailablePluginIndicator())
      continue;

    if (element->NeedsPluginUpdate() && element->GetLayoutObject())
      element->UpdatePlugin();
    if (EmbeddedContentView* view = element->OwnedEmbeddedContentView())
      view->UpdateGeometry();

    // Prevent plugins from causing infinite updates of themselves.
    // FIXME: Do we really need to prevent this?
    part_update_set_.erase(&object);
  }

  return part_update_set_.empty();
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
  if (update_plugins_timer_.IsActive() || part_update_set_.empty())
    return;
  update_plugins_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void LocalFrameView::PerformPostLayoutTasks(bool visual_viewport_size_changed) {
  // FIXME: We can reach here, even when the page is not active!
  // http/tests/inspector/elements/html-link-import.html and many other
  // tests hit that case.
  // We should DCHECK(isActive()); or at least return early if we can!

  // Always called before or after performLayout(), part of the highest-level
  // layout() call.
  DCHECK(!IsInPerformLayout());
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::performPostLayoutTasks");

  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.trees"), "LayoutTree", this,
      TracedLayoutObject::Create(*GetLayoutView(), true));
  layout_count_for_testing_++;
  Document* document = GetFrame().GetDocument();
  DCHECK(document);

  UpdateDocumentDraggableRegions();
  ExecutePendingStickyUpdates();

  frame_->Selection().DidLayout();

  FontFaceSetDocument::DidLayout(*document);
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
  if (visual_viewport_size_changed && !document->Printing())
    frame_->GetDocument()->EnqueueVisualViewportResizeEvent();
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

  if (!user_scrollable_areas_)
    return;

  for (const auto& scrollable_area : user_scrollable_areas_->Values()) {
    if (!scrollable_area->ScrollbarsCanBeActive())
      continue;

    scrollable_area->ContentAreaWillPaint();
  }
}

void LocalFrameView::UpdateDocumentDraggableRegions() const {
  Document* document = frame_->GetDocument();
  if (!document->HasDraggableRegions() ||
      !frame_->GetPage()->GetChromeClient().SupportsDraggableRegions()) {
    return;
  }

  Vector<DraggableRegionValue> new_regions;
  CollectDraggableRegions(*(document->GetLayoutBox()), new_regions);
  if (new_regions == document->DraggableRegions()) {
    return;
  }

  document->SetDraggableRegions(new_regions);
  frame_->GetPage()->GetChromeClient().DraggableRegionsChanged();
}

void LocalFrameView::DidAttachDocument() {
  Page* page = frame_->GetPage();
  DCHECK(page);

  VisualViewport& visual_viewport = page->GetVisualViewport();

  if (frame_->IsMainFrame() && visual_viewport.IsActiveViewport()) {
    // If this frame is provisional it's not yet the Page's main frame. In that
    // case avoid creating a root scroller as it has Page-global effects; it
    // will be initialized when the frame becomes the Page's main frame.
    if (!frame_->IsProvisional())
      InitializeRootScroller();
  }

  if (frame_->IsMainFrame()) {
    // Allow for commits to be deferred because this is a new document.
    have_deferred_main_frame_commits_ = false;
  }
}

void LocalFrameView::InitializeRootScroller() {
  Page* page = frame_->GetPage();
  DCHECK(page);

  DCHECK_EQ(frame_, page->MainFrame());
  DCHECK(frame_->GetDocument());
  DCHECK(frame_->GetDocument()->IsActive());

  VisualViewport& visual_viewport = frame_->GetPage()->GetVisualViewport();
  DCHECK(visual_viewport.IsActiveViewport());

  ScrollableArea* layout_viewport = LayoutViewport();
  DCHECK(layout_viewport);

  // This method may be called multiple times during loading. If the root
  // scroller is already initialized this call will be a no-op.
  if (viewport_scrollable_area_)
    return;

  auto* root_frame_viewport = MakeGarbageCollected<RootFrameViewport>(
      visual_viewport, *layout_viewport);
  viewport_scrollable_area_ = root_frame_viewport;

  DCHECK(frame_->GetDocument());
  page->GlobalRootScrollerController().Initialize(*root_frame_viewport,
                                                  *frame_->GetDocument());
}

Color LocalFrameView::DocumentBackgroundColor() {
  // The LayoutView's background color is set in
  // StyleResolver::PropagateStyleToViewport(). Blend this with the base
  // background color of the LocalFrameView. This should match the color drawn
  // by ViewPainter::paintBoxDecorationBackground.
  Color result = BaseBackgroundColor();

  bool blend_with_base = true;
  LayoutObject* background_source = GetLayoutView();

  // If we have a fullscreen element grab the fullscreen color from the
  // backdrop.
  if (Document* doc = frame_->GetDocument()) {
    if (Element* element = Fullscreen::FullscreenElementFrom(*doc)) {
      if (LayoutObject* layout_object =
              element->PseudoElementLayoutObject(kPseudoIdBackdrop)) {
        background_source = layout_object;
      }
      if (doc->IsXrOverlay()) {
        // Use the fullscreened element's background directly. Don't bother
        // blending with the backdrop since that's transparent.
        blend_with_base = false;
        if (LayoutObject* layout_object = element->GetLayoutObject())
          background_source = layout_object;
      }
    }
  }

  if (!background_source)
    return result;

  Color doc_bg =
      background_source->ResolveColor(GetCSSPropertyBackgroundColor());
  if (background_source->StyleRef().ColorSchemeForced()) {
    // TODO(https://crbug.com/1351544): The DarkModeFilter operate on SkColor4f,
    // and DocumentBackgroundColor should return an SkColor4f.
    doc_bg = Color::FromSkColor4f(EnsureDarkModeFilter().InvertColorIfNeeded(
        doc_bg.toSkColor4f(), DarkModeFilter::ElementRole::kBackground));
  }
  if (blend_with_base)
    return result.Blend(doc_bg);
  return doc_bg;
}

void LocalFrameView::WillBeRemovedFromFrame() {
  if (paint_artifact_compositor_)
    paint_artifact_compositor_->WillBeRemovedFromFrame();
}

bool LocalFrameView::IsUpdatingLifecycle() const {
  LocalFrameView* root_view = GetFrame().LocalFrameRoot().View();
  DCHECK(root_view);
  return root_view->target_state_ != DocumentLifecycle::kUninitialized;
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

bool LocalFrameView::LoadAllLazyLoadedIframes() {
  bool result = false;
  ForAllChildViewsAndPlugins([&](EmbeddedContentView& view) {
    if (auto* embed = view.GetLayoutEmbeddedContent()) {
      if (auto* node = embed->GetNode()) {
        if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
          result = result || frame_owner->LoadImmediatelyIfLazy();
        }
      }
    }
  });
  return result;
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
  AllowThrottlingScope allow_throttling(*this);
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
             frame_view.IsDisplayLocked() ||
             frame_view.Lifecycle().GetState() >=
                 DocumentLifecycle::kLayoutClean);
    });
  }
#endif

  return updated;
}

bool LocalFrameView::UpdateAllLifecyclePhasesForTest() {
  bool result = UpdateAllLifecyclePhases(DocumentUpdateReason::kTest);
  RunPostLifecycleSteps();
  return result;
}

bool LocalFrameView::UpdateLifecycleToPrePaintClean(
    DocumentUpdateReason reason) {
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kPrePaintClean, reason);
}

bool LocalFrameView::UpdateLifecycleToCompositingInputsClean(
    DocumentUpdateReason reason) {
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kCompositingInputsClean, reason);
}

bool LocalFrameView::UpdateAllLifecyclePhasesExceptPaint(
    DocumentUpdateReason reason) {
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kPrePaintClean, reason);
}

void LocalFrameView::UpdateLifecyclePhasesForPrinting() {
  auto* local_frame_view_root = GetFrame().LocalFrameRoot().View();
  local_frame_view_root->UpdateLifecyclePhases(
      DocumentLifecycle::kPrePaintClean, DocumentUpdateReason::kPrinting);

  if (local_frame_view_root != this && !IsAttached()) {
    // We are printing a detached frame which is not reached above. Make sure
    // the frame is ready for painting.
    UpdateLifecyclePhases(DocumentLifecycle::kPrePaintClean,
                          DocumentUpdateReason::kPrinting);
  }
}

bool LocalFrameView::UpdateLifecycleToLayoutClean(DocumentUpdateReason reason) {
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kLayoutClean, reason);
}

LocalFrameView::InvalidationDisallowedScope::InvalidationDisallowedScope(
    const LocalFrameView& frame_view)
    : resetter_(&frame_view.GetFrame()
                     .LocalFrameRoot()
                     .View()
                     ->invalidation_disallowed_,
                true) {
  DCHECK_EQ(instance_count_, 0);
  ++instance_count_;
}

LocalFrameView::InvalidationDisallowedScope::~InvalidationDisallowedScope() {
  --instance_count_;
}

void LocalFrameView::ScheduleVisualUpdateForVisualOverflowIfNeeded() {
  LocalFrame& local_frame_root = GetFrame().LocalFrameRoot();
  // We need a full lifecycle update to recompute visual overflow if we are
  // not already targeting kPaintClean or we have already passed
  // CompositingInputs in the current frame.
  if (local_frame_root.View()->target_state_ < DocumentLifecycle::kPaintClean ||
      Lifecycle().GetState() >= DocumentLifecycle::kCompositingInputsClean) {
    // Schedule visual update to process the paint invalidation in the next
    // cycle.
    local_frame_root.ScheduleVisualUpdateUnlessThrottled();
  }
  // Otherwise the visual overflow will be updated in the compositing inputs
  // phase of this lifecycle.
}

void LocalFrameView::ScheduleVisualUpdateForPaintInvalidationIfNeeded() {
  LocalFrame& local_frame_root = GetFrame().LocalFrameRoot();
  // We need a full lifecycle update to clear pending paint invalidations.
  if (local_frame_root.View()->target_state_ < DocumentLifecycle::kPaintClean ||
      Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean) {
    // Schedule visual update to process the paint invalidation in the next
    // cycle.
    local_frame_root.ScheduleVisualUpdateUnlessThrottled();
  }
  // Otherwise the paint invalidation will be handled in the pre-paint and paint
  // phase of this full lifecycle update.
}

bool LocalFrameView::NotifyResizeObservers() {
  // Return true if lifecycles need to be re-run
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::NotifyResizeObservers");

  // Controller exists only if ResizeObserver was created.
  ResizeObserverController* resize_controller =
      ResizeObserverController::FromIfExists(*GetFrame().DomWindow());
  if (!resize_controller)
    return false;

  size_t min_depth = resize_controller->GatherObservations();

  if (min_depth != ResizeObserverController::kDepthBottom) {
    resize_controller->DeliverObservations();
  } else {
    // Observation depth limit reached
    if (resize_controller->SkippedObservations() &&
        !resize_controller->IsLoopLimitErrorDispatched()) {
      resize_controller->ClearObservations();
      ErrorEvent* error = ErrorEvent::Create(
          "ResizeObserver loop completed with undelivered notifications.",
          CaptureSourceLocation(frame_->DomWindow()), nullptr);
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

bool LocalFrameView::LocalFrameTreeAllowsThrottling() const {
  if (LocalFrameView* root_view = GetFrame().LocalFrameRoot().View())
    return root_view->allow_throttling_;
  return false;
}

bool LocalFrameView::LocalFrameTreeForcesThrottling() const {
  if (LocalFrameView* root_view = GetFrame().LocalFrameRoot().View())
    return root_view->force_throttling_;
  return false;
}

void LocalFrameView::PrepareForLifecycleUpdateRecursive() {
  // We will run lifecycle phases for LocalFrameViews that are unthrottled; or
  // are throttled but require IntersectionObserver steps to run.
  if (!ShouldThrottleRendering() ||
      intersection_observation_state_ == kRequired) {
    Lifecycle().EnsureStateAtMost(DocumentLifecycle::kVisualUpdatePending);
    ForAllChildLocalFrameViews([](LocalFrameView& child) {
      child.PrepareForLifecycleUpdateRecursive();
    });
  }
}

// TODO(leviw): We don't assert lifecycle information from documents in child
// WebPluginContainerImpls.
bool LocalFrameView::UpdateLifecyclePhases(
    DocumentLifecycle::LifecycleState target_state,
    DocumentUpdateReason reason) {
  // If the lifecycle is postponed, which can happen if the inspector requests
  // it, then we shouldn't update any lifecycle phases.
  if (frame_->GetDocument() &&
      frame_->GetDocument()->Lifecycle().LifecyclePostponed()) [[unlikely]] {
    return false;
  }

  // Prevent reentrance.
  // TODO(vmpstr): Should we just have a DCHECK instead here?
  if (IsUpdatingLifecycle()) [[unlikely]] {
    DUMP_WILL_BE_NOTREACHED()
        << "LocalFrameView::updateLifecyclePhasesInternal() reentrance";
    return false;
  }

  // This must be called from the root frame, or a detached frame for printing,
  // since it recurses down, not up. Otherwise the lifecycles of the frames
  // might be out of sync.
  DCHECK(frame_->IsLocalRoot() || !IsAttached());

  DCHECK(LocalFrameTreeAllowsThrottling() ||
         (target_state < DocumentLifecycle::kPaintClean));

  // Only the following target states are supported.
  DCHECK(target_state == DocumentLifecycle::kLayoutClean ||
         target_state == DocumentLifecycle::kCompositingInputsClean ||
         target_state == DocumentLifecycle::kPrePaintClean ||
         target_state == DocumentLifecycle::kPaintClean);

  // If the document is not active then it is either not yet initialized, or it
  // is stopping. In either case, we can't reach one of the supported target
  // states.
  if (!frame_->GetDocument()->IsActive())
    return false;

  // If we're throttling and we aren't required to run the IntersectionObserver
  // steps, then we don't need to update lifecycle phases. The throttling status
  // will get updated in RunPostLifecycleSteps().
  if (ShouldThrottleRendering() &&
      intersection_observation_state_ < kRequired) {
    return Lifecycle().GetState() == target_state;
  }

  PrepareForLifecycleUpdateRecursive();

  // This is used to guard against reentrance. It is also used in conjunction
  // with the current lifecycle state to determine which phases are yet to run
  // in this cycle. Note that this may change the return value of
  // ShouldThrottleRendering(), hence it cannot be moved before the preceeding
  // code, which relies on the prior value of ShouldThrottleRendering().
  base::AutoReset<DocumentLifecycle::LifecycleState> target_state_scope(
      &target_state_, target_state);

  lifecycle_data_.start_time = base::TimeTicks::Now();
  ++lifecycle_data_.count;

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

  std::optional<base::AutoReset<bool>> force_debug_info;
  if (reason == DocumentUpdateReason::kTest)
    force_debug_info.emplace(&paint_debug_info_enabled_, true);

  // Run the lifecycle updates.
  UpdateLifecyclePhasesInternal(target_state);

  if (target_state == DocumentLifecycle::kPaintClean) {
    TRACE_EVENT0("blink", "LocalFrameView::DidFinishLifecycleUpdate");

    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      auto lifecycle_observers = frame_view.lifecycle_observers_;
      for (auto& observer : lifecycle_observers)
        observer->DidFinishLifecycleUpdate(frame_view);
    });
    if (frame_->GetWidgetForLocalRoot() &&
        RuntimeEnabledFeatures::ReportVisibleLineBoundsEnabled()) {
      frame_->GetWidgetForLocalRoot()->UpdateLineBounds();
    }
  }

  // Hit testing metrics include the entire time processing a document update
  // in preparation for a hit test.
  if (reason == DocumentUpdateReason::kHitTest) {
    if (auto* metrics_aggregator = GetUkmAggregator()) {
      metrics_aggregator->RecordTimerSample(
          static_cast<size_t>(LocalFrameUkmAggregator::kHitTestDocumentUpdate),
          lifecycle_data_.start_time, base::TimeTicks::Now());
    }
  }

  return Lifecycle().GetState() == target_state;
}

void LocalFrameView::UpdateLifecyclePhasesInternal(
    DocumentLifecycle::LifecycleState target_state) {
  // TODO(https://crbug.com/1196853): Switch to ScriptForbiddenScope once
  // failures are fixed.
  BlinkLifecycleScopeWillBeScriptForbidden forbid_script;

  // RunScrollSnapshotClientSteps must not run more than once.
  bool should_run_scroll_snapshot_client_steps = true;

  // Run style, layout, compositing and prepaint lifecycle phases and deliver
  // resize observations if required. Resize observer callbacks/delegates have
  // the potential to dirty layout (until loop limit is reached) and therefore
  // the above lifecycle phases need to be re-run until the limit is reached
  // or no layout is pending.
  // Note that after ResizeObserver has settled, we also run intersection
  // observations that need to be delievered in post-layout. This process can
  // also dirty layout, which will run this loop again.

  // A LocalFrameView can be unthrottled at this point, but become throttled as
  // it advances through lifecycle stages. If that happens, it will prevent
  // subsequent passes through the loop from updating the newly-throttled views.
  // To avoid that, we lock in the set of unthrottled views before entering the
  // loop.
  HeapVector<Member<LocalFrameView>> unthrottled_frame_views;
  ForAllNonThrottledLocalFrameViews(
      [&unthrottled_frame_views](LocalFrameView& frame_view) {
        unthrottled_frame_views.push_back(&frame_view);
      });

  while (true) {
    for (LocalFrameView* frame_view : unthrottled_frame_views) {
      // RunResizeObserverSteps may run arbitrary script, which can cause a
      // frame to become detached.
      if (frame_view->GetFrame().IsAttached()) {
        frame_view->Lifecycle().EnsureStateAtMost(
            DocumentLifecycle::kVisualUpdatePending);
      }
    }
    bool run_more_lifecycle_phases =
        RunStyleAndLayoutLifecyclePhases(target_state);
    if (!run_more_lifecycle_phases)
      return;
    DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean);

    // ScrollSnapshotClients may be associated with scrollers that never had a
    // chance to get a layout box at the time style was calculated; when this
    // situation happens, RunScrollTimelineSteps will re-snapshot all affected
    // clients and dirty style for associated effect targets.
    //
    // https://github.com/w3c/csswg-drafts/issues/5261
    if (should_run_scroll_snapshot_client_steps) {
      should_run_scroll_snapshot_client_steps = false;
      bool needs_to_repeat_lifecycle = RunScrollSnapshotClientSteps();
      if (needs_to_repeat_lifecycle)
        continue;
    }

    if (!GetLayoutView())
      return;

    {
      // We need scoping braces here because this
      // DisallowLayoutInvalidationScope is meant to be in effect during
      // pre-paint, but not during ResizeObserver or ViewTransition.
#if DCHECK_IS_ON()
      DisallowLayoutInvalidationScope disallow_layout_invalidation(this);
#endif

      DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT_WITH_CATEGORIES(
          TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "SetLayerTreeId",
          inspector_set_layer_tree_id::Data, frame_.Get());
      // The Compositing Inputs lifecycle phase should be integrated into the
      // PrePaint lifecycle phase in the future. The difference between these
      // two stages is not relevant to web developers, so include them both
      // under PrePaint.
      DEVTOOLS_TIMELINE_TRACE_EVENT("PrePaint", inspector_pre_paint_event::Data,
                                    frame_.Get());
      run_more_lifecycle_phases =
          RunCompositingInputsLifecyclePhase(target_state);
      if (!run_more_lifecycle_phases)
        return;

      run_more_lifecycle_phases = RunPrePaintLifecyclePhase(target_state);
    }

    if (!run_more_lifecycle_phases) {
      // If we won't be proceeding to paint, update view transition stylesheet
      // here.
      bool needs_to_repeat_lifecycle = RunViewTransitionSteps(target_state);
      if (needs_to_repeat_lifecycle)
        continue;
    }

      DCHECK(ShouldThrottleRendering() ||
             Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean);
      if (ShouldThrottleRendering() || !run_more_lifecycle_phases)
        return;

    // Some features may require several passes over style and layout
    // within the same lifecycle update.
    bool needs_to_repeat_lifecycle = false;

    // ResizeObserver and post-layout IntersectionObserver observation
    // deliveries may dirty style and layout. RunResizeObserverSteps will return
    // true if any observer ran that may have dirtied style or layout;
    // RunPostLayoutIntersectionObserverSteps will return true if any
    // observations led to content-visibility intersection changing visibility
    // state synchronously (which happens on the first intersection
    // observeration of a context).
    //
    // Note that we run the content-visibility intersection observation first.
    // The idea is that we want to synchronously determine the initial,
    // first-time-rendered state of on- or off-screen `content-visibility:
    // auto` subtrees before dispatching any kind of resize observations,
    // including the contain-intrinsic-size resize observer. If we repeat the
    // lifecycle here or in the resize observer, the second observation will be
    // asynchronous and will always defer posting observations. This is
    // contrasted with the alternative in which both resize observer and
    // intersection observer can repeat the lifecycle causing another resize
    // observer call to now see different sizes and in the worst case issue a
    // console error and schedule an additional frame of work.
    needs_to_repeat_lifecycle = RunPostLayoutIntersectionObserverSteps();
    if (needs_to_repeat_lifecycle)
      continue;

    {
      ScriptForbiddenScope::AllowUserAgentScript allow_script;
      base::AutoReset<DocumentLifecycle::LifecycleState> saved_target_state(
          &target_state_, DocumentLifecycle::kUninitialized);
      needs_to_repeat_lifecycle = RunResizeObserverSteps(target_state);
    }
    // Only run the rest of the steps here if resize observer is done.
    if (needs_to_repeat_lifecycle)
      continue;

    // ViewTransition mutates the tree and mirrors post layout transform for
    // transitioning elements to UA created elements. This may dirty
    // style/layout requiring another lifecycle update.
    needs_to_repeat_lifecycle = RunViewTransitionSteps(target_state);
    if (!needs_to_repeat_lifecycle)
      break;
  }

  // This must be after all other updates for position-visibility.
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.frame_->CheckPositionAnchorsForChainedVisibilityChanges();
  });

  // Once we exit the ResizeObserver / IntersectionObserver loop above, we need
  // to clear the resize observer limits so that next time we run this, we can
  // deliver more observations.
  ClearResizeObserverLimit();

  // Layout invalidation scope was disabled for resize observer
  // re-enable it for subsequent steps
#if DCHECK_IS_ON()
  DisallowLayoutInvalidationScope disallow_layout_invalidation(this);
#endif

  // This needs to be done prior to paint: it will update the cc::Layer bounds
  // for the remote frame views, which will be wrapped during paint in
  // ForeignLayerDisplayItem's whose visual rect is set at construction based
  // on cc::Layer bounds.
  ForAllRemoteFrameViews(
      [](RemoteFrameView& frame_view) { frame_view.UpdateCompositingRect(); });

  DCHECK_EQ(target_state, DocumentLifecycle::kPaintClean);
  RunPaintLifecyclePhase(PaintBenchmarkMode::kNormal);
  DCHECK(ShouldThrottleRendering() || AnyFrameIsPrintingOrPaintingPreview() ||
         Lifecycle().GetState() == DocumentLifecycle::kPaintClean);
}

bool LocalFrameView::RunScrollSnapshotClientSteps() {
  // TODO(crbug.com/1329159): Determine if the source for a view timeline has
  // changed, which may in turn require a fresh style/layout cycle.

  DCHECK_GE(Lifecycle().GetState(), DocumentLifecycle::kLayoutClean);
  bool re_run_lifecycles = false;
  ForAllNonThrottledLocalFrameViews(
      [&re_run_lifecycles](LocalFrameView& frame_view) {
        bool valid = frame_view.GetFrame().ValidateScrollSnapshotClients();
        re_run_lifecycles |= !valid;
      });
  return re_run_lifecycles;
}

bool LocalFrameView::RunViewTransitionSteps(
    DocumentLifecycle::LifecycleState target_state) {
  DCHECK(frame_ && frame_->GetDocument());
  DCHECK(frame_->IsLocalRoot() || !IsAttached());

  if (target_state < DocumentLifecycle::kPrePaintClean)
    return false;

  bool re_run_lifecycle = false;
  ForAllNonThrottledLocalFrameViews(
      [&re_run_lifecycle, target_state](LocalFrameView& frame_view) {
        const auto* document = frame_view.GetFrame().GetDocument();
        if (!document)
          return;

        DCHECK_GE(document->Lifecycle().GetState(),
                  DocumentLifecycle::kPrePaintClean);
        auto* transition = ViewTransitionUtils::GetTransition(*document);
        if (!transition)
          return;

        if (target_state == DocumentLifecycle::kPaintClean)
          transition->RunViewTransitionStepsDuringMainFrame();
        else
          transition->RunViewTransitionStepsOutsideMainFrame();

        re_run_lifecycle |= document->Lifecycle().GetState() <
                                DocumentLifecycle::kPrePaintClean ||
                            frame_view.NeedsLayout();
      });

  return re_run_lifecycle;
}

bool LocalFrameView::RunResizeObserverSteps(
    DocumentLifecycle::LifecycleState target_state) {
  if (target_state != DocumentLifecycle::kPaintClean)
    return false;

  for (auto& element : disconnected_elements_with_remembered_size_) {
    if (!element->isConnected()) {
      element->SetLastRememberedBlockSize(std::nullopt);
      element->SetLastRememberedInlineSize(std::nullopt);
    }
  }
  disconnected_elements_with_remembered_size_.clear();

  // https://drafts.csswg.org/css-anchor-position-1/#last-successful-position-option
  bool re_run_lifecycles = UpdateLastSuccessfulPositionFallbacks();

  ForAllNonThrottledLocalFrameViews(
      [&re_run_lifecycles](LocalFrameView& frame_view) {
        bool result = frame_view.NotifyResizeObservers();
        re_run_lifecycles = re_run_lifecycles || result;
      });
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

bool LocalFrameView::ShouldDeferLayoutSnap() const {
  // Scrollers that are snap containers normally need to re-snap after layout
  // changes, but we defer the snap until the user is done scrolling to avoid
  // fighting with snap animations on the compositor thread.
  if (auto* web_frame = WebLocalFrameImpl::FromFrame(frame_)) {
    if (auto* widget = web_frame->LocalRootFrameWidget()) {
      return widget->IsScrollGestureActive();
    }
  }
  return false;
}

void LocalFrameView::EnqueueScrollSnapChangingFromImplIfNecessary() {
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    const auto* scrollable_areas = frame_view.UserScrollableAreas();
    if (!scrollable_areas) {
      return;
    }
    for (const auto& area : scrollable_areas->Values()) {
      area->EnqueueScrollSnapChangingEventFromImplIfNeeded();
    }
  });
}

bool LocalFrameView::RunStyleAndLayoutLifecyclePhases(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::RunStyleAndLayoutLifecyclePhases");
  UpdateStyleAndLayoutIfNeededRecursive();
  DCHECK(ShouldThrottleRendering() ||
         Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean);
  if (Lifecycle().GetState() < DocumentLifecycle::kLayoutClean)
    return false;

  // PerformRootScrollerSelection can dirty layout if an effective root
  // scroller is changed so make sure we get back to LayoutClean.
  if (frame_->GetDocument()
          ->GetRootScrollerController()
          .PerformRootScrollerSelection() &&
      RuntimeEnabledFeatures::ImplicitRootScrollerEnabled()) {
    UpdateStyleAndLayoutIfNeededRecursive();
  }

  if (target_state == DocumentLifecycle::kLayoutClean)
    return false;

  // Now we can run post layout steps in preparation for further phases.
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.PerformScrollAnchoringAdjustments();
  });

  ExecutePendingSnapUpdates();

  // Fire scrollsnapchanging events based on the new layout if necessary.
  EnqueueScrollSnapChangingFromImplIfNecessary();

  EnqueueScrollEvents();

  frame_->GetPage()->GetValidationMessageClient().LayoutOverlay();

  if (target_state == DocumentLifecycle::kPaintClean) {
    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      frame_view.NotifyFrameRectsChangedIfNeeded();
    });
  }

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    auto lifecycle_observers = frame_view.lifecycle_observers_;
    for (auto& observer : lifecycle_observers) {
      observer->DidFinishLayout();
    }
  });

  return Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean;
}

bool LocalFrameView::RunCompositingInputsLifecyclePhase(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::RunCompositingInputsLifecyclePhase");
  auto* layout_view = GetLayoutView();
  DCHECK(layout_view);

  SCOPED_UMA_AND_UKM_TIMER(GetUkmAggregator(),
                           LocalFrameUkmAggregator::kCompositingInputs);
  // TODO(pdr): This descendant dependent treewalk should be integrated into
  // the prepaint tree walk.
  {
#if DCHECK_IS_ON()
    SetIsUpdatingDescendantDependentFlags(true);
#endif
    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      frame_view.Lifecycle().AdvanceTo(
          DocumentLifecycle::kInCompositingInputsUpdate);

      // Validate all HighlightMarkers of all non-throttled LocalFrameViews
      // before compositing inputs phase so the nodes affected by markers
      // removed/added are invalidated (for both visual overflow and repaint)
      // and then painted during this lifecycle.
      if (LocalDOMWindow* window = frame_view.GetFrame().DomWindow()) {
        if (HighlightRegistry* highlight_registry =
                window->Supplementable<LocalDOMWindow>::RequireSupplement<
                    HighlightRegistry>()) {
          highlight_registry->ValidateHighlightMarkers();
        }
      }

      frame_view.GetLayoutView()->CommitPendingSelection();
      frame_view.GetLayoutView()->Layer()->UpdateDescendantDependentFlags();
    });
#if DCHECK_IS_ON()
    SetIsUpdatingDescendantDependentFlags(false);
#endif
  }

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(
        DocumentLifecycle::kCompositingInputsClean);
  });

  return target_state > DocumentLifecycle::kCompositingInputsClean;
}

bool LocalFrameView::RunPrePaintLifecyclePhase(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::RunPrePaintLifecyclePhase");

  ForAllNonThrottledLocalFrameViews(
      [](LocalFrameView& frame_view) {
        frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPrePaint);

        // Propagate dirty bits in the frame into the parent frame so that
        // pre-paint reaches into this frame.
        if (LayoutView* layout_view = frame_view.GetLayoutView()) {
          if (auto* owner = frame_view.GetFrame().OwnerLayoutObject()) {
            if (layout_view->NeedsPaintPropertyUpdate() ||
                layout_view->DescendantNeedsPaintPropertyUpdate()) {
              owner->SetDescendantNeedsPaintPropertyUpdate();
            }
            if (layout_view->ShouldCheckForPaintInvalidation()) {
              owner->SetShouldCheckForPaintInvalidation();
            }
            if (layout_view->EffectiveAllowedTouchActionChanged() ||
                layout_view->DescendantEffectiveAllowedTouchActionChanged()) {
              owner->MarkDescendantEffectiveAllowedTouchActionChanged();
            }
            if (layout_view->BlockingWheelEventHandlerChanged() ||
                layout_view->DescendantBlockingWheelEventHandlerChanged()) {
              owner->MarkDescendantBlockingWheelEventHandlerChanged();
            }
          }
        }
      },
      // Use post-order to ensure correct flag propagation for nested frames.
      kPostOrder);

  {
    SCOPED_UMA_AND_UKM_TIMER(GetUkmAggregator(),
                             LocalFrameUkmAggregator::kPrePaint);

    GetPage()->GetLinkHighlight().UpdateBeforePrePaint();
    PrePaintTreeWalk().WalkTree(*this);
    GetPage()->GetLinkHighlight().UpdateAfterPrePaint();

    frame_->GetPage()->GetValidationMessageClient().UpdatePrePaint();
    ForAllNonThrottledLocalFrameViews([](LocalFrameView& view) {
      view.frame_->UpdateFrameColorOverlayPrePaint();
      view.frame_->CheckPositionAnchorsForCssVisibilityChanges();
    });
    if (auto* web_local_frame_impl = WebLocalFrameImpl::FromFrame(frame_))
      web_local_frame_impl->UpdateDevToolsOverlaysPrePaint();
  }

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPrePaintClean);
  });

  return target_state > DocumentLifecycle::kPrePaintClean;
}

bool LocalFrameView::AnyFrameIsPrintingOrPaintingPreview() {
  bool any_frame_is_printing_or_painting_preview = false;
  ForAllNonThrottledLocalFrameViews(
      [&any_frame_is_printing_or_painting_preview](LocalFrameView& frame_view) {
        if (frame_view.GetFrame().GetDocument()->IsPrintingOrPaintingPreview())
          any_frame_is_printing_or_painting_preview = true;
      });
  return any_frame_is_printing_or_painting_preview;
}

void LocalFrameView::RunPaintLifecyclePhase(PaintBenchmarkMode benchmark_mode) {
  DCHECK(ScriptForbiddenScope::WillBeScriptForbidden());
  DCHECK(LocalFrameTreeAllowsThrottling());
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::RunPaintLifecyclePhase");
  // While printing or capturing a paint preview of a document, the paint walk
  // is done into a special canvas. There is no point doing a normal paint step
  // (or animations update) when in this mode.
  if (AnyFrameIsPrintingOrPaintingPreview())
    return;

  bool needed_update;
  {
    // paint_controller will be constructed when PaintTree repaints, and will
    // be destructed after PushPaintArtifactToCompositor.
    std::optional<PaintController> paint_controller;
    PaintTree(benchmark_mode, paint_controller);

    if (paint_artifact_compositor_ &&
        benchmark_mode ==
            PaintBenchmarkMode::kForcePaintArtifactCompositorUpdate) {
      paint_artifact_compositor_->SetNeedsUpdate();
    }
    needed_update = !paint_artifact_compositor_ ||
                    paint_artifact_compositor_->NeedsUpdate();
    PushPaintArtifactToCompositor(paint_controller.has_value());
  }

  size_t total_animations_count = 0;
  ForAllNonThrottledLocalFrameViews(
      [this, needed_update,
       &total_animations_count](LocalFrameView& frame_view) {
        if (auto* scrollable_area = frame_view.GetScrollableArea())
          scrollable_area->UpdateCompositorScrollAnimations();
        if (const auto* animating_scrollable_areas =
                frame_view.AnimatingScrollableAreas()) {
          for (PaintLayerScrollableArea* area : *animating_scrollable_areas)
            area->UpdateCompositorScrollAnimations();
        }
        frame_view.GetPage()->GetLinkHighlight().UpdateAfterPaint(
            paint_artifact_compositor_.Get());
        Document& document = frame_view.GetLayoutView()->GetDocument();
        // Attach the compositor timeline during the commit as it blocks on
        // the previous commit completion.
        document.AttachCompositorTimeline(
            document.Timeline().CompositorTimeline());
        {
          // Updating animations can notify ready promises which could mutate
          // the DOM. We should delay these until we have finished the lifecycle
          // update. https://crbug.com/1196781
          ScriptForbiddenScope forbid_script;
          document.GetDocumentAnimations().UpdateAnimations(
              DocumentLifecycle::kPaintClean, paint_artifact_compositor_.Get(),
              needed_update);
        }
        total_animations_count +=
            document.GetDocumentAnimations().GetAnimationsCount();
      });

  // If this is a throttled local root, then we shouldn't run animation steps
  // below, because the cc animation data structures might not even exist.
  if (frame_->IsLocalRoot() && ShouldThrottleRendering())
    return;

  if (auto* animation_host = GetCompositorAnimationHost()) {
    animation_host->SetAnimationCounts(total_animations_count);
  }

  // Initialize animation properties in the newly created paint property
  // nodes according to the current animation state. This is mainly for
  // the running composited animations which didn't change state during
  // above UpdateAnimations() but associated with new paint property nodes.
  if (needed_update) {
    auto* root_layer = RootCcLayer();
    if (root_layer && root_layer->layer_tree_host()) {
      root_layer->layer_tree_host()->mutator_host()->InitClientAnimationState();
    }
  }

  if (GetPage())
    GetPage()->Animator().ReportFrameAnimations(GetCompositorAnimationHost());
}

void LocalFrameView::RunAccessibilitySteps() {
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::RunAccessibilitySteps");

  SCOPED_UMA_AND_UKM_TIMER(GetUkmAggregator(),
                           LocalFrameUkmAggregator::kAccessibility);

  // Reduce redundant ancestor chain walking for display lock computations.
  auto display_lock_memoization_scope =
      DisplayLockUtilities::CreateLockCheckMemoizationScope();

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    if (AXObjectCache* cache = frame_view.ExistingAXObjectCache()) {
      cache->CommitAXUpdates(*frame_view.GetFrame().GetDocument(),
                             /*force=*/false);
    }
  });
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
      // The CSS scroll-start property should take precedence over scroll
      // anchoring.
      if (scroller->IsApplyingScrollStart()) {
        scroller->GetScrollAnchor()->CancelAdjustment();
        continue;
      }
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

void LocalFrameView::PaintTree(
    PaintBenchmarkMode benchmark_mode,
    std::optional<PaintController>& paint_controller) {
  SCOPED_UMA_AND_UKM_TIMER(GetUkmAggregator(), LocalFrameUkmAggregator::kPaint);

  DCHECK(GetFrame().IsLocalRoot());

  std::optional<MobileFriendlinessChecker::PaintScope> mf_scope;
  if (mobile_friendliness_checker_)
    mf_scope.emplace(*mobile_friendliness_checker_);

  auto* layout_view = GetLayoutView();
  DCHECK(layout_view);

  CullRectUpdater(*layout_view->Layer()).Update();

  bool debug_info_newly_enabled =
      UpdatePaintDebugInfoEnabled() && PaintDebugInfoEnabled();

  paint_frame_count_++;
  ForAllNonThrottledLocalFrameViews(
      [debug_info_newly_enabled](LocalFrameView& frame_view) {
        frame_view.MarkFirstEligibleToPaint();
        frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
        // Propagate child frame PaintLayer NeedsRepaint flag into the owner
        // frame.
        if (auto* frame_layout_view = frame_view.GetLayoutView()) {
          if (auto* owner = frame_view.GetFrame().OwnerLayoutObject()) {
            PaintLayer* frame_root_layer = frame_layout_view->Layer();
            DCHECK(frame_root_layer);
            DCHECK(owner->Layer());
            if (frame_root_layer->SelfOrDescendantNeedsRepaint())
              owner->Layer()->SetDescendantNeedsRepaint();
          }
          // If debug info was just enabled, then the paint cache won't have any
          // debug info; we need to force a full repaint to generate it.
          if (debug_info_newly_enabled)
            frame_layout_view->InvalidatePaintForViewAndDescendants();
        }
      },
      // Use post-order to ensure correct flag propagation for nested frames.
      kPostOrder);

  ForAllThrottledLocalFrameViews(
      [](LocalFrameView& frame_view) { frame_view.MarkIneligibleToPaint(); });

  bool needs_clear_repaint_flags = false;

  if (benchmark_mode >= PaintBenchmarkMode::kForcePaint ||
      !paint_controller_persistent_data_ ||
      GetLayoutView()->Layer()->SelfOrDescendantNeedsRepaint() ||
      visual_viewport_or_overlay_needs_repaint_) {
    const PaintArtifact& previous_artifact =
        EnsurePaintControllerPersistentData().GetPaintArtifact();
    paint_controller.emplace(PaintDebugInfoEnabled(),
                             paint_controller_persistent_data_.Get(),
                             benchmark_mode);
    GraphicsContext graphics_context(*paint_controller);

    // Draw the WebXR DOM overlay if present.
    if (PaintLayer* full_screen_layer = GetXROverlayLayer()) {
      PaintLayerPainter(*full_screen_layer).Paint(graphics_context);
    } else {
      PaintFrame(graphics_context);

      GetPage()->GetValidationMessageClient().PaintOverlay(graphics_context);
      ForAllNonThrottledLocalFrameViews(
          [&graphics_context](LocalFrameView& view) {
            view.frame_->PaintFrameColorOverlay(graphics_context);
          });

      // Devtools overlays query the inspected page's paint data so this
      // update needs to be after other paintings.
      if (auto* web_local_frame_impl = WebLocalFrameImpl::FromFrame(frame_))
        web_local_frame_impl->PaintDevToolsOverlays(graphics_context);

      if (frame_->IsMainFrame())
        GetPage()->GetVisualViewport().Paint(graphics_context);
    }

    // Link highlights paint after all other paintings.
    GetPage()->GetLinkHighlight().Paint(graphics_context);

    paint_controller->CommitNewDisplayItems();

    needs_clear_repaint_flags = true;
    if (paint_artifact_compositor_) {
      paint_artifact_compositor_->SetNeedsFullUpdateAfterPaintIfNeeded(
          previous_artifact,
          paint_controller_persistent_data_->GetPaintArtifact());
    }
  }

  visual_viewport_or_overlay_needs_repaint_ = false;

  ForAllNonThrottledLocalFrameViews(
      [needs_clear_repaint_flags](LocalFrameView& frame_view) {
        frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
        if (needs_clear_repaint_flags) {
          if (auto* layout_view = frame_view.GetLayoutView())
            layout_view->Layer()->ClearNeedsRepaintRecursively();
        }
        frame_view.GetPaintTimingDetector().NotifyPaintFinished();
      });
}

cc::Layer* LocalFrameView::RootCcLayer() {
  return paint_artifact_compositor_ ? paint_artifact_compositor_->RootLayer()
                                    : nullptr;
}

const cc::Layer* LocalFrameView::RootCcLayer() const {
  return const_cast<LocalFrameView*>(this)->RootCcLayer();
}

void LocalFrameView::CreatePaintTimelineEvents() {
  if (const cc::Layer* root_layer = paint_artifact_compositor_->RootLayer()) {
    for (const auto& layer : root_layer->children()) {
      if (!layer->update_rect().IsEmpty()) {
        DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT_WITH_CATEGORIES(
            "devtools.timeline,rail", "Paint", inspector_paint_event::Data,
            &GetFrame(), /*layout_object=*/nullptr,
            GetQuadForTimelinePaintEvent(layer), layer->id());
      }
    }
  }
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
    paint_artifact_compositor_ = MakeGarbageCollected<PaintArtifactCompositor>(
        page->GetScrollingCoordinator()->GetScrollCallbacks());
    page->GetChromeClient().AttachRootLayer(
        paint_artifact_compositor_->RootLayer(), &GetFrame());
  }

  paint_artifact_compositor_->SetLCDTextPreference(
      page->GetSettings().GetLCDTextPreference());

  SCOPED_UMA_AND_UKM_TIMER(GetUkmAggregator(),
                           LocalFrameUkmAggregator::kCompositingCommit);
  DEVTOOLS_TIMELINE_TRACE_EVENT("Layerize", inspector_layerize_event::Data,
                                frame_.Get());

  // Skip updating property trees, pushing cc::Layers, and issuing raster
  // invalidations if possible.
  if (!paint_artifact_compositor_->NeedsUpdate()) {
    if (repainted) {
      paint_artifact_compositor_->UpdateRepaintedLayers(
          paint_controller_persistent_data_->GetPaintArtifact());
      CreatePaintTimelineEvents();
    }
    // TODO(pdr): Should we clear the property tree state change bits (
    // |PaintArtifactCompositor::ClearPropertyTreeChangedState|)?
    return;
  }

  paint_artifact_compositor_->SetLayerDebugInfoEnabled(
      paint_debug_info_enabled_);

  PaintArtifactCompositor::ViewportProperties viewport_properties;
  if (const auto& viewport = page->GetVisualViewport();
      GetFrame().IsMainFrame() && viewport.IsActiveViewport()) {
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

  PaintArtifactCompositor::StackScrollTranslationVector
      scroll_translation_nodes;
  ForAllNonThrottledLocalFrameViews(
      [&scroll_translation_nodes](LocalFrameView& frame_view) {
        if (const auto* scrollable_areas = frame_view.UserScrollableAreas()) {
          for (const auto& area : scrollable_areas->Values()) {
            const auto* paint_properties =
                area->GetLayoutBox()->FirstFragment().PaintProperties();
            if (paint_properties && paint_properties->Scroll()) {
              scroll_translation_nodes.push_back(
                  paint_properties->ScrollTranslation());
            }
          }
        }
      });

  WTF::Vector<std::unique_ptr<ViewTransitionRequest>> view_transition_requests;
  AppendViewTransitionRequests(view_transition_requests);

  paint_artifact_compositor_->Update(
      paint_controller_persistent_data_->GetPaintArtifact(),
      viewport_properties, scroll_translation_nodes,
      std::move(view_transition_requests));

  CreatePaintTimelineEvents();
}

void LocalFrameView::AppendViewTransitionRequests(
    WTF::Vector<std::unique_ptr<ViewTransitionRequest>>& requests) {
  DCHECK(frame_ && frame_->GetDocument());
  DCHECK(frame_->IsLocalRoot());

  ForAllNonThrottledLocalFrameViews([&requests](LocalFrameView& frame_view) {
    if (!frame_view.GetFrame().GetDocument())
      return;

    auto pending_requests = ViewTransitionUtils::GetPendingRequests(
        *frame_view.GetFrame().GetDocument());
    for (auto& pending_request : pending_requests)
      requests.push_back(std::move(pending_request));
  });
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

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("blink.debug"),
               "LocalFrameView::updateStyleAndLayoutIfNeededRecursive");

  UpdateStyleAndLayout();

  // WebView plugins need to update regardless of whether the
  // LayoutEmbeddedObject that owns them needed layout.
  // TODO(rendering-core) This currently runs the entire lifecycle on plugin
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
  frame_->GetDocument()->GetLayoutView()->AssertFragmentTree();
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

void LocalFrameView::UpdateStyleAndLayout() {
#if DCHECK_IS_ON()
  DCHECK(!is_updating_layout_);
  base::AutoReset<bool> is_updating_layout(&is_updating_layout_, true);
#endif
  TRACE_EVENT("blink", "LocalFrameView::UpdateStyleAndLayout");

  if (IsInPerformLayout() || ShouldThrottleRendering() ||
      !frame_->GetDocument()->IsActive() || frame_->IsProvisional() ||
      Lifecycle().LifecyclePostponed()) {
    return;
  }

  gfx::Size visual_viewport_size =
      GetScrollableArea()->VisibleContentRect().size();

  bool did_layout = false;
  {
    // Script is allowed during the initial style and layout as we will rerun
    // at least once more if anything was invalidated.
    ScriptForbiddenScope::AllowUserAgentScript allow_script;
    did_layout = UpdateStyleAndLayoutInternal();
  }

  // Update counters after layout since counters may be added during layout for
  // generated ::scroll-markers.
  frame_->GetDocument()->GetStyleEngine().UpdateCounters();

  // Second pass: run autosize until it stabilizes
  if (auto_size_info_) {
    while (auto_size_info_->AutoSizeIfNeeded())
      did_layout |= UpdateStyleAndLayoutInternal();
    auto_size_info_->Clear();
  }

  // Third pass: if layout hasn't stabilized, don't update layout viewport size
  // based on content size.
  if (NeedsLayout()) {
    base::AutoReset<bool> suppress(&suppress_adjust_view_size_, true);
    did_layout |= UpdateStyleAndLayoutInternal();
  }

#if DCHECK_IS_ON()
  if (!Lifecycle().LifecyclePostponed() && !ShouldThrottleRendering()) {
    DCHECK(!frame_->GetDocument()->NeedsLayoutTreeUpdate());
    CheckDoesNotNeedLayout();
    DCHECK(layout_subtree_root_list_.IsEmpty());
    if (did_layout)
      GetLayoutView()->AssertSubtreeIsLaidOut();
  }
#endif

  // Once all of the layout is finished, update the focused element. This
  // shouldn't be done before since focusability check sometimes requires an
  // layout update, which would recurse into this function. This update is only
  // required if we still need layout though, which should be cleared already.
  frame_->GetDocument()->ClearFocusedElementIfNeeded();

  if (did_layout) {
    gfx::Size new_visual_viewport_size =
        GetScrollableArea()->VisibleContentRect().size();
    bool visual_viewport_size_changed =
        (new_visual_viewport_size != visual_viewport_size);
    SetNeedsUpdateGeometries();
    PerformPostLayoutTasks(visual_viewport_size_changed);
    GetFrame().GetDocument()->LayoutUpdated();
  }
  UpdateGeometriesIfNeeded();
}

bool LocalFrameView::UpdateStyleAndLayoutInternal() {
  PostStyleUpdateScope post_style_update_scope(*frame_->GetDocument());

  bool layout_updated = false;

  do {
    {
      frame_->GetDocument()->UpdateStyleAndLayoutTreeForThisDocument();

      // Update style for all embedded SVG documents underneath this frame, so
      // that intrinsic size computation for any embedded objects has up-to-date
      // information before layout.
      ForAllChildLocalFrameViews([](LocalFrameView& view) {
        Document& document = *view.GetFrame().GetDocument();
        if (document.IsSVGDocument()) {
          document.UpdateStyleAndLayoutTreeForThisDocument();
        }
      });
    }

    UpdateCanCompositeBackgroundAttachmentFixed();

    if (NeedsLayout()) {
      SCOPED_UMA_AND_UKM_TIMER(GetUkmAggregator(),
                               LocalFrameUkmAggregator::kLayout);
      UpdateLayout();
      layout_updated = true;
    }
  } while (post_style_update_scope.Apply());

  return layout_updated;
}

void LocalFrameView::EnableAutoSizeMode(const gfx::Size& min_size,
                                        const gfx::Size& max_size) {
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

void LocalFrameView::ForceLayoutForPagination(float maximum_shrink_factor) {
  pagination_state_ = MakeGarbageCollected<PaginationState>();

  LayoutView* layout_view = GetLayoutView();
  if (!layout_view) {
    return;
  }

  Document& document = *frame_->GetDocument();
  auto LayoutForPrinting = [&layout_view, &document]() {
    document.GetStyleEngine().UpdateViewportSize();
    document.MarkViewportUnitsDirty();
    layout_view->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
        layout_invalidation_reason::kPrintingChanged);
    document.UpdateStyleAndLayout(DocumentUpdateReason::kPrinting);
  };

  // Need to update computed style before we can set the initial containing
  // block size. A zoom factor may have been set, and it shouldn't be applied
  // when printing, e.g. when resolving @page margins.
  document.UpdateStyleAndLayoutTree();

  // Set up the initial containing block size for pagination. This is defined as
  // the page area size of the *first* page. [1] The size of the first page may
  // not be fully known yet, e.g. if the first page is named [2] and given a
  // specific size. Page names are resolved during layout. For now, set an
  // initial containing block size based on the information that's currently
  // available. If this turns out to be wrong, we need to set a new size and lay
  // out again. See below.
  //
  // [1] https://www.w3.org/TR/css-page-3/#page-model
  // [2] https://www.w3.org/TR/css-page-3/#using-named-pages
  PhysicalSize initial_containing_block_size =
      CalculateInitialContainingBlockSizeForPagination(document);
  layout_view->SetInitialContainingBlockSizeForPrinting(
      initial_containing_block_size);

  LayoutForPrinting();

  PhysicalSize new_initial_containing_block_size =
      CalculateInitialContainingBlockSizeForPagination(document);
  if (new_initial_containing_block_size != initial_containing_block_size) {
    // If the first page was named (this isn't something we can detect without
    // laying out first), and the size of the first page is different from what
    // we got above, the initial containing block used was wrong (which affects
    // e.g. elements with viewport units). Set a new size and lay out again.
    layout_view->SetInitialContainingBlockSizeForPrinting(
        new_initial_containing_block_size);

    LayoutForPrinting();
  }

  // If we don't fit in the given page width, we'll lay out again. If we don't
  // fit in the page width when shrunk, we will lay out at maximum shrink and
  // clip extra content.
  // FIXME: We are assuming a shrink-to-fit printing implementation. A cropping
  // implementation should not do this!
  float overall_scale_factor =
      CalculateOverflowShrinkForPrinting(*layout_view, maximum_shrink_factor);

  if (overall_scale_factor > 1.0) {
    // Re-layout and apply the same scale factor to all pages.
    // PaginationScaleFactor() has already been set to honor any scale factor
    // from print settings. That has to be included as well.
    layout_view->SetPaginationScaleFactor(layout_view->PaginationScaleFactor() *
                                          overall_scale_factor);
    PhysicalSize new_size =
        CalculateInitialContainingBlockSizeForPagination(document);
    layout_view->SetInitialContainingBlockSizeForPrinting(new_size);
    LayoutForPrinting();
  }

  if (TextAutosizer* text_autosizer = document.GetTextAutosizer()) {
    text_autosizer->UpdatePageInfo();
  }
  AdjustViewSize();
  UpdateStyleAndLayout();
}

void LocalFrameView::DestroyPaginationLayout() {
  if (!pagination_state_) {
    return;
  }
  pagination_state_->DestroyAnonymousPageLayoutObjects();
  pagination_state_ = nullptr;
}

gfx::Rect LocalFrameView::RootFrameToDocument(
    const gfx::Rect& rect_in_root_frame) {
  gfx::Point offset = RootFrameToDocument(rect_in_root_frame.origin());
  gfx::Rect local_rect = rect_in_root_frame;
  local_rect.set_origin(offset);
  return local_rect;
}

gfx::Point LocalFrameView::RootFrameToDocument(
    const gfx::Point& point_in_root_frame) {
  return gfx::ToFlooredPoint(
      RootFrameToDocument(gfx::PointF(point_in_root_frame)));
}

gfx::PointF LocalFrameView::RootFrameToDocument(
    const gfx::PointF& point_in_root_frame) {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return point_in_root_frame;

  gfx::PointF local_frame = ConvertFromRootFrame(point_in_root_frame);
  return local_frame + layout_viewport->GetScrollOffset();
}

gfx::Rect LocalFrameView::DocumentToFrame(
    const gfx::Rect& rect_in_document) const {
  gfx::Rect rect_in_frame = rect_in_document;
  rect_in_frame.set_origin(DocumentToFrame(rect_in_document.origin()));
  return rect_in_frame;
}

gfx::Point LocalFrameView::DocumentToFrame(
    const gfx::Point& point_in_document) const {
  return gfx::ToFlooredPoint(DocumentToFrame(gfx::PointF(point_in_document)));
}

gfx::PointF LocalFrameView::DocumentToFrame(
    const gfx::PointF& point_in_document) const {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return point_in_document;

  return point_in_document - layout_viewport->GetScrollOffset();
}

PhysicalOffset LocalFrameView::DocumentToFrame(
    const PhysicalOffset& offset_in_document) const {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return offset_in_document;

  return offset_in_document -
         PhysicalOffset::FromVector2dFRound(layout_viewport->GetScrollOffset());
}

PhysicalRect LocalFrameView::DocumentToFrame(
    const PhysicalRect& rect_in_document) const {
  return PhysicalRect(DocumentToFrame(rect_in_document.offset),
                      rect_in_document.size);
}

gfx::Point LocalFrameView::FrameToDocument(
    const gfx::Point& point_in_frame) const {
  return ToFlooredPoint(FrameToDocument(PhysicalOffset(point_in_frame)));
}

PhysicalOffset LocalFrameView::FrameToDocument(
    const PhysicalOffset& offset_in_frame) const {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return offset_in_frame;

  return offset_in_frame +
         PhysicalOffset::FromVector2dFRound(layout_viewport->GetScrollOffset());
}

gfx::Rect LocalFrameView::FrameToDocument(
    const gfx::Rect& rect_in_frame) const {
  return gfx::Rect(FrameToDocument(rect_in_frame.origin()),
                   rect_in_frame.size());
}

PhysicalRect LocalFrameView::FrameToDocument(
    const PhysicalRect& rect_in_frame) const {
  return PhysicalRect(FrameToDocument(rect_in_frame.offset),
                      rect_in_frame.size);
}

gfx::Rect LocalFrameView::ConvertToContainingEmbeddedContentView(
    const gfx::Rect& local_rect) const {
  if (ParentFrameView()) {
    auto* layout_object = GetLayoutEmbeddedContent();
    if (!layout_object)
      return local_rect;

    // Add borders and padding etc.
    gfx::Rect rect = layout_object->BorderBoxFromEmbeddedContent(local_rect);
    return ToPixelSnappedRect(
        layout_object->LocalToAbsoluteRect(PhysicalRect(rect)));
  }

  return local_rect;
}

gfx::Rect LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const gfx::Rect& parent_rect) const {
  if (ParentFrameView()) {
    gfx::Rect local_rect = parent_rect;
    local_rect.Offset(-Location().OffsetFromOrigin());
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
    // Add borders and padding etc.
    point = layout_object->BorderBoxFromEmbeddedContent(point);
    return layout_object->LocalToAbsolutePoint(point);
  }

  return local_offset;
}

gfx::PointF LocalFrameView::ConvertToContainingEmbeddedContentView(
    const gfx::PointF& local_point) const {
  if (ParentFrameView()) {
    auto* layout_object = GetLayoutEmbeddedContent();
    if (!layout_object)
      return local_point;

    PhysicalOffset point = PhysicalOffset::FromPointFRound(local_point);
    // Add borders and padding etc.
    point = layout_object->BorderBoxFromEmbeddedContent(point);
    return static_cast<gfx::PointF>(layout_object->LocalToAbsolutePoint(point));
  }

  return local_point;
}

PhysicalOffset LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const PhysicalOffset& parent_offset) const {
  return PhysicalOffset::FromPointFRound(
      ConvertFromContainingEmbeddedContentView(gfx::PointF(parent_offset)));
}

gfx::PointF LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const gfx::PointF& parent_point) const {
  if (ParentFrameView()) {
    // Get our layoutObject in the parent view
    auto* layout_object = GetLayoutEmbeddedContent();
    if (!layout_object)
      return parent_point;

    gfx::PointF point = layout_object->AbsoluteToLocalPoint(parent_point);
    // Subtract borders and padding etc.
    point = layout_object->EmbeddedContentFromBorderBox(point);
    return point;
  }

  return parent_point;
}

void LocalFrameView::SetTracksRasterInvalidations(
    bool track_raster_invalidations) {
  if (!GetFrame().IsLocalRoot()) {
    GetFrame().LocalFrameRoot().View()->SetTracksRasterInvalidations(
        track_raster_invalidations);
    return;
  }
  if (track_raster_invalidations == is_tracking_raster_invalidations_)
    return;

  // Ensure the document is up-to-date before tracking invalidations.
  UpdateAllLifecyclePhasesForTest();

  is_tracking_raster_invalidations_ = track_raster_invalidations;
  if (paint_artifact_compositor_) {
    paint_artifact_compositor_->SetTracksRasterInvalidations(
        track_raster_invalidations);
  }

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"),
                       "LocalFrameView::setTracksPaintInvalidations",
                       TRACE_EVENT_SCOPE_GLOBAL, "enabled",
                       track_raster_invalidations);
}

void LocalFrameView::ServiceScrollAnimations(base::TimeTicks start_time) {
  bool can_throttle = CanThrottleRendering();
  // Disallow throttling in case any script needs to do a synchronous
  // lifecycle update in other frames which are throttled.
  DisallowThrottlingScope disallow_throttling(*this);
  Document* document = GetFrame().GetDocument();
  DCHECK(document);
  if (!can_throttle) {
    if (ScrollableArea* scrollable_area = GetScrollableArea()) {
      scrollable_area->ServiceScrollAnimations(
          start_time.since_origin().InSecondsF());
    }
    if (const ScrollableAreaSet* animating_scrollable_areas =
            AnimatingScrollableAreas()) {
      // Iterate over a copy, since ScrollableAreas may deregister
      // themselves during the iteration.
      HeapVector<Member<PaintLayerScrollableArea>>
          animating_scrollable_areas_copy(*animating_scrollable_areas);
      for (PaintLayerScrollableArea* scrollable_area :
           animating_scrollable_areas_copy) {
        scrollable_area->ServiceScrollAnimations(
            start_time.since_origin().InSecondsF());
      }
    }
    // After scroll updates, snapshot scroll state once at top of animation
    // frame.
    GetFrame().UpdateScrollSnapshots();

    if (SVGDocumentExtensions::ServiceSmilOnAnimationFrame(*document))
      GetPage()->Animator().SetHasSmilAnimation();
    SVGDocumentExtensions::ServiceWebAnimationsOnAnimationFrame(*document);
    document->GetDocumentAnimations().UpdateAnimationTimingForAnimationFrame();
  }
}

void LocalFrameView::ScheduleAnimation(base::TimeDelta delay,
                                       base::Location location) {
  TRACE_EVENT("cc", "LocalFrameView::ScheduleAnimation", "frame", GetFrame(),
              "delay", delay, "location", location);
  if (auto* client = GetChromeClient())
    client->ScheduleAnimation(this, delay);
}

void LocalFrameView::OnCommitRequested() {
  DCHECK(frame_->IsLocalRoot());
  if (frame_->GetDocument() &&
      !frame_->GetDocument()->IsInitialEmptyDocument() && GetUkmAggregator()) {
    GetUkmAggregator()->OnCommitRequested();
  }
}

void LocalFrameView::AddScrollAnchoringScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  DCHECK(scrollable_area);
  if (!scroll_anchoring_scrollable_areas_) {
    scroll_anchoring_scrollable_areas_ =
        MakeGarbageCollected<ScrollableAreaSet>();
  }
  scroll_anchoring_scrollable_areas_->insert(scrollable_area);
}

void LocalFrameView::RemoveScrollAnchoringScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  if (scroll_anchoring_scrollable_areas_)
    scroll_anchoring_scrollable_areas_->erase(scrollable_area);
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

void LocalFrameView::AddUserScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  DCHECK(scrollable_area);
  if (!user_scrollable_areas_)
    user_scrollable_areas_ = MakeGarbageCollected<ScrollableAreaMap>();
  user_scrollable_areas_->insert(scrollable_area->GetScrollElementId(),
                                 scrollable_area);
}

void LocalFrameView::RemoveUserScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  if (user_scrollable_areas_) {
    user_scrollable_areas_->erase(scrollable_area->GetScrollElementId());
  }
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
                               parent_view->CanThrottleRendering(),
                               IsDisplayLocked());

  // This is to handle a special case: a display:none iframe may have a fully
  // populated layout tree if it contains an <embed>. In that case, we must
  // ensure that the embed's compositing layer is properly reattached.
  // crbug.com/749737 for context.
  if (auto* layout_view = GetLayoutView())
    layout_view->Layer()->SetNeedsCompositingInputsUpdate();

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

  // To limit the number of Mojo communications, only notify the browser when
  // the rect's size changes, not when the position changes. The size needs to
  // be replicated if the iframe goes out-of-process.
  gfx::Size frame_size = FrameRect().size();
  if (!frame_size_ || *frame_size_ != frame_size) {
    frame_size_ = frame_size;
    GetFrame().GetLocalFrameHostRemote().FrameSizeChanged(frame_size);
  }
}

void LocalFrameView::ZoomFactorChanged(float zoom_factor) {
  GetFrame().SetLayoutZoomFactor(zoom_factor);
}

void LocalFrameView::SetLayoutSizeInternal(const gfx::Size& size) {
  if (layout_size_ == size)
    return;
  layout_size_ = size;
  SetNeedsLayout();
  Document* document = GetFrame().GetDocument();
  if (!document || !document->IsActive())
    return;
  document->LayoutViewportWasResized();
  if (frame_->IsMainFrame())
    TextAutosizer::UpdatePageInfoInAllFrames(frame_);
}

void LocalFrameView::DidChangeScrollOffset() {
  GetFrame().Client()->DidChangeScrollOffset();
  if (GetFrame().IsOutermostMainFrame()) {
    GetFrame()
        .GetPage()
        ->GetChromeClient()
        .OutermostMainFrameScrollOffsetChanged();
  }
}

ScrollableArea* LocalFrameView::ScrollableAreaWithElementId(
    const CompositorElementId& id) {
  // Check for the layout viewport, which may not be in user_scrollable_areas_
  // if it is styled overflow: hidden.  (Other overflow: hidden elements won't
  // have composited scrolling layers per crbug.com/784053, so we don't have to
  // worry about them.)
  ScrollableArea* viewport = LayoutViewport();
  if (id == viewport->GetScrollElementId())
    return viewport;

  if (user_scrollable_areas_) {
    auto it = user_scrollable_areas_->find(id);
    if (it != user_scrollable_areas_->end()) {
      return it->value;
    }
  }
  return nullptr;
}

void LocalFrameView::ScrollRectToVisibleInRemoteParent(
    const PhysicalRect& rect_to_scroll,
    mojom::blink::ScrollIntoViewParamsPtr params) {
  DCHECK(GetFrame().IsLocalRoot());
  DCHECK(!GetFrame().IsOutermostMainFrame());

  // If the scroll doesn't cross origin boundaries then it must already have
  // been blocked for a scroll crossing an embedded frame tree boundary.
  DCHECK(params->cross_origin_boundaries ||
         (!GetFrame().IsMainFrame() || GetFrame().IsOutermostMainFrame()));

  DCHECK(params->cross_origin_boundaries ||
         GetFrame()
             .Tree()
             .Parent()
             ->GetSecurityContext()
             ->GetSecurityOrigin()
             ->CanAccess(GetFrame().GetSecurityContext()->GetSecurityOrigin()));
  PhysicalRect new_rect = ConvertToRootFrame(rect_to_scroll);
  GetFrame().GetLocalFrameHostRemote().ScrollRectToVisibleInParentFrame(
      gfx::RectF(new_rect), std::move(params));
}

void LocalFrameView::NotifyFrameRectsChangedIfNeeded() {
  if (root_layer_did_scroll_) {
    root_layer_did_scroll_ = false;
    PropagateFrameRects();
  }
}

PhysicalOffset LocalFrameView::ViewportToFrame(
    const PhysicalOffset& point_in_viewport) const {
  PhysicalOffset point_in_root_frame = PhysicalOffset::FromPointFRound(
      frame_->GetPage()->GetVisualViewport().ViewportToRootFrame(
          gfx::PointF(point_in_viewport)));
  return ConvertFromRootFrame(point_in_root_frame);
}

gfx::PointF LocalFrameView::ViewportToFrame(
    const gfx::PointF& point_in_viewport) const {
  gfx::PointF point_in_root_frame(
      frame_->GetPage()->GetVisualViewport().ViewportToRootFrame(
          point_in_viewport));
  return ConvertFromRootFrame(point_in_root_frame);
}

gfx::Rect LocalFrameView::ViewportToFrame(
    const gfx::Rect& rect_in_viewport) const {
  gfx::Rect rect_in_root_frame =
      frame_->GetPage()->GetVisualViewport().ViewportToRootFrame(
          rect_in_viewport);
  return ConvertFromRootFrame(rect_in_root_frame);
}

gfx::Point LocalFrameView::ViewportToFrame(
    const gfx::Point& point_in_viewport) const {
  return ToRoundedPoint(ViewportToFrame(PhysicalOffset(point_in_viewport)));
}

gfx::Rect LocalFrameView::FrameToViewport(
    const gfx::Rect& rect_in_frame) const {
  gfx::Rect rect_in_root_frame = ConvertToRootFrame(rect_in_frame);
  return frame_->GetPage()->GetVisualViewport().RootFrameToViewport(
      rect_in_root_frame);
}

gfx::Point LocalFrameView::FrameToViewport(
    const gfx::Point& point_in_frame) const {
  gfx::Point point_in_root_frame = ConvertToRootFrame(point_in_frame);
  return frame_->GetPage()->GetVisualViewport().RootFrameToViewport(
      point_in_root_frame);
}

gfx::PointF LocalFrameView::FrameToViewport(
    const gfx::PointF& point_in_frame) const {
  gfx::PointF point_in_root_frame = ConvertToRootFrame(point_in_frame);
  return frame_->GetPage()->GetVisualViewport().RootFrameToViewport(
      point_in_root_frame);
}

gfx::Rect LocalFrameView::FrameToScreen(const gfx::Rect& rect) const {
  if (auto* client = GetChromeClient())
    return client->LocalRootToScreenDIPs(ConvertToRootFrame(rect), this);
  return gfx::Rect();
}

gfx::Point LocalFrameView::SoonToBeRemovedUnscaledViewportToContents(
    const gfx::Point& point_in_viewport) const {
  gfx::Point point_in_root_frame = gfx::ToFlooredPoint(
      frame_->GetPage()->GetVisualViewport().ViewportCSSPixelsToRootFrame(
          gfx::PointF(point_in_viewport)));
  return ConvertFromRootFrame(point_in_root_frame);
}

LocalFrameView::AllowThrottlingScope::AllowThrottlingScope(
    const LocalFrameView& frame_view)
    : value_(&frame_view.GetFrame().LocalFrameRoot().View()->allow_throttling_,
             true) {}

LocalFrameView::DisallowThrottlingScope::DisallowThrottlingScope(
    const LocalFrameView& frame_view)
    : value_(&frame_view.GetFrame().LocalFrameRoot().View()->allow_throttling_,
             false) {}

LocalFrameView::ForceThrottlingScope::ForceThrottlingScope(
    const LocalFrameView& frame_view)
    : allow_scope_(frame_view),
      value_(&frame_view.GetFrame().LocalFrameRoot().View()->force_throttling_,
             true) {}

PaintControllerPersistentData&
LocalFrameView::EnsurePaintControllerPersistentData() {
  if (!paint_controller_persistent_data_) {
    paint_controller_persistent_data_ =
        MakeGarbageCollected<PaintControllerPersistentData>();
  }
  return *paint_controller_persistent_data_;
}

bool LocalFrameView::CapturePaintPreview(
    GraphicsContext& context,
    const gfx::Vector2d& paint_offset) const {
  std::optional<base::UnguessableToken> maybe_embedding_token =
      GetFrame().GetEmbeddingToken();

  // Avoid crashing if a local frame doesn't have an embedding token.
  // e.g. it was unloaded or hasn't finished loading (crbug/1103157).
  if (!maybe_embedding_token.has_value())
    return false;

  // Ensure a recording canvas is properly created.
  DrawingRecorder recorder(context, *GetFrame().OwnerLayoutObject(),
                           DisplayItem::kDocumentBackground);
  context.Save();
  context.Translate(paint_offset.x(), paint_offset.y());
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
                           PaintFlags paint_flags,
                           const CullRect& cull_rect,
                           const gfx::Vector2d& paint_offset) const {
  const auto* owner_layout_object = GetFrame().OwnerLayoutObject();
  std::optional<Document::PaintPreviewScope> paint_preview;
  if (owner_layout_object &&
      owner_layout_object->GetDocument().GetPaintPreviewState() !=
          Document::kNotPaintingPreview) {
    paint_preview.emplace(
        *GetFrame().GetDocument(),
        owner_layout_object->GetDocument().GetPaintPreviewState());
    // When capturing a Paint Preview we want to capture scrollable embedded
    // content separately. Paint should stop here and ask the browser to
    // coordinate painting such frames as a separate task.
    if (LayoutViewport()->ScrollsOverflow()) {
      // If capture fails we should fallback to capturing inline if possible.
      if (CapturePaintPreview(context, paint_offset))
        return;
    }
  }

  if (!cull_rect.Rect().Intersects(FrameRect()))
    return;

  // |paint_offset| is not used because paint properties of the contents will
  // ensure the correct location.
  PaintFrame(context, paint_flags);
}

void LocalFrameView::PaintFrame(GraphicsContext& context,
                                PaintFlags paint_flags) const {
  FramePainter(*this).Paint(context, paint_flags);
}

void LocalFrameView::PrintPage(GraphicsContext& context,
                               wtf_size_t page_index,
                               const CullRect& cull_rect) {
  DCHECK(GetFrame().GetDocument()->Printing());
  if (pagination_state_) {
    pagination_state_->SetCurrentPageIndex(page_index);
  }
  const PaintFlags flags =
      PaintFlag::kOmitCompositingInfo | PaintFlag::kAddUrlMetadata;
  PaintOutsideOfLifecycle(context, flags, cull_rect);
}

static bool PaintOutsideOfLifecycleIsAllowed(GraphicsContext& context,
                                             const LocalFrameView& frame_view) {
  // A paint outside of lifecycle should not conflict about paint controller
  // caching with the default painting executed during lifecycle update,
  // otherwise the caller should either use a transient paint controller or
  // explicitly skip cache.
  if (context.GetPaintController().IsSkippingCache())
    return true;
  return false;
}

void LocalFrameView::PaintOutsideOfLifecycle(GraphicsContext& context,
                                             const PaintFlags paint_flags,
                                             const CullRect& cull_rect) {
  DCHECK(PaintOutsideOfLifecycleIsAllowed(context, *this));

  UpdateAllLifecyclePhasesExceptPaint(DocumentUpdateReason::kPrinting);

  SCOPED_UMA_AND_UKM_TIMER(GetUkmAggregator(), LocalFrameUkmAggregator::kPaint);

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
  });

  {
    if (pagination_state_) {
      pagination_state_->UpdateContentAreaPropertiesForCurrentPage(
          *GetLayoutView());
    }

    bool disable_expansion = paint_flags & PaintFlag::kOmitCompositingInfo;
    OverriddenCullRectScope force_cull_rect(*GetLayoutView()->Layer(),
                                            cull_rect, disable_expansion);
    context.GetPaintController().SetRecordDebugInfo(PaintDebugInfoEnabled());
    PaintFrame(context, paint_flags);
  }

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
  });
}

void LocalFrameView::PaintOutsideOfLifecycleWithThrottlingAllowed(
    GraphicsContext& context,
    const PaintFlags paint_flags,
    const CullRect& cull_rect) {
  AllowThrottlingScope allow_throttling(*this);
  PaintOutsideOfLifecycle(context, paint_flags, cull_rect);
}

void LocalFrameView::PaintForTest(const CullRect& cull_rect) {
  AllowThrottlingScope allow_throttling(*this);
  Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
  CullRectUpdater(*GetLayoutView()->Layer()).UpdateForTesting(cull_rect);
  if (GetLayoutView()->Layer()->SelfOrDescendantNeedsRepaint()) {
    PaintController paint_controller(PaintDebugInfoEnabled(),
                                     &EnsurePaintControllerPersistentData());
    GraphicsContext graphics_context(paint_controller);
    PaintFrame(graphics_context);
    paint_controller.CommitNewDisplayItems();
  }
  Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
  CullRectUpdater(*GetLayoutView()->Layer())
      .UpdateForTesting(CullRect::Infinite());
}

PaintRecord LocalFrameView::GetPaintRecord(const gfx::Rect* cull_rect) const {
  DCHECK_EQ(DocumentLifecycle::kPaintClean, Lifecycle().GetState());
  DCHECK(frame_->IsLocalRoot());
  DCHECK(paint_controller_persistent_data_);
  return paint_controller_persistent_data_->GetPaintArtifact().GetPaintRecord(
      PropertyTreeState::Root(), cull_rect);
}

const PaintArtifact& LocalFrameView::GetPaintArtifact() const {
  CHECK_EQ(DocumentLifecycle::kPaintClean, Lifecycle().GetState());
  return GetFrame()
      .LocalFrameRoot()
      .View()
      ->EnsurePaintControllerPersistentData()
      .GetPaintArtifact();
}

gfx::Rect LocalFrameView::ConvertToRootFrame(
    const gfx::Rect& local_rect) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    gfx::Rect parent_rect = ConvertToContainingEmbeddedContentView(local_rect);
    return parent->ConvertToRootFrame(parent_rect);
  }
  return local_rect;
}

gfx::Point LocalFrameView::ConvertToRootFrame(
    const gfx::Point& local_point) const {
  return ToRoundedPoint(ConvertToRootFrame(PhysicalOffset(local_point)));
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

gfx::PointF LocalFrameView::ConvertToRootFrame(
    const gfx::PointF& local_point) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    gfx::PointF parent_point =
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

gfx::Rect LocalFrameView::ConvertFromRootFrame(
    const gfx::Rect& rect_in_root_frame) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    gfx::Rect parent_rect = parent->ConvertFromRootFrame(rect_in_root_frame);
    return ConvertFromContainingEmbeddedContentView(parent_rect);
  }
  return rect_in_root_frame;
}

gfx::Point LocalFrameView::ConvertFromRootFrame(
    const gfx::Point& point_in_root_frame) const {
  return ToRoundedPoint(
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

gfx::PointF LocalFrameView::ConvertFromRootFrame(
    const gfx::PointF& point_in_root_frame) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    gfx::PointF parent_point =
        parent->ConvertFromRootFrame(point_in_root_frame);
    return ConvertFromContainingEmbeddedContentView(parent_point);
  }
  return point_in_root_frame;
}

void LocalFrameView::ParentVisibleChanged() {
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
  }
}

int LocalFrameView::ViewportWidth() const {
  int viewport_width = GetLayoutSize().width();
  return AdjustForAbsoluteZoom::AdjustInt(viewport_width, GetLayoutView());
}

int LocalFrameView::ViewportHeight() const {
  int viewport_height = GetLayoutSize().height();
  return AdjustForAbsoluteZoom::AdjustInt(viewport_height, GetLayoutView());
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

void LocalFrameView::CollectDraggableRegions(
    LayoutObject& layout_object,
    Vector<DraggableRegionValue>& regions) const {
  // LayoutTexts don't have their own style, they just use their parent's style,
  // so we don't want to include them.
  if (layout_object.IsText())
    return;

  layout_object.AddDraggableRegions(regions);
  for (LayoutObject* curr = layout_object.SlowFirstChild(); curr;
       curr = curr->NextSibling())
    CollectDraggableRegions(*curr, regions);
}

bool LocalFrameView::UpdateViewportIntersectionsForSubtree(
    unsigned parent_flags,
    ComputeIntersectionsContext& context) {
  // This will be recomputed, but default to the previous computed value if
  // there's an early return.
  bool needs_occlusion_tracking = false;
  IntersectionObserverController* controller =
      GetFrame().GetDocument()->GetIntersectionObserverController();
  if (controller) {
    needs_occlusion_tracking = controller->NeedsOcclusionTracking();
  }

  // TODO(dcheng): Since LocalFrameView tree updates are deferred, FrameViews
  // might still be in the LocalFrameView hierarchy even though the associated
  // Document is already detached. Investigate if this check and a similar check
  // in lifecycle updates are still needed when there are no more deferred
  // LocalFrameView updates: https://crbug.com/561683
  if (!GetFrame().GetDocument()->IsActive()) {
    return needs_occlusion_tracking;
  }

  unsigned flags = GetIntersectionObservationFlags(parent_flags);
  if (!NeedsLayout() || IsDisplayLocked()) {
    // Notify javascript IntersectionObservers
    if (controller) {
      needs_occlusion_tracking = controller->ComputeIntersections(
          flags, *this,
          accumulated_scroll_delta_since_last_intersection_update_, context);
      accumulated_scroll_delta_since_last_intersection_update_ =
          gfx::Vector2dF();
    }
    intersection_observation_state_ = kNotNeeded;
  }

  {
    SCOPED_UMA_AND_UKM_TIMER(
        GetUkmAggregator(),
        LocalFrameUkmAggregator::kUpdateViewportIntersection);
    UpdateViewportIntersection(flags, needs_occlusion_tracking);
  }

  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    needs_occlusion_tracking |=
        child->View()->UpdateViewportIntersectionsForSubtree(flags, context);
  }

  if (DocumentFencedFrames* fenced_frames =
          DocumentFencedFrames::Get(*frame_->GetDocument())) {
    for (HTMLFencedFrameElement* fenced_frame :
         fenced_frames->GetFencedFrames()) {
      if (Frame* frame = fenced_frame->ContentFrame()) {
        needs_occlusion_tracking |=
            frame->View()->UpdateViewportIntersectionsForSubtree(flags,
                                                                 context);
      }
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

void LocalFrameView::CrossOriginToNearestMainFrameChanged() {
  // If any of these conditions hold, then a change in cross-origin status does
  // not affect throttling.
  if (lifecycle_updates_throttled_ || IsSubtreeThrottled() ||
      IsDisplayLocked() || !IsHiddenForThrottling()) {
    return;
  }
  RenderThrottlingStatusChanged();
  // Immediately propagate changes to children.
  UpdateRenderThrottlingStatus(IsHiddenForThrottling(), IsSubtreeThrottled(),
                               IsDisplayLocked(), true);
}

void LocalFrameView::CrossOriginToParentFrameChanged() {
  if (LayoutView* layout_view = GetLayoutView()) {
    if (PaintLayer* root_layer = layout_view->Layer())
      root_layer->SetNeedsCompositingInputsUpdate();
  }
}

void LocalFrameView::SetViewportIntersection(
    const mojom::blink::ViewportIntersectionState& intersection_state) {
  if (!last_intersection_state_.Equals(intersection_state)) {
    last_intersection_state_ = intersection_state;
    int viewport_intersect_area =
        intersection_state.viewport_intersection.size()
            .GetCheckedArea()
            .ValueOrDefault(INT_MAX);
    int outermost_main_frame_area =
        intersection_state.outermost_main_frame_size.GetCheckedArea()
            .ValueOrDefault(INT_MAX);
    float ratio = 1.0f * viewport_intersect_area / outermost_main_frame_area;
    const float ratio_threshold =
        1.0f * features::kLargeFrameSizePercentThreshold.Get() / 100;
    if (FrameScheduler* frame_scheduler = frame_->GetFrameScheduler()) {
      frame_scheduler->SetVisibleAreaLarge(ratio > ratio_threshold);
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

  // LocalFrameClient member may not be valid in some tests.
  if (frame_->Client() && frame_->Client()->GetWebFrame() &&
      frame_->Client()->GetWebFrame()->Client()) {
    frame_->Client()->GetWebFrame()->Client()->OnFrameVisibilityChanged(
        visibility);
  }
}

void LocalFrameView::RenderThrottlingStatusChanged() {
  TRACE_EVENT0("blink", "LocalFrameView::RenderThrottlingStatusChanged");
  DCHECK(!IsInPerformLayout());
  DCHECK(!frame_->GetDocument() || !frame_->GetDocument()->InStyleRecalc());

  // When a frame is throttled, we delete its previous painted output, so it
  // will need to be repainted, even if nothing else has changed.
  if (LayoutView* layout_view = GetLayoutView()) {
    layout_view->Layer()->SetNeedsRepaint();
  }
  // The painted output of the frame may be included in a cached subsequence
  // associated with the embedding document, so invalidate the owner.
  if (auto* owner = GetFrame().OwnerLayoutObject()) {
    if (PaintLayer* owner_layer = owner->Layer()) {
      owner_layer->SetNeedsRepaint();
    }
  }

  if (!CanThrottleRendering()) {
    // Start ticking animation frames again if necessary.
    if (GetPage())
      GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());
    // Ensure we'll recompute viewport intersection for the frame subtree during
    // the scheduled visual update.
    SetIntersectionObservationState(kRequired);
  } else if (GetFrame().IsLocalRoot()) {
    DCHECK(!IsUpdatingLifecycle());
    ForceThrottlingScope force_throttling(*this);
    // TODO(https://crbug.com/1196853): Switch to ScriptForbiddenScope once
    // failures are fixed.
    BlinkLifecycleScopeWillBeScriptForbidden forbid_script;
    RunPaintLifecyclePhase(PaintBenchmarkMode::kNormal);
  }

#if DCHECK_IS_ON()
  // Make sure we never have an unthrottled frame inside a throttled one.
  LocalFrameView* parent = ParentFrameView();
  while (parent) {
    DCHECK(CanThrottleRendering() || !parent->CanThrottleRendering());
    parent = parent->ParentFrameView();
  }
#endif
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

void LocalFrameView::UpdateIntersectionObservationStateOnScroll(
    gfx::Vector2dF scroll_delta) {
  accumulated_scroll_delta_since_last_intersection_update_ +=
      gfx::Vector2dF(std::abs(scroll_delta.x()), std::abs(scroll_delta.y()));
  SetIntersectionObservationState(kScrollAndVisibilityOnly);
}

void LocalFrameView::SetVisualViewportOrOverlayNeedsRepaint() {
  if (LocalFrameView* root = GetFrame().LocalFrameRoot().View())
    root->visual_viewport_or_overlay_needs_repaint_ = true;
}

bool LocalFrameView::VisualViewportOrOverlayNeedsRepaintForTesting() const {
  DCHECK(GetFrame().IsLocalRoot());
  return visual_viewport_or_overlay_needs_repaint_;
}

void LocalFrameView::SetPaintArtifactCompositorNeedsUpdate() {
  LocalFrameView* root = GetFrame().LocalFrameRoot().View();
  if (root && root->paint_artifact_compositor_)
    root->paint_artifact_compositor_->SetNeedsUpdate();
}

PaintArtifactCompositor* LocalFrameView::GetPaintArtifactCompositor() const {
  LocalFrameView* root = GetFrame().LocalFrameRoot().View();
  return root ? root->paint_artifact_compositor_.Get() : nullptr;
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

  if (!target_frame.IsLocalRoot() && !target_frame.OwnerLayoutObject())
    flags |= IntersectionObservation::kAncestorFrameIsDetachedFromLayout;

  // Observers with explicit roots only need to be checked on the same frame,
  // since in this case target and root must be in the same document.
  if (intersection_observation_state_ != kNotNeeded) {
    flags |= (IntersectionObservation::kExplicitRootObserversNeedUpdate |
              IntersectionObservation::kImplicitRootObserversNeedUpdate);
    if (intersection_observation_state_ == kScrollAndVisibilityOnly) {
      flags |= IntersectionObservation::kScrollAndVisibilityOnly;
    }
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
  if (LocalFrameTreeForcesThrottling())
    return true;
  bool throttled_for_global_reasons = LocalFrameTreeAllowsThrottling() &&
                                      CanThrottleRendering() &&
                                      frame_->GetDocument();
  if (!throttled_for_global_reasons)
    return false;

  // If we're currently running a lifecycle update, and we are required to run
  // the IntersectionObserver steps at the end of the update, then there are two
  // courses of action, depending on whether this frame is display locked by its
  // parent frame:
  //
  //   - If it is NOT display locked, then we suppress throttling to force the
  // lifecycle update to proceed up to the state required to run
  // IntersectionObserver.
  //
  //   - If it IS display locked, then we still need IntersectionObserver to
  // run; but the display lock status will short-circuit the
  // IntersectionObserver algorithm and create degenerate "not intersecting"
  // notifications. Hence, we don't need to force lifecycle phases to run,
  // because IntersectionObserver will not need access to up-to-date
  // geometry. So there is no point in suppressing throttling here.
  auto* local_frame_root_view = GetFrame().LocalFrameRoot().View();
  if (local_frame_root_view->IsUpdatingLifecycle() &&
      intersection_observation_state_ == kRequired && !IsDisplayLocked()) {
    return Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean;
  }

  return true;
}

bool LocalFrameView::ShouldThrottleRenderingForTest() const {
  AllowThrottlingScope allow_throttling(*this);
  return ShouldThrottleRendering();
}

bool LocalFrameView::CanThrottleRendering() const {
  if (lifecycle_updates_throttled_ || IsSubtreeThrottled() ||
      IsDisplayLocked() || throttled_for_view_transition_) {
    return true;
  }
  // We only throttle hidden cross-origin frames. This is to avoid a situation
  // where an ancestor frame directly depends on the pipeline timing of a
  // descendant and breaks as a result of throttling. The rationale is that
  // cross-origin frames must already communicate with asynchronous messages,
  // so they should be able to tolerate some delay in receiving replies from a
  // throttled peer.
  return IsHiddenForThrottling() && frame_->IsCrossOriginToNearestMainFrame();
}

void LocalFrameView::UpdateRenderThrottlingStatus(bool hidden_for_throttling,
                                                  bool subtree_throttled,
                                                  bool display_locked,
                                                  bool recurse) {
  bool was_throttled = CanThrottleRendering();
  FrameView::UpdateRenderThrottlingStatus(
      hidden_for_throttling, subtree_throttled, display_locked, recurse);
  if (was_throttled != CanThrottleRendering())
    RenderThrottlingStatusChanged();
}

void LocalFrameView::SetThrottledForViewTransition(bool throttled) {
  if (throttled_for_view_transition_ == throttled) {
    return;
  }

  bool was_throttled = CanThrottleRendering();
  throttled_for_view_transition_ = throttled;

  // Invalidating paint here will cause the iframe to draw with no content
  // instead of showing old content. This will be fixed by paint holding for
  // local iframes.
  if (RuntimeEnabledFeatures::PaintHoldingForLocalIframesEnabled() &&
      was_throttled != CanThrottleRendering()) {
    RenderThrottlingStatusChanged();
  }
}

void LocalFrameView::BeginLifecycleUpdates() {
  TRACE_EVENT("blink", "LocalFrameView::BeginLifecycleUpdates");
  lifecycle_updates_throttled_ = false;

  LayoutView* layout_view = GetLayoutView();
  bool layout_view_is_empty = layout_view && !layout_view->FirstChild();
  if (layout_view_is_empty && !DidFirstLayout() && !NeedsLayout()) {
    // Make sure a display:none iframe gets an initial layout pass.
    layout_view->SetNeedsLayout(layout_invalidation_reason::kAddedToLayout,
                                kMarkOnlyThis);
  }

  ScheduleAnimation();
  SetIntersectionObservationState(kRequired);

  // Do not report paint timing for the initially empty document.
  if (GetFrame().GetDocument()->IsInitialEmptyDocument())
    MarkIneligibleToPaint();

  // Non-main-frame lifecycle and commit deferral are controlled by their
  // main frame.
  if (!GetFrame().IsMainFrame())
    return;

  ChromeClient& chrome_client = GetFrame().GetPage()->GetChromeClient();

  // Determine if we want to defer commits to the compositor once lifecycle
  // updates start. Doing so allows us to update the page lifecycle but not
  // present the results to screen until we see first contentful paint is
  // available or until a timer expires.
  // This is enabled only when the document loading is regular HTML served
  // over HTTP/HTTPs. And only defer commits once. This method gets called
  // multiple times, and we do not want to defer a second time if we have
  // already done so once and resumed commits already.
  if (WillDoPaintHoldingForFCP()) {
    have_deferred_main_frame_commits_ = true;
    chrome_client.StartDeferringCommits(
        GetFrame(), base::Milliseconds(kCommitDelayDefaultInMs),
        cc::PaintHoldingReason::kFirstContentfulPaint);
  }

  chrome_client.BeginLifecycleUpdates(GetFrame());
}

bool LocalFrameView::WillDoPaintHoldingForFCP() const {
  Document* document = GetFrame().GetDocument();
  return document && document->DeferredCompositorCommitIsAllowed() &&
         !have_deferred_main_frame_commits_ &&
         GetFrame().IsOutermostMainFrame();
}

String LocalFrameView::MainThreadScrollingReasonsAsText() {
  MainThreadScrollingReasons reasons = 0;
  DCHECK_GE(Lifecycle().GetState(), DocumentLifecycle::kPaintClean);
  const auto* properties = GetLayoutView()->FirstFragment().PaintProperties();
  if (properties && properties->Scroll()) {
    const auto* compositor =
        GetFrame().LocalFrameRoot().View()->paint_artifact_compositor_.Get();
    CHECK(compositor);
    reasons = compositor->GetMainThreadScrollingReasons(*properties->Scroll());
  }
  return String(cc::MainThreadScrollingReason::AsText(reasons).c_str());
}

bool LocalFrameView::MapToVisualRectInRemoteRootFrame(
    PhysicalRect& rect,
    bool apply_overflow_clip) {
  DCHECK(frame_->IsLocalRoot());
  // This is the top-level frame, so no mapping necessary.
  if (frame_->IsOutermostMainFrame())
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
  if (frame_->IsOutermostMainFrame())
    return;
  transform_state.ApplyTransform(GetFrame().RemoteMainFrameTransform(),
                                 TransformState::kAccumulateTransform);
}

LayoutUnit LocalFrameView::CaretWidth() const {
  return LayoutUnit(std::max<float>(
      1.0f, GetChromeClient()->WindowToViewportScalar(&GetFrame(), 1.0f)));
}

void LocalFrameView::RegisterTapEvent(Element* target) {
  if (tap_friendliness_checker_) {
    tap_friendliness_checker_->RegisterTapEvent(target);
  }
}

LocalFrameUkmAggregator* LocalFrameView::GetUkmAggregator() {
  DCHECK(frame_->IsLocalRoot() || !ukm_aggregator_);
  LocalFrameView* local_root = frame_->LocalFrameRoot().View();

  // TODO(crbug.com/1392462): Avoid checking whether we need to create the
  // aggregator on every access.
  if (!local_root->ukm_aggregator_) {
    if (!local_root->frame_->GetChromeClient().IsIsolatedSVGChromeClient()) {
      local_root->ukm_aggregator_ =
          base::MakeRefCounted<LocalFrameUkmAggregator>();
    }
  }
  return local_root->ukm_aggregator_.get();
}

void LocalFrameView::ResetUkmAggregatorForTesting() {
  ukm_aggregator_.reset();
}

void LocalFrameView::OnFirstContentfulPaint() {
  if (frame_->IsMainFrame()) {
    // Restart commits that may have been deferred.
    GetPage()->GetChromeClient().StopDeferringCommits(
        *frame_, cc::PaintHoldingCommitTrigger::kFirstContentfulPaint);
    if (frame_->GetDocument()->ShouldMarkFontPerformance())
      FontPerformance::MarkFirstContentfulPaint();
  }

  if (auto* metrics_aggregator = GetUkmAggregator())
    metrics_aggregator->DidReachFirstContentfulPaint();
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
  return !fullscreen_video_elements_.empty();
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

bool LocalFrameView::UpdatePaintDebugInfoEnabled() {
  DCHECK(frame_->IsLocalRoot());
#if DCHECK_IS_ON()
  DCHECK(paint_debug_info_enabled_);
#else
  bool should_enable =
      cc::frame_viewer_instrumentation::IsTracingLayerTreeSnapshots() ||
      RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
      WebTestSupport::IsRunningWebTest() ||
      CoreProbeSink::HasAgentsGlobal(CoreProbeSink::kInspectorLayerTreeAgent);
  if (should_enable != paint_debug_info_enabled_) {
    paint_debug_info_enabled_ = should_enable;
    SetPaintArtifactCompositorNeedsUpdate();
    return true;
  }
#endif
  return false;
}

OverlayInterstitialAdDetector&
LocalFrameView::EnsureOverlayInterstitialAdDetector() {
  if (!overlay_interstitial_ad_detector_) {
    overlay_interstitial_ad_detector_ =
        std::make_unique<OverlayInterstitialAdDetector>();
  }
  return *overlay_interstitial_ad_detector_.get();
}

StickyAdDetector& LocalFrameView::EnsureStickyAdDetector() {
  if (!sticky_ad_detector_) {
    sticky_ad_detector_ = std::make_unique<StickyAdDetector>();
  }
  return *sticky_ad_detector_.get();
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

PaintLayer* LocalFrameView::GetXROverlayLayer() const {
  Document* doc = frame_->GetDocument();
  DCHECK(doc);

  // For WebXR DOM Overlay, the fullscreen overlay layer comes from either the
  // overlay element itself, or from an iframe element if the overlay element is
  // in an OOPIF. This layer is needed even for non-main-frame scenarios to
  // ensure the background remains transparent.
  if (doc->IsXrOverlay())
    return GetXrOverlayLayer(*doc);

  return nullptr;
}

void LocalFrameView::SetCullRectNeedsUpdateForFrames(bool disable_expansion) {
  ForAllNonThrottledLocalFrameViews(
      [disable_expansion](LocalFrameView& frame_view) {
        // Propagate child frame PaintLayer NeedsCullRectUpdate flag into the
        // owner frame.
        if (auto* frame_layout_view = frame_view.GetLayoutView()) {
          if (auto* owner = frame_view.GetFrame().OwnerLayoutObject()) {
            PaintLayer* frame_root_layer = frame_layout_view->Layer();
            DCHECK(frame_root_layer);
            DCHECK(owner->Layer());
            if (frame_root_layer->NeedsCullRectUpdate() ||
                frame_root_layer->DescendantNeedsCullRectUpdate()) {
              owner->Layer()->SetDescendantNeedsCullRectUpdate();
            }
          }
        }
        // If we disable cull rect expansion in a OverriddenCullRectScope,
        // invalidate cull rects for user scrollable areas. This may not
        // invalidate all cull rects affected by disable_expansion but it
        // doesn't affect correctness.
        if (disable_expansion && frame_view.UserScrollableAreas()) {
          for (const auto& area : frame_view.UserScrollableAreas()->Values()) {
            area->Layer()->SetNeedsCullRectUpdate();
          }
        }
      },
      // Use post-order to ensure correct flag propagation for nested frames.
      kPostOrder);
}

void LocalFrameView::RunPaintBenchmark(int repeat_count,
                                       cc::PaintBenchmarkResult& result) {
  DCHECK_EQ(Lifecycle().GetState(), DocumentLifecycle::kPaintClean);
  DCHECK(GetFrame().IsLocalRoot());
  AllowThrottlingScope allow_throttling(*this);

  auto run_benchmark = [&](PaintBenchmarkMode mode) -> double {
    constexpr int kTimeCheckInterval = 1;
    constexpr int kWarmupRuns = 0;
    constexpr base::TimeDelta kTimeLimit = base::Milliseconds(1);

    base::TimeDelta min_time = base::TimeDelta::Max();
    for (int i = 0; i < repeat_count; i++) {
      // Run for a minimum amount of time to avoid problems with timer
      // quantization when the time is very small.
      base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
      do {
        // TODO(https://crbug.com/1196853): Switch to ScriptForbiddenScope once
        // failures are fixed.
        BlinkLifecycleScopeWillBeScriptForbidden forbid_script;
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
  result.raster_invalidation_and_convert_time_ms =
      run_benchmark(PaintBenchmarkMode::kForceRasterInvalidationAndConvert);
  result.paint_artifact_compositor_update_time_ms =
      run_benchmark(PaintBenchmarkMode::kForcePaintArtifactCompositorUpdate);

  result.painter_memory_usage = 0;
  if (paint_controller_persistent_data_) {
    result.painter_memory_usage +=
        paint_controller_persistent_data_->ApproximateUnsharedMemoryUsage();
  }
  if (paint_artifact_compositor_) {
    result.painter_memory_usage +=
        paint_artifact_compositor_->ApproximateUnsharedMemoryUsage();
  }
}

DarkModeFilter& LocalFrameView::EnsureDarkModeFilter() {
  if (!dark_mode_filter_) {
    dark_mode_filter_ =
        std::make_unique<DarkModeFilter>(GetCurrentDarkModeSettings());
  }
  return *dark_mode_filter_;
}

void LocalFrameView::AddPendingTransformUpdate(LayoutObject& object) {
  if (!pending_transform_updates_) {
    pending_transform_updates_ =
        MakeGarbageCollected<HeapHashSet<Member<LayoutObject>>>();
  }
  pending_transform_updates_->insert(&object);
}

bool LocalFrameView::RemovePendingTransformUpdate(const LayoutObject& object) {
  if (!pending_transform_updates_)
    return false;
  auto it =
      pending_transform_updates_->find(const_cast<LayoutObject*>(&object));
  if (it == pending_transform_updates_->end())
    return false;
  pending_transform_updates_->erase(it);
  return true;
}

void LocalFrameView::AddPendingOpacityUpdate(LayoutObject& object) {
  if (!pending_opacity_updates_) {
    pending_opacity_updates_ =
        MakeGarbageCollected<HeapHashSet<Member<LayoutObject>>>();
  }
  pending_opacity_updates_->insert(&object);
}

bool LocalFrameView::RemovePendingOpacityUpdate(const LayoutObject& object) {
  if (!pending_opacity_updates_)
    return false;
  auto it = pending_opacity_updates_->find(const_cast<LayoutObject*>(&object));
  if (it == pending_opacity_updates_->end())
    return false;
  pending_opacity_updates_->erase(it);
  return true;
}

bool LocalFrameView::ExecuteAllPendingUpdates() {
  DCHECK(GetFrame().IsLocalRoot() || !IsAttached());
  bool updated = false;
  ForAllNonThrottledLocalFrameViews([&updated](LocalFrameView& frame_view) {
    if (frame_view.pending_opacity_updates_ &&
        !frame_view.pending_opacity_updates_->empty()) {
      for (LayoutObject* object : *frame_view.pending_opacity_updates_) {
        DCHECK(
            !DisplayLockUtilities::LockedAncestorPreventingPrePaint(*object));
        PaintPropertyTreeBuilder::DirectlyUpdateOpacityValue(*object);
      }
      updated = true;
      frame_view.pending_opacity_updates_->clear();
    }
    if (frame_view.pending_transform_updates_ &&
        !frame_view.pending_transform_updates_->empty()) {
      for (LayoutObject* object : *frame_view.pending_transform_updates_) {
        DCHECK(
            !DisplayLockUtilities::LockedAncestorPreventingPrePaint(*object));
        PaintPropertyTreeBuilder::DirectlyUpdateTransformMatrix(*object);
      }
      updated = true;
      frame_view.SetIntersectionObservationState(kDesired);
      frame_view.pending_transform_updates_->clear();
    }
  });
  return updated;
}

void LocalFrameView::RemoveAllPendingUpdates() {
  if (pending_opacity_updates_) {
    for (LayoutObject* object : *pending_opacity_updates_) {
      object->SetNeedsPaintPropertyUpdate();
    }
    pending_opacity_updates_->clear();
  }
  if (pending_transform_updates_) {
    for (LayoutObject* object : *pending_transform_updates_) {
      object->SetNeedsPaintPropertyUpdate();
    }
    pending_transform_updates_->clear();
  }
}

void LocalFrameView::AddPendingStickyUpdate(PaintLayerScrollableArea* object) {
  if (!pending_sticky_updates_) {
    pending_sticky_updates_ =
        MakeGarbageCollected<HeapHashSet<Member<PaintLayerScrollableArea>>>();
  }
  pending_sticky_updates_->insert(object);
}

bool LocalFrameView::HasPendingStickyUpdate(
    PaintLayerScrollableArea* object) const {
  if (pending_sticky_updates_) {
    return pending_sticky_updates_->Contains(object);
  }
  return false;
}

void LocalFrameView::ExecutePendingStickyUpdates() {
  if (pending_sticky_updates_) {
    UseCounter::Count(frame_->GetDocument(), WebFeature::kPositionSticky);

    // Iteration order of the scrollable-areas doesn't matter as
    // sticky-positioned objects are contained within each scrollable-area.
    for (PaintLayerScrollableArea* scrollable_area : *pending_sticky_updates_) {
      scrollable_area->UpdateAllStickyConstraints();
    }
    pending_sticky_updates_->clear();
  }
}

void LocalFrameView::AddPendingSnapUpdate(PaintLayerScrollableArea* object) {
  if (!pending_snap_updates_) {
    pending_snap_updates_ =
        MakeGarbageCollected<HeapHashSet<Member<PaintLayerScrollableArea>>>();
  }
  pending_snap_updates_->insert(object);
}

void LocalFrameView::RemovePendingSnapUpdate(PaintLayerScrollableArea* object) {
  if (pending_snap_updates_) {
    pending_snap_updates_->erase(object);
  }
}

void LocalFrameView::ExecutePendingSnapUpdates() {
  if (pending_snap_updates_) {
    // Iteration order of the objects doesn't matter as the snap-areas are
    // contained within each scroll-container.
    for (PaintLayerScrollableArea* scrollable_area : *pending_snap_updates_) {
      auto* snap_container = scrollable_area->GetLayoutBox();
      DCHECK(snap_container->IsScrollContainer());
      if (SnapCoordinator::UpdateSnapContainerData(*snap_container)) {
        if (!pending_perform_snap_) {
          pending_perform_snap_ = MakeGarbageCollected<
              HeapHashSet<Member<PaintLayerScrollableArea>>>();
        }
        pending_perform_snap_->insert(scrollable_area);
      }
    }
    pending_snap_updates_->clear();
  }

  if (pending_perform_snap_ && !ShouldDeferLayoutSnap()) {
    for (PaintLayerScrollableArea* scrollable_area : *pending_perform_snap_) {
      scrollable_area->SnapAfterLayout();
    }
    pending_perform_snap_->clear();
  }
}

void LocalFrameView::NotifyElementWithRememberedSizeDisconnected(
    Element* element) {
  disconnected_elements_with_remembered_size_.insert(element);
}

bool LocalFrameView::UpdateLastSuccessfulPositionFallbacks() {
  return GetFrame()
      .GetDocument()
      ->GetStyleEngine()
      .UpdateLastSuccessfulPositionFallbacks();
}

}  // namespace blink
