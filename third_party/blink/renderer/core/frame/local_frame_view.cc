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

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/element_visibility_observer.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/editing/compute_layer_selection.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/frame_view_auto_size_info.h"
#include "third_party/blink/renderer/core/frame/link_highlights.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/frame/root_frame_viewport.h"
#include "third_party/blink/renderer/core/frame/scroll_into_view_options.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_frame_element.h"
#include "third_party/blink/renderer/core/html/html_plugin_element.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_init.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/jank_tracker.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/layout/traced_layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/print_context.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_util.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator_context.h"
#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/page/validation_message_client.h"
#include "third_party/blink/renderer/core/paint/block_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/compositing_inputs_updater.h"
#include "third_party/blink/renderer/core/paint/compositing/graphics_layer_tree_as_text.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/frame_painter.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/paint/paint_tracker.h"
#include "third_party/blink/renderer/core/paint/pre_paint_tree_walk.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_controller.h"
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
#include "third_party/blink/renderer/platform/graphics/link_highlight.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/platform/transforms/transform_state.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

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

// Page dimensions in pixels at 72 DPI.
constexpr int kA4PortraitPageWidth = 595;
constexpr int kA4PortraitPageHeight = 842;
constexpr int kLetterPortraitPageWidth = 612;
constexpr int kLetterPortraitPageHeight = 792;

constexpr char kCssFragmentIdentifierPrefix[] = "targetElement=";
constexpr size_t kCssFragmentIdentifierPrefixLength =
    base::size(kCssFragmentIdentifierPrefix);

// Logs a UseCounter for the size of the cursor that will be set. This will be
// used for compatibility analysis to determine whether the maximum size can be
// reduced.
void LogCursorSizeCounter(LocalFrame* frame, const Cursor& cursor) {
  DCHECK(frame);
  Image* image = cursor.GetImage();
  if (!image)
    return;
  // Should not overflow, this calculation is done elsewhere when determining
  // whether the cursor exceeds its maximum size (see event_handler.cc).
  IntSize scaled_size = image->Size();
  scaled_size.Scale(1 / cursor.ImageScaleFactor());
  if (scaled_size.Width() > 32 || scaled_size.Height() > 32) {
    UseCounter::Count(frame, WebFeature::kCursorImageGT32x32);
  } else {
    UseCounter::Count(frame, WebFeature::kCursorImageLE32x32);
  }
}

}  // namespace

// Defines a UKM that is part of a hierarchical ukm, recorded in
// microseconds equal to the duration of the current lexical scope after
// declaration of the macro. Example usage:
//
// void LocalFrameView::DoExpensiveThing() {
//   SCOPED_UMA_AND_UKM_TIMER(kUkmEnumName);
//   // Do computation of expensive thing
//
// }
//
// |ukm_enum| should be an entry in LocalFrameUkmAggregator's enum of
// metric names (which in turn corresponds to names in from ukm.xml).
#define SCOPED_UMA_AND_UKM_TIMER(ukm_enum) \
  auto scoped_ukm_hierarchical_timer =     \
      EnsureUkmAggregator().GetScopedTimer(static_cast<size_t>(ukm_enum));

using namespace HTMLNames;

// The maximum number of updatePlugins iterations that should be done before
// returning.
static const unsigned kMaxUpdatePluginsIterations = 2;

static bool g_initial_track_all_paint_invalidations = false;

LocalFrameView::LocalFrameView(LocalFrame& frame, IntRect frame_rect)
    : frame_(frame),
      frame_rect_(frame_rect),
      is_attached_(false),
      display_mode_(kWebDisplayModeBrowser),
      can_have_scrollbars_(true),
      has_pending_layout_(false),
      layout_scheduling_enabled_(true),
      layout_count_for_testing_(0),
      lifecycle_update_count_for_testing_(0),
      nested_layout_count_(0),
      update_plugins_timer_(frame.GetTaskRunner(TaskType::kInternalDefault),
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
      fragment_anchor_(nullptr),
      sticky_position_object_count_(0),
      input_events_scale_factor_for_emulation_(1),
      layout_size_fixed_to_frame_size_(true),
      needs_update_geometries_(false),
      root_layer_did_scroll_(false),
      frame_timing_requests_dirty_(true),
      hidden_for_throttling_(false),
      subtree_throttled_(false),
      // The compositor throttles the main frame using deferred commits, we
      // can't throttle it here or it seems the root compositor doesn't get
      // setup properly.
      lifecycle_updates_throttled_(!GetFrame().IsMainFrame()),
      current_update_lifecycle_phases_target_state_(
          DocumentLifecycle::kUninitialized),
      past_layout_lifecycle_update_(false),
      suppress_adjust_view_size_(false),
      intersection_observation_state_(kNotNeeded),
      needs_forced_compositing_update_(false),
      needs_focus_on_fragment_(false),
      tracked_object_paint_invalidations_(
          base::WrapUnique(g_initial_track_all_paint_invalidations
                               ? new Vector<ObjectPaintInvalidation>
                               : nullptr)),
      main_thread_scrolling_reasons_(0),
      forced_layout_stack_depth_(0),
      forced_layout_start_time_(TimeTicks()),
      paint_frame_count_(0),
      unique_id_(NewUniqueObjectId()),
      jank_tracker_(std::make_unique<JankTracker>(this)),
      paint_tracker_(new PaintTracker(this)) {
  // Propagate the marginwidth/height and scrolling modes to the view.
  if (frame_->Owner() &&
      frame_->Owner()->ScrollingMode() == kScrollbarAlwaysOff)
    SetCanHaveScrollbars(false);
}

LocalFrameView* LocalFrameView::Create(LocalFrame& frame) {
  LocalFrameView* view = new LocalFrameView(frame, IntRect());
  view->Show();
  return view;
}

LocalFrameView* LocalFrameView::Create(LocalFrame& frame,
                                       const IntSize& initial_size) {
  LocalFrameView* view =
      new LocalFrameView(frame, IntRect(IntPoint(), initial_size));
  view->SetLayoutSizeInternal(initial_size);

  view->Show();
  return view;
}

LocalFrameView::~LocalFrameView() {
#if DCHECK_IS_ON()
  DCHECK(has_been_disposed_);
#endif
}

void LocalFrameView::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(parent_);
  visitor->Trace(fragment_anchor_);
  visitor->Trace(scrollable_areas_);
  visitor->Trace(animating_scrollable_areas_);
  visitor->Trace(auto_size_info_);
  visitor->Trace(plugins_);
  visitor->Trace(scrollbars_);
  visitor->Trace(viewport_scrollable_area_);
  visitor->Trace(visibility_observer_);
  visitor->Trace(anchoring_adjustment_queue_);
  visitor->Trace(print_context_);
  visitor->Trace(paint_tracker_);
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
}

template <typename Function>
void LocalFrameView::ForAllChildLocalFrameViews(const Function& function) {
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (!child->IsLocalFrame())
      continue;
    if (LocalFrameView* child_view = ToLocalFrame(child)->View())
      function(*child_view);
  }
}

// Call function for each non-throttled frame view in pre tree order.
template <typename Function>
void LocalFrameView::ForAllNonThrottledLocalFrameViews(
    const Function& function) {
  if (ShouldThrottleRendering())
    return;

  function(*this);

  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (!child->IsLocalFrame())
      continue;
    if (LocalFrameView* child_view = ToLocalFrame(child)->View())
      child_view->ForAllNonThrottledLocalFrameViews(function);
  }
}

