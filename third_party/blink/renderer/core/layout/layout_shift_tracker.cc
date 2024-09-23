// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"

#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

using ReattachHookScope = LayoutShiftTracker::ReattachHookScope;
ReattachHookScope* ReattachHookScope::top_ = nullptr;

using ContainingBlockScope = LayoutShiftTracker::ContainingBlockScope;
ContainingBlockScope* ContainingBlockScope::top_ = nullptr;

namespace {

constexpr base::TimeDelta kTimerDelay = base::Milliseconds(500);
const float kMovementThreshold = 3.0;  // CSS pixels.

// Calculates the physical coordinates of the starting point in the current
// coordinate space. |paint_offset| is the physical offset of the top-left
// corner. The starting point can be any of the four corners of the box,
// depending on the writing mode and text direction. Note that the result is
// still in physical coordinates, just may be of a different corner.
// See https://wicg.github.io/layout-instability/#starting-point.
gfx::PointF StartingPoint(const PhysicalOffset& paint_offset,
                          const LayoutBox& box,
                          const PhysicalSize& size) {
  PhysicalOffset starting_point = paint_offset;
  auto writing_direction = box.StyleRef().GetWritingDirection();
  if (writing_direction.IsFlippedBlocks()) [[unlikely]] {
    starting_point.left += size.width;
  }
  if (writing_direction.IsRtl()) [[unlikely]] {
    if (writing_direction.IsHorizontal())
      starting_point.left += size.width;
    else
      starting_point.top += size.height;
  }
  return gfx::PointF(starting_point);
}

// Returns the part a rect logically below a starting point.
PhysicalRect RectBelowStartingPoint(const PhysicalRect& rect,
                                    const PhysicalOffset& starting_point,
                                    LayoutUnit logical_height,
                                    WritingDirectionMode writing_direction) {
  PhysicalRect result = rect;
  if (writing_direction.IsHorizontal()) {
    result.ShiftTopEdgeTo(starting_point.top);
    result.SetHeight(logical_height);
  } else {
    result.SetWidth(logical_height);
    if (writing_direction.IsFlippedBlocks())
      result.ShiftRightEdgeTo(starting_point.left);
    else
      result.ShiftLeftEdgeTo(starting_point.left);
  }
  return result;
}

float GetMoveDistance(const gfx::PointF& old_starting_point,
                      const gfx::PointF& new_starting_point) {
  gfx::Vector2dF location_delta = new_starting_point - old_starting_point;
  return std::max(fabs(location_delta.x()), fabs(location_delta.y()));
}

bool EqualWithinMovementThreshold(const gfx::PointF& a,
                                  const gfx::PointF& b,
                                  float threshold_physical_px) {
  return fabs(a.x() - b.x()) < threshold_physical_px &&
         fabs(a.y() - b.y()) < threshold_physical_px;
}

bool SmallerThanRegionGranularity(const PhysicalRect& rect) {
  // Normally we paint by snapping to whole pixels, so rects smaller than half
  // a pixel may be invisible.
  return rect.Width() < 0.5 || rect.Height() < 0.5;
}

void RectToTracedValue(const gfx::Rect& rect,
                       TracedValue& value,
                       const char* key = nullptr) {
  if (key)
    value.BeginArray(key);
  else
    value.BeginArray();
  value.PushInteger(rect.x());
  value.PushInteger(rect.y());
  value.PushInteger(rect.width());
  value.PushInteger(rect.height());
  value.EndArray();
}

void RegionToTracedValue(const LayoutShiftRegion& region, TracedValue& value) {
  cc::Region blink_region;
  for (const gfx::Rect& rect : region.GetRects())
    blink_region.Union(rect);

  value.BeginArray("region_rects");
  for (gfx::Rect rect : blink_region)
    RectToTracedValue(rect, value);
  value.EndArray();
}

bool ShouldLog(const LocalFrame& frame) {
  if (!VLOG_IS_ON(1))
    return false;

  DCHECK(frame.GetDocument());
  const String& url = frame.GetDocument()->Url().GetString();
  return !url.StartsWith("devtools:");
}

}  // namespace

