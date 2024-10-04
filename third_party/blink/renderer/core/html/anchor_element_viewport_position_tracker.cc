// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_viewport_position_tracker.h"

#include "base/metrics/field_trial_params.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"

namespace blink {

namespace {

constexpr float kIntersectionRatioThreshold = 0.5f;

wtf_size_t GetMaxNumberOfObservations() {
  const base::FeatureParam<int> max_number_of_observations{
      &features::kNavigationPredictor, "max_intersection_observations", -1};
  int value = max_number_of_observations.Get();
  return value >= 0 ? value : std::numeric_limits<wtf_size_t>::max();
}

base::TimeDelta GetIntersectionObserverDelay() {
  const base::FeatureParam<base::TimeDelta> param{
      &features::kNavigationPredictor, "intersection_observer_delay",
      base::Milliseconds(100)};
  return param.Get();
}

bool ShouldReportViewportPositions() {
  return base::FeatureList::IsEnabled(
      features::kNavigationPredictorNewViewportFeatures);
}

float GetBrowserControlsHeight(Document& document) {
  BrowserControls& controls = document.GetPage()->GetBrowserControls();
  if (controls.ShrinkViewport()) {
    return controls.ContentOffset();
  }
  return 0.f;
}

}  // namespace

void AnchorElementViewportPositionTracker::Observer::AnchorPositionUpdate::
    Trace(Visitor* visitor) const {
  visitor->Trace(anchor_element);
}

// static
const char AnchorElementViewportPositionTracker::kSupplementName[] =
    "DocumentAnchorElementViewportPositionTracker";

AnchorElementViewportPositionTracker::AnchorElementViewportPositionTracker(
    Document& document)
    : Supplement<Document>(document),
      max_number_of_observations_(GetMaxNumberOfObservations()),
      intersection_observer_delay_(GetIntersectionObserverDelay()),
      position_update_timer_(
          document.GetExecutionContext()->GetTaskRunner(
              TaskType::kInternalDefault),
          this,
          &AnchorElementViewportPositionTracker::PositionUpdateTimerFired) {
  intersection_observer_ = IntersectionObserver::Create(
      document,
      WTF::BindRepeating(
          &AnchorElementViewportPositionTracker::UpdateVisibleAnchors,
          WrapWeakPersistent(this)),
      LocalFrameUkmAggregator::kAnchorElementMetricsIntersectionObserver,
      {.thresholds = {kIntersectionRatioThreshold},
       .delay = intersection_observer_delay_});
}

AnchorElementViewportPositionTracker::~AnchorElementViewportPositionTracker() =
    default;

void AnchorElementViewportPositionTracker::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
  visitor->Trace(intersection_observer_);
  visitor->Trace(anchors_in_viewport_);
  visitor->Trace(position_update_timer_);
  visitor->Trace(observers_);
}

// static
AnchorElementViewportPositionTracker*
AnchorElementViewportPositionTracker::MaybeGetOrCreateFor(Document& document) {
  if (!document.GetFrame() || !document.GetFrame()->IsOutermostMainFrame()) {
    return nullptr;
  }

  AnchorElementViewportPositionTracker* tracker =
      Supplement<Document>::From<AnchorElementViewportPositionTracker>(
          document);
  if (tracker) {
    return tracker;
  }

  tracker =
      MakeGarbageCollected<AnchorElementViewportPositionTracker>(document);
  ProvideTo(document, tracker);
  return tracker;
}

void AnchorElementViewportPositionTracker::AddObserver(
    AnchorElementViewportPositionTracker::Observer* observer) {
  observers_.insert(observer);
}

HTMLAnchorElementBase* AnchorElementViewportPositionTracker::MaybeObserveAnchor(
    HTMLAnchorElementBase& anchor_element,
    const mojom::blink::AnchorElementMetrics& metrics) {
  if (!max_number_of_observations_) {
    return nullptr;
  }

  int percent_area =
      std::clamp(static_cast<int>(metrics.ratio_area * 100.0f), 0, 100);
  bool should_observe = false;
  HTMLAnchorElementBase* anchor_unobserved = nullptr;
  if (observed_anchors_.size() < max_number_of_observations_) {
    should_observe = true;
  } else if (auto smallest_observed_anchor_it = observed_anchors_.begin();
             smallest_observed_anchor_it->percent_area < percent_area) {
    should_observe = true;
    Node* node =
        DOMNodeIds::NodeForId(smallest_observed_anchor_it->dom_node_id);
    CHECK(node);
    anchor_unobserved = To<HTMLAnchorElementBase>(node);
    intersection_observer_->unobserve(anchor_unobserved);
    not_observed_anchors_.insert(
        observed_anchors_.extract(smallest_observed_anchor_it));
  }

  if (should_observe) {
    // Observe the element to collect time_in_viewport stats.
    intersection_observer_->observe(&anchor_element);
    observed_anchors_.insert(
        {.percent_area = percent_area,
         .dom_node_id = DOMNodeIds::IdForNode(&anchor_element)});
  } else {
    not_observed_anchors_.insert(
        {.percent_area = percent_area,
         .dom_node_id = DOMNodeIds::IdForNode(&anchor_element)});
  }

  return anchor_unobserved;
}