void LocalFrameView::SetupRenderThrottling() {
  if (visibility_observer_)
    return;

  // We observe the frame owner element instead of the document element, because
  // if the document has no content we can falsely think the frame is invisible.
  // Note that this means we cannot throttle top-level frames or (currently)
  // frames whose owner element is remote.
  Element* target_element = GetFrame().DeprecatedLocalOwner();
  if (!target_element)
    return;

  visibility_observer_ = new ElementVisibilityObserver(
      target_element, WTF::BindRepeating(
                          [](LocalFrameView* frame_view, bool is_visible) {
                            if (!frame_view)
                              return;
                            frame_view->UpdateRenderThrottlingStatus(
                                !is_visible, frame_view->subtree_throttled_);
                          },
                          WrapWeakPersistent(this)));
  visibility_observer_->Start();
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

  ClearPrintContext();

  ukm_aggregator_.reset();
  jank_tracker_->Dispose();

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

void LocalFrameView::SetLifecycleUpdatesThrottledForTesting() {
  lifecycle_updates_throttled_ = true;
}

void LocalFrameView::InvalidateRect(const IntRect& rect) {
  auto* layout_object = frame_->OwnerLayoutObject();
  if (!layout_object)
    return;

  IntRect paint_invalidation_rect = rect;
  paint_invalidation_rect.Move(
      (layout_object->BorderLeft() + layout_object->PaddingLeft()).ToInt(),
      (layout_object->BorderTop() + layout_object->PaddingTop()).ToInt());
  layout_object->InvalidatePaintRectangle(LayoutRect(paint_invalidation_rect));
}

void LocalFrameView::SetFrameRect(const IntRect& unclamped_frame_rect) {
  IntRect frame_rect(SaturatedRect(unclamped_frame_rect));
  if (frame_rect == frame_rect_)
    return;
  const bool width_changed = frame_rect_.Width() != frame_rect.Width();
  const bool height_changed = frame_rect_.Height() != frame_rect.Height();
  frame_rect_ = frame_rect;

  FrameRectsChanged();

  if (auto* layout_view = GetLayoutView())
    layout_view->SetShouldCheckForPaintInvalidation();

  if (width_changed || height_changed) {
    ViewportSizeChanged(width_changed, height_changed);

    if (frame_->IsMainFrame())
      frame_->GetPage()->GetVisualViewport().MainFrameDidChangeSize();

    GetFrame().Loader().RestoreScrollPositionAndViewState();
  }
}

IntPoint LocalFrameView::Location() const {
  IntPoint location(frame_rect_.Location());

  // As an optimization, we don't include the root layer's scroll offset in the
  // frame rect.  As a result, we don't need to recalculate the frame rect every
  // time the root layer scrolls, but we need to add it in here.
  LayoutEmbeddedContent* owner = frame_->OwnerLayoutObject();
  if (owner) {
    LayoutView* owner_layout_view = owner->View();
    DCHECK(owner_layout_view);
    if (owner_layout_view->HasOverflowClip()) {
      IntSize scroll_offset(owner_layout_view->ScrolledContentOffset());
      location.SaturatedMove(-scroll_offset.Width(), -scroll_offset.Height());
    }
  }
  return location;
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

CompositorAnimationHost* LocalFrameView::GetCompositorAnimationHost() const {
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
  SetLayoutOverflowSize(layout_view->DocumentRect().Size());
}

void LocalFrameView::AdjustViewSizeAndLayout() {
  AdjustViewSize();
  if (NeedsLayout()) {
    base::AutoReset<bool> suppress_adjust_view_size(&suppress_adjust_view_size_,
                                                    true);
    UpdateLayout();
  }
}

void LocalFrameView::UpdateAcceleratedCompositingSettings() {
  if (auto* layout_view = GetLayoutView())
    layout_view->Compositor()->UpdateAcceleratedCompositingSettings();
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
    document->MediaQueryAffectingValueChanged();
  } else if (was_resized) {
    document->EvaluateMediaQueryList();
  }

  document->UpdateStyleAndLayoutTree();
  Lifecycle().AdvanceTo(DocumentLifecycle::kStyleClean);

  if (was_resized)
    document->ClearResizedForViewportUnits();
}

void LocalFrameView::LayoutFromRootObject(LayoutObject& root) {
  LayoutState layout_state(root);
  if (!root.IsBoxModelObject()) {
    root.UpdateLayout();
  } else {
    // Laying out the root may change its visual overflow. If so, that
    // visual overflow needs to propagate to its containing block.
    LayoutBoxModelObject& box_object = ToLayoutBoxModelObject(root);
    LayoutRect previous_visual_overflow_rect = box_object.VisualOverflowRect();
    box_object.UpdateLayout();
    if (box_object.VisualOverflowRect() != previous_visual_overflow_rect) {
      // TODO(chrishtr): this can probably just set the dirty bit for
      // visual overflow, not layout overflow.
      box_object.SetNeedsOverflowRecalc();
      GetLayoutView()->RecalcOverflow();
    }
  }
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
    return TracedValue::Create();
  std::unique_ptr<TracedValue> value = analyzer_->ToTracedValue();
  value->SetString("host", GetLayoutView()->GetDocument().location()->host());
  value->SetString(
      "frame",
      String::Format("0x%" PRIxPTR, reinterpret_cast<uintptr_t>(frame_.Get())));
  value->SetInteger("contentsHeightAfterLayout",
                    GetLayoutView()->DocumentRect().Height());
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

  int contents_height_before_layout = GetLayoutView()->DocumentRect().Height();
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
        if (!root->NeedsLayout())
          continue;
        LayoutFromRootObject(*root);

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

  {
    ScriptForbiddenScope forbid_script;

    if (IsInPerformLayout() || ShouldThrottleRendering() ||
        !frame_->GetDocument()->IsActive())
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
                       InspectorLayoutEvent::BeginData(this));
    probe::UpdateLayout probe(document);

    PerformPreLayoutTasks();

    VisualViewport& visual_viewport = frame_->GetPage()->GetVisualViewport();
    DoubleSize viewport_size(visual_viewport.VisibleWidthCSSPx(),
                             visual_viewport.VisibleHeightCSSPx());

    // TODO(crbug.com/460956): The notion of a single root for layout is no
    // longer applicable. Remove or update this code.
    LayoutObject* root_for_this_layout = GetLayoutView();

    FontCachePurgePreventer font_cache_purge_preventer;
    {
      base::AutoReset<bool> change_scheduling_enabled(
          &layout_scheduling_enabled_, false);
      nested_layout_count_++;

      // If the layout view was marked as needing layout after we added items in
      // the subtree roots we need to clear the roots and do the layout from the
      // layoutView.
      if (GetLayoutView()->NeedsLayout())
        ClearLayoutSubtreeRootsAndMarkContainingBlocks();
      GetLayoutView()->ClearHitTestCache();

      bool in_subtree_layout = IsSubtreeLayout();

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
          if (IsHTMLFrameSetElement(*body)) {
            body->GetLayoutObject()->SetChildNeedsLayout();
          } else if (IsHTMLBodyElement(*body)) {
            if (!first_layout_ && size_.Height() != GetLayoutSize().Height() &&
                body->GetLayoutObject()->EnclosingBox()->StretchesToViewport())
              body->GetLayoutObject()->SetChildNeedsLayout();
          }
        }

        if (first_layout_) {
          first_layout_ = false;
          last_viewport_size_ = GetLayoutSize();
          last_zoom_factor_ = GetLayoutView()->StyleRef().Zoom();

          ScrollbarMode h_mode;
          ScrollbarMode v_mode;
          GetLayoutView()->CalculateScrollbarModes(h_mode, v_mode);
          if (v_mode == kScrollbarAuto) {
            GetLayoutView()
                ->GetScrollableArea()
                ->ForceVerticalScrollbarForFirstLayout();
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
        SetNeedsLayout();
        MarkViewportConstrainedObjectsForLayout(
            old_size.Width() != new_size.Width(),
            old_size.Height() != new_size.Height());
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

    // FIXME: Could find the common ancestor layer of all dirty subtrees and
    // mark from there. crbug.com/462719
    GetLayoutView()->EnclosingLayer()->UpdateLayerPositionsAfterLayout();

    TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
        TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.trees"), "LayoutTree",
        this, TracedLayoutObject::Create(*GetLayoutView(), true));

    GetLayoutView()->Compositor()->DidLayout();
    layout_count_for_testing_++;

    if (AXObjectCache* cache = document->ExistingAXObjectCache()) {
      const KURL& url = document->Url();
      if (url.IsValid() && !url.IsAboutBlankURL()) {
        cache->HandleLayoutComplete(document);
        cache->ProcessUpdatesAfterLayout(*document);
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
                     InspectorLayoutEvent::EndData(root_for_this_layout));
    probe::didChangeViewport(frame_.Get());

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
  }  // ScriptForbiddenScope

  GetFrame().GetDocument()->LayoutUpdated();
  CheckDoesNotNeedLayout();
}

void LocalFrameView::WillStartForcedLayout() {
  // UpdateLayoutIgnoringPendingStyleSheets is re-entrant for auto-sizing
  // and plugins. So keep track of stack depth.
  forced_layout_stack_depth_++;
  if (forced_layout_stack_depth_ > 1)
    return;
  forced_layout_start_time_ = CurrentTimeTicks();
}

void LocalFrameView::DidFinishForcedLayout() {
  CHECK_GT(forced_layout_stack_depth_, (unsigned)0);
  forced_layout_stack_depth_--;
  if (!forced_layout_stack_depth_ && base::TimeTicks::IsHighResolution()) {
    LocalFrameUkmAggregator& aggregator = EnsureUkmAggregator();
    aggregator.RecordSample(
        (size_t)LocalFrameUkmAggregator::kForcedStyleAndLayout,
        forced_layout_start_time_, CurrentTimeTicks());
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
      layout_size.Expand(
          0, browser_controls.TotalHeight() / page_scale_at_layout_width);
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
  LayoutEmbeddedContent* layout = frame_->OwnerLayoutObject();
  if (!layout)
    return;

  bool did_need_layout = NeedsLayout();

  LayoutRect new_frame = layout->ReplacedContentRect();
  DCHECK(new_frame.Size() == RoundedIntSize(new_frame.Size()));
  bool bounds_will_change = LayoutSize(Size()) != new_frame.Size();

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
  DCHECK(IsInPerformLayout());
  // Tell the DOM element that it needs a Plugin update.
  Node* node = object.GetNode();
  DCHECK(node);
  if (IsHTMLObjectElement(*node) || IsHTMLEmbedElement(*node))
    ToHTMLPlugInElement(node)->SetNeedsPluginUpdate(true);

  part_update_set_.insert(&object);
}

void LocalFrameView::SetDisplayMode(WebDisplayMode mode) {
  if (mode == display_mode_)
    return;

  display_mode_ = mode;

  if (frame_->GetDocument())
    frame_->GetDocument()->MediaQueryAffectingValueChanged();
}

void LocalFrameView::SetDisplayShape(DisplayShape display_shape) {
  if (display_shape == display_shape_)
    return;

  display_shape_ = display_shape;

  if (frame_->GetDocument())
    frame_->GetDocument()->MediaQueryAffectingValueChanged();
}

void LocalFrameView::SetMediaType(const AtomicString& media_type) {
  DCHECK(frame_->GetDocument());
  media_type_ = media_type;
  frame_->GetDocument()->MediaQueryAffectingValueChanged();
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

  frame_->GetDocument()->SetNeedsStyleRecalc(
      kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                               style_change_reason::kStyleSheetChange));
}

void LocalFrameView::AddBackgroundAttachmentFixedObject(LayoutObject* object) {
  DCHECK(!background_attachment_fixed_objects_.Contains(object));

  background_attachment_fixed_objects_.insert(object);
  if (ScrollingCoordinator* scrolling_coordinator =
          this->GetScrollingCoordinator()) {
    scrolling_coordinator
        ->FrameViewHasBackgroundAttachmentFixedObjectsDidChange(this);
  }

  // Ensure main thread scrolling reasons are recomputed.
  SetNeedsPaintPropertyUpdate();
  // The object's scroll properties are not affected by its own background.
  object->SetAncestorsNeedPaintPropertyUpdateForMainThreadScrolling();
}

void LocalFrameView::RemoveBackgroundAttachmentFixedObject(
    LayoutObject* object) {
  DCHECK(background_attachment_fixed_objects_.Contains(object));

  background_attachment_fixed_objects_.erase(object);
  if (ScrollingCoordinator* scrolling_coordinator =
          this->GetScrollingCoordinator()) {
    scrolling_coordinator
        ->FrameViewHasBackgroundAttachmentFixedObjectsDidChange(this);
  }

  // Ensure main thread scrolling reasons are recomputed.
  SetNeedsPaintPropertyUpdate();
  // The object's scroll properties are not affected by its own background.
  object->SetAncestorsNeedPaintPropertyUpdateForMainThreadScrolling();
}

void LocalFrameView::AddViewportConstrainedObject(LayoutObject& object) {
  if (!viewport_constrained_objects_) {
    viewport_constrained_objects_ =
        std::make_unique<ViewportConstrainedObjectSet>();
  }

  if (!viewport_constrained_objects_->Contains(&object)) {
    viewport_constrained_objects_->insert(&object);

    if (ScrollingCoordinator* scrolling_coordinator =
            this->GetScrollingCoordinator())
      scrolling_coordinator->FrameViewFixedObjectsDidChange(this);
  }
}

void LocalFrameView::RemoveViewportConstrainedObject(LayoutObject& object) {
  if (viewport_constrained_objects_ &&
      viewport_constrained_objects_->Contains(&object)) {
    viewport_constrained_objects_->erase(&object);

    if (ScrollingCoordinator* scrolling_coordinator =
            this->GetScrollingCoordinator())
      scrolling_coordinator->FrameViewFixedObjectsDidChange(this);
  }
}

void LocalFrameView::ViewportSizeChanged(bool width_changed,
                                         bool height_changed) {
  DCHECK(width_changed || height_changed);
  DCHECK(frame_->GetPage());
  if (frame_->GetDocument() &&
      frame_->GetDocument()->Lifecycle().LifecyclePostponed())
    return;

  auto* layout_view = GetLayoutView();
  if (layout_view) {
    // If this is the main frame, we might have got here by hiding/showing the
    // top controls.  In that case, layout won't be triggered, so we need to
    // clamp the scroll offset here.
    if (GetFrame().IsMainFrame()) {
      layout_view->Layer()->UpdateSize();
      layout_view->GetScrollableArea()->ClampScrollOffsetAfterOverflowChange();
    }

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
            LayoutInvalidationReason::kSizeChanged);
      }
    }
    if (height_changed) {
      if (style.Height().IsFixed() &&
          (style.Top().IsAuto() || style.Bottom().IsAuto())) {
        layout_object->SetNeedsPositionedMovementLayout();
      } else {
        layout_object->SetNeedsLayoutAndFullPaintInvalidation(
            LayoutInvalidationReason::kSizeChanged);
      }
    }
  }
}

bool LocalFrameView::ShouldSetCursor() const {
  Page* page = GetFrame().GetPage();
  return page &&
         page->VisibilityState() != mojom::PageVisibilityState::kHidden &&
         !frame_->GetEventHandler().IsMousePositionUnknown() &&
         page->GetFocusController().IsActive();
}

void LocalFrameView::NotifyFrameRectsChangedIfNeededRecursive() {
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.NotifyFrameRectsChangedIfNeeded();
  });
}

void LocalFrameView::InvalidateBackgroundAttachmentFixedDescendants(
    const LayoutObject& object) {
  for (auto* const layout_object : background_attachment_fixed_objects_) {
    if (object != GetLayoutView() && !layout_object->IsDescendantOf(&object))
      continue;
    layout_object->SetBackgroundNeedsFullPaintInvalidation();
  }
}

bool LocalFrameView::HasBackgroundAttachmentFixedDescendants(
    const LayoutObject& object) const {
  if (object == GetLayoutView())
    return !background_attachment_fixed_objects_.IsEmpty();

  for (const auto* potential_descendant :
       background_attachment_fixed_objects_) {
    if (potential_descendant == &object)
      continue;
    if (potential_descendant->IsDescendantOf(&object))
      return true;
  }
  return false;
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

    if (layer->IsPaintInvalidationContainer())
      continue;

    // If the layer has no visible content, then we shouldn't invalidate; but
    // if we're not compositing-inputs-clean, then we can't query
    // layer->SubtreeIsInvisible() here.
    layout_object->SetSubtreeShouldCheckForPaintInvalidation();
    if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
        !layer->NeedsRepaint()) {
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

    TRACE_EVENT_INSTANT1(
        TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
        "ScrollInvalidationTracking", TRACE_EVENT_SCOPE_THREAD, "data",
        InspectorScrollInvalidationTrackingEvent::Data(*layout_object));

    // If the fixed layer has a blur/drop-shadow filter applied on at least one
    // of its parents, we cannot scroll using the fast path, otherwise the
    // outsets of the filter will be moved around the page.
    if (layer->HasAncestorWithFilterThatMovesPixels())
      fast_path_allowed = false;
  }
  return fast_path_allowed;
}

void LocalFrameView::ProcessUrlFragment(const KURL& url,
                                        UrlFragmentBehavior behavior) {
  // If our URL has no ref, then we have no place we need to jump to.
  // OTOH If CSS target was set previously, we want to set it to 0, recalc
  // and possibly paint invalidation because :target pseudo class may have been
  // set (see bug 11321).
  // Similarly for svg, if we had a previous svgView() then we need to reset
  // the initial view if we don't have a fragment.
  if (!url.HasFragmentIdentifier() && !frame_->GetDocument()->CssTarget() &&
      !frame_->GetDocument()->IsSVGDocument())
    return;

  UseCounter::Count(&GetFrame(), WebFeature::kScrollToFragmentRequested);
  // Try the raw fragment for HTML documents, but skip it for `svgView()`:
  String fragment_identifier = url.FragmentIdentifier();
  if (!frame_->GetDocument()->IsSVGDocument() &&
      ProcessUrlFragmentHelper(fragment_identifier, behavior)) {
    UseCounter::Count(&GetFrame(), WebFeature::kScrollToFragmentSucceedWithRaw);
    return;
  }

  // Try again after decoding the fragment.
  if (frame_->GetDocument()->Encoding().IsValid()) {
    DecodeURLResult decode_result;
    if (ProcessUrlFragmentHelper(
            DecodeURLEscapeSequences(fragment_identifier, &decode_result),
            behavior)) {
      switch (decode_result) {
        case DecodeURLResult::kAsciiOnly:
          UseCounter::Count(&GetFrame(),
                            WebFeature::kScrollToFragmentSucceedWithASCII);
          break;
        case DecodeURLResult::kUTF8:
          UseCounter::Count(&GetFrame(),
                            WebFeature::kScrollToFragmentSucceedWithUTF8);
          break;
        case DecodeURLResult::kIsomorphic:
          UseCounter::Count(&GetFrame(),
                            WebFeature::kScrollToFragmentSucceedWithIsomorphic);
          break;
      }
    } else {
      switch (decode_result) {
        case DecodeURLResult::kAsciiOnly:
          UseCounter::Count(&GetFrame(),
                            WebFeature::kScrollToFragmentFailWithASCII);
          break;
        case DecodeURLResult::kUTF8:
          UseCounter::Count(&GetFrame(),
                            WebFeature::kScrollToFragmentFailWithUTF8);
          break;
        case DecodeURLResult::kIsomorphic:
          UseCounter::Count(&GetFrame(),
                            WebFeature::kScrollToFragmentFailWithIsomorphic);
          break;
      }
    }
  } else {
    UseCounter::Count(&GetFrame(),
                      WebFeature::kScrollToFragmentFailWithInvalidEncoding);
  }
}