LayoutShiftTracker::LayoutShiftTracker(LocalFrameView* frame_view)
    : frame_view_(frame_view),
      // This eliminates noise from the private Page object created by
      // SVGImage::DataChanged.
      is_active_(!frame_view->GetFrame()
                      .GetChromeClient()
                      .IsIsolatedSVGChromeClient()),
      score_(0.0),
      weighted_score_(0.0),
      timer_(frame_view->GetFrame().GetTaskRunner(TaskType::kInternalDefault),
             this,
             &LayoutShiftTracker::TimerFired),
      frame_max_distance_(0.0),
      overall_max_distance_(0.0),
      observed_input_or_scroll_(false),
      most_recent_input_timestamp_initialized_(false) {}

bool LayoutShiftTracker::NeedsToTrack(const LayoutObject& object) const {
  if (!is_active_)
    return false;

  if (object.GetDocument().IsPrintingOrPaintingPreview())
    return false;

  // SVG elements don't participate in the normal layout algorithms and are
  // more likely to be used for animations.
  if (object.IsSVGChild())
    return false;

  if (object.StyleRef().UsedVisibility() != EVisibility::kVisible) {
    return false;
  }

  if (IsA<LayoutText>(object)) {
    if (!ContainingBlockScope::top_)
      return false;
    if (object.IsBR())
      return false;
    if (object.StyleRef().GetFont().ShouldSkipDrawing())
      return false;
    return true;
  }

  const auto* box = DynamicTo<LayoutBox>(object);
  if (!box) {
    return false;
  }

  if (SmallerThanRegionGranularity(box->VisualOverflowRectAllowingUnset())) {
    return false;
  }

  if (auto* display_lock_context = box->GetDisplayLockContext()) {
    if (display_lock_context->IsAuto() && display_lock_context->IsLocked())
      return false;
  }

  // Don't report shift of anonymous objects. Will report the children because
  // we want report real DOM nodes.
  if (box->IsAnonymous()) {
    return false;
  }

  // Ignore sticky-positioned objects that move on scroll.
  // TODO(skobes): Find a way to detect when these objects shift.
  if (box->IsStickyPositioned()) {
    return false;
  }

  // A LayoutView can't move by itself.
  if (box->IsLayoutView()) {
    return false;
  }

  if (Element* element = DynamicTo<Element>(object.GetNode())) {
    if (element->IsSliderThumbElement())
      return false;
  }

  if (const auto* block = DynamicTo<LayoutBlock>(box)) {
    // Just check the simplest case. For more complex cases, we should suggest
    // the developer to use visibility:hidden.
    if (block->FirstChild()) {
      return true;
    }
    if (box->HasBoxDecorationBackground() || box->GetScrollableArea() ||
        box->StyleRef().HasVisualOverflowingEffect()) {
      return true;
    }
    return false;
  }

  return true;
}

