// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_shift_tracker.h"

#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

using ReattachHookScope = LayoutShiftTracker::ReattachHookScope;
ReattachHookScope* ReattachHookScope::top_ = nullptr;

using ContainingBlockScope = LayoutShiftTracker::ContainingBlockScope;
ContainingBlockScope* ContainingBlockScope::top_ = nullptr;

namespace {

constexpr base::TimeDelta kTimerDelay = base::TimeDelta::FromMilliseconds(500);
const float kMovementThreshold = 3.0;  // CSS pixels.

// Calculates the physical coordinates of the starting point in the current
// coordinate space. |paint_offset| is the physical offset of the top-left
// corner. The starting point can be any of the four corners of the box,
// depending on the writing mode and text direction. Note that the result is
// still in physical coordinates, just may be of a different corner.
// See https://wicg.github.io/layout-instability/#starting-point.
FloatPoint StartingPoint(const PhysicalOffset& paint_offset,
                         const LayoutBox& box,
                         const LayoutSize& size) {
  PhysicalOffset starting_point = paint_offset;
  auto writing_direction = box.StyleRef().GetWritingDirection();
  if (UNLIKELY(writing_direction.IsFlippedBlocks()))
    starting_point.left += size.Width();
  if (UNLIKELY(writing_direction.IsRtl())) {
    if (writing_direction.IsHorizontal())
      starting_point.left += size.Width();
    else
      starting_point.top += size.Height();
  }
  return FloatPoint(starting_point);
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

float GetMoveDistance(const FloatPoint& old_starting_point,
                      const FloatPoint& new_starting_point) {
  FloatSize location_delta = new_starting_point - old_starting_point;
  return std::max(fabs(location_delta.Width()), fabs(location_delta.Height()));
}

bool EqualWithinMovementThreshold(const FloatPoint& a,
                                  const FloatPoint& b,
                                  float threshold_physical_px) {
  return fabs(a.X() - b.X()) < threshold_physical_px &&
         fabs(a.Y() - b.Y()) < threshold_physical_px;
}

bool SmallerThanRegionGranularity(const PhysicalRect& rect) {
  // The region uses integer coordinates, so the rects are snapped to
  // pixel boundaries. Ignore rects smaller than half a pixel.
  return rect.Width() < 0.5 || rect.Height() < 0.5;
}

void RectToTracedValue(const IntRect& rect,
                       TracedValue& value,
                       const char* key = nullptr) {
  if (key)
    value.BeginArray(key);
  else
    value.BeginArray();
  value.PushInteger(rect.X());
  value.PushInteger(rect.Y());
  value.PushInteger(rect.Width());
  value.PushInteger(rect.Height());
  value.EndArray();
}

void RegionToTracedValue(const LayoutShiftRegion& region, TracedValue& value) {
  Region blink_region;
  for (IntRect rect : region.GetRects())
    blink_region.Unite(Region(rect));

  value.BeginArray("region_rects");
  for (const IntRect& rect : blink_region.Rects())
    RectToTracedValue(rect, value);
  value.EndArray();
}

#if DCHECK_IS_ON()
bool ShouldLog(const LocalFrame& frame) {
  const String& url = frame.GetDocument()->Url().GetString();
  return !url.StartsWith("devtools:");
}
#endif

}  // namespace

LayoutShiftTracker::LayoutShiftTracker(LocalFrameView* frame_view)
    : frame_view_(frame_view),
      // This eliminates noise from the private Page object created by
      // SVGImage::DataChanged.
      is_active_(
          !frame_view_->GetFrame().GetChromeClient().IsSVGImageChromeClient()),
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

  // SVG elements don't participate in the normal layout algorithms and are
  // more likely to be used for animations.
  if (object.IsSVGChild())
    return false;

  if (object.IsText())
    return !object.IsBR() && ContainingBlockScope::top_;

  if (!object.IsBox())
    return false;