bool LocalFrameView::ProcessUrlFragmentHelper(const String& name,
                                              UrlFragmentBehavior behavior) {
  DCHECK(frame_->GetDocument());

  Element* anchor_node;
  String selector;
  if (RuntimeEnabledFeatures::CSSFragmentIdentifiersEnabled() &&
      ParseCSSFragmentIdentifier(name, &selector)) {
    anchor_node =
        FindCSSFragmentAnchor(AtomicString(selector), frame_->GetDocument());
  } else {
    anchor_node = frame_->GetDocument()->FindAnchor(name);
  }

  // Setting to null will clear the current target.
  frame_->GetDocument()->SetCSSTarget(anchor_node);

  if (frame_->GetDocument()->IsSVGDocument()) {
    if (SVGSVGElement* svg =
            ToSVGSVGElementOrNull(frame_->GetDocument()->documentElement())) {
      svg->SetupInitialView(name, anchor_node);
      if (!anchor_node)
        return false;
    }
    // If this is not the top-level frame, then don't scroll to the
    // anchor position.
    if (!frame_->IsMainFrame())
      return false;
  }

  // Implement the rule that "" and "top" both mean top of page as in other
  // browsers.
  if (!anchor_node &&
      !(name.IsEmpty() || DeprecatedEqualIgnoringCase(name, "top")))
    return false;

  if (anchor_node)
    anchor_node->DispatchActivateInvisibleEventIfNeeded();

  if (behavior == kUrlFragmentDontScroll)
    return true;

  if (!anchor_node) {
    fragment_anchor_ = frame_->GetDocument();
    needs_focus_on_fragment_ = false;
  } else {
    fragment_anchor_ = anchor_node;
    needs_focus_on_fragment_ = true;
  }

  // If rendering is blocked, we'll necessarily have a layout to kick off the
  // scroll and focus.
  if (frame_->GetDocument()->IsRenderingReady()) {
    frame_->GetDocument()->UpdateStyleAndLayoutTree();

    // If layout is needed, we will scroll in performPostLayoutTasks. Otherwise,
    // scroll and focus immediately.
    if (NeedsLayout())
      UpdateLayout();
    else
      ScrollAndFocusFragmentAnchor();
  }

  return true;
}

Element* LocalFrameView::FindCSSFragmentAnchor(const AtomicString& selector,
                                               Document* document) {
  DummyExceptionStateForTesting exception_state;
  return document->QuerySelector(selector, exception_state);
}

bool LocalFrameView::ParseCSSFragmentIdentifier(const String& fragment,
                                                String* selector) {
  size_t pos = fragment.Find(kCssFragmentIdentifierPrefix);
  if (pos == 0) {
    *selector = fragment.Substring(kCssFragmentIdentifierPrefixLength - 1);
    return true;
  }

  return false;
}

void LocalFrameView::ClearFragmentAnchor() {
  fragment_anchor_ = nullptr;
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

  // If there is a pending layout, the fragment anchor will be cleared when it
  // finishes.
  if (!NeedsLayout())
    ClearFragmentAnchor();
}

void LocalFrameView::ClearLayoutSubtreeRoot(const LayoutObject& root) {
  layout_subtree_root_list_.Remove(const_cast<LayoutObject&>(root));
}

void LocalFrameView::ClearLayoutSubtreeRootsAndMarkContainingBlocks() {
  layout_subtree_root_list_.ClearAndMarkContainingBlocksForLayout();
}

void LocalFrameView::AddOrthogonalWritingModeRoot(LayoutBox& root) {
  DCHECK(!root.IsLayoutScrollbarPart());
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
    if ((cb->NormalChildNeedsLayout() || cb->SelfNeedsLayout()) &&
        cb->IsLayoutBlockFlow()) {
      ToLayoutBlockFlow(cb)->RemoveFloatingObjectsFromDescendants();
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

  // Do not pre-layout objects that are fully managed by LayoutNG; it is not
  // necessary and may lead to double layouts. We do need to pre-layout
  // objects whose containing block is a legacy object so that it can
  // properly compute its intrinsic size.
  if (RuntimeEnabledFeatures::LayoutNGEnabled() && IsManagedByLayoutNG(root))
    return false;

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
                       InspectorInvalidateLayoutEvent::Data(frame_.Get()));

  ClearLayoutSubtreeRootsAndMarkContainingBlocks();

  if (has_pending_layout_)
    return;
  has_pending_layout_ = true;

  if (!ShouldThrottleRendering())
    GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());
}

void LocalFrameView::ScheduleRelayoutOfSubtree(LayoutObject* relayout_root) {
  DCHECK(frame_->View() == this);

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

    Lifecycle().EnsureStateAtMost(DocumentLifecycle::kStyleClean);
  }
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "InvalidateLayout", TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorInvalidateLayoutEvent::Data(frame_.Get()));
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
  layout_view->SetNeedsLayout(LayoutInvalidationReason::kUnknown);
}

bool LocalFrameView::HasOpaqueBackground() const {
  return !base_background_color_.HasAlpha();
}

Color LocalFrameView::BaseBackgroundColor() const {
  return base_background_color_;
}

void LocalFrameView::SetBaseBackgroundColor(const Color& background_color) {
  if (base_background_color_ == background_color)
    return;

  base_background_color_ = background_color;

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

  if (!ShouldThrottleRendering())
    GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());
}

void LocalFrameView::UpdateBaseBackgroundColorRecursively(
    const Color& base_background_color) {
  ForAllNonThrottledLocalFrameViews(
      [base_background_color](LocalFrameView& frame_view) {
        frame_view.SetBaseBackgroundColor(base_background_color);
      });
}

void LocalFrameView::ScrollAndFocusFragmentAnchor() {
  Node* anchor_node = fragment_anchor_;
  if (!anchor_node)
    return;

  if (!frame_->GetDocument()->IsRenderingReady())
    return;

  if (anchor_node->GetLayoutObject()) {
    LayoutRect rect;
    if (anchor_node != frame_->GetDocument()) {
      rect = anchor_node->BoundingBoxForScrollIntoView();
    } else {
      if (Element* document_element = frame_->GetDocument()->documentElement())
        rect = document_element->BoundingBoxForScrollIntoView();
    }

    Frame* boundary_frame = frame_->FindUnsafeParentScrollPropagationBoundary();

    // FIXME: Handle RemoteFrames
    if (boundary_frame && boundary_frame->IsLocalFrame()) {
      ToLocalFrame(boundary_frame)
          ->View()
          ->SetSafeToPropagateScrollToParent(false);
    }

    Element* anchor_element = anchor_node->IsElementNode()
                                  ? ToElement(anchor_node)
                                  : frame_->GetDocument()->documentElement();
    if (anchor_element) {
      ScrollIntoViewOptions options;
      options.setBlock("start");
      options.setInlinePosition("nearest");
      anchor_element->ScrollIntoViewNoVisualUpdate(options);
    }

    if (boundary_frame && boundary_frame->IsLocalFrame()) {
      ToLocalFrame(boundary_frame)
          ->View()
          ->SetSafeToPropagateScrollToParent(true);
    }

    if (AXObjectCache* cache = frame_->GetDocument()->ExistingAXObjectCache())
      cache->HandleScrolledToAnchor(anchor_node);

    // If the anchor accepts keyboard focus and fragment scrolling is allowed,
    // move focus there to aid users relying on keyboard navigation.
    // If anchorNode is not focusable or fragment scrolling is not allowed,
    // clear focus, which matches the behavior of other browsers.
    if (needs_focus_on_fragment_) {
      if (anchor_node->IsElementNode() &&
          ToElement(anchor_node)->IsFocusable()) {
        ToElement(anchor_node)->focus();
      } else {
        frame_->GetDocument()->SetSequentialFocusNavigationStartingPoint(
            anchor_node);
        frame_->GetDocument()->ClearFocusedElement();
      }
      needs_focus_on_fragment_ = false;
    }
  }

  // The fragment anchor should only be maintained while the frame is still
  // loading.  If the frame is done loading, clear the anchor now. Otherwise,
  // restore it since it may have been cleared during scrollRectToVisible.
  fragment_anchor_ =
      frame_->GetDocument()->IsLoadCompleted() ? nullptr : anchor_node;
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
    HTMLPlugInElement* element = ToHTMLPlugInElement(object.GetNode());

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
  update_plugins_timer_.StartOneShot(TimeDelta(), FROM_HERE);
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
  frame_->GetEventHandler().MayUpdateHoverWhenContentUnderMouseChanged(
      MouseEventManager::UpdateHoverReason::kLayoutOrStyleChanged);

  UpdateGeometriesIfNeeded();

  // Plugins could have torn down the page inside updateGeometries().
  if (!GetLayoutView())
    return;

  ScheduleUpdatePluginsIfNecessary();

  if (ScrollingCoordinator* scrolling_coordinator =
          this->GetScrollingCoordinator()) {
    scrolling_coordinator->NotifyGeometryChanged(this);
  }

  if (SnapCoordinator* snap_coordinator =
          frame_->GetDocument()->GetSnapCoordinator())
    snap_coordinator->UpdateAllSnapContainerData();

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
    probe::didResizeMainFrame(frame_.Get());
}

void LocalFrameView::SetInputEventsScaleForEmulation(
    float content_scale_factor) {
  input_events_scale_factor_for_emulation_ = content_scale_factor;
}

float LocalFrameView::InputEventsScaleFactor() const {
  float page_scale = frame_->GetPage()->GetVisualViewport().Scale();
  return page_scale * input_events_scale_factor_for_emulation_;
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

    RootFrameViewport* root_frame_viewport =
        RootFrameViewport::Create(visual_viewport, *layout_viewport);
    viewport_scrollable_area_ = root_frame_viewport;

    page->GlobalRootScrollerController().InitializeViewportScrollCallback(
        *root_frame_viewport);
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
}

LocalFrameView* LocalFrameView::ParentFrameView() const {
  if (!is_attached_)
    return nullptr;

  Frame* parent_frame = frame_->Tree().Parent();
  if (parent_frame && parent_frame->IsLocalFrame())
    return ToLocalFrame(parent_frame)->View();

  return nullptr;
}

void LocalFrameView::VisualViewportScrollbarsChanged() {
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
}

void LocalFrameView::UpdateAllLifecyclePhases() {
  GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kPaintClean);
}

// TODO(chrishtr): add a scrolling update lifecycle phase.
bool LocalFrameView::UpdateLifecycleToCompositingCleanPlusScrolling() {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    return UpdateAllLifecyclePhasesExceptPaint();
  } else {
    return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
        DocumentLifecycle::kCompositingClean);
  }
}

bool LocalFrameView::UpdateLifecycleToCompositingInputsClean() {
  // When SPv2 is enabled, the standard compositing lifecycle steps do not
  // exist; compositing is done after paint instead.
  DCHECK(!RuntimeEnabledFeatures::SlimmingPaintV2Enabled());
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kCompositingInputsClean);
}

bool LocalFrameView::UpdateAllLifecyclePhasesExceptPaint() {
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kPrePaintClean);
}

void LocalFrameView::UpdateLifecyclePhasesForPrinting() {
  auto* local_frame_view_root = GetFrame().LocalFrameRoot().View();
  local_frame_view_root->UpdateLifecyclePhases(
      DocumentLifecycle::kPrePaintClean);

  auto* detached_frame_view = this;
  while (detached_frame_view->is_attached_ &&
         detached_frame_view != local_frame_view_root)
    detached_frame_view = detached_frame_view->parent_.Get();

  if (detached_frame_view == local_frame_view_root)
    return;
  DCHECK(!detached_frame_view->is_attached_);

  // We are printing a detached frame or a descendant of a detached frame which
  // was not reached in some phases during during |local_frame_view_root->
  // UpdateLifecyclePhasesnormal()|. We need the subtree to be ready for
  // painting.
  detached_frame_view->UpdateLifecyclePhases(DocumentLifecycle::kPrePaintClean);
}

bool LocalFrameView::UpdateLifecycleToLayoutClean() {
  return GetFrame().LocalFrameRoot().View()->UpdateLifecyclePhases(
      DocumentLifecycle::kLayoutClean);
}

void LocalFrameView::RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) {
  LocalFrameUkmAggregator& ukm_aggregator = EnsureUkmAggregator();
  ukm_aggregator.RecordPrimarySample(frame_begin_time, CurrentTimeTicks());
}

void LocalFrameView::ScheduleVisualUpdateForPaintInvalidationIfNeeded() {
  LocalFrame& local_frame_root = GetFrame().LocalFrameRoot();
  if (local_frame_root.View()->current_update_lifecycle_phases_target_state_ <
          DocumentLifecycle::kPrePaintClean ||
      Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean) {
    // Schedule visual update to process the paint invalidation in the next
    // cycle.
    local_frame_root.ScheduleVisualUpdateUnlessThrottled();
  }
  // Otherwise the paint invalidation will be handled in the pre-paint
  // phase of this cycle.
}

void LocalFrameView::NotifyResizeObservers() {
  // Controller exists only if ResizeObserver was created.
  if (!GetFrame().GetDocument()->GetResizeObserverController())
    return;

  ResizeObserverController& resize_controller =
      frame_->GetDocument()->EnsureResizeObserverController();

  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean);

  size_t min_depth = 0;
  for (min_depth = resize_controller.GatherObservations(0);
       min_depth != ResizeObserverController::kDepthBottom;
       min_depth = resize_controller.GatherObservations(min_depth)) {
    resize_controller.DeliverObservations();
    GetFrame().GetDocument()->UpdateStyleAndLayout();
  }

  if (resize_controller.SkippedObservations()) {
    resize_controller.ClearObservations();
    ErrorEvent* error = ErrorEvent::Create(
        "ResizeObserver loop limit exceeded",
        SourceLocation::Capture(frame_->GetDocument()), nullptr);
    // We're using |kSharableCrossOrigin| as the error is made by blink itself.
    // TODO(yhirano): Reconsider this.
    frame_->GetDocument()->DispatchErrorEvent(error, kSharableCrossOrigin);
    // Ensure notifications will get delivered in next cycle.
    ScheduleAnimation();
  }

  DCHECK(!GetLayoutView()->NeedsLayout());
}