void LayoutShiftTracker::ObjectShifted(
    const LayoutObject& object,
    const PropertyTreeStateOrAlias& property_tree_state,
    const PhysicalRect& old_rect,
    const PhysicalRect& new_rect,
    const gfx::PointF& old_starting_point,
    const gfx::Vector2dF& translation_delta,
    const gfx::Vector2dF& scroll_delta,
    const gfx::Vector2dF& scroll_anchor_adjustment,
    const gfx::PointF& new_starting_point) {
  // The caller should ensure these conditions.
  DCHECK(!old_rect.IsEmpty());
  DCHECK(!new_rect.IsEmpty());

  float threshold_physical_px =
      kMovementThreshold * object.StyleRef().EffectiveZoom();

  // Check shift of starting point, including 2d-translation and scroll
  // deltas.
  if (EqualWithinMovementThreshold(old_starting_point, new_starting_point,
                                   threshold_physical_px))
    return;

  // Check shift of 2d-translation-indifferent starting point.
  if (!translation_delta.IsZero() &&
      EqualWithinMovementThreshold(old_starting_point + translation_delta,
                                   new_starting_point, threshold_physical_px))
    return;

  // Check shift of scroll-indifferent starting point.
  if (!scroll_delta.IsZero() &&
      EqualWithinMovementThreshold(old_starting_point + scroll_delta,
                                   new_starting_point, threshold_physical_px))
    return;

  if (!scroll_anchor_adjustment.IsZero() &&
      EqualWithinMovementThreshold(
          old_starting_point + scroll_delta + scroll_anchor_adjustment,
          new_starting_point, threshold_physical_px)) {
    return;
  }

  // Check shift of 2d-translation-and-scroll-indifferent starting point.
  gfx::Vector2dF translation_and_scroll_delta =
      scroll_delta + translation_delta;
  if (!translation_and_scroll_delta.IsZero() &&
      EqualWithinMovementThreshold(
          old_starting_point + translation_and_scroll_delta, new_starting_point,
          threshold_physical_px))
    return;

  const auto& root_state =
      object.View()->FirstFragment().LocalBorderBoxProperties();
  FloatClipRect clip_rect =
      GeometryMapper::LocalToAncestorClipRect(property_tree_state, root_state);
  if (frame_view_->GetFrame().IsMainFrame()) {
    // Apply the visual viewport clip.
    clip_rect.Intersect(FloatClipRect(
        frame_view_->GetPage()->GetVisualViewport().VisibleRect()));
  }

  // If the clip region is empty, then the resulting layout shift isn't visible
  // in the viewport so ignore it.
  if (clip_rect.Rect().IsEmpty())
    return;

  auto transform = GeometryMapper::SourceToDestinationProjection(
      property_tree_state.Transform(), root_state.Transform());
  // TODO(crbug.com/1187979): Shift by |scroll_delta| to keep backward
  // compatibility in https://crrev.com/c/2754969. See the bug for details.
  gfx::PointF old_starting_point_in_root =
      transform.MapPoint(old_starting_point + scroll_delta);
  gfx::PointF new_starting_point_in_root =
      transform.MapPoint(new_starting_point);

  if (EqualWithinMovementThreshold(old_starting_point_in_root,
                                   new_starting_point_in_root,
                                   threshold_physical_px))
    return;

  gfx::RectF old_rect_in_root(old_rect);
  // TODO(crbug.com/1187979): Shift by |scroll_delta| to keep backward
  // compatibility in https://crrev.com/c/2754969. See the bug for details.
  old_rect_in_root.Offset(scroll_delta);
  old_rect_in_root = transform.MapRect(old_rect_in_root);
  gfx::RectF new_rect_in_root(new_rect);
  new_rect_in_root = transform.MapRect(new_rect_in_root);

  gfx::Rect visible_old_rect = gfx::ToRoundedRect(
      gfx::IntersectRects(old_rect_in_root, clip_rect.Rect()));
  gfx::Rect visible_new_rect = gfx::ToRoundedRect(
      gfx::IntersectRects(new_rect_in_root, clip_rect.Rect()));
  if (visible_old_rect.IsEmpty() && visible_new_rect.IsEmpty())
    return;

  // If the object moved from or to out of view, ignore the shift if it's in
  // the inline direction only.
  if (visible_old_rect.IsEmpty() || visible_new_rect.IsEmpty()) {
    gfx::PointF old_inline_direction_indifferent_starting_point_in_root =
        old_starting_point_in_root;
    if (object.IsHorizontalWritingMode()) {
      old_inline_direction_indifferent_starting_point_in_root.set_x(
          new_starting_point_in_root.x());
    } else {
      old_inline_direction_indifferent_starting_point_in_root.set_y(
          new_starting_point_in_root.y());
    }
    if (EqualWithinMovementThreshold(
            old_inline_direction_indifferent_starting_point_in_root,
            new_starting_point_in_root, threshold_physical_px)) {
      return;
    }
  }

  // Compute move distance based on starting points in root, to accurately
  // determine how much the element moved.
  float move_distance =
      GetMoveDistance(old_starting_point_in_root, new_starting_point_in_root);
  if (!std::isfinite(move_distance)) {
    return;
  }
  DCHECK_GT(move_distance, 0.f);
  frame_max_distance_ = std::max(frame_max_distance_, move_distance);

  LocalFrame& frame = frame_view_->GetFrame();
  if (ShouldLog(frame)) {
    VLOG(2) << "in " << (frame.IsOutermostMainFrame() ? "" : "subframe ")
            << frame.GetDocument()->Url() << ", " << object << " moved from "
            << old_rect_in_root.ToString() << " to "
            << new_rect_in_root.ToString() << " (visible from "
            << visible_old_rect.ToString() << " to "
            << visible_new_rect.ToString() << ")";
    if (old_starting_point_in_root != old_rect_in_root.origin() ||
        new_starting_point_in_root != new_rect_in_root.origin() ||
        !translation_delta.IsZero() || !scroll_delta.IsZero()) {
      VLOG(2) << " (starting point from "
              << old_starting_point_in_root.ToString() << " to "
              << new_starting_point_in_root.ToString() << ")";
    }
  }

  region_.AddRect(visible_old_rect);
  region_.AddRect(visible_new_rect);

  if (Node* node = object.GetNode()) {
    MaybeRecordAttribution(
        {node->GetDomNodeId(), visible_old_rect, visible_new_rect});
  }
}

