// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_video_visibility_tracker.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_video.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

namespace {

// Do a rect-based penetrating list hit test, with a callback that is executed
// at each node.
HitTestResult HitTestForOcclusionRatio(
    const HTMLVideoElement& video_element,
    const PhysicalRect& hit_rect,
    std::optional<HitTestRequest::HitNodeCb> hit_node_cb) {
  LocalFrame* frame = video_element.GetDocument().GetFrame();
  DCHECK(!frame->View()->NeedsLayout());
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kIgnorePointerEventsNone | HitTestRequest::kReadOnly |
      HitTestRequest::kIgnoreClipping |
      HitTestRequest::kIgnoreZeroOpacityObjects |
      HitTestRequest::kHitTestVisualOverflow | HitTestRequest::kListBased |
      HitTestRequest::kPenetratingList | HitTestRequest::kAvoidCache;
  HitTestLocation location(hit_rect);
  return frame->GetEventHandler().HitTestResultAtLocation(
      location, hit_type, video_element.GetLayoutObject(), true, hit_node_cb);
}

float ComputeArea(const PhysicalRect& rect) {
  return static_cast<float>(ToRoundedSize(rect.size).Area64());
}

bool HasEnoughVisibleAreaRemaining(float occluded_area,
                                   const PhysicalRect& video_element_rect,
                                   const int visibility_threshold) {
  return ComputeArea(video_element_rect) - occluded_area >=
         visibility_threshold;
}

float ComputeOccludingArea(const Vector<SkIRect>& occluding_rects,
                           float video_element_area) {
  // Record the total time spent computing the occluding area.
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Media.MediaVideoVisibilityTracker.ComputeOcclusion.ComputeOccludingArea."
      "TotalDuration");

  float occluding_area = 0.0;

  std::vector<SkIRect> sk_rects;
  for (const auto rect : occluding_rects) {
    sk_rects.push_back(rect);
  }

  SkRegion region;
  bool compute_area = region.setRects(sk_rects.data(), occluding_rects.size());

  if (!compute_area) {
    return occluding_area;
  }

  for (SkRegion::Iterator it(region); !it.done(); it.next()) {
    auto occluding_rect = it.rect();
    occluding_area +=
        ComputeArea(PhysicalRect(gfx::SkIRectToRect(occluding_rect)));

    if (occluding_area >= video_element_area) {
      return video_element_area;
    }
  }

  return occluding_area;
}