void LocalFrameView::DispatchEventsForPrintingOnAllFrames() {
  DCHECK(frame_->IsMainFrame());
  for (Frame* current_frame = frame_; current_frame;
       current_frame = current_frame->Tree().TraverseNext(frame_)) {
    if (current_frame->IsLocalFrame())
      ToLocalFrame(current_frame)->GetDocument()->DispatchEventsForPrinting();
  }
}

void LocalFrameView::SetupPrintContext() {
  if (frame_->GetDocument()->Printing())
    return;
  if (!print_context_) {
    print_context_ = new PrintContext(frame_, /*use_printing_layout=*/true);
  }
  if (frame_->GetSettings())
    frame_->GetSettings()->SetShouldPrintBackgrounds(true);
  bool is_us = DefaultLanguage() == "en-US";
  int width = is_us ? kLetterPortraitPageWidth : kA4PortraitPageWidth;
  int height = is_us ? kLetterPortraitPageHeight : kA4PortraitPageHeight;
  print_context_->BeginPrintMode(width, height);
  print_context_->ComputePageRects(FloatSize(width, height));
  DispatchEventsForPrintingOnAllFrames();
}

void LocalFrameView::ClearPrintContext() {
  if (!print_context_)
    return;
  print_context_->EndPrintMode();
  print_context_.Clear();
}

// TODO(leviw): We don't assert lifecycle information from documents in child
// WebPluginContainerImpls.
bool LocalFrameView::UpdateLifecyclePhases(
    DocumentLifecycle::LifecycleState target_state) {
  // If the lifecycle is postponed, which can happen if the inspector requests
  // it, then we shouldn't update any lifecycle phases.
  if (UNLIKELY(frame_->GetDocument() &&
               frame_->GetDocument()->Lifecycle().LifecyclePostponed())) {
    return false;
  }

  // Prevent reentrance.
  // TODO(vmpstr): Should we just have a DCHECK instead here?
  if (UNLIKELY(current_update_lifecycle_phases_target_state_ !=
               DocumentLifecycle::kUninitialized)) {
    NOTREACHED()
        << "LocalFrameView::updateLifecyclePhasesInternal() reentrance";
    return false;
  }

  // This must be called from the root frame, or a detached frame for printing,
  // since it recurses down, not up. Otherwise the lifecycles of the frames
  // might be out of sync.
  DCHECK(frame_->IsLocalRoot() || !is_attached_);

  // Only the following target states are supported.
  DCHECK(target_state == DocumentLifecycle::kLayoutClean ||
         target_state == DocumentLifecycle::kCompositingInputsClean ||
         target_state == DocumentLifecycle::kCompositingClean ||
         target_state == DocumentLifecycle::kPrePaintClean ||
         target_state == DocumentLifecycle::kPaintClean);
  lifecycle_update_count_for_testing_++;

  // If the document is not active then it is either not yet initialized, or it
  // is stopping. In either case, we can't reach one of the supported target
  // states.
  if (!frame_->GetDocument()->IsActive())
    return false;

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

  // If we're throttling, then we don't need to update lifecycle phases, only
  // the throttling status.
  if (ShouldThrottleRendering()) {
    UpdateThrottlingStatusForSubtree();
    return Lifecycle().GetState() == target_state;
  }

  // If we're in PrintBrowser mode, setup a print context.
  // TODO(vmpstr): It doesn't seem like we need to do this every lifecycle
  // update, but rather once somewhere at creation time.
  if (RuntimeEnabledFeatures::PrintBrowserEnabled())
    SetupPrintContext();
  else
    ClearPrintContext();

  // Run the lifecycle updates.
  UpdateLifecyclePhasesInternal(target_state);

  // Update intersection observations if needed.
  if (target_state == DocumentLifecycle::kPaintClean) {
    TRACE_EVENT0("blink,benchmark",
                 "LocalFrameView::UpdateViewportIntersectionsForSubtree");
    SCOPED_UMA_AND_UKM_TIMER(LocalFrameUkmAggregator::kIntersectionObservation);
    UpdateViewportIntersectionsForSubtree();
  }

  UpdateThrottlingStatusForSubtree();

  return Lifecycle().GetState() == target_state;
}

void LocalFrameView::UpdateLifecyclePhasesInternal(
    DocumentLifecycle::LifecycleState target_state) {
  bool run_more_lifecycle_phases =
      RunStyleAndLayoutLifecyclePhases(target_state);
  if (!run_more_lifecycle_phases)
    return;
  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean);

  if (!GetLayoutView())
    return;

#if DCHECK_IS_ON()
  DisallowLayoutInvalidationScope disallow_layout_invalidation(this);
#endif

  {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "SetLayerTreeId", TRACE_EVENT_SCOPE_THREAD, "data",
                         InspectorSetLayerTreeId::Data(frame_.Get()));
    TRACE_EVENT1("devtools.timeline", "UpdateLayerTree", "data",
                 InspectorUpdateLayerTreeEvent::Data(frame_.Get()));

    run_more_lifecycle_phases = RunCompositingLifecyclePhase(target_state);
    if (!run_more_lifecycle_phases)
      return;

    // TODO(pdr): PrePaint should be under the "Paint" devtools timeline
    // step for slimming paint v2.
    run_more_lifecycle_phases = RunPrePaintLifecyclePhase(target_state);
    DCHECK(ShouldThrottleRendering() ||
           Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean);
    if (!run_more_lifecycle_phases)
      return;
  }

  DCHECK_EQ(target_state, DocumentLifecycle::kPaintClean);
  RunPaintLifecyclePhase();
  DCHECK(ShouldThrottleRendering() ||
         (frame_->GetDocument()->Printing() &&
          !RuntimeEnabledFeatures::PrintBrowserEnabled()) ||
         Lifecycle().GetState() == DocumentLifecycle::kPaintClean);
}

bool LocalFrameView::RunStyleAndLayoutLifecyclePhases(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::RunStyleAndLayoutLifecyclePhases");
  {
    SCOPED_UMA_AND_UKM_TIMER(LocalFrameUkmAggregator::kStyleAndLayout);
    UpdateStyleAndLayoutIfNeededRecursive();
  }
  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean);

  frame_->GetDocument()
      ->GetRootScrollerController()
      .PerformRootScrollerSelection();

  // PerformRootScrollerSelection can dirty layout if an effective root
  // scroller is changed so make sure we get back to LayoutClean.
  if (RuntimeEnabledFeatures::ImplicitRootScrollerEnabled() ||
      RuntimeEnabledFeatures::SetRootScrollerEnabled()) {
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

  frame_->GetPage()->GetValidationMessageClient().LayoutOverlay();
  frame_->GetPage()->UpdatePageColorOverlay();

  if (target_state == DocumentLifecycle::kPaintClean) {
    ForAllNonThrottledLocalFrameViews(
        [](LocalFrameView& frame_view) { frame_view.NotifyResizeObservers(); });

    NotifyFrameRectsChangedIfNeededRecursive();
  }
  // If we exceed the number of re-layouts during ResizeObserver notifications,
  // then we shouldn't continue with the lifecycle updates. At that time, we
  // have scheduled an animation and we'll try again.
  DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean ||
         Lifecycle().GetState() == DocumentLifecycle::kVisualUpdatePending);
  return Lifecycle().GetState() >= DocumentLifecycle::kLayoutClean;
}

bool LocalFrameView::RunCompositingLifecyclePhase(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark",
               "LocalFrameView::RunCompositingLifecyclePhase");
  auto* layout_view = GetLayoutView();
  DCHECK(layout_view);

  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    SCOPED_UMA_AND_UKM_TIMER(LocalFrameUkmAggregator::kCompositing);
    layout_view->Compositor()->UpdateIfNeededRecursive(target_state);
  } else {
    ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
      frame_view.GetLayoutView()->Layer()->UpdateDescendantDependentFlags();
      frame_view.GetLayoutView()->CommitPendingSelection();
    });
  }

  // We may be in kCompositingInputsClean clean, which does not need to notify
  // the global root scroller controller.
  if (target_state >= DocumentLifecycle::kCompositingClean) {
    frame_->GetPage()->GlobalRootScrollerController().DidUpdateCompositing(
        *this);
  }

  // We need to run more phases only if the target is beyond kCompositingClean.
  if (target_state > DocumentLifecycle::kCompositingClean) {
    // TODO(vmpstr): Why is composited selection only updated if we're moving
    // past kCompositingClean?
    UpdateCompositedSelectionIfNeeded();
    return true;
  }
  return false;
}

bool LocalFrameView::RunPrePaintLifecyclePhase(
    DocumentLifecycle::LifecycleState target_state) {
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::RunPrePaintLifecyclePhase");

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPrePaint);
    if (frame_view.CanThrottleRendering()) {
      // This frame can be throttled but not throttled, meaning we are not in an
      // AllowThrottlingScope. Now this frame may contain dirty paint flags, and
      // we need to propagate the flags into the ancestor chain so that
      // PrePaintTreeWalk can reach this frame.
      frame_view.SetNeedsPaintPropertyUpdate();
      if (auto* owner = frame_view.GetFrame().OwnerLayoutObject())
        owner->SetShouldCheckForPaintInvalidation();
    }
  });

  {
    SCOPED_UMA_AND_UKM_TIMER(LocalFrameUkmAggregator::kPrePaint);

    PrePaintTreeWalk().WalkTree(*this);
  }

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPrePaintClean);
  });

  return target_state > DocumentLifecycle::kPrePaintClean;
}

void LocalFrameView::RunPaintLifecyclePhase() {
  TRACE_EVENT0("blink,benchmark", "LocalFrameView::RunPaintLifecyclePhase");
  // While printing a document, the paint walk is done by the printing
  // component into a special canvas. There is no point doing a normal paint
  // step (or animations update for BlinkGenPropertyTrees/SPv2) when in this
  // mode.
  //
  // RuntimeEnabledFeatures::PrintBrowserEnabled is a mode which runs the
  // browser normally, but renders every page as if it were being printed.
  // See crbug.com/667547
  bool print_mode_enabled = frame_->GetDocument()->Printing() &&
                            !RuntimeEnabledFeatures::PrintBrowserEnabled();
  if (!print_mode_enabled)
    PaintTree();

  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    if (GetLayoutView()->Compositor()->InCompositingMode()) {
      GetScrollingCoordinator()->UpdateAfterPaint(this);
    }
  }

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
      RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    if (!print_mode_enabled) {
      base::Optional<CompositorElementIdSet> composited_element_ids =
          CompositorElementIdSet();
      PushPaintArtifactToCompositor(composited_element_ids.value());
      // TODO(wkorman): Add call to UpdateCompositorScrollAnimations here.
      ForAllNonThrottledLocalFrameViews(
          [&composited_element_ids](LocalFrameView& frame_view) {
            DocumentAnimations::UpdateAnimations(
                frame_view.GetLayoutView()->GetDocument(),
                DocumentLifecycle::kPaintClean, composited_element_ids);
          });

      // Notify the controller that the artifact has been pushed and some
      // lifecycle state can be freed (such as raster invalidations).
      paint_controller_->FinishCycle();
      // PaintController for BlinkGenPropertyTrees is transient.
      if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() &&
          !RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
        paint_controller_ = nullptr;
      }
    }
  }
}

void LocalFrameView::EnqueueScrollAnchoringAdjustment(
    ScrollableArea* scrollable_area) {
  anchoring_adjustment_queue_.insert(scrollable_area);
}

void LocalFrameView::DequeueScrollAnchoringAdjustment(
    ScrollableArea* scrollable_area) {
  anchoring_adjustment_queue_.erase(scrollable_area);
}

void LocalFrameView::PerformScrollAnchoringAdjustments() {
  // Adjust() will cause a scroll which could end up causing a layout and
  // reentering this method. Copy and clear the queue so we don't modify it
  // during iteration.
  AnchoringAdjustmentQueue queue_copy = anchoring_adjustment_queue_;
  anchoring_adjustment_queue_.clear();

  for (WeakMember<ScrollableArea>& scroller : queue_copy) {
    if (scroller) {
      DCHECK(scroller->GetScrollAnchor());
      scroller->GetScrollAnchor()->Adjust();
    }
  }
}