  // Don't report shift of anonymous objects. Will report the children because
  // we want report real DOM nodes.
  if (object.IsAnonymous())
    return false;

  if (object.StyleRef().Visibility() != EVisibility::kVisible)
    return false;

  // Ignore sticky-positioend objects that move on scroll.
  // TODO(skobes): Find a way to detect when these objects shift.
  if (object.IsStickyPositioned())
    return false;

  if (object.IsLayoutView())
    return false;

  if (Element* element = DynamicTo<Element>(object.GetNode())) {
    if (element->IsSliderThumbElement())
      return false;
  }

  return true;
}

void LayoutShiftTracker::ObjectShifted(
    const LayoutObject& object,
    const PropertyTreeStateOrAlias& property_tree_state,
    const PhysicalRect& old_rect,
    const PhysicalRect& new_rect,
    const FloatPoint& old_starting_point,
    const FloatPoint& new_starting_point) {
  // The caller should ensure these conditions.
  DCHECK(!old_rect.IsEmpty());
  DCHECK(!new_rect.IsEmpty());

  float threshold_physical_px =
      kMovementThreshold * object.StyleRef().EffectiveZoom();

  if (EqualWithinMovementThreshold(old_starting_point, new_starting_point,
                                   threshold_physical_px))
    return;

  if (SmallerThanRegionGranularity(old_rect) &&
      SmallerThanRegionGranularity(new_rect))
    return;

  const auto& root_state =
      object.View()->FirstFragment().LocalBorderBoxProperties();
  FloatClipRect clip_rect =
      GeometryMapper::LocalToAncestorClipRect(property_tree_state, root_state);

  // If the clip region is empty, then the resulting layout shift isn't visible
  // in the viewport so ignore it.
  if (clip_rect.Rect().IsEmpty())
    return;

  auto transform = GeometryMapper::SourceToDestinationProjection(
      property_tree_state.Transform(), root_state.Transform());
  FloatPoint old_starting_point_in_root =
      transform.MapPoint(old_starting_point);
  FloatPoint new_starting_point_in_root =
      transform.MapPoint(new_starting_point);

  if (EqualWithinMovementThreshold(old_starting_point_in_root,
                                   new_starting_point_in_root,
                                   threshold_physical_px))
    return;

  if (EqualWithinMovementThreshold(
          old_starting_point_in_root + frame_scroll_delta_,
          new_starting_point_in_root, threshold_physical_px)) {
    // TODO(skobes): Checking frame_scroll_delta_ is an imperfect solution to
    // allowing counterscrolled layout shifts. Ideally, we would map old_rect
    // to viewport coordinates using the previous frame's scroll tree.
    return;
  }

  FloatRect old_rect_in_root(old_rect);
  transform.MapRect(old_rect_in_root);
  FloatRect new_rect_in_root(new_rect);
  transform.MapRect(new_rect_in_root);

  IntRect visible_old_rect =
      RoundedIntRect(Intersection(old_rect_in_root, clip_rect.Rect()));
  IntRect visible_new_rect =
      RoundedIntRect(Intersection(new_rect_in_root, clip_rect.Rect()));
  if (visible_old_rect.IsEmpty() && visible_new_rect.IsEmpty())
    return;

  // Compute move distance based on unclipped rects, to accurately determine how
  // much the element moved.
  float move_distance =
      GetMoveDistance(old_starting_point_in_root, new_starting_point_in_root);
  frame_max_distance_ = std::max(frame_max_distance_, move_distance);

#if DCHECK_IS_ON()
  LocalFrame& frame = frame_view_->GetFrame();
  if (ShouldLog(frame)) {
    DVLOG(2) << "in " << (frame.IsMainFrame() ? "" : "subframe ")
             << frame.GetDocument()->Url() << ", " << object << " moved from "
             << old_rect_in_root << " to " << new_rect_in_root
             << " (visible from " << visible_old_rect << " to "
             << visible_new_rect << ")";
    if (old_starting_point_in_root != old_rect_in_root.Location() ||
        new_starting_point_in_root != new_rect_in_root.Location()) {
      DVLOG(2) << " (starting point from " << old_starting_point_in_root
               << " to " << new_starting_point_in_root << ")";
    }
  }
#endif

  region_.AddRect(visible_old_rect);
  region_.AddRect(visible_new_rect);

  if (Node* node = object.GetNode()) {
    MaybeRecordAttribution(
        {DOMNodeIds::IdForNode(node), visible_old_rect, visible_new_rect});
  }
}