// Records various UMA metrics related to hit testing and occlusion. All metrics
// recorded by this method represent total counts/percentages after identifying
// whether the `VideoElement` visibility threshold is met (or not).
void RecordTotalCounts(const MediaVideoVisibilityTracker::Metrics& counts) {
  // Limit used to indicate whether a linear histogram will be recorded or not.
  // If any of the method parameters is <= kRecordLinearHistogramLimit, a linear
  // histogram will be recorded for that parameter.
  //
  // The limit is used to be able to get fine grained detail at the lower end of
  // the range. Once we know the overall distribution, future linear histograms
  // can be added as needed.
  const int kRecordLinearHistogramLimit = 101;

  //////////////////////////////////////////////////////////////////////////////
  // Record counts.

  // Record the total number of hit tested nodes that contribute to occlusion.
  UMA_HISTOGRAM_COUNTS_1000(
      "Media.MediaVideoVisibilityTracker."
      "HitTestedNodesContributingToOcclusionCount.ExponentialHistogram."
      "TotalCount",
      counts.total_hit_tested_nodes_contributing_to_occlusion);

  if (counts.total_hit_tested_nodes_contributing_to_occlusion <=
      kRecordLinearHistogramLimit) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Media.MediaVideoVisibilityTracker."
        "HitTestedNodesContributingToOcclusionCount.LinearHistogram.TotalCount",
        counts.total_hit_tested_nodes_contributing_to_occlusion,
        kRecordLinearHistogramLimit);
  }

  // Record the total number of hit tested nodes.
  UMA_HISTOGRAM_COUNTS_1000(
      "Media.MediaVideoVisibilityTracker.HitTestedNodesCount."
      "ExponentialHistogram.TotalCount",
      counts.total_hit_tested_nodes);

  if (counts.total_hit_tested_nodes <= kRecordLinearHistogramLimit) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Media.MediaVideoVisibilityTracker.HitTestedNodesCount.LinearHistogram."
        "TotalCount",
        counts.total_hit_tested_nodes, kRecordLinearHistogramLimit);
  }

  // Record the total number of hit tested nodes that are ignored due to not
  // being opaque.
  UMA_HISTOGRAM_COUNTS_1000(
      "Media.MediaVideoVisibilityTracker.IgnoredNodesNotOpaqueCount."
      "ExponentialHistogram.TotalCount",
      counts.total_ignored_nodes_not_opaque);

  if (counts.total_ignored_nodes_not_opaque <= kRecordLinearHistogramLimit) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Media.MediaVideoVisibilityTracker.IgnoredNodesNotOpaqueCount."
        "LinearHistogram.TotalCount",
        counts.total_ignored_nodes_not_opaque, kRecordLinearHistogramLimit);
  }

  // Record the total number of hit tested nodes that are ignored due to being
  // in the shadow root and of user agent type.
  UMA_HISTOGRAM_COUNTS_1000(
      "Media.MediaVideoVisibilityTracker.IgnoredNodesUserAgentShadowRootCount."
      "ExponentialHistogram."
      "TotalCount",
      counts.total_ignored_nodes_user_agent_shadow_root);

  if (counts.total_ignored_nodes_user_agent_shadow_root <=
      kRecordLinearHistogramLimit) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Media.MediaVideoVisibilityTracker."
        "IgnoredNodesUserAgentShadowRootCount.LinearHistogram.TotalCount",
        counts.total_ignored_nodes_user_agent_shadow_root,
        kRecordLinearHistogramLimit);
  }

  // Record the total number of occluding rects.
  UMA_HISTOGRAM_COUNTS_1000(
      "Media.MediaVideoVisibilityTracker.OccludingRectsCount."
      "ExponentialHistogram.TotalCount",
      counts.total_occluding_rects);

  if (counts.total_occluding_rects <= kRecordLinearHistogramLimit) {
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Media.MediaVideoVisibilityTracker.OccludingRectsCount.LinearHistogram."
        "TotalCount",
        counts.total_occluding_rects, kRecordLinearHistogramLimit);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Record percentages.

  int ignored_nodes_not_opaque_percentage = 0;
  int ignored_nodes_user_agent_shadow_root_percentage = 0;
  int total_hit_tested_nodes_contributing_to_occlusion_percentage = 0;

  if (counts.total_hit_tested_nodes != 0) {
    ignored_nodes_not_opaque_percentage =
        100 * counts.total_ignored_nodes_not_opaque /
        counts.total_hit_tested_nodes;
    ignored_nodes_user_agent_shadow_root_percentage =
        100 * counts.total_ignored_nodes_user_agent_shadow_root /
        counts.total_hit_tested_nodes;
    total_hit_tested_nodes_contributing_to_occlusion_percentage =
        100 * counts.total_hit_tested_nodes_contributing_to_occlusion /
        counts.total_hit_tested_nodes;
  }

  // Record the percentage of the total hit tested nodes that are ignored due to
  // not being opaque.
  UMA_HISTOGRAM_PERCENTAGE(
      "Media.MediaVideoVisibilityTracker.IgnoredNodesNotOpaque.Percentage",
      ignored_nodes_not_opaque_percentage);

  // Record the percentage of the total hit tested nodes that are ignored due to
  // being in the shadow root and of user agent type.
  UMA_HISTOGRAM_PERCENTAGE(
      "Media.MediaVideoVisibilityTracker.IgnoredNodesUserAgentShadowRoot."
      "Percentage",
      ignored_nodes_user_agent_shadow_root_percentage);

  // Record the percentage of the total hit tested nodes that contribute to
  // occlusion. total_hit_tested_nodes_contributing_to_occlusion_percentage
  UMA_HISTOGRAM_PERCENTAGE(
      "Media.MediaVideoVisibilityTracker."
      "NodesContributingToOcclusion.Percentage",
      total_hit_tested_nodes_contributing_to_occlusion_percentage);
}