static void CollectViewportLayersForLayerList(GraphicsContext& context,
                                              VisualViewport& visual_viewport) {
  DCHECK(RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled());

  // Collect the visual viewport container layer.
  {
    GraphicsLayer* container_layer = visual_viewport.ContainerLayer();
    ScopedPaintChunkProperties container_scope(
        context.GetPaintController(), container_layer->GetPropertyTreeState(),
        *container_layer, DisplayItem::kForeignLayerWrapper);

    // TODO(trchen): Currently the GraphicsLayer hierarchy is still built during
    // CompositingUpdate, and we have to clear them here to ensure no extraneous
    // layers are still attached. In future we will disable all those layer
    // hierarchy code so we won't need this line.
    container_layer->CcLayer()->RemoveAllChildren();
    RecordForeignLayer(context, *container_layer,
                       DisplayItem::kForeignLayerWrapper,
                       container_layer->CcLayer(), FloatPoint(),
                       IntSize(container_layer->CcLayer()->bounds()));
  }

  // Collect the page scale layer.
  {
    GraphicsLayer* scale_layer = visual_viewport.PageScaleLayer();
    ScopedPaintChunkProperties scale_scope(
        context.GetPaintController(), scale_layer->GetPropertyTreeState(),
        *scale_layer, DisplayItem::kForeignLayerWrapper);

    scale_layer->CcLayer()->RemoveAllChildren();
    RecordForeignLayer(context, *scale_layer, DisplayItem::kForeignLayerWrapper,
                       scale_layer->CcLayer(), FloatPoint(), IntSize());
  }

  // Collect the visual viewport scroll layer.
  {
    GraphicsLayer* scroll_layer = visual_viewport.ScrollLayer();
    ScopedPaintChunkProperties scroll_scope(
        context.GetPaintController(), scroll_layer->GetPropertyTreeState(),
        *scroll_layer, DisplayItem::kForeignLayerWrapper);

    scroll_layer->CcLayer()->RemoveAllChildren();
    RecordForeignLayer(context, *scroll_layer,
                       DisplayItem::kForeignLayerWrapper,
                       scroll_layer->CcLayer(), FloatPoint(),
                       IntSize(scroll_layer->CcLayer()->bounds()));
  }
}

static void CollectDrawableLayersForLayerListRecursively(
    GraphicsContext& context,
    const GraphicsLayer* layer) {
  DCHECK(RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled());

  if (!layer || layer->Client().ShouldThrottleRendering())
    return;

  // We need to collect all layers that draw content, as well as some layers
  // that don't for the purposes of hit testing. For example, an empty div
  // will not draw content but needs to create a layer to ensure scroll events
  // do not pass through it.
  if (layer->DrawsContent() || layer->GetHitTestableWithoutDrawsContent()) {
    ScopedPaintChunkProperties scope(context.GetPaintController(),
                                     layer->GetPropertyTreeState(), *layer,
                                     DisplayItem::kForeignLayerWrapper);
    // TODO(trchen): Currently the GraphicsLayer hierarchy is still built
    // during CompositingUpdate, and we have to clear them here to ensure no
    // extraneous layers are still attached. In future we will disable all
    // those layer hierarchy code so we won't need this line.
    layer->CcLayer()->RemoveAllChildren();
    RecordForeignLayer(context, *layer, DisplayItem::kForeignLayerWrapper,
                       layer->CcLayer(),
                       FloatPoint(layer->GetOffsetFromTransformNode()),
                       IntSize(layer->Size()));
  }

  if (scoped_refptr<cc::Layer> contents_layer = layer->ContentsLayer()) {
    ScopedPaintChunkProperties scope(
        context.GetPaintController(), layer->GetContentsPropertyTreeState(),
        *layer, DisplayItem::kForeignLayerContentsWrapper);
    auto size = contents_layer->bounds();
    RecordForeignLayer(context, *layer,
                       DisplayItem::kForeignLayerContentsWrapper,
                       std::move(contents_layer),
                       FloatPoint(layer->GetContentsOffsetFromTransformNode()),
                       IntSize(size.width(), size.height()));
  }

  DCHECK(!layer->ContentsClippingMaskLayer());
  for (const auto* child : layer->Children())
    CollectDrawableLayersForLayerListRecursively(context, child);
  CollectDrawableLayersForLayerListRecursively(context, layer->MaskLayer());
}

static void CollectLinkHighlightLayersForLayerListRecursively(
    GraphicsContext& context,
    const GraphicsLayer* layer) {
  DCHECK(RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled());

  if (!layer || layer->Client().ShouldThrottleRendering())
    return;

  for (auto* highlight : layer->GetLinkHighlights()) {
    DCHECK(highlight->effect())
        << "Highlight effect node should have been created in PrePaint.";
    auto* highlight_layer = highlight->Layer();
    auto property_tree_state = layer->GetPropertyTreeState();
    property_tree_state.SetEffect(highlight->effect());
    ScopedPaintChunkProperties scope(context.GetPaintController(),
                                     property_tree_state, *highlight,
                                     DisplayItem::kForeignLayerLinkHighlight);
    auto position = highlight_layer->position();
    auto size = highlight_layer->bounds();
    RecordForeignLayer(context, *highlight,
                       DisplayItem::kForeignLayerLinkHighlight, highlight_layer,
                       layer->GetOffsetFromTransformNode() +
                           FloatSize(position.x(), position.y()),
                       IntSize(size.width(), size.height()));
  }

  for (const auto* child : layer->Children())
    CollectLinkHighlightLayersForLayerListRecursively(context, child);
}

static void PaintGraphicsLayerRecursively(GraphicsLayer* layer) {
  layer->PaintRecursively();

#if DCHECK_IS_ON()
  VerboseLogGraphicsLayerTree(layer);
#endif
}

void LocalFrameView::PaintTree() {
  SCOPED_UMA_AND_UKM_TIMER(LocalFrameUkmAggregator::kPaint);

  DCHECK(GetFrame().IsLocalRoot());

  auto* layout_view = GetLayoutView();
  DCHECK(layout_view);
  paint_frame_count_++;
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
  });

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    if (!paint_controller_)
      paint_controller_ = PaintController::Create();

    if (GetLayoutView()->Layer()->NeedsRepaint()) {
      GraphicsContext graphics_context(*paint_controller_);
      if (RuntimeEnabledFeatures::PrintBrowserEnabled())
        graphics_context.SetPrinting(true);

      if (Settings* settings = frame_->GetSettings()) {
        HighContrastSettings high_contrast_settings;
        high_contrast_settings.mode = settings->GetHighContrastMode();
        high_contrast_settings.grayscale = settings->GetHighContrastGrayscale();
        high_contrast_settings.contrast = settings->GetHighContrastContrast();
        high_contrast_settings.image_policy =
            settings->GetHighContrastImagePolicy();
        graphics_context.SetHighContrast(high_contrast_settings);
      }

      PaintInternal(graphics_context, kGlobalPaintNormalPhase,
                    CullRect(LayoutRect::InfiniteIntRect()));
      paint_controller_->CommitNewDisplayItems();
    }
  } else {
    // A null graphics layer can occur for painting of SVG images that are not
    // parented into the main frame tree, or when the LocalFrameView is the main
    // frame view of a page overlay. The page overlay is in the layer tree of
    // the host page and will be painted during painting of the host page.
    if (GraphicsLayer* root_graphics_layer =
            layout_view->Compositor()->PaintRootGraphicsLayer()) {
      PaintGraphicsLayerRecursively(root_graphics_layer);
    }

    // TODO(sataya.m):Main frame doesn't create RootFrameViewport in some
    // webkit_unit_tests (http://crbug.com/644788).
    if (viewport_scrollable_area_) {
      if (GraphicsLayer* layer_for_horizontal_scrollbar =
              viewport_scrollable_area_->LayerForHorizontalScrollbar()) {
        PaintGraphicsLayerRecursively(layer_for_horizontal_scrollbar);
      }
      if (GraphicsLayer* layer_for_vertical_scrollbar =
              viewport_scrollable_area_->LayerForVerticalScrollbar()) {
        PaintGraphicsLayerRecursively(layer_for_vertical_scrollbar);
      }
      if (GraphicsLayer* layer_for_scroll_corner =
              viewport_scrollable_area_->LayerForScrollCorner()) {
        PaintGraphicsLayerRecursively(layer_for_scroll_corner);
      }
    }
  }

  // TODO(chrishtr): Link highlights don't currently paint themselves,
  // it's still driven by cc. Fix this.
  // This uses an invalidation approach based on graphics layer raster
  // invalidation so it must be after paint. This adds/removes link highlight
  // layers so it must be before |CollectDrawableLayersForLayerListRecursively|.
  frame_->GetPage()->GetLinkHighlights().UpdateGeometry();

  frame_->GetPage()->GetValidationMessageClient().PaintOverlay();
  frame_->GetPage()->PaintPageColorOverlay();

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
    if (auto* layout_view = frame_view.GetLayoutView())
      layout_view->Layer()->ClearNeedsRepaintRecursively();
  });

  // Devtools overlays query the inspected page's paint data so this update
  // needs to be after the lifecycle advance to kPaintClean. Because devtools
  // overlays can add layers, this needs to be before layers are collected.
  if (auto* web_local_frame_impl = WebLocalFrameImpl::FromFrame(frame_))
    web_local_frame_impl->UpdateDevToolsOverlays();

  if (RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() &&
      !RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    // BlinkGenPropertyTrees just needs a transient PaintController to
    // collect the foreign layers which doesn't need caching. It also
    // shouldn't affect caching status of DisplayItemClients because it's
    // FinishCycle() is not synchronized with other PaintControllers.
    paint_controller_ = PaintController::Create(PaintController::kTransient);

    GraphicsContext context(*paint_controller_);
    // Note: Some blink unit tests run without turning on compositing which
    // means we don't create viewport layers. OOPIFs also don't have their own
    // viewport layers.
    if (GetLayoutView()->Compositor()->InCompositingMode() &&
        GetFrame() == GetPage()->MainFrame()) {
      // TODO(bokan): We should eventually stop creating layers for the visual
      // viewport. At that point, we can remove this. However, for now, CC
      // still has some dependencies on the viewport scale and scroll layers.
      CollectViewportLayersForLayerList(context,
                                        frame_->GetPage()->GetVisualViewport());
    }
    // With BlinkGenPropertyTrees, |PaintRootGraphicsLayer| is the ancestor of
    // all drawable layers (see: PaintLayerCompositor::PaintRootGraphicsLayer)
    // so we do not need to collect scrollbars separately.
    auto* root = layout_view->Compositor()->PaintRootGraphicsLayer();
    CollectDrawableLayersForLayerListRecursively(context, root);
    // Link highlights paint after all other layers.
    if (!frame_->GetPage()->GetLinkHighlights().IsEmpty())
      CollectLinkHighlightLayersForLayerListRecursively(context, root);
    paint_controller_->CommitNewDisplayItems();
  }
}

void LocalFrameView::PushPaintArtifactToCompositor(
    CompositorElementIdSet& composited_element_ids) {
  TRACE_EVENT0("blink", "LocalFrameView::pushPaintArtifactToCompositor");

  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
         RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled());

  if (!frame_->GetSettings()->GetAcceleratedCompositingEnabled())
    return;

  Page* page = GetFrame().GetPage();
  if (!page)
    return;

  if (!paint_artifact_compositor_) {
    paint_artifact_compositor_ =
        PaintArtifactCompositor::Create(WTF::BindRepeating(
            &ScrollingCoordinator::DidScroll,
            // The layer being scrolled is destroyed before the
            // ScrollingCoordinator.
            WrapWeakPersistent(page->GetScrollingCoordinator())));
    page->GetChromeClient().AttachRootLayer(
        paint_artifact_compositor_->RootLayer(), &GetFrame());
  }

  SCOPED_UMA_AND_UKM_TIMER(LocalFrameUkmAggregator::kCompositingCommit);

  paint_artifact_compositor_->Update(
      paint_controller_->GetPaintArtifactShared(), composited_element_ids,
      frame_->GetPage()->GetVisualViewport().GetPageScaleNode());
}

std::unique_ptr<JSONObject> LocalFrameView::CompositedLayersAsJSON(
    LayerTreeFlags flags) {
  return GetFrame()
      .LocalFrameRoot()
      .View()
      ->paint_artifact_compositor_->LayersAsJSON(flags);
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

  frame_->GetDocument()->UpdateStyleAndLayoutTree();

  // Update style for all embedded SVG documents underneath this frame, so
  // that intrinsic size computation for any embedded objects has up-to-date
  // information before layout.
  ForAllChildLocalFrameViews([](LocalFrameView& view) {
    Document& document = *view.GetFrame().GetDocument();
    if (document.IsSVGDocument())
      document.UpdateStyleAndLayoutTree();
  });

  CHECK(!ShouldThrottleRendering());
  CHECK(frame_->GetDocument()->IsActive());
  CHECK(!nested_layout_count_);

  if (NeedsLayout())
    UpdateLayout();

  CheckDoesNotNeedLayout();

  // WebView plugins need to update regardless of whether the
  // LayoutEmbeddedObject that owns them needed layout.
  // TODO(leviw): This currently runs the entire lifecycle on plugin WebViews.
  // We should have a way to only run these other Documents to the same
  // lifecycle stage as this frame.
  for (const auto& plugin : plugins_) {
    plugin->UpdateAllLifecyclePhases();
  }
  CheckDoesNotNeedLayout();

  // FIXME: Calling layout() shouldn't trigger script execution or have any
  // observable effects on the frame tree but we're not quite there yet.
  HeapVector<Member<LocalFrameView>> frame_views;
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (!child->IsLocalFrame())
      continue;
    if (LocalFrameView* view = ToLocalFrame(child)->View())
      frame_views.push_back(view);
  }

  for (const auto& frame_view : frame_views)
    frame_view->UpdateStyleAndLayoutIfNeededRecursive();

  // These asserts ensure that parent frames are clean, when child frames
  // finished updating layout and style.
  CheckDoesNotNeedLayout();
#if DCHECK_IS_ON()
  frame_->GetDocument()->GetLayoutView()->AssertLaidOut();