LayoutShiftTracker::Attribution::Attribution() : node_id(kInvalidDOMNodeId) {}
LayoutShiftTracker::Attribution::Attribution(DOMNodeId node_id_arg,
                                             IntRect old_visual_rect_arg,
                                             IntRect new_visual_rect_arg)
    : node_id(node_id_arg),
      old_visual_rect(old_visual_rect_arg),
      new_visual_rect(new_visual_rect_arg) {}

LayoutShiftTracker::Attribution::operator bool() const {
  return node_id != kInvalidDOMNodeId;
}

bool LayoutShiftTracker::Attribution::Encloses(const Attribution& other) const {
  return old_visual_rect.Contains(other.old_visual_rect) &&
         new_visual_rect.Contains(other.new_visual_rect);
}

int LayoutShiftTracker::Attribution::Area() const {
  int old_area = old_visual_rect.Width() * old_visual_rect.Height();
  int new_area = new_visual_rect.Width() * new_visual_rect.Height();

  IntRect intersection = Intersection(old_visual_rect, new_visual_rect);
  int shared_area = intersection.Width() * intersection.Height();
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
    const PhysicalOffset& new_paint_offset) {
  DCHECK(NeedsToTrack(box));
  ObjectShifted(box, property_tree_state, old_rect, new_rect,
                StartingPoint(old_paint_offset, box, box.PreviousSize()),
                StartingPoint(new_paint_offset, box, box.Size()));
}

void LayoutShiftTracker::NotifyTextPrePaint(
    const LayoutText& text,
    const PropertyTreeStateOrAlias& property_tree_state,
    const LogicalOffset& old_starting_point,
    const LogicalOffset& new_starting_point,
    const PhysicalOffset& old_paint_offset,
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
                FloatPoint(old_physical_starting_point),
                FloatPoint(new_physical_starting_point));
}

double LayoutShiftTracker::SubframeWeightingFactor() const {
  LocalFrame& frame = frame_view_->GetFrame();
  if (frame.IsMainFrame())
    return 1;

  // Map the subframe view rect into the coordinate space of the local root.
  FloatClipRect subframe_cliprect =
      FloatClipRect(FloatRect(FloatPoint(), FloatSize(frame_view_->Size())));
  GeometryMapper::LocalToAncestorVisualRect(
      frame_view_->GetLayoutView()->FirstFragment().LocalBorderBoxProperties(),
      PropertyTreeState::Root(), subframe_cliprect);
  auto subframe_rect = PhysicalRect::EnclosingRect(subframe_cliprect.Rect());

  // Intersect with the portion of the local root that overlaps the main frame.
  frame.LocalFrameRoot().View()->MapToVisualRectInRemoteRootFrame(
      subframe_rect);
  IntSize subframe_visible_size = subframe_rect.PixelSnappedSize();
  IntSize main_frame_size = frame.GetPage()->GetVisualViewport().Size();

  // TODO(crbug.com/940711): This comparison ignores page scale and CSS
  // transforms above the local root.
  return static_cast<double>(subframe_visible_size.Area()) /
         main_frame_size.Area();
}