void AnchorElementViewportPositionTracker::RemoveAnchor(
    HTMLAnchorElementBase& anchor) {
  if (DOMNodeId node_id = DOMNodeIds::ExistingIdForNode(&anchor);
      node_id && max_number_of_observations_) {
    // Note: We use base::ranges::find instead of std::set::find here
    // (and below) as we don't have a way to map HTMLAnchorElementBase ->
    // AnchorObservation. We could add one if doing an O(N) find here is too
    // expensive.
    if (auto observed_anchors_it = base::ranges::find(
            observed_anchors_, node_id, &AnchorObservation::dom_node_id);
        observed_anchors_it != observed_anchors_.end()) {
      intersection_observer_->unobserve(&anchor);
      observed_anchors_.erase(observed_anchors_it);
      if (!not_observed_anchors_.empty()) {
        auto largest_non_observed_anchor_it =
            std::prev(not_observed_anchors_.end());
        intersection_observer_->observe(To<Element>(DOMNodeIds::NodeForId(
            largest_non_observed_anchor_it->dom_node_id)));
        observed_anchors_.insert(
            not_observed_anchors_.extract(largest_non_observed_anchor_it));
      }
    } else if (auto not_observed_anchors_it =
                   base::ranges::find(not_observed_anchors_, node_id,
                                      &AnchorObservation::dom_node_id);
               not_observed_anchors_it != not_observed_anchors_.end()) {
      not_observed_anchors_.erase(not_observed_anchors_it);
    }
  }
}

void AnchorElementViewportPositionTracker::RecordPointerDown(
    const PointerEvent& pointer_event) {
  CHECK_EQ(pointer_event.type(), event_type_names::kPointerdown);
  Document* document = pointer_event.GetDocument();
  // TODO(crbug.com/347719430): LocalFrameView::FrameToViewport called below
  // doesn't work for subframes whose local root is not the main frame.
  if (!document || !document->GetFrame()->LocalFrameRoot().IsMainFrame()) {
    return;
  }

  gfx::PointF pointer_down_location = pointer_event.AbsoluteLocation();
  pointer_down_location =
      document->GetFrame()->View()->FrameToViewport(pointer_down_location);
  pointer_down_location.Offset(0,
                               GetBrowserControlsHeight(*GetSupplementable()));
  last_pointer_down_ = pointer_down_location.y();
}

void AnchorElementViewportPositionTracker::OnScrollEnd() {
  if (!ShouldReportViewportPositions()) {
    return;
  }

  // At this point, we're unsure of whether we have the latest
  // IntersectionObserver data or not (`intersection_observer_` is configured
  // with a delay), and the post-scroll intersection computations may or may not
  // have happened yet. We set a timer for `intersection_observer_delay_` and
  // wait for either:
  // 1) `UpdateVisibleAnchors` to be called before the timer (we stop the timer)
  // 2) The timer finishes (no intersection changes and `UpdateVisibleAnchors`
  //    wasn't called)
  // After either of the two conditions are met, we wait for a lifecycle update
  // before computing anchor element position metrics.

  // `position_update_timer_` might already be active in a scenario where a
  // second scroll completes before the timer finishes.
  if (!position_update_timer_.IsActive()) {
    position_update_timer_.StartOneShot(intersection_observer_delay_,
                                        FROM_HERE);
  }
}

IntersectionObserver*
AnchorElementViewportPositionTracker::GetIntersectionObserverForTesting() {
  return intersection_observer_;
}

void AnchorElementViewportPositionTracker::UpdateVisibleAnchors(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  DCHECK(base::FeatureList::IsEnabled(features::kNavigationPredictor));
  DCHECK(!entries.empty());
  if (!GetSupplementable()->GetFrame()) {
    return;
  }

  HeapVector<Member<const HTMLAnchorElementBase>> entered_viewport;
  HeapVector<Member<const HTMLAnchorElementBase>> left_viewport;

  for (const auto& entry : entries) {
    const Element* element = entry->target();
    const HTMLAnchorElementBase& anchor_element =
        To<HTMLAnchorElementBase>(*element);
    if (!entry->isIntersecting()) {
      // The anchor is leaving the viewport.
      anchors_in_viewport_.erase(&anchor_element);
      left_viewport.push_back(&anchor_element);
    } else {
      // The anchor is visible.
      anchors_in_viewport_.insert(&anchor_element);
      entered_viewport.push_back(&anchor_element);
    }
  }

  if (position_update_timer_.IsActive()) {
    CHECK(ShouldReportViewportPositions());
    position_update_timer_.Stop();
    RegisterForLifecycleNotifications();
  }

  for (Observer* observer : observers_) {
    observer->ViewportIntersectionUpdate(entered_viewport, left_viewport);
  }
}