#endif

  UpdateGeometriesIfNeeded();

  if (Lifecycle().GetState() < DocumentLifecycle::kLayoutClean)
    Lifecycle().AdvanceTo(DocumentLifecycle::kLayoutClean);

  if (AXObjectCache* cache = GetFrame().GetDocument()->ExistingAXObjectCache())
    cache->ProcessUpdatesAfterLayout(*GetFrame().GetDocument());

  // Ensure that we become visually non-empty eventually.
  // TODO(esprehn): This should check isRenderingReady() instead.
  if (GetFrame().GetDocument()->HasFinishedParsing() &&
      GetFrame().Loader().StateMachine()->CommittedFirstRealDocumentLoad())
    is_visually_non_empty_ = true;

  GetFrame().Selection().UpdateStyleAndLayoutIfNeeded();
  GetFrame().GetPage()->GetDragCaret().UpdateStyleAndLayoutIfNeeded();
}

void LocalFrameView::EnableAutoSizeMode(const IntSize& min_size,
                                        const IntSize& max_size) {
  if (!auto_size_info_)
    auto_size_info_ = FrameViewAutoSizeInfo::Create(this);

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
  GetLayoutView()->SetAutosizeScrollbarModes(kScrollbarAuto, kScrollbarAuto);
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
    layout_view->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
        LayoutInvalidationReason::kPrintingChanged);
    UpdateLayout();

    // If we don't fit in the given page width, we'll lay out again. If we don't
    // fit in the page width when shrunk, we will lay out at maximum shrink and
    // clip extra content.
    // FIXME: We are assuming a shrink-to-fit printing implementation.  A
    // cropping implementation should not do this!
    bool horizontal_writing_mode =
        layout_view->StyleRef().IsHorizontalWritingMode();
    const LayoutRect& document_rect = LayoutRect(layout_view->DocumentRect());
    LayoutUnit doc_logical_width = horizontal_writing_mode
                                       ? document_rect.Width()
                                       : document_rect.Height();
    if (doc_logical_width > page_logical_width) {
      FloatSize expected_page_size(
          std::min<float>(document_rect.Width().ToFloat(),
                          page_size.Width() * maximum_shrink_factor),
          std::min<float>(document_rect.Height().ToFloat(),
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
      layout_view->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
          LayoutInvalidationReason::kPrintingChanged);
      UpdateLayout();

      const LayoutRect& updated_document_rect =
          LayoutRect(layout_view->DocumentRect());
      LayoutUnit doc_logical_height = horizontal_writing_mode
                                          ? updated_document_rect.Height()
                                          : updated_document_rect.Width();
      LayoutUnit doc_logical_top = horizontal_writing_mode
                                       ? updated_document_rect.Y()
                                       : updated_document_rect.X();
      LayoutUnit doc_logical_right = horizontal_writing_mode
                                         ? updated_document_rect.MaxX()
                                         : updated_document_rect.MaxY();
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

IntRect LocalFrameView::ConvertFromLayoutObject(
    const LayoutObject& layout_object,
    const IntRect& layout_object_rect) const {
  // Convert from page ("absolute") to LocalFrameView coordinates.
  LayoutRect rect = EnclosingLayoutRect(
      layout_object.LocalToAbsoluteQuad(FloatRect(layout_object_rect))
          .BoundingBox());
  return PixelSnappedIntRect(rect);
}

IntRect LocalFrameView::ConvertToLayoutObject(const LayoutObject& layout_object,
                                              const IntRect& frame_rect) const {
  FloatQuad quad((FloatRect(frame_rect)));
  quad = layout_object.AbsoluteToLocalQuad(quad, kUseTransforms);
  return RoundedIntRect(quad.BoundingBox());
}

IntPoint LocalFrameView::ConvertFromLayoutObject(
    const LayoutObject& layout_object,
    const IntPoint& layout_object_point) const {
  return RoundedIntPoint(
      ConvertFromLayoutObject(layout_object, LayoutPoint(layout_object_point)));
}

IntPoint LocalFrameView::ConvertToLayoutObject(
    const LayoutObject& layout_object,
    const IntPoint& frame_point) const {
  return RoundedIntPoint(
      ConvertToLayoutObject(layout_object, LayoutPoint(frame_point)));
}

LayoutPoint LocalFrameView::ConvertFromLayoutObject(
    const LayoutObject& layout_object,
    const LayoutPoint& layout_object_point) const {
  return LayoutPoint(layout_object.LocalToAbsolute(
      FloatPoint(layout_object_point), kUseTransforms));
}

LayoutPoint LocalFrameView::ConvertToLayoutObject(
    const LayoutObject& layout_object,
    const LayoutPoint& frame_point) const {
  return LayoutPoint(
      ConvertToLayoutObject(layout_object, FloatPoint(frame_point)));
}

FloatPoint LocalFrameView::ConvertToLayoutObject(
    const LayoutObject& layout_object,
    const FloatPoint& frame_point) const {
  return layout_object.AbsoluteToLocal(frame_point, kUseTransforms);
}

IntPoint LocalFrameView::ConvertSelfToChild(const EmbeddedContentView& child,
                                            const IntPoint& point) const {
  IntPoint new_point(point);
  new_point.MoveBy(-child.FrameRect().Location());
  return new_point;
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

DoublePoint LocalFrameView::DocumentToFrame(
    const DoublePoint& point_in_document) const {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return point_in_document;

  return point_in_document - layout_viewport->GetScrollOffset();
}

FloatPoint LocalFrameView::DocumentToFrame(
    const FloatPoint& point_in_document) const {
  return FloatPoint(DocumentToFrame(DoublePoint(point_in_document)));
}

LayoutPoint LocalFrameView::DocumentToFrame(
    const LayoutPoint& point_in_document) const {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return point_in_document;

  return point_in_document - LayoutSize(layout_viewport->GetScrollOffset());
}

LayoutRect LocalFrameView::DocumentToFrame(
    const LayoutRect& rect_in_document) const {
  // With RLS turned off, this will be a no-op.
  return LayoutRect(DocumentToFrame(rect_in_document.Location()),
                    rect_in_document.Size());
}

LayoutPoint LocalFrameView::FrameToDocument(
    const LayoutPoint& point_in_frame) const {
  ScrollableArea* layout_viewport = LayoutViewport();
  if (!layout_viewport)
    return point_in_frame;

  return point_in_frame + LayoutSize(layout_viewport->GetScrollOffset());
}

LayoutRect LocalFrameView::FrameToDocument(
    const LayoutRect& rect_in_frame) const {
  return LayoutRect(FrameToDocument(rect_in_frame.Location()),
                    rect_in_frame.Size());
}

IntRect LocalFrameView::ConvertToContainingEmbeddedContentView(
    const IntRect& local_rect) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    auto* layout_object = frame_->OwnerLayoutObject();
    if (!layout_object)
      return local_rect;

    IntRect rect(local_rect);
    // Add borders and padding
    rect.Move(
        (layout_object->BorderLeft() + layout_object->PaddingLeft()).ToInt(),
        (layout_object->BorderTop() + layout_object->PaddingTop()).ToInt());
    return parent->ConvertFromLayoutObject(*layout_object, rect);
  }

  return local_rect;
}

IntRect LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const IntRect& parent_rect) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    IntRect local_rect = parent_rect;
    local_rect.SetLocation(
        parent->ConvertSelfToChild(*this, local_rect.Location()));
    return local_rect;
  }

  return parent_rect;
}

LayoutPoint LocalFrameView::ConvertToContainingEmbeddedContentView(
    const LayoutPoint& local_point) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    auto* layout_object = frame_->OwnerLayoutObject();
    if (!layout_object)
      return local_point;

    LayoutPoint point(local_point);

    // Add borders and padding
    point.Move((layout_object->BorderLeft() + layout_object->PaddingLeft()),
               (layout_object->BorderTop() + layout_object->PaddingTop()));
    return parent->ConvertFromLayoutObject(*layout_object, point);
  }

  return local_point;
}

FloatPoint LocalFrameView::ConvertToContainingEmbeddedContentView(
    const FloatPoint& local_point) const {
  if (ParentFrameView()) {
    auto* layout_object = frame_->OwnerLayoutObject();
    if (!layout_object)
      return local_point;

    FloatPoint point(local_point);

    // Add borders and padding
    point.Move(
        (layout_object->BorderLeft() + layout_object->PaddingLeft()).ToFloat(),
        (layout_object->BorderTop() + layout_object->PaddingTop()).ToFloat());
    return layout_object->LocalToAbsolute(point, kUseTransforms);
  }

  return local_point;
}

LayoutPoint LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const LayoutPoint& parent_point) const {
  return LayoutPoint(
      ConvertFromContainingEmbeddedContentView(DoublePoint(parent_point)));
}

FloatPoint LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const FloatPoint& parent_point) const {
  return FloatPoint(
      ConvertFromContainingEmbeddedContentView(DoublePoint(parent_point)));
}

DoublePoint LocalFrameView::ConvertFromContainingEmbeddedContentView(
    const DoublePoint& parent_point) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    // Get our layoutObject in the parent view
    auto* layout_object = frame_->OwnerLayoutObject();
    if (!layout_object)
      return parent_point;

    DoublePoint point = DoublePoint(parent->ConvertToLayoutObject(
        *layout_object, FloatPoint(parent_point)));
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
      ConvertToContainingEmbeddedContentView(LayoutPoint(local_point)));
}

void LocalFrameView::SetInitialTracksPaintInvalidationsForTesting(
    bool track_paint_invalidations) {
  g_initial_track_all_paint_invalidations = track_paint_invalidations;
}

void LocalFrameView::SetTracksPaintInvalidations(
    bool track_paint_invalidations) {
  if (track_paint_invalidations == IsTrackingPaintInvalidations())
    return;

  // Ensure the document is up-to-date before tracking invalidations.
  UpdateAllLifecyclePhases();

  for (Frame* frame = &frame_->Tree().Top(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (!frame->IsLocalFrame())
      continue;
    if (auto* layout_view = ToLocalFrame(frame)->ContentLayoutObject()) {
      layout_view->GetFrameView()->tracked_object_paint_invalidations_ =
          base::WrapUnique(track_paint_invalidations
                               ? new Vector<ObjectPaintInvalidation>
                               : nullptr);
      if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
        if (paint_artifact_compositor_) {
          paint_artifact_compositor_->SetTracksRasterInvalidations(
              track_paint_invalidations);
        }
      } else {
        layout_view->Compositor()->UpdateTrackingRasterInvalidations();
      }
    }
  }

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"),
                       "LocalFrameView::setTracksPaintInvalidations",
                       TRACE_EVENT_SCOPE_GLOBAL, "enabled",
                       track_paint_invalidations);
}

void LocalFrameView::TrackObjectPaintInvalidation(
    const DisplayItemClient& client,
    PaintInvalidationReason reason) {
  if (!tracked_object_paint_invalidations_)
    return;

  ObjectPaintInvalidation invalidation = {client.DebugName(), reason};
  tracked_object_paint_invalidations_->push_back(invalidation);
}

void LocalFrameView::AddResizerArea(LayoutBox& resizer_box) {
  if (!resizer_areas_)
    resizer_areas_ = std::make_unique<ResizerAreaSet>();
  resizer_areas_->insert(&resizer_box);
}

void LocalFrameView::RemoveResizerArea(LayoutBox& resizer_box) {
  if (!resizer_areas_)
    return;

  ResizerAreaSet::iterator it = resizer_areas_->find(&resizer_box);
  if (it != resizer_areas_->end())
    resizer_areas_->erase(it);
}

void LocalFrameView::ScheduleAnimation() {
  if (auto* client = GetChromeClient())
    client->ScheduleAnimation(this);
}

bool LocalFrameView::FrameIsScrollableDidChange() {
  DCHECK(GetFrame().IsLocalRoot());
  return GetScrollingContext()->WasScrollable() !=
         LayoutViewport()->ScrollsOverflow();
}

void LocalFrameView::ClearFrameIsScrollableDidChange() {
  GetScrollingContext()->SetWasScrollable(
      GetFrame().LocalFrameRoot().View()->LayoutViewport()->ScrollsOverflow());
}

void LocalFrameView::ScrollableAreasDidChange() {
  // Layout may update scrollable area bounding boxes. It also sets the same
  // dirty flag making this one redundant (See
  // |ScrollingCoordinator::notifyGeometryChanged|).
  // So if layout is expected, ignore this call allowing scrolling coordinator
  // to be notified post-layout to recompute gesture regions.
  // TODO(wjmaclean): It would be nice to move the !NeedsLayout() check from
  // here to SetScrollGestureRegionIsDirty(), but at present doing so breaks
  // layout tests. This suggests that there is something that wants to set the
  // dirty bit when layout is needed, and won't re-try setting the bit after
  // layout has completed - it would be nice to find that and fix it.
  if (!NeedsLayout())
    GetScrollingContext()->SetScrollGestureRegionIsDirty(true);
}

void LocalFrameView::AddScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  DCHECK(scrollable_area);
  if (!scrollable_areas_)
    scrollable_areas_ = new ScrollableAreaSet;
  scrollable_areas_->insert(scrollable_area);

  if (GetScrollingCoordinator())
    ScrollableAreasDidChange();
}

void LocalFrameView::RemoveScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  if (!scrollable_areas_)
    return;
  scrollable_areas_->erase(scrollable_area);

  if (GetScrollingCoordinator())
    ScrollableAreasDidChange();
}

void LocalFrameView::AddAnimatingScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  DCHECK(scrollable_area);
  if (!animating_scrollable_areas_)
    animating_scrollable_areas_ = new ScrollableAreaSet;
  animating_scrollable_areas_->insert(scrollable_area);
}

void LocalFrameView::RemoveAnimatingScrollableArea(
    PaintLayerScrollableArea* scrollable_area) {
  if (!animating_scrollable_areas_)
    return;
  animating_scrollable_areas_->erase(scrollable_area);
}