const Vector<AtomicString>& FullscreenEventTypes() {
  DEFINE_STATIC_LOCAL(const Vector<AtomicString>, fullscreen_change_event_types,
                      ({event_type_names::kWebkitfullscreenchange,
                        event_type_names::kFullscreenchange}));
  return fullscreen_change_event_types;
}

// Returns true if `target` has `listener` event listener registered.
bool HasEventListenerRegistered(EventTarget& target,
                                const AtomicString& event_type,
                                const EventListener* listener) {
  EventListenerVector* listeners = target.GetEventListeners(event_type);
  if (!listeners) {
    return false;
  }

  for (const auto& registered_listener : *listeners) {
    if (registered_listener->Callback() == listener) {
      return true;
    }
  }

  return false;
}

// Returns true if `type` is of content type, false otherwise.
//
// In the context of the `MediaVideoVisibilityTracker`, we consider a
// `DisplayItem::Type` to be of content type if it is used to draw content that
// is relevant to occlusion computations.
bool IsContentType(DisplayItem::Type type) {
  return !(type == DisplayItem::kFrameOverlay ||
           type == DisplayItem::kForeignLayerLinkHighlight ||
           type == DisplayItem::kForeignLayerViewportScroll ||
           type == DisplayItem::kForeignLayerViewportScrollbar);
}

void RecordVideoOcclusionState(
    const HTMLVideoElement& video_element,
    const MediaVideoVisibilityTracker::OcclusionState& occlusion_state,
    bool has_sufficiently_visible_video,
    const int visibility_threshold) {
  std::ostringstream occluding_rects_stream;
  const auto& occluding_rects = occlusion_state.occluding_rects;

  for (wtf_size_t i = 0; i < occluding_rects.size(); ++i) {
    const auto& rect = gfx::SkIRectToRect(occluding_rects[i]);
    occluding_rects_stream << String::Format(
        "[x: %d, y: %d, width: %d, height: %d]", rect.x(), rect.y(),
        rect.width(), rect.height());
    if (i >= 0 && i < occluding_rects.size() - 1) {
      occluding_rects_stream << ", ";
    }
  }

  if (occluding_rects_stream.tellp() == 0) {
    occluding_rects_stream << "None";
  }

  const auto& intersection_rect = occlusion_state.intersection_rect;
  const String intersection_rect_string =
      intersection_rect.IsEmpty()
          ? "None"
          : String::Format("x: %d, y: %d, width: %d, height: %d",
                           intersection_rect.X().ToInt(),
                           intersection_rect.Y().ToInt(),
                           intersection_rect.Width().ToInt(),
                           intersection_rect.Height().ToInt());

  const auto& video_element_rect = occlusion_state.video_element_rect;
  const String video_element_rect_string =
      video_element_rect.IsEmpty()
          ? "None"
          : String::Format("x: %d, y: %d, width: %d, height: %d",
                           video_element_rect.X().ToInt(),
                           video_element_rect.Y().ToInt(),
                           video_element_rect.Width().ToInt(),
                           video_element_rect.Height().ToInt());

  const String occlusion_state_string = String::Format(
      "has sufficiently visible video: {%s}, occluded area: {%.2f}, occluding "
      "rects: {%s}, intersection rect: {%s}, video element rect: {%s}, "
      "visibility threshold: {%d}",
      has_sufficiently_visible_video ? "True" : "False",
      occlusion_state.occluded_area, occluding_rects_stream.str().c_str(),
      intersection_rect_string.Ascii().c_str(),
      video_element_rect_string.Ascii().c_str(), visibility_threshold);

  video_element.RecordVideoOcclusionState(
      occlusion_state_string.Ascii().c_str());
}

}  // anonymous namespace