void AnchorElementViewportPositionTracker::PositionUpdateTimerFired(
    TimerBase*) {
  CHECK(ShouldReportViewportPositions());
  if (LocalFrameView* view = GetSupplementable()->View()) {
    view->ScheduleAnimation();
    RegisterForLifecycleNotifications();
  }
}

void AnchorElementViewportPositionTracker::DidFinishLifecycleUpdate(
    const LocalFrameView& local_frame_view) {
  CHECK(ShouldReportViewportPositions());
  Document* document = local_frame_view.GetFrame().GetDocument();
  if (document->Lifecycle().GetState() <
      DocumentLifecycle::kAfterPerformLayout) {
    return;
  }
  if (!GetSupplementable()->GetFrame()) {
    return;
  }
  DispatchAnchorElementsPositionUpdates();
  DCHECK_EQ(&local_frame_view, GetSupplementable()->View());
  DCHECK(is_registered_for_lifecycle_notifications_);
  GetSupplementable()->View()->UnregisterFromLifecycleNotifications(this);
  is_registered_for_lifecycle_notifications_ = false;
}

void AnchorElementViewportPositionTracker::
    DispatchAnchorElementsPositionUpdates() {
  CHECK(ShouldReportViewportPositions());

  Screen* screen = GetSupplementable()->domWindow()->screen();
  FrameWidget* widget =
      GetSupplementable()->GetFrame()->GetWidgetForLocalRoot();
  Page* page = GetSupplementable()->GetPage();
  if (!screen || !widget || !page) {
    return;
  }

  const int screen_height_dips = screen->height();
  const int viewport_height = page->GetVisualViewport().Size().height();
  if (!screen_height_dips || !viewport_height) {
    return;
  }

  const float screen_height = widget->DIPsToBlinkSpace(screen_height_dips);
  const float browser_controls_height =
      GetBrowserControlsHeight(*GetSupplementable());

  HeapVector<Member<Observer::AnchorPositionUpdate>> position_updates;
  for (const HTMLAnchorElementBase* anchor : anchors_in_viewport_) {
    LocalFrame* frame = anchor->GetDocument().GetFrame();
    if (!frame) {
      continue;
    }
    const LocalFrame& local_root = frame->LocalFrameRoot();
    // TODO(crbug.com/347719430): LocalFrameView::FrameToViewport called below
    // doesn't work for subframes whose local root is not the main frame.
    if (!local_root.IsMainFrame()) {
      continue;
    }

    gfx::Rect rect = anchor->VisibleBoundsInLocalRoot();
    if (rect.IsEmpty()) {
      continue;
    }
    rect = local_root.View()->FrameToViewport(rect);
    rect.Offset(0, browser_controls_height);
    float center_point_y = gfx::RectF(rect).CenterPoint().y();

    // TODO(crbug.com/347638530): Ideally we would do this entire calculation
    // in screen coordinates and use screen_height (that would be a more useful
    // metric for us), but we don't have an accurate way to do so right now.
    float vertical_position =
        center_point_y / (viewport_height + browser_controls_height);

    std::optional<float> distance_from_pointer_down_ratio;
    if (last_pointer_down_.has_value()) {
      // Note: Distances in viewport space should be the same as distances in
      // screen space, so dividing by |screen_height| instead of viewport height
      // is fine (and likely a more useful metric).
      float distance_from_pointer_down =
          center_point_y - last_pointer_down_.value();
      distance_from_pointer_down_ratio =
          distance_from_pointer_down / screen_height;
    }
    auto* position_update =
        MakeGarbageCollected<Observer::AnchorPositionUpdate>();
    position_update->anchor_element = anchor;
    position_update->vertical_position = vertical_position;
    position_update->distance_from_pointer_down =
        distance_from_pointer_down_ratio;
    position_updates.push_back(position_update);
  }

  for (Observer* observer : observers_) {
    observer->AnchorPositionsUpdated(position_updates);
  }
}

void AnchorElementViewportPositionTracker::RegisterForLifecycleNotifications() {
  if (is_registered_for_lifecycle_notifications_) {
    return;
  }

  if (LocalFrameView* view = GetSupplementable()->View()) {
    view->RegisterForLifecycleNotifications(this);
    is_registered_for_lifecycle_notifications_ = true;
  }
}

}  // namespace blink