void LocalFrameView::AttachToLayout() {
  // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout crashes.
  CHECK(!is_attached_);
  if (frame_->GetDocument())
    CHECK_NE(Lifecycle().GetState(), DocumentLifecycle::kStopping);
  is_attached_ = true;
  parent_ = ParentFrameView();
  if (!parent_) {
    Frame* parent_frame = frame_->Tree().Parent();
    CHECK(parent_frame);
    CHECK(parent_frame->IsLocalFrame());
    CHECK(parent_frame->View());
  }
  CHECK(parent_);
  if (parent_->IsVisible())
    SetParentVisible(true);
  SetupRenderThrottling();
  subtree_throttled_ = ParentFrameView()->CanThrottleRendering();

  // We may have updated paint properties in detached frame subtree for
  // printing (see UpdateLifecyclePhasesForPrinting()). The paint properties
  // may change after the frame is attached.
  if (auto* layout_view = GetLayoutView()) {
    layout_view->AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kPrinting);
  }
}

void LocalFrameView::DetachFromLayout() {
  // TODO(crbug.com/729196): Trace why LocalFrameView::DetachFromLayout crashes.
  CHECK(is_attached_);
  LocalFrameView* parent = ParentFrameView();
  if (!parent) {
    Frame* parent_frame = frame_->Tree().Parent();
    CHECK(parent_frame);
    CHECK(parent_frame->IsLocalFrame());
    CHECK(parent_frame->View());
  }
  CHECK(parent == parent_);
  SetParentVisible(false);
  is_attached_ = false;

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

void LocalFrameView::SetCursor(const Cursor& cursor) {
  Page* page = GetFrame().GetPage();
  if (!page || frame_->GetEventHandler().IsMousePositionUnknown())
    return;
  LogCursorSizeCounter(&GetFrame(), cursor);
  page->GetChromeClient().SetCursor(cursor, frame_);
}

void LocalFrameView::FrameRectsChanged() {
  TRACE_EVENT0("blink", "LocalFrameView::frameRectsChanged");
  if (LayoutSizeFixedToFrameSize())
    SetLayoutSizeInternal(Size());

  ForAllChildViewsAndPlugins([](EmbeddedContentView& embedded_content_view) {
    if (!embedded_content_view.IsLocalFrameView() ||
        !ToLocalFrameView(embedded_content_view).ShouldThrottleRendering())
      embedded_content_view.FrameRectsChanged();
  });

  GetFrame().Client()->FrameRectsChanged(FrameRect());
}

void LocalFrameView::SetLayoutSizeInternal(const IntSize& size) {
  if (layout_size_ == size)
    return;
  layout_size_ = size;

  if (frame_->IsMainFrame() && frame_->GetDocument()) {
    if (TextAutosizer* text_autosizer =
            frame_->GetDocument()->GetTextAutosizer())
      text_autosizer->UpdatePageInfoInAllFrames();
  }

  SetNeedsLayout();
}

void LocalFrameView::ClipPaintRect(FloatRect* paint_rect) const {
  // Paint the whole rect if "mainFrameClipsContent" is false, meaning that
  // WebPreferences::record_whole_document is true.
  if (!frame_->GetSettings()->GetMainFrameClipsContent())
    return;

  paint_rect->Intersect(
      GetPage()->GetChromeClient().VisibleContentRectForPainting().value_or(
          IntRect(IntPoint(), Size())));
}

void LocalFrameView::DidChangeScrollOffset() {
  GetFrame().Client()->DidChangeScrollOffset();
  if (GetFrame().IsMainFrame())
    GetFrame().GetPage()->GetChromeClient().MainFrameScrollOffsetChanged();
}

ScrollableArea* LocalFrameView::ScrollableAreaWithElementId(
    const CompositorElementId& id) {
  // Check for the layout viewport, which may not be in scrollable_areas_ if it
  // is styled overflow: hidden.  (Other overflow: hidden elements won't have
  // composited scrolling layers per crbug.com/784053, so we don't have to worry
  // about them.)
  ScrollableArea* viewport = LayoutViewport();
  if (id == viewport->GetCompositorElementId())
    return viewport;

  if (scrollable_areas_) {
    // This requires iterating over all scrollable areas. We may want to store a
    // map of ElementId to ScrollableArea if this is an issue for performance.
    for (ScrollableArea* scrollable_area : *scrollable_areas_) {
      if (id == scrollable_area->GetCompositorElementId())
        return scrollable_area;
    }
  }
  return nullptr;
}

void LocalFrameView::ScrollRectToVisibleInRemoteParent(
    const LayoutRect& rect_to_scroll,
    const WebScrollIntoViewParams& params) {
  DCHECK(GetFrame().IsLocalRoot() && !GetFrame().IsMainFrame() &&
         safe_to_propagate_scroll_to_parent_);
  LayoutRect new_rect = ConvertToRootFrame(rect_to_scroll);
  GetFrame().Client()->ScrollRectToVisibleInParentFrame(
      WebRect(new_rect.X().ToInt(), new_rect.Y().ToInt(),
              new_rect.Width().ToInt(), new_rect.Height().ToInt()),
      params);
}

void LocalFrameView::NotifyFrameRectsChangedIfNeeded() {
  if (root_layer_did_scroll_) {
    root_layer_did_scroll_ = false;
    FrameRectsChanged();
  }
}

LayoutPoint LocalFrameView::ViewportToFrame(
    const LayoutPoint& point_in_viewport) const {
  LayoutPoint point_in_root_frame(
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
  return RoundedIntPoint(ViewportToFrame(LayoutPoint(point_in_viewport)));
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

void LocalFrameView::Paint(GraphicsContext& context,
                           const GlobalPaintFlags global_paint_flags,
                           const CullRect& cull_rect,
                           const IntSize& paint_offset) const {
  // |paint_offset| is not used because paint properties of the contents will
  // ensure the correct location.
  PaintInternal(context, global_paint_flags, cull_rect);
}

void LocalFrameView::PaintWithLifecycleUpdate(
    GraphicsContext& context,
    const GlobalPaintFlags global_paint_flags,
    const CullRect& cull_rect) {
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
  });

  PaintInternal(context, global_paint_flags, cull_rect);

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
  });
}

void LocalFrameView::PaintInternal(GraphicsContext& context,
                                   const GlobalPaintFlags global_paint_flags,
                                   const CullRect& cull_rect) const {
  FramePainter(*this).Paint(context, global_paint_flags, cull_rect);
}

void LocalFrameView::PaintContents(GraphicsContext& context,
                                   const GlobalPaintFlags global_paint_flags,
                                   const IntRect& damage_rect) {
  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kInPaint);
  });

  FramePainter(*this).PaintContents(context, global_paint_flags, damage_rect);

  ForAllNonThrottledLocalFrameViews([](LocalFrameView& frame_view) {
    frame_view.Lifecycle().AdvanceTo(DocumentLifecycle::kPaintClean);
  });
}

IntRect LocalFrameView::ConvertToRootFrame(const IntRect& local_rect) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    IntRect parent_rect = ConvertToContainingEmbeddedContentView(local_rect);
    return parent->ConvertToRootFrame(parent_rect);
  }
  return local_rect;
}

IntPoint LocalFrameView::ConvertToRootFrame(const IntPoint& local_point) const {
  return RoundedIntPoint(ConvertToRootFrame(LayoutPoint(local_point)));
}

LayoutPoint LocalFrameView::ConvertToRootFrame(
    const LayoutPoint& local_point) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    LayoutPoint parent_point =
        ConvertToContainingEmbeddedContentView(local_point);
    return parent->ConvertToRootFrame(parent_point);
  }
  return local_point;
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

LayoutRect LocalFrameView::ConvertToRootFrame(
    const LayoutRect& local_rect) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    LayoutPoint parent_point =
        ConvertToContainingEmbeddedContentView(local_rect.Location());
    LayoutRect parent_rect(parent_point, local_rect.Size());
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
      ConvertFromRootFrame(LayoutPoint(point_in_root_frame)));
}

LayoutPoint LocalFrameView::ConvertFromRootFrame(
    const LayoutPoint& point_in_root_frame) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    LayoutPoint parent_point =
        parent->ConvertFromRootFrame(point_in_root_frame);
    return ConvertFromContainingEmbeddedContentView(parent_point);
  }
  return point_in_root_frame;
}

FloatPoint LocalFrameView::ConvertFromRootFrame(
    const FloatPoint& point_in_root_frame) const {
  if (LocalFrameView* parent = ParentFrameView()) {
    FloatPoint parent_point = parent->ConvertFromRootFrame(point_in_root_frame);
    return ConvertFromContainingEmbeddedContentView(parent_point);
  }
  return point_in_root_frame;
}

void LocalFrameView::SetParentVisible(bool visible) {
  if (IsParentVisible() == visible)
    return;

  // As parent visibility changes, we may need to recomposite this frame view
  // and potentially child frame views.
  SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);

  parent_visible_ = visible;

  if (!IsSelfVisible())
    return;

  ForAllChildViewsAndPlugins(
      [visible](EmbeddedContentView& embedded_content_view) {
        embedded_content_view.SetParentVisible(visible);
      });
}

void LocalFrameView::SetSelfVisible(bool visible) {
  if (visible != self_visible_) {
    // Frame view visibility affects PLC::CanBeComposited, which in turn
    // affects compositing inputs.
    if (LayoutView* view = GetLayoutView())
      view->Layer()->SetNeedsCompositingInputsUpdate();
  }
  self_visible_ = visible;
}

