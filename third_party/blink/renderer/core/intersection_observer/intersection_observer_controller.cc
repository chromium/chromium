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

base::TimeTicks ComputeIntersectionsContext::GetMonotonicTime() {
  if (monotonic_time_.is_null()) {
    monotonic_time_ = base::DefaultTickClock::GetInstance()->NowTicks();
  }
  return monotonic_time_;
}

DOMHighResTimeStamp ComputeIntersectionsContext::GetTimeStamp(
    const IntersectionObserver& observer) {
  ExecutionContext* context = observer.GetExecutionContext();
  CHECK(context);
  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    if (context == implicit_root_execution_context_) {
      return implicit_root_timestamp_;
    }
    if (context == explicit_root_execution_context_) {
      return explicit_root_timestamp_;
    }
  }

  DOMHighResTimeStamp timestamp =
      DOMWindowPerformance::performance(To<LocalDOMWindow>(*context))
          ->MonotonicTimeToDOMHighResTimeStamp(GetMonotonicTime());

  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    if (observer.RootIsImplicit()) {
      implicit_root_execution_context_ = context;
      implicit_root_timestamp_ = timestamp;
    } else {
      explicit_root_execution_context_ = context;
      explicit_root_timestamp_ = timestamp;
    }
  }
  return timestamp;
}

std::optional<IntersectionGeometry::RootGeometry>&
ComputeIntersectionsContext::GetRootGeometry(
    const IntersectionObserver& observer,
    unsigned flags) {
  if (observer.RootIsImplicit()) {
    if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
      if (&observer != implicit_root_geometry_observer_) {
        implicit_root_geometry_observer_ = &observer;
        if (implicit_root_geometry_) {
          implicit_root_geometry_->UpdateMargin(
              flags & IntersectionGeometry::kShouldReportRootBounds
                  ? observer.RootMargin()
                  : Vector<Length>());
        }
      }
    } else {
      implicit_root_geometry_.reset();
    }
    return implicit_root_geometry_;
  }

  if (&observer != explicit_root_geometry_observer_) {
    explicit_root_geometry_observer_ = &observer;
    explicit_root_geometry_.reset();
  }
  return explicit_root_geometry_;
}

void ComputeIntersectionsContext::UpdateNextRunDelay(base::TimeDelta delay) {
  next_run_delay_ = std::min(next_run_delay_, delay);
}

base::TimeDelta ComputeIntersectionsContext::GetAndResetNextRunDelay() {
  base::TimeDelta result = next_run_delay_;
  next_run_delay_ = base::TimeDelta::Max();
  return result;
}

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
    LocalFrameView& frame_view,
    gfx::Vector2dF accumulated_scroll_delta_since_last_update,
    ComputeIntersectionsContext& context) {
  needs_occlusion_tracking_ = false;
  if (!GetExecutionContext()) {
    return false;
  }
  TRACE_EVENT0("blink,devtools.timeline",
               "IntersectionObserverController::"
               "computeIntersections");

  int64_t internal_observation_count = 0;
  int64_t javascript_observation_count = 0;

  std::optional<LocalFrameUkmAggregator::IterativeTimer> metrics_timer;
  LocalFrameUkmAggregator* metrics_aggregator = frame_view.GetUkmAggregator();
  if (metrics_aggregator) {
    metrics_timer.emplace(*metrics_aggregator);
  }

  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    auto compute_observer_intersections = [&](IntersectionObserver& observer,
                                              const auto& observations) {
      CHECK(!observations.empty());
      needs_occlusion_tracking_ |= observer.trackVisibility();
      if (metrics_timer && observer.GetUkmMetricId()) {
        metrics_timer->StartInterval(observer.GetUkmMetricId().value());
      }
      int64_t count = 0;
      for (auto& observation : observations) {
        count += observation->ComputeIntersection(
            flags, accumulated_scroll_delta_since_last_update, context);
      }
      if (observer.IsInternal()) {
        internal_observation_count += count;
      } else {
        javascript_observation_count += count;
      }
    };

    HeapVector<Member<IntersectionObserver>> observers_to_remove;
    for (auto& observer : tracked_explicit_root_observers_) {
      DCHECK(!observer->RootIsImplicit());
      if (observer->HasObservations()) {
        compute_observer_intersections(*observer, observer->Observations());
      } else {
        observers_to_remove.push_back(observer);
      }
    }
    for (auto& observer : observers_to_remove) {
      tracked_explicit_root_observers_.erase(observer);
    }

    for (auto& [observer, observations] : tracked_implicit_root_observations_) {
      DCHECK(observer->RootIsImplicit());
      compute_observer_intersections(*observer, observations);
    }
  } else {
    HeapVector<Member<IntersectionObserver>> observers_to_process(
        tracked_explicit_root_observers_);
    HeapVector<Member<IntersectionObservation>> observations_to_process;
    for (auto& observations : tracked_implicit_root_observations_.Values()) {
      observations_to_process.AppendRange(observations.begin(),
                                          observations.end());
    }
    for (auto& observer : observers_to_process) {
      DCHECK(!observer->RootIsImplicit());
      if (observer->HasObservations()) {
        if (metrics_timer && observer->GetUkmMetricId()) {
          metrics_timer->StartInterval(observer->GetUkmMetricId().value());
        }
        int64_t count = observer->ComputeIntersections(flags, context);
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
      if (metrics_timer && observation->Observer()->GetUkmMetricId()) {
        metrics_timer->StartInterval(
            observation->Observer()->GetUkmMetricId().value());
      }
      int64_t count =
          observation->ComputeIntersection(flags, gfx::Vector2dF(), context);
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

  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    base::TimeDelta delay = context.GetAndResetNextRunDelay();
    if (delay.is_positive()) {
      // TODO(crbug.com/40873583): Handle the case that the frame becomes
      // throttled during the delay,
      frame_view.ScheduleAnimation(delay);
    }
  }

  return needs_occlusion_tracking_;
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
  tracked_implicit_root_observations_
      .insert(observer, HeapHashSet<Member<IntersectionObservation>>())
      .stored_value->value.insert(&observation);
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
  auto it = tracked_implicit_root_observations_.find(observer);
  if (it != tracked_implicit_root_observations_.end()) {
    it->value.erase(&observation);
    if (it->value.empty()) {
      tracked_implicit_root_observations_.erase(it);
    }
  }
}

wtf_size_t
IntersectionObserverController::GetTrackedObservationCountForTesting() const {
  wtf_size_t count = 0;
  for (auto& observations : tracked_implicit_root_observations_.Values()) {
    count += observations.size();
  }
  return count;
}

void IntersectionObserverController::Trace(Visitor* visitor) const {
  visitor->Trace(tracked_explicit_root_observers_);
  visitor->Trace(tracked_implicit_root_observations_);
  visitor->Trace(pending_intersection_observers_);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