void LayoutShiftTracker::NotifyPrePaintFinished() {
  if (!is_active_)
    return;
  if (region_.IsEmpty())
    return;

  IntRect viewport = frame_view_->GetScrollableArea()->VisibleContentRect();
  if (viewport.IsEmpty())
    return;

  double viewport_area = double(viewport.Width()) * double(viewport.Height());
  double impact_fraction = region_.Area() / viewport_area;
  DCHECK_GT(impact_fraction, 0);

  DCHECK_GT(frame_max_distance_, 0.0);
  double viewport_max_dimension = std::max(viewport.Width(), viewport.Height());
  double move_distance_factor =
      (frame_max_distance_ < viewport_max_dimension)
          ? double(frame_max_distance_) / viewport_max_dimension
          : 1.0;
  double score_delta = impact_fraction * move_distance_factor;
  double weighted_score_delta = score_delta * SubframeWeightingFactor();

  overall_max_distance_ = std::max(overall_max_distance_, frame_max_distance_);

#if DCHECK_IS_ON()
  LocalFrame& frame = frame_view_->GetFrame();
  if (ShouldLog(frame)) {
    DVLOG(1) << "in " << (frame.IsMainFrame() ? "" : "subframe ")
             << frame.GetDocument()->Url() << ", viewport was "
             << (impact_fraction * 100) << "% impacted with distance fraction "
             << move_distance_factor;
  }
#endif

  if (pointerdown_pending_data_.saw_pointerdown) {
    pointerdown_pending_data_.score_delta += score_delta;
    pointerdown_pending_data_.weighted_score_delta += weighted_score_delta;
  } else {
    ReportShift(score_delta, weighted_score_delta);
  }

  if (!region_.IsEmpty())
    SetLayoutShiftRects(region_.GetRects());
  region_.Reset();

  frame_max_distance_ = 0.0;
  frame_scroll_delta_ = ScrollOffset();
  attributions_.fill(Attribution());
}