void LocalFrameView::Show() {
  if (!IsSelfVisible()) {
    SetSelfVisible(true);
    if (GetScrollingCoordinator())
      GetScrollingContext()->SetScrollGestureRegionIsDirty(true);
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
    if (GetScrollingCoordinator())
      GetScrollingContext()->SetScrollGestureRegionIsDirty(true);
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

void LocalFrameView::UpdateViewportIntersectionsForSubtree() {
  // TODO(dcheng): Since LocalFrameView tree updates are deferred, FrameViews
  // might still be in the LocalFrameView hierarchy even though the associated
  // Document is already detached. Investigate if this check and a similar check
  // in lifecycle updates are still needed when there are no more deferred
  // LocalFrameView updates: https://crbug.com/561683
  if (!GetFrame().GetDocument()->IsActive())
    return;

  RecordDeferredLoadingStats();
  if (!NeedsLayout()) {
    // Notify javascript IntersectionObservers
    if (GetFrame().GetDocument()->GetIntersectionObserverController()) {
      GetFrame()
          .GetDocument()
          ->GetIntersectionObserverController()
          ->ComputeTrackedIntersectionObservations();
    }
  }

  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    child->View()->UpdateViewportIntersectionsForSubtree();
  }

  intersection_observation_state_ = kNotNeeded;
}

void LocalFrameView::UpdateThrottlingStatusForSubtree() {
  if (!GetFrame().GetDocument()->IsActive())
    return;

  // Don't throttle display:none frames (see updateRenderThrottlingStatus).
  HTMLFrameOwnerElement* owner_element = frame_->DeprecatedLocalOwner();
  if (hidden_for_throttling_ && owner_element &&
      !owner_element->GetLayoutObject()) {
    // No need to notify children because descendants of display:none frames
    // should remain throttled.
    UpdateRenderThrottlingStatus(hidden_for_throttling_, subtree_throttled_,
                                 kDontForceThrottlingInvalidation,
                                 kDontNotifyChildren);
  }

  ForAllChildLocalFrameViews([](LocalFrameView& child_view) {
    child_view.UpdateThrottlingStatusForSubtree();
  });
}

void LocalFrameView::UpdateRenderThrottlingStatusForTesting() {
  visibility_observer_->DeliverObservationsForTesting();
}

void LocalFrameView::CrossOriginStatusChanged() {
  // Cross-domain status is not stored as a dirty bit within LocalFrameView,
  // so force-invalidate throttling status when it changes regardless of
  // previous or new value.
  UpdateRenderThrottlingStatus(hidden_for_throttling_, subtree_throttled_,
                               kForceThrottlingInvalidation);
}

void LocalFrameView::UpdateRenderThrottlingStatus(
    bool hidden,
    bool subtree_throttled,
    ForceThrottlingInvalidationBehavior force_throttling_invalidation_behavior,
    NotifyChildrenBehavior notify_children_behavior) {
  TRACE_EVENT0("blink", "LocalFrameView::updateRenderThrottlingStatus");
  DCHECK(!IsInPerformLayout());
  DCHECK(!frame_->GetDocument() || !frame_->GetDocument()->InStyleRecalc());
  bool was_throttled = CanThrottleRendering();

  // Note that we disallow throttling of 0x0 and display:none frames because
  // some sites use them to drive UI logic.
  hidden_for_throttling_ = hidden && !Size().IsEmpty();
  subtree_throttled_ = subtree_throttled;
  HTMLFrameOwnerElement* owner_element = frame_->DeprecatedLocalOwner();
  if (owner_element)
    hidden_for_throttling_ &= !!owner_element->GetLayoutObject();

  bool is_throttled = CanThrottleRendering();
  bool became_unthrottled = was_throttled && !is_throttled;

  // If this LocalFrameView became unthrottled or throttled, we must make sure
  // all its children are notified synchronously. Otherwise we 1) might attempt
  // to paint one of the children with an out-of-date layout before
  // |updateRenderThrottlingStatus| has made it throttled or 2) fail to
  // unthrottle a child whose parent is unthrottled by a later notification.
  if (notify_children_behavior == kNotifyChildren &&
      (was_throttled != is_throttled ||
       force_throttling_invalidation_behavior ==
           kForceThrottlingInvalidation)) {
    ForAllChildLocalFrameViews([is_throttled](LocalFrameView& frame_view) {
      frame_view.UpdateRenderThrottlingStatus(frame_view.hidden_for_throttling_,
                                              is_throttled);
    });
  }

  ScrollingCoordinator* scrolling_coordinator = this->GetScrollingCoordinator();
  if (became_unthrottled ||
      force_throttling_invalidation_behavior == kForceThrottlingInvalidation) {
    // ScrollingCoordinator needs to update according to the new throttling
    // status.
    if (scrolling_coordinator)
      scrolling_coordinator->NotifyGeometryChanged(this);
    // Start ticking animation frames again if necessary.
    if (GetPage())
      GetPage()->Animator().ScheduleVisualUpdate(frame_.Get());
    // Force a full repaint of this frame to ensure we are not left with a
    // partially painted version of this frame's contents if we skipped
    // painting them while the frame was throttled.
    auto* layout_view = GetLayoutView();
    if (layout_view) {
      layout_view->InvalidatePaintForViewAndCompositedLayers();
      // Also need to update all paint properties that might be skipped while
      // the frame was throttled.
      layout_view->AddSubtreePaintPropertyUpdateReason(
          SubtreePaintPropertyUpdateReason::kPreviouslySkipped);
    }
  }

  EventHandlerRegistry& registry = frame_->GetEventHandlerRegistry();
  bool has_handlers =
      (registry.HasEventHandlers(EventHandlerRegistry::kTouchAction) ||
       registry.HasEventHandlers(
           EventHandlerRegistry::kTouchStartOrMoveEventBlocking) ||
       registry.HasEventHandlers(
           EventHandlerRegistry::kTouchStartOrMoveEventBlockingLowLatency));
  if (was_throttled != CanThrottleRendering() && scrolling_coordinator &&
      has_handlers) {
    scrolling_coordinator->TouchEventTargetRectsDidChange(
        &GetFrame().LocalFrameRoot());
  }

  if (FrameScheduler* frame_scheduler = frame_->GetFrameScheduler()) {
    frame_scheduler->SetFrameVisible(!hidden_for_throttling_);
    frame_scheduler->SetCrossOrigin(frame_->IsCrossOriginSubframe());
    frame_scheduler->TraceUrlChange(frame_->GetDocument()->Url().GetString());
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

void LocalFrameView::RecordDeferredLoadingStats() {
  if (!GetFrame().GetDocument()->GetFrame() ||
      !GetFrame().IsCrossOriginSubframe())
    return;

  LocalFrameView* parent = ParentFrameView();
  if (!parent) {
    HTMLFrameOwnerElement* element = GetFrame().DeprecatedLocalOwner();
    // We would fall into an else block on some teardowns and other weird cases.
    if (!element || !element->GetLayoutObject()) {
      GetFrame().GetDocument()->RecordDeferredLoadReason(
          WouldLoadReason::kNoParent);
    }
    return;
  }
  // Small inaccuracy: frames with origins that match the top level might be
  // nested in a cross-origin frame. To keep code simpler, count such frames as
  // WouldLoadVisible, even when their parent is offscreen.
  WouldLoadReason why_parent_loaded = WouldLoadReason::kVisible;
  if (parent->ParentFrameView() && parent->GetFrame().IsCrossOriginSubframe())
    why_parent_loaded = parent->GetFrame().GetDocument()->DeferredLoadReason();

  // If the parent wasn't loaded, the children won't be either.
  if (why_parent_loaded == WouldLoadReason::kCreated)
    return;
  // These frames are never meant to be seen so we will need to load them.
  IntRect frame_rect(FrameRect());
  if (frame_rect.IsEmpty() || frame_rect.MaxY() < 0 || frame_rect.MaxX() < 0) {
    GetFrame().GetDocument()->RecordDeferredLoadReason(why_parent_loaded);
    return;
  }

  IntSize parent_size(parent->Size());
  // First clause: for this rough data collection we assume the user never
  // scrolls right.
  if (frame_rect.X() >= parent_size.Width() || parent_size.Height() <= 0)
    return;

  int this_frame_screens_away = 0;
  // If an frame is created above the current scoll position, this logic counts
  // it as visible.
  if (frame_rect.Y() > 0)
    this_frame_screens_away = frame_rect.Y() / parent_size.Height();
  DCHECK_GE(this_frame_screens_away, 0);

  int parent_screens_away = 0;
  if (why_parent_loaded <= WouldLoadReason::kVisible) {
    parent_screens_away = static_cast<int>(WouldLoadReason::kVisible) -
                          static_cast<int>(why_parent_loaded);
  }

  int total_screens_away = this_frame_screens_away + parent_screens_away;

  // We're collecting data for frames that are at most 3 screens away.
  if (total_screens_away > 3)
    return;

  GetFrame().GetDocument()->RecordDeferredLoadReason(
      static_cast<WouldLoadReason>(static_cast<int>(WouldLoadReason::kVisible) -
                                   total_screens_away));
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
}

unsigned LocalFrameView::GetIntersectionObservationFlags() const {
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
  if (intersection_observation_state_ != kNotNeeded)
    flags |= IntersectionObservation::kExplicitRootObserversNeedUpdate;

  // For observers with implicit roots, we need to check state on the whole
  // local frame tree.
  const LocalFrameView* local_root_view = target_frame.LocalFrameRoot().View();
  for (const LocalFrameView* view = this; view;
       view = view->ParentFrameView()) {
    if (view->intersection_observation_state_ != kNotNeeded) {
      flags |= IntersectionObservation::kImplicitRootObserversNeedUpdate;
      break;
    }
    if (view == local_root_view)
      break;
  }

  return flags;
}

bool LocalFrameView::ShouldThrottleRendering() const {
  bool throttled_for_global_reasons = CanThrottleRendering() &&
                                      frame_->GetDocument() &&
                                      Lifecycle().ThrottlingAllowed();
  if (!throttled_for_global_reasons || needs_forced_compositing_update_)
    return false;

  // Only lifecycle phases up to layout are needed to generate an
  // intersection observation.
  return intersection_observation_state_ != kRequired ||
         GetFrame().LocalFrameRoot().View()->past_layout_lifecycle_update_;
}

bool LocalFrameView::CanThrottleRendering() const {
  if (lifecycle_updates_throttled_)
    return true;
  if (!RuntimeEnabledFeatures::RenderingPipelineThrottlingEnabled())
    return false;
  if (subtree_throttled_)
    return true;
  // We only throttle hidden cross-origin frames. This is to avoid a situation
  // where an ancestor frame directly depends on the pipeline timing of a
  // descendant and breaks as a result of throttling. The rationale is that
  // cross-origin frames must already communicate with asynchronous messages,
  // so they should be able to tolerate some delay in receiving replies from a
  // throttled peer.
  return hidden_for_throttling_ && frame_->IsCrossOriginSubframe();
}

void LocalFrameView::BeginLifecycleUpdates() {
  // Avoid pumping frames for the initially empty document.
  if (!GetFrame().Loader().StateMachine()->CommittedFirstRealDocumentLoad())
    return;
  lifecycle_updates_throttled_ = false;
  if (auto* owner = GetFrame().OwnerLayoutObject())
    owner->SetShouldCheckForPaintInvalidation();

  LayoutView* layout_view = GetLayoutView();
  bool layout_view_is_empty = layout_view && !layout_view->FirstChild();
  if (layout_view_is_empty && !DidFirstLayout() && !NeedsLayout()) {
    // Make sure a display:none iframe gets an initial layout pass.
    layout_view->SetNeedsLayout(LayoutInvalidationReason::kAddedToLayout,
                                kMarkOnlyThis);
  }

  SetupRenderThrottling();
  UpdateRenderThrottlingStatus(hidden_for_throttling_, subtree_throttled_);
  // The compositor will "defer commits" for the main frame until we
  // explicitly request them.
  if (GetFrame().IsMainFrame())
    GetFrame().GetPage()->GetChromeClient().BeginLifecycleUpdates();
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

void LocalFrameView::UpdateSubFrameScrollOnMainReason(
    const Frame& frame,
    MainThreadScrollingReasons parent_reason) {
  MainThreadScrollingReasons reasons = parent_reason;

  if (!GetPage()->GetSettings().GetThreadedScrollingEnabled())
    reasons |= MainThreadScrollingReason::kThreadedScrollingDisabled;

  if (!frame.IsLocalFrame())
    return;

  LocalFrameView& frame_view = *ToLocalFrame(frame).View();
  if (frame_view.ShouldThrottleRendering())
    return;

  if (!frame_view.LayoutViewport())
    return;

  reasons |= frame_view.MainThreadScrollingReasonsPerFrame();
  if (GraphicsLayer* layer_for_scrolling =
          ToLocalFrame(frame).View()->LayoutViewport()->LayerForScrolling()) {
    if (cc::Layer* platform_layer_for_scrolling =
            layer_for_scrolling->CcLayer()) {
      if (reasons) {
        platform_layer_for_scrolling->AddMainThreadScrollingReasons(reasons);
      } else {
        // Clear all main thread scrolling reasons except the one that's set
        // if there is a running scroll animation.
        platform_layer_for_scrolling->ClearMainThreadScrollingReasons(
            ~MainThreadScrollingReason::kHandlingScrollFromMainThread);
      }
    }
  }

  Frame* child = frame.Tree().FirstChild();
  while (child) {
    UpdateSubFrameScrollOnMainReason(*child, reasons);
    child = child->Tree().NextSibling();
  }

  if (frame.IsMainFrame())
    main_thread_scrolling_reasons_ = reasons;
  DCHECK(!MainThreadScrollingReason::HasNonCompositedScrollReasons(
      main_thread_scrolling_reasons_));
}

MainThreadScrollingReasons LocalFrameView::MainThreadScrollingReasonsPerFrame()
    const {
  MainThreadScrollingReasons reasons =
      static_cast<MainThreadScrollingReasons>(0);

  if (ShouldThrottleRendering())
    return reasons;

  if (HasBackgroundAttachmentFixedObjects())
    reasons |= MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;

  // Force main-thread scrolling if the frame has uncomposited position: fixed
  // elements.  Note: we care about this not only for input-scrollable frames
  // but also for overflow: hidden frames, because script can run composited
  // smooth-scroll animations.  For this reason, we use HasOverflow instead of
  // ScrollsOverflow (which is false for overflow: hidden).
  if (LayoutViewport()->HasOverflow() &&
      GetLayoutView()->StyleRef().VisibleToHitTesting() &&
      HasVisibleSlowRepaintViewportConstrainedObjects()) {
    reasons |=
        MainThreadScrollingReason::kHasNonLayerViewportConstrainedObjects;
  }
  return reasons;
}

MainThreadScrollingReasons LocalFrameView::GetMainThreadScrollingReasons()
    const {
  MainThreadScrollingReasons reasons =
      static_cast<MainThreadScrollingReasons>(0);

  if (!GetPage()->GetSettings().GetThreadedScrollingEnabled())
    reasons |= MainThreadScrollingReason::kThreadedScrollingDisabled;

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
    if (!frame->IsLocalFrame())
      continue;
    reasons |=
        ToLocalFrame(frame)->View()->MainThreadScrollingReasonsPerFrame();
  }

  DCHECK(!MainThreadScrollingReason::HasNonCompositedScrollReasons(reasons));
  return reasons;
}

String LocalFrameView::MainThreadScrollingReasonsAsText() {
  MainThreadScrollingReasons reasons = main_thread_scrolling_reasons_;
  // TODO(pdr): We should also use the property tree main thread scrolling
  // reasons when RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled is true.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kPrePaintClean);

    // Slimming paint v2 stores main thread scrolling reasons on property
    // trees instead of in |main_thread_scrolling_reasons_|.
    if (const auto* scroll =
            GetLayoutView()->FirstFragment().PaintProperties()->Scroll()) {
      reasons = scroll->GetMainThreadScrollingReasons();
    }
  } else {
    DCHECK(Lifecycle().GetState() >= DocumentLifecycle::kCompositingClean);
    if (GraphicsLayer* layer_for_scrolling =
            LayoutViewport()->LayerForScrolling()) {
      if (cc::Layer* cc_layer = layer_for_scrolling->CcLayer())
        reasons = cc_layer->main_thread_scrolling_reasons();
    }
  }

  return String(MainThreadScrollingReason::AsText(reasons).c_str());
}

bool LocalFrameView::MapToVisualRectInTopFrameSpace(LayoutRect& rect) {
  // This is the top-level frame, so no mapping necessary.
  if (frame_->IsMainFrame())
    return true;

  LayoutRect viewport_intersection_rect(
      GetFrame().RemoteViewportIntersection());
  rect.Intersect(viewport_intersection_rect);
  if (rect.IsEmpty())
    return false;
  return true;
}

void LocalFrameView::ApplyTransformForTopFrameSpace(
    TransformState& transform_state) {
  // This is the top-level frame, so no mapping necessary.
  if (frame_->IsMainFrame())
    return;

  LayoutRect viewport_intersection_rect(
      GetFrame().RemoteViewportIntersection());
  transform_state.Move(LayoutSize(-viewport_intersection_rect.X(),
                                  -viewport_intersection_rect.Y()));
}

LayoutUnit LocalFrameView::CaretWidth() const {
  return LayoutUnit(
      std::max<float>(1.0, GetChromeClient()->WindowToViewportScalar(1)));
}

LocalFrameUkmAggregator& LocalFrameView::EnsureUkmAggregator() {
  if (!ukm_aggregator_) {
    ukm_aggregator_.reset(
        new LocalFrameUkmAggregator(frame_->GetDocument()->UkmSourceID(),
                                    frame_->GetDocument()->UkmRecorder()));
  }
  return *ukm_aggregator_;
}

}  // namespace blink