MediaVideoVisibilityTracker::MediaVideoVisibilityTracker(
    HTMLVideoElement& video,
    const int visibility_threshold,
    ReportVisibilityCb report_visibility_cb,
    base::TimeDelta hit_test_interval)
    : video_element_(video),
      visibility_threshold_(visibility_threshold),
      report_visibility_cb_(std::move(report_visibility_cb)),
      hit_test_interval_(hit_test_interval) {
  DCHECK(report_visibility_cb_);
  DCHECK_GT(visibility_threshold_, 0)
      << "Invalid threshold: " << visibility_threshold_;
  DCHECK_GE(hit_test_interval_, kMinimumAllowedHitTestInterval);
}

MediaVideoVisibilityTracker::~MediaVideoVisibilityTracker() {
  DCHECK(!tracker_attached_to_document_);
}

void MediaVideoVisibilityTracker::Attach() {
  const auto& video_element = VideoElement();
  auto* document = &video_element.GetDocument();
  auto* document_view = document->View();

  if (tracker_attached_to_document_) {
    DCHECK_EQ(tracker_attached_to_document_, document);
    return;
  }

  if (!video_element.isConnected() || !document_view) {
    return;
  }

  document_view->RegisterForLifecycleNotifications(this);
  MaybeAddFullscreenEventListeners();

  tracker_attached_to_document_ = document;
}

void MediaVideoVisibilityTracker::Detach() {
  if (!tracker_attached_to_document_) {
    return;
  }

  if (auto* view = tracker_attached_to_document_->View()) {
    view->UnregisterFromLifecycleNotifications(this);
  }

  MaybeRemoveFullscreenEventListeners();

  meets_visibility_threshold_ = false;
  tracker_attached_to_document_ = nullptr;
}

void MediaVideoVisibilityTracker::UpdateVisibilityTrackerState() {
  const auto& video_element = VideoElement();

  // `fullscreen_element` is used to determine if any element within the
  // document is in fullscreen. This could be the video element itself, or any
  // other element.
  Element* fullscreen_element =
      Fullscreen::FullscreenElementFrom(video_element.GetDocument());

  if (video_element.GetWebMediaPlayer() &&
      video_element.GetExecutionContext() && !video_element.paused() &&
      !fullscreen_element) {
    Attach();
  } else {
    Detach();
  }
}

void MediaVideoVisibilityTracker::ElementDidMoveToNewDocument() {
  Detach();
}

void MediaVideoVisibilityTracker::Invoke(ExecutionContext* context,
                                         Event* event) {
  DCHECK(base::Contains(FullscreenEventTypes(), event->type()));

  // Video is not loaded yet.
  if (VideoElement().getReadyState() < HTMLMediaElement::kHaveMetadata) {
    return;
  }

  UpdateVisibilityTrackerState();
}

void MediaVideoVisibilityTracker::MaybeAddFullscreenEventListeners() {
  auto& document = VideoElement().GetDocument();
  for (const auto& event_type : FullscreenEventTypes()) {
    // Ignore event listeners that have already been registered.
    if (HasEventListenerRegistered(document, event_type, this)) {
      continue;
    }
    document.addEventListener(event_type, this, true);
  }
}

void MediaVideoVisibilityTracker::MaybeRemoveFullscreenEventListeners() {
  DCHECK(tracker_attached_to_document_);
  auto& video_element = VideoElement();
  auto& document = VideoElement().GetDocument();

  if (video_element.isConnected() &&
      document == tracker_attached_to_document_) {
    return;
  }

  if (!video_element.isConnected()) {
    // Ignore event listeners that have already been removed.
    for (const auto& event_type : FullscreenEventTypes()) {
      if (!HasEventListenerRegistered(document, event_type, this)) {
        continue;
      }
      document.removeEventListener(event_type, this, true);
    }
  }

  if (document != tracker_attached_to_document_) {
    // Ignore event listeners that have already been removed.
    for (const auto& event_type : FullscreenEventTypes()) {
      if (!HasEventListenerRegistered(*tracker_attached_to_document_.Get(),
                                      event_type, this)) {
        continue;
      }
      tracker_attached_to_document_->removeEventListener(event_type, this,
                                                         true);
    }
  }
}