LayoutShift::AttributionList LayoutShiftTracker::CreateAttributionList() const {
  LayoutShift::AttributionList list;
  for (const Attribution& att : attributions_) {
    if (att.node_id == kInvalidDOMNodeId)
      break;
    list.push_back(LayoutShiftAttribution::Create(
        DOMNodeIds::NodeForId(att.node_id),
        DOMRectReadOnly::FromIntRect(att.old_visual_rect),
        DOMRectReadOnly::FromIntRect(att.new_visual_rect)));
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

  double input_timestamp =
      most_recent_input_timestamp_initialized_
          ? performance->MonotonicTimeToDOMHighResTimeStamp(
                most_recent_input_timestamp_)
          : 0.0;
  LayoutShift* entry =
      LayoutShift::Create(performance->now(), score_delta, had_recent_input,
                          input_timestamp, CreateAttributionList());

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

  TRACE_EVENT_INSTANT2("loading", "LayoutShift", TRACE_EVENT_SCOPE_THREAD,
                       "data", PerFrameTraceData(score_delta, had_recent_input),
                       "frame", ToTraceValue(&frame));

#if DCHECK_IS_ON()
  if (ShouldLog(frame)) {
    DVLOG(1) << "in " << (frame.IsMainFrame() ? "" : "subframe ")
             << frame.GetDocument()->Url().GetString() << ", layout shift of "
             << score_delta
             << (had_recent_input ? " excluded by recent input" : " reported")
             << "; cumulative score is " << score_;
  }
#endif
}

void LayoutShiftTracker::NotifyInput(const WebInputEvent& event) {
  const WebInputEvent::Type type = event.GetType();
  const bool saw_pointerdown = pointerdown_pending_data_.saw_pointerdown;
  const bool pointerdown_became_tap =
      saw_pointerdown && type == WebInputEvent::Type::kPointerUp;
  const bool event_type_stops_pointerdown_buffering =
      type == WebInputEvent::Type::kPointerUp ||
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
      pointerdown_became_tap;

  if (should_trigger_shift_exclusion) {
    observed_input_or_scroll_ = true;

    // This cancels any previously scheduled task from the same timer.
    timer_.StartOneShot(kTimerDelay, FROM_HERE);
    UpdateInputTimestamp(event.TimeStamp());
  }

  if (saw_pointerdown && event_type_stops_pointerdown_buffering) {
    double score_delta = pointerdown_pending_data_.score_delta;
    if (score_delta > 0)
      ReportShift(score_delta, pointerdown_pending_data_.weighted_score_delta);
    pointerdown_pending_data_ = PointerdownPendingData();
  }
  if (type == WebInputEvent::Type::kPointerDown && !is_hovering_pointerdown)
    pointerdown_pending_data_.saw_pointerdown = true;
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
  frame_scroll_delta_ += delta;

  // Only set observed_input_or_scroll_ for user-initiated scrolls, and not
  // other scrolls such as hash fragment navigations.
  if (scroll_type == mojom::blink::ScrollType::kUser ||
      scroll_type == mojom::blink::ScrollType::kCompositor)
    observed_input_or_scroll_ = true;
}

void LayoutShiftTracker::NotifyViewportSizeChanged() {
  // This cancels any previously scheduled task from the same timer.
  timer_.StartOneShot(kTimerDelay, FROM_HERE);
  UpdateInputTimestamp(base::TimeTicks::Now());
}

void LayoutShiftTracker::NotifyFindInPageInput() {
  // This cancels any previously scheduled task from the same timer.
  timer_.StartOneShot(kTimerDelay, FROM_HERE);
  UpdateInputTimestamp(base::TimeTicks::Now());
}

std::unique_ptr<TracedValue> LayoutShiftTracker::PerFrameTraceData(
    double score_delta,
    bool input_detected) const {
  auto value = std::make_unique<TracedValue>();
  value->SetDouble("score", score_delta);
  value->SetDouble("cumulative_score", score_);
  value->SetDouble("overall_max_distance", overall_max_distance_);
  value->SetDouble("frame_max_distance", frame_max_distance_);
  RegionToTracedValue(region_, *value);
  value->SetBoolean("is_main_frame", frame_view_->GetFrame().IsMainFrame());
  value->SetBoolean("had_recent_input", input_detected);
  AttributionsToTracedValue(*value);
  return value;
}

void LayoutShiftTracker::AttributionsToTracedValue(TracedValue& value) const {
  const Attribution* it = attributions_.begin();
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

void LayoutShiftTracker::SetLayoutShiftRects(const Vector<IntRect>& int_rects) {
  // Store the layout shift rects in the HUD layer.
  auto* cc_layer = frame_view_->RootCcLayer();
  if (cc_layer && cc_layer->layer_tree_host()) {
    if (!cc_layer->layer_tree_host()->GetDebugState().show_layout_shift_regions)
      return;
    if (cc_layer->layer_tree_host()->hud_layer()) {
      WebVector<gfx::Rect> rects;
      Region blink_region;
      for (IntRect rect : int_rects)
        blink_region.Unite(Region(rect));
      for (const IntRect& rect : blink_region.Rects())
        rects.emplace_back(rect);
      cc_layer->layer_tree_host()->hud_layer()->SetLayoutShiftRects(
          rects.ReleaseVector());
      cc_layer->layer_tree_host()->hud_layer()->SetNeedsPushProperties();
    }
  }
}

void LayoutShiftTracker::Trace(Visitor* visitor) const {
  visitor->Trace(frame_view_);
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
  if (!layout_object || !layout_object->IsBox())
    return;

  auto& map = top_->geometries_before_detach_;
  auto& fragment = layout_object->GetMutableForPainting().FirstFragment();

  // Save the visual rect for restoration on future reattachment.
  const auto& box = To<LayoutBox>(*layout_object);
  PhysicalRect visual_overflow_rect = box.PreviousPhysicalVisualOverflowRect();
  if (visual_overflow_rect.IsEmpty() && box.PreviousSize().IsEmpty())
    return;
  map.Set(&node, Geometry{fragment.PaintOffset(), box.PreviousSize(),
                          visual_overflow_rect});
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
}

}  // namespace blink