LayoutShiftTracker::Attribution::operator bool() const {
  return node_id != kInvalidDOMNodeId;
}

bool LayoutShiftTracker::Attribution::Encloses(const Attribution& other) const {
  return old_visual_rect.Contains(other.old_visual_rect) &&
         new_visual_rect.Contains(other.new_visual_rect);
}

uint64_t LayoutShiftTracker::Attribution::Area() const {
  uint64_t old_area = old_visual_rect.size().Area64();
  uint64_t new_area = new_visual_rect.size().Area64();

  gfx::Rect intersection =
      gfx::IntersectRects(old_visual_rect, new_visual_rect);
  uint64_t shared_area = intersection.size().Area64();
  return old_area + new_area - shared_area;
}

bool LayoutShiftTracker::Attribution::MoreImpactfulThan(
    const Attribution& other) const {
  return Area() > other.Area();
}

void LayoutShiftTracker::MaybeRecordAttribution(
    const Attribution& attribution) {
  Attribution* smallest = nullptr;
  for (auto& slot : attributions_) {
    if (!slot || attribution.Encloses(slot)) {
      slot = attribution;
      return;
    }
    if (slot.Encloses(attribution))
      return;
    if (!smallest || smallest->MoreImpactfulThan(slot))
      smallest = &slot;
  }
  // No empty slots or redundancies. Replace smallest existing slot if larger.
  if (attribution.MoreImpactfulThan(*smallest))
    *smallest = attribution;
}

void LayoutShiftTracker::NotifyBoxPrePaint(
    const LayoutBox& box,
    const PropertyTreeStateOrAlias& property_tree_state,
    const PhysicalRect& old_rect,
    const PhysicalRect& new_rect,
    const PhysicalOffset& old_paint_offset,
    const gfx::Vector2dF& translation_delta,
    const gfx::Vector2dF& scroll_delta,
    const gfx::Vector2dF& scroll_anchor_adjustment,
    const PhysicalOffset& new_paint_offset) {
  DCHECK(NeedsToTrack(box));
  ObjectShifted(box, property_tree_state, old_rect, new_rect,
                StartingPoint(old_paint_offset, box, box.PreviousSize()),
                translation_delta, scroll_delta, scroll_anchor_adjustment,
                StartingPoint(new_paint_offset, box, box.Size()));
}

void LayoutShiftTracker::NotifyTextPrePaint(
    const LayoutText& text,
    const PropertyTreeStateOrAlias& property_tree_state,
    const LogicalOffset& old_starting_point,
    const LogicalOffset& new_starting_point,
    const PhysicalOffset& old_paint_offset,
    const gfx::Vector2dF& translation_delta,
    const gfx::Vector2dF& scroll_delta,
    const gfx::Vector2dF& scroll_anchor_adjustment,
    const PhysicalOffset& new_paint_offset,
    LayoutUnit logical_height) {
  DCHECK(NeedsToTrack(text));
  auto* block = ContainingBlockScope::top_;
  DCHECK(block);

  auto writing_direction = text.StyleRef().GetWritingDirection();
  PhysicalOffset old_physical_starting_point =
      old_paint_offset + old_starting_point.ConvertToPhysical(writing_direction,
                                                              block->old_size_,
                                                              PhysicalSize());
  PhysicalOffset new_physical_starting_point =
      new_paint_offset + new_starting_point.ConvertToPhysical(writing_direction,
                                                              block->new_size_,
                                                              PhysicalSize());

  PhysicalRect old_rect =
      RectBelowStartingPoint(block->old_rect_, old_physical_starting_point,
                             logical_height, writing_direction);
  if (old_rect.IsEmpty())
    return;
  PhysicalRect new_rect =
      RectBelowStartingPoint(block->new_rect_, new_physical_starting_point,
                             logical_height, writing_direction);
  if (new_rect.IsEmpty())
    return;

  ObjectShifted(text, property_tree_state, old_rect, new_rect,
                gfx::PointF(old_physical_starting_point), translation_delta,
                scroll_delta, scroll_anchor_adjustment,
                gfx::PointF(new_physical_starting_point));
}