void MediaVideoVisibilityTracker::RequestVisibility(
    RequestVisibilityCallback request_visibility_callback) {
  // Latest requests take precedence over old ones. Therefore, if we had a
  // pending request, we simply run the current callback with `false` and store
  // the new one.
  if (request_visibility_callback_) {
    std::move(request_visibility_callback_).Run(false);
  }

  request_visibility_callback_ = std::move(request_visibility_callback);
  MaybeComputeVisibility(ShouldReportVisibility::kNo);
}

const MediaVideoVisibilityTracker::ClientIdsSet
MediaVideoVisibilityTracker::GetClientIdsSet(
    DisplayItemClientId start_after_display_item_client_id) const {
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Media.MediaVideoVisibilityTracker.GetClientIdsSet.SetConstruction."
      "TotalDuration");

  auto* document_view = VideoElement().GetDocument().View();

  if (!document_view) {
    return {};
  }

  LocalFrameView::InvalidationDisallowedScope invalidation_disallowed(
      *document_view);

  const auto& paint_artifact = document_view->GetPaintArtifact();
  const DisplayItemList& display_item_list =
      paint_artifact.GetDisplayItemList();
  if (display_item_list.IsEmpty()) {
    return {};
  }

  wtf_size_t begin_index = 0;
  wtf_size_t end_index = display_item_list.size();
  while (begin_index < end_index && display_item_list[begin_index].ClientId() !=
                                        start_after_display_item_client_id) {
    begin_index++;
  }

  // Skip DisplayItem with `start_after_display_item_client_id`
  // DisplayItemClientId.
  begin_index++;
  if (begin_index == kNotFound || begin_index >= end_index) {
    return {};
  }

  // TODO(crbug.com/40275580): Remove `IsContentType` method, if the set size is
  // not significantly reduced.
  //
  // Ignore display items that are not of content type.
  // This is strictly an optimization, in an attempt to reduce the resulting set
  // size.
  //
  // We start at the end of the list, since the `DisplayItemList` entries are
  // stored in paint order. `DisplayItem` s that are not of content type can
  // still appear in other locations within the list, however for most cases,
  // these `DisplayItem` types are painted last.
  int not_content_type_count = 0;
  while (end_index > begin_index &&
         !IsContentType(display_item_list[end_index - 1].GetType())) {
    not_content_type_count++;
    end_index--;
  }
  UMA_HISTOGRAM_COUNTS_10000(
      "Media.MediaVideoVisibilityTracker.GetClientIdsSet.NotContentTypeCount."
      "TotalCount",
      not_content_type_count);

  if (begin_index == end_index) {
    return {};
  }

  MediaVideoVisibilityTracker::ClientIdsSet set;
  for (const auto& display_item :
       display_item_list.ItemsInRange(begin_index, end_index)) {
    if (display_item.ClientId() != kInvalidDisplayItemClientId) {
      set.insert(display_item.ClientId());
    }
  }

  int set_size = base::saturated_cast<int>(set.size());
  UMA_HISTOGRAM_COUNTS_10000(
      "Media.MediaVideoVisibilityTracker.GetClientIdsSet.ItemsInSetCount."
      "TotalCount",
      set_size);

  int not_content_type_percentage = 0;
  if (set_size > 0) {
    not_content_type_percentage = 100 * not_content_type_count / set_size;
  }
  UMA_HISTOGRAM_PERCENTAGE(
      "Media.MediaVideoVisibilityTracker.GetClientIdsSet.NotContentType."
      "Percentage",
      not_content_type_percentage);

  return set;
}

