// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

IntersectionObserverController::IntersectionObserverController(
    ExecutionContext* context)
    : ExecutionContextClient(context) {}

IntersectionObserverController::~IntersectionObserverController() = default;

void IntersectionObserverController::PostTaskToDeliverNotifications() {
  DCHECK(GetExecutionContext());
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kInternalIntersectionObserver)
      ->PostTask(
          FROM_HERE,
          WTF::BindOnce(&IntersectionObserverController::DeliverNotifications,
                        WrapWeakPersistent(this),
                        IntersectionObserver::kPostTaskToDeliver));
}

void IntersectionObserverController::ScheduleIntersectionObserverForDelivery(
    IntersectionObserver& observer) {
  pending_intersection_observers_.insert(&observer);
  if (observer.GetDeliveryBehavior() ==
      IntersectionObserver::kPostTaskToDeliver)
    PostTaskToDeliverNotifications();
}

void IntersectionObserverController::DeliverNotifications(
    IntersectionObserver::DeliveryBehavior behavior) {
  ExecutionContext* context = GetExecutionContext();
  if (!context) {
    pending_intersection_observers_.clear();
    return;
  }
  HeapVector<Member<IntersectionObserver>> intersection_observers_being_invoked;
  for (auto& observer : pending_intersection_observers_) {
    if (observer->GetDeliveryBehavior() == behavior)
      intersection_observers_being_invoked.push_back(observer);
  }
  for (auto& observer : intersection_observers_being_invoked) {
    pending_intersection_observers_.erase(observer);
    observer->Deliver();
  }
}

bool IntersectionObserverController::ComputeIntersections(
    unsigned flags,
    LocalFrameUkmAggregator* metrics_aggregator,
    absl::optional<base::TimeTicks>& monotonic_time) {
  needs_occlusion_tracking_ = false;
  if (!GetExecutionContext())
    return false;
  TRACE_EVENT0("blink,devtools.timeline",
               "IntersectionObserverController::"
               "computeIntersections");
  HeapVector<Member<IntersectionObserver>> observers_to_process(
      tracked_explicit_root_observers_);
  HeapVector<Member<IntersectionObservation>> observations_to_process(
      tracked_implicit_root_observations_);
  int64_t internal_observation_count = 0;
  int64_t javascript_observation_count = 0;
  {
    absl::optional<LocalFrameUkmAggregator::IterativeTimer> metrics_timer;
    if (metrics_aggregator)
      metrics_timer.emplace(*metrics_aggregator);
    for (auto& observer : observers_to_process) {
      if (observer->HasObservations()) {
        if (metrics_timer)
          metrics_timer->StartInterval(observer->GetUkmMetricId());
        int64_t count = observer->ComputeIntersections(flags, monotonic_time);
        if (observer->IsInternal())
          internal_observation_count += count;
        else
          javascript_observation_count += count;
        needs_occlusion_tracking_ |= observer->trackVisibility();
      } else {
        tracked_explicit_root_observers_.erase(observer);
      }
    }
    for (auto& observation : observations_to_process) {
      if (metrics_timer)
        metrics_timer->StartInterval(observation->Observer()->GetUkmMetricId());
      int64_t count = observation->ComputeIntersection(flags, monotonic_time);
      if (observation->Observer()->IsInternal())
        internal_observation_count += count;
      else
        javascript_observation_count += count;
      needs_occlusion_tracking_ |= observation->Observer()->trackVisibility();
    }
  }

  if (metrics_aggregator) {
    metrics_aggregator->RecordCountSample(
        LocalFrameUkmAggregator::kIntersectionObservationInternalCount,
        internal_observation_count);
    metrics_aggregator->RecordCountSample(
        LocalFrameUkmAggregator::kIntersectionObservationJavascriptCount,
        javascript_observation_count);
  }

  return needs_occlusion_tracking_;
}

gfx::Vector2dF IntersectionObserverController::MinScrollDeltaToUpdate() const {
  DCHECK(RuntimeEnabledFeatures::IntersectionOptimizationEnabled());
  gfx::Vector2dF result(std::numeric_limits<float>::max(),
                        std::numeric_limits<float>::max());
  for (const auto& observer : tracked_explicit_root_observers_) {
    result.SetToMin(observer->MinScrollDeltaToUpdate());
  }
  for (const auto& observation : tracked_implicit_root_observations_) {
    result.SetToMin(observation->MinScrollDeltaToUpdate());
  }
  return result;
}

void IntersectionObserverController::AddTrackedObserver(
    IntersectionObserver& observer) {
  // We only track explicit-root observers that have active observations.
  if (observer.RootIsImplicit() || !observer.HasObservations())
    return;
  tracked_explicit_root_observers_.insert(&observer);
  if (observer.trackVisibility()) {
    needs_occlusion_tracking_ = true;
    if (LocalFrameView* frame_view = observer.root()->GetDocument().View()) {
      if (FrameOwner* frame_owner = frame_view->GetFrame().Owner()) {
        // Set this bit as early as possible, rather than waiting for a
        // lifecycle update to recompute it.
        frame_owner->SetNeedsOcclusionTracking(true);
      }
    }
  }
}

void IntersectionObserverController::RemoveTrackedObserver(
    IntersectionObserver& observer) {
  if (observer.RootIsImplicit())
    return;
  // Note that we don't try to opportunistically turn off the 'needs occlusion
  // tracking' bit here, like the way we turn it on in AddTrackedObserver. The
  // bit will get recomputed on the next lifecycle update; there's no
  // compelling reason to do it here, so we avoid the iteration through
  // observers and observations here.
  tracked_explicit_root_observers_.erase(&observer);
}

void IntersectionObserverController::AddTrackedObservation(
    IntersectionObservation& observation) {
  IntersectionObserver* observer = observation.Observer();
  DCHECK(observer);
  if (!observer->RootIsImplicit())
    return;
  tracked_implicit_root_observations_.insert(&observation);
  if (observer->trackVisibility()) {
    needs_occlusion_tracking_ = true;
    if (LocalFrameView* frame_view =
            observation.Target()->GetDocument().View()) {
      if (FrameOwner* frame_owner = frame_view->GetFrame().Owner()) {
        frame_owner->SetNeedsOcclusionTracking(true);
      }
    }
  }
}

void IntersectionObserverController::RemoveTrackedObservation(
    IntersectionObservation& observation) {
  IntersectionObserver* observer = observation.Observer();
  DCHECK(observer);
  if (!observer->RootIsImplicit())
    return;
  tracked_implicit_root_observations_.erase(&observation);
}

void IntersectionObserverController::Trace(Visitor* visitor) const {
  visitor->Trace(tracked_explicit_root_observers_);
  visitor->Trace(tracked_implicit_root_observations_);
  visitor->Trace(pending_intersection_observers_);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