double LayoutShiftTracker::SubframeWeightingFactor() const {
  LocalFrame& frame = frame_view_->GetFrame();
  if (frame.IsOutermostMainFrame())
    return 1;

  // TODO(crbug.com/1346602): Enabling frames from a fenced frame tree to map
  // to the outermost main frame enables fenced content to learn about its
  // position in the embedder which can be used to communicate from embedder to
  // embeddee. For now, assume any frame in a fenced frame is fully visible to
  // avoid introducing a side channel but this will require design work to fix
  // in the long term.
  if (frame.IsInFencedFrameTree()) {
    return 1;
  }

  // Map the subframe view rect into the coordinate space of the local root.
  FloatClipRect subframe_cliprect(gfx::RectF(gfx::SizeF(frame_view_->Size())));
  const LocalFrame& local_root = frame.LocalFrameRoot();
  GeometryMapper::LocalToAncestorVisualRect(
      frame_view_->GetLayoutView()->FirstFragment().LocalBorderBoxProperties(),
      local_root.ContentLayoutObject()
          ->FirstFragment()
          .LocalBorderBoxProperties(),
      subframe_cliprect);
  auto subframe_rect = PhysicalRect::EnclosingRect(subframe_cliprect.Rect());

  // Intersect with the portion of the local root that overlaps the main frame.
  local_root.View()->MapToVisualRectInRemoteRootFrame(subframe_rect);
  gfx::Size subframe_visible_size = subframe_rect.PixelSnappedSize();
  gfx::Size main_frame_size = frame.GetPage()->GetVisualViewport().Size();

  if (main_frame_size.Area64() == 0) {
    return 0;
  }
  // TODO(crbug.com/940711): This comparison ignores page scale and CSS
  // transforms above the local root.
  return static_cast<double>(subframe_visible_size.Area64()) /
         main_frame_size.Area64();
}

void LayoutShiftTracker::NotifyPrePaintFinishedInternal() {
  if (!is_active_)
    return;
  if (region_.IsEmpty())
    return;

  gfx::Rect viewport = frame_view_->GetScrollableArea()->VisibleContentRect();
  if (viewport.IsEmpty())
    return;

  double viewport_area = double(viewport.width()) * double(viewport.height());
  double impact_fraction = region_.Area() / viewport_area;
  DCHECK_GT(impact_fraction, 0);

  DCHECK_GT(frame_max_distance_, 0.0);
  double viewport_max_dimension = std::max(viewport.width(), viewport.height());
  double move_distance_factor =
      (frame_max_distance_ < viewport_max_dimension)
          ? double(frame_max_distance_) / viewport_max_dimension
          : 1.0;
  double score_delta = impact_fraction * move_distance_factor;
  double weighted_score_delta = score_delta * SubframeWeightingFactor();

  overall_max_distance_ = std::max(overall_max_distance_, frame_max_distance_);

  LocalFrame& frame = frame_view_->GetFrame();
  if (ShouldLog(frame)) {
    VLOG(2) << "in " << (frame.IsOutermostMainFrame() ? "" : "subframe ")
            << frame.GetDocument()->Url() << ", viewport was "
            << (impact_fraction * 100) << "% impacted with distance fraction "
            << move_distance_factor << " and subframe weighting factor "
            << SubframeWeightingFactor();
  }

  if (pointerdown_pending_data_.num_pointerdowns > 0 ||
      pointerdown_pending_data_.num_pressed_mouse_buttons > 0) {
    pointerdown_pending_data_.score_delta += score_delta;
    pointerdown_pending_data_.weighted_score_delta += weighted_score_delta;
  } else {
    ReportShift(score_delta, weighted_score_delta);
  }

  if (!region_.IsEmpty() && !timer_.IsActive())
    SendLayoutShiftRectsToHud(region_.GetRects());
}