ListBasedHitTestBehavior MediaVideoVisibilityTracker::ComputeOcclusion(
    const ClientIdsSet& client_ids_set,
    Metrics& counts,
    const Node& node) {
  counts.total_hit_tested_nodes++;

  if (node == VideoElement()) {
    return kStopHitTesting;
  }

  // Ignore nodes with a containing shadow root of type
  // ShadowRootType::kUserAgent (e.g Video Controls).
  if (node.IsInShadowTree() && node.ContainingShadowRoot() &&
      node.ContainingShadowRoot()->IsUserAgent()) {
    counts.total_ignored_nodes_user_agent_shadow_root++;
    return kContinueHitTesting;
  }

  // Ignore nodes that are not opaque. We are only interested on evaluating
  // nodes that visually occlude the video, as seen by the user.
  if (!node.GetLayoutObject()->HasNonZeroEffectiveOpacity()) {
    counts.total_ignored_nodes_not_opaque++;
    return kContinueHitTesting;
  }

  // Ignore nodes that do not produce any visual content.
  if (!client_ids_set.empty() &&
      !client_ids_set.Contains(node.GetLayoutObject()->Id())) {
    return kContinueHitTesting;
  }

  // Only account for the intersection of |node_rect| BoundingBox with
  // |occlusion_state_.intersection_rect|. Note that BoundingBox represents an
  // approximation of the total area that is painted. The actual painted area
  // can be larger (e.g., if the object paints drop shadows), or smaller (e.g.,
  // if the object is clipped).
  PhysicalRect node_rect = node.BoundingBox();
  node_rect.Intersect(occlusion_state_.intersection_rect);

  // Add the current occluding node rect to `occlusion_state_.occluding_rects`
  // and compute the total occluded area.
  occlusion_state_.occluding_rects.push_back(
      gfx::RectToSkIRect(ToPixelSnappedRect(node_rect)));
  occlusion_state_.occluded_area =
      ComputeOccludingArea(occlusion_state_.occluding_rects,
                           ComputeArea(occlusion_state_.video_element_rect));

  counts.total_hit_tested_nodes_contributing_to_occlusion++;

  if (HasEnoughVisibleAreaRemaining(occlusion_state_.occluded_area,
                                    occlusion_state_.video_element_rect,
                                    visibility_threshold_)) {
    return kContinueHitTesting;
  }

  return kStopHitTesting;
}

bool MediaVideoVisibilityTracker::MeetsVisibilityThreshold(
    Metrics& counts,
    const PhysicalRect& rect) {
  const ClientIdsSet client_ids_set =
      GetClientIdsSet(VideoElement().GetLayoutObject()->Id());

  {
    // Record the total time spent computing occlusion.
    SCOPED_UMA_HISTOGRAM_TIMER(
        "Media.MediaVideoVisibilityTracker.ComputeOcclusion.TotalDuration");

    HitTestResult result(HitTestForOcclusionRatio(
        VideoElement(), rect,
        WTF::BindRepeating(&MediaVideoVisibilityTracker::ComputeOcclusion,
                           WrapPersistent(this), client_ids_set,
                           std::ref(counts))));
  }

  return HasEnoughVisibleAreaRemaining(occlusion_state_.occluded_area,
                                       occlusion_state_.video_element_rect,
                                       visibility_threshold_)
             ? true
             : false;
}

bool MediaVideoVisibilityTracker::ComputeVisibility() {
  DCHECK(VideoElement().GetLayoutObject());
  occlusion_state_.occluded_area =
      ComputeOccludingArea(occlusion_state_.occluding_rects,
                           ComputeArea(occlusion_state_.video_element_rect));
  auto intersection_area = ComputeArea(occlusion_state_.intersection_rect);

  auto* layout = VideoElement().GetLayoutObject();
  // Return early if the area of the video that intersects with the view is
  // below |visibility_threshold_|.
  if (!layout || intersection_area < visibility_threshold_) {
    return false;
  }

  Metrics counts;
  bool meets_visibility_threshold =
      MeetsVisibilityThreshold(counts, occlusion_state_.intersection_rect);

  counts.total_occluding_rects =
      base::saturated_cast<int>(occlusion_state_.occluding_rects.size());
  RecordTotalCounts(counts);

  if (meets_visibility_threshold) {
    return true;
  }

  return false;
}

void MediaVideoVisibilityTracker::ComputeAreaOccludedByViewport(
    const LocalFrameView& local_frame_view) {
  DCHECK(VideoElement().GetLayoutObject());

  LayoutBox* box = To<LayoutBox>(VideoElement().GetLayoutObject());
  gfx::Rect bounds(box->AbsoluteBoundingBoxRect());

  gfx::Rect content_bounds;
  if (auto* layout_video =
          DynamicTo<LayoutVideo>(VideoElement().GetLayoutObject())) {
    PhysicalRect content_rect = layout_video->ReplacedContentRect();
    content_bounds = VideoElement().GetDocument().View()->FrameToViewport(
        ToEnclosingRect(layout_video->LocalToAbsoluteRect(content_rect)));
    content_bounds.Intersect(bounds);
  }

  // Fallback to using the video element bounds, if the computed
  // `content_bounds` is empty.
  if (content_bounds.IsEmpty()) {
    content_bounds = bounds;
  }

  gfx::Rect viewport_in_root_frame = ToEnclosingRect(
      local_frame_view.GetFrame().GetPage()->GetVisualViewport().VisibleRect());
  gfx::Rect absolute_viewport(
      local_frame_view.ConvertFromRootFrame(viewport_in_root_frame));
  occlusion_state_.intersection_rect =
      PhysicalRect(IntersectRects(absolute_viewport, content_bounds));

  occlusion_state_.video_element_rect = PhysicalRect(content_bounds);

  // Compute the VideoElement area that is occluded by the viewport, if any.
  SkRegion region;
  region.setRect(gfx::RectToSkIRect(content_bounds));
  if (region.op(gfx::RectToSkIRect(absolute_viewport),
                SkRegion::kDifference_Op)) {
    for (SkRegion::Iterator it(region); !it.done(); it.next()) {
      auto occluding_rect = it.rect();
      occlusion_state_.occluding_rects.push_back(occluding_rect);
      it.next();
    }
  }
}

void MediaVideoVisibilityTracker::MaybeComputeVisibility(
    ShouldReportVisibility should_report_visibility) {
  if (!tracker_attached_to_document_ ||
      !tracker_attached_to_document_->GetFrame()->View() ||
      !tracker_attached_to_document_->GetFrame()->IsOutermostMainFrame() ||
      !VideoElement().GetLayoutObject()) {
    if (request_visibility_callback_) {
      RecordVideoOcclusionState(VideoElement(), occlusion_state_, false,
                                visibility_threshold_);
      std::move(request_visibility_callback_).Run(false);
    }
    return;
  }

  if (VideoElement().GetDocument().Lifecycle().GetState() !=
      DocumentLifecycle::kPaintClean) {
    // If we have a pending visibility request, run it now with the cached
    // `meets_visibility_threshold_` value.
    if (request_visibility_callback_) {
      RecordVideoOcclusionState(VideoElement(), occlusion_state_,
                                meets_visibility_threshold_,
                                visibility_threshold_);
      std::move(request_visibility_callback_).Run(meets_visibility_threshold_);
    }
    return;
  }

  SCOPED_UMA_HISTOGRAM_TIMER(
      "Media.MediaVideoVisibilityTracker.UpdateTime.TotalDuration");

  occlusion_state_ = {};
  ComputeAreaOccludedByViewport(
      *tracker_attached_to_document_->GetFrame()->View());

  meets_visibility_threshold_ = ComputeVisibility();
  if (should_report_visibility == ShouldReportVisibility::kYes) {
    report_visibility_cb_.Run(meets_visibility_threshold_);
  }
  if (request_visibility_callback_) {
    RecordVideoOcclusionState(VideoElement(), occlusion_state_,
                              meets_visibility_threshold_,
                              visibility_threshold_);
    std::move(request_visibility_callback_).Run(meets_visibility_threshold_);
  }
}

void MediaVideoVisibilityTracker::DidFinishLifecycleUpdate(
    const LocalFrameView& local_frame_view) {
  if ((base::TimeTicks::Now() - last_hit_test_timestamp_ <
       hit_test_interval_) &&
      !request_visibility_callback_) {
    return;
  }

  if (!VideoElement().GetLayoutObject()) {
    return;
  }

  last_hit_test_timestamp_ = base::TimeTicks::Now();

  MaybeComputeVisibility(ShouldReportVisibility::kYes);
}

void MediaVideoVisibilityTracker::Trace(Visitor* visitor) const {
  NativeEventListener::Trace(visitor);
  visitor->Trace(video_element_);
  visitor->Trace(tracker_attached_to_document_);
}

}  // namespace blink