void LayoutShiftTracker::NotifyPrePaintFinished() {
  NotifyPrePaintFinishedInternal();

  // Reset accumulated state.
  region_.Reset();
  frame_max_distance_ = 0.0;
  attributions_.fill(Attribution());
}

LayoutShift::AttributionList LayoutShiftTracker::CreateAttributionList() const {
  LayoutShift::AttributionList list;
  for (const Attribution& att : attributions_) {
    if (att.node_id == kInvalidDOMNodeId)
      break;
    list.push_back(LayoutShiftAttribution::Create(
        DOMNodeIds::NodeForId(att.node_id),
        DOMRectReadOnly::FromRect(att.old_visual_rect),
        DOMRectReadOnly::FromRect(att.new_visual_rect)));
  }
  return list;
}

void LayoutShiftTracker::SubmitPerformanceEntry(double score_delta,
                                                bool had_recent_input) const {
  LocalDOMWindow* window = frame_view_->GetFrame().DomWindow();
  if (!window)
    return;
  WindowPerformance* performance = DOMWindowPerformance::performance(*window);
  DCHECK(performance);

  double input_timestamp = LastInputTimestamp();
  LayoutShift* entry =
      LayoutShift::Create(performance->now(), score_delta, had_recent_input,
                          input_timestamp, CreateAttributionList(), window);

  // Add WPT for LayoutShift. See crbug.com/1320878.

  performance->AddLayoutShiftEntry(entry);
}

void LayoutShiftTracker::ReportShift(double score_delta,
                                     double weighted_score_delta) {
  LocalFrame& frame = frame_view_->GetFrame();
  bool had_recent_input = timer_.IsActive();

  if (!had_recent_input) {
    score_ += score_delta;
    if (weighted_score_delta > 0) {
      weighted_score_ += weighted_score_delta;
      frame.Client()->DidObserveLayoutShift(weighted_score_delta,
                                            observed_input_or_scroll_);
    }
  }

  SubmitPerformanceEntry(score_delta, had_recent_input);

  TRACE_EVENT_INSTANT2(
      "loading", "LayoutShift", TRACE_EVENT_SCOPE_THREAD, "data",
      PerFrameTraceData(score_delta, weighted_score_delta, had_recent_input),
      "frame", GetFrameIdForTracing(&frame));

  if (ShouldLog(frame)) {
    VLOG(2) << "in " << (frame.IsOutermostMainFrame() ? "" : "subframe ")
            << frame.GetDocument()->Url().GetString() << ", layout shift of "
            << score_delta
            << (had_recent_input ? " excluded by recent input" : " reported")
            << "; cumulative score is " << score_;
  }
}

void LayoutShiftTracker::NotifyInput(const WebInputEvent& event) {
  const WebInputEvent::Type type = event.GetType();
  bool release_all_mouse_buttons = false;
  if (type == WebInputEvent::Type::kMouseUp) {
    if (pointerdown_pending_data_.num_pressed_mouse_buttons > 0)
      pointerdown_pending_data_.num_pressed_mouse_buttons--;
    release_all_mouse_buttons =
        pointerdown_pending_data_.num_pressed_mouse_buttons == 0;
  }
  bool release_all_pointers = false;
  if (type == WebInputEvent::Type::kPointerUp) {
    if (pointerdown_pending_data_.num_pointerdowns > 0)
      pointerdown_pending_data_.num_pointerdowns--;
    release_all_pointers = pointerdown_pending_data_.num_pointerdowns == 0;
  }

  const bool event_type_stops_pointerdown_buffering =
      type == WebInputEvent::Type::kPointerCausedUaAction ||
      type == WebInputEvent::Type::kPointerCancel;

  // Only non-hovering pointerdown requires buffering.
  const bool is_hovering_pointerdown =
      type == WebInputEvent::Type::kPointerDown &&
      static_cast<const WebPointerEvent&>(event).hovering;

  const bool should_trigger_shift_exclusion =
      type == WebInputEvent::Type::kMouseDown ||
      type == WebInputEvent::Type::kKeyDown ||
      type == WebInputEvent::Type::kRawKeyDown ||
      // We need to explicitly include tap, as if there are no listeners, we
      // won't receive the pointer events.
      type == WebInputEvent::Type::kGestureTap || is_hovering_pointerdown ||
      release_all_pointers || release_all_mouse_buttons;

  if (should_trigger_shift_exclusion) {
    observed_input_or_scroll_ = true;

    // This cancels any previously scheduled task from the same timer.
    timer_.StartOneShot(kTimerDelay, FROM_HERE);
    UpdateInputTimestamp(event.TimeStamp());
  }

  if (event_type_stops_pointerdown_buffering || release_all_mouse_buttons ||
      release_all_pointers) {
    double score_delta = pointerdown_pending_data_.score_delta;
    if (score_delta > 0)
      ReportShift(score_delta, pointerdown_pending_data_.weighted_score_delta);
    pointerdown_pending_data_ = PointerdownPendingData();
  }
  if (type == WebInputEvent::Type::kPointerDown && !is_hovering_pointerdown)
    pointerdown_pending_data_.num_pointerdowns++;
  if (type == WebInputEvent::Type::kMouseDown)
    pointerdown_pending_data_.num_pressed_mouse_buttons++;
}

void LayoutShiftTracker::UpdateInputTimestamp(base::TimeTicks timestamp) {
  if (!most_recent_input_timestamp_initialized_) {
    most_recent_input_timestamp_ = timestamp;
    most_recent_input_timestamp_initialized_ = true;
  } else if (timestamp > most_recent_input_timestamp_) {
    most_recent_input_timestamp_ = timestamp;
  }
}

void LayoutShiftTracker::NotifyScroll(mojom::blink::ScrollType scroll_type,
                                      ScrollOffset delta) {
  // Only set observed_input_or_scroll_ for user-initiated scrolls, and not
  // other scrolls such as hash fragment navigations.
  if (scroll_type == mojom::blink::ScrollType::kUser ||
      scroll_type == mojom::blink::ScrollType::kCompositor)
    observed_input_or_scroll_ = true;
}

void LayoutShiftTracker::NotifyViewportSizeChanged() {
  UpdateTimerAndInputTimestamp();
}

void LayoutShiftTracker::NotifyFindInPageInput() {
  UpdateTimerAndInputTimestamp();
}

void LayoutShiftTracker::NotifyChangeEvent() {
  UpdateTimerAndInputTimestamp();
}

void LayoutShiftTracker::NotifyZoomLevelChanged() {
  UpdateTimerAndInputTimestamp();
}

void LayoutShiftTracker::NotifyBrowserInitiatedSameDocumentNavigation() {
  UpdateTimerAndInputTimestamp();
}

void LayoutShiftTracker::UpdateTimerAndInputTimestamp() {
  // This cancels any previously scheduled task from the same timer.
  timer_.StartOneShot(kTimerDelay, FROM_HERE);
  UpdateInputTimestamp(base::TimeTicks::Now());
}

double LayoutShiftTracker::LastInputTimestamp() const {
  LocalDOMWindow* window = frame_view_->GetFrame().DomWindow();
  if (!window)
    return 0.0;
  WindowPerformance* performance = DOMWindowPerformance::performance(*window);
  DCHECK(performance);

  return most_recent_input_timestamp_initialized_
             ? performance->MonotonicTimeToDOMHighResTimeStamp(
                   most_recent_input_timestamp_)
             : 0.0;
}

std::unique_ptr<TracedValue> LayoutShiftTracker::PerFrameTraceData(
    double score_delta,
    double weighted_score_delta,
    bool input_detected) const {
  auto value = std::make_unique<TracedValue>();
  value->SetDouble("score", score_delta);
  value->SetDouble("weighted_score_delta", weighted_score_delta);
  value->SetDouble("cumulative_score", score_);
  value->SetDouble("overall_max_distance", overall_max_distance_);
  value->SetDouble("frame_max_distance", frame_max_distance_);
  RegionToTracedValue(region_, *value);
  value->SetBoolean("is_main_frame",
                    frame_view_->GetFrame().IsOutermostMainFrame());
  value->SetBoolean("had_recent_input", input_detected);
  value->SetDouble("last_input_timestamp", LastInputTimestamp());
  AttributionsToTracedValue(*value);
  return value;
}

void LayoutShiftTracker::AttributionsToTracedValue(TracedValue& value) const {
  auto it = attributions_.begin();
  if (!*it)
    return;

  bool should_include_names;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("layout_shift.debug"), &should_include_names);

  value.BeginArray("impacted_nodes");
  while (it != attributions_.end() && it->node_id != kInvalidDOMNodeId) {
    value.BeginDictionary();
    value.SetInteger("node_id", it->node_id);
    RectToTracedValue(it->old_visual_rect, value, "old_rect");
    RectToTracedValue(it->new_visual_rect, value, "new_rect");
    if (should_include_names) {
      Node* node = DOMNodeIds::NodeForId(it->node_id);
      value.SetString("debug_name", node ? node->DebugName() : "");
    }
    value.EndDictionary();
    it++;
  }
  value.EndArray();
}

void LayoutShiftTracker::SendLayoutShiftRectsToHud(
    const Vector<gfx::Rect>& int_rects) {
  // Store the layout shift rects in the HUD layer.
  auto* cc_layer = frame_view_->RootCcLayer();
  if (cc_layer && cc_layer->layer_tree_host()) {
    if (!cc_layer->layer_tree_host()->GetDebugState().show_layout_shift_regions)
      return;
    if (cc_layer->layer_tree_host()->hud_layer()) {
      WebVector<gfx::Rect> rects;
      cc::Region blink_region;
      for (const gfx::Rect& rect : int_rects)
        blink_region.Union(rect);
      for (gfx::Rect rect : blink_region)
        rects.emplace_back(rect);
      cc_layer->layer_tree_host()->hud_layer()->SetLayoutShiftRects(
          rects.ReleaseVector());
      cc_layer->layer_tree_host()->hud_layer()->SetNeedsPushProperties();
    }
  }
}

void LayoutShiftTracker::ResetTimerForTesting() {
  timer_.Stop();
}

void LayoutShiftTracker::Trace(Visitor* visitor) const {
  visitor->Trace(frame_view_);
  visitor->Trace(timer_);
}

ReattachHookScope::ReattachHookScope(const Node& node) : outer_(top_) {
  if (node.GetLayoutObject())
    top_ = this;
}

ReattachHookScope::~ReattachHookScope() {
  top_ = outer_;
}

void ReattachHookScope::NotifyDetach(const Node& node) {
  if (!top_)
    return;
  auto* layout_object = node.GetLayoutObject();
  if (!layout_object || layout_object->ShouldSkipNextLayoutShiftTracking() ||
      !layout_object->IsBox())
    return;

  auto& map = top_->geometries_before_detach_;
  auto& fragment = layout_object->GetMutableForPainting().FirstFragment();

  // Save the visual rect for restoration on future reattachment.
  const auto& box = To<LayoutBox>(*layout_object);
  PhysicalRect visual_overflow_rect = box.PreviousVisualOverflowRect();
  if (visual_overflow_rect.IsEmpty() && box.PreviousSize().IsEmpty())
    return;
  bool has_paint_offset_transform = false;
  if (auto* properties = fragment.PaintProperties())
    has_paint_offset_transform = properties->PaintOffsetTranslation();
  map.Set(&node, Geometry{fragment.PaintOffset(), box.PreviousSize(),
                          visual_overflow_rect, has_paint_offset_transform});
}

void ReattachHookScope::NotifyAttach(const Node& node) {
  if (!top_)
    return;
  auto* layout_object = node.GetLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return;
  auto& map = top_->geometries_before_detach_;

  // Restore geometries that was saved during detach. Note: this does not
  // affect paint invalidation; we will fully invalidate the new layout object.
  auto iter = map.find(&node);
  if (iter == map.end())
    return;
  To<LayoutBox>(layout_object)
      ->GetMutableForPainting()
      .SetPreviousGeometryForLayoutShiftTracking(
          iter->value.paint_offset, iter->value.size,
          iter->value.visual_overflow_rect);
  layout_object->SetShouldSkipNextLayoutShiftTracking(false);
  layout_object->SetShouldAssumePaintOffsetTranslationForLayoutShiftTracking(
      iter->value.has_paint_offset_translation);
}

}  // namespace blink
