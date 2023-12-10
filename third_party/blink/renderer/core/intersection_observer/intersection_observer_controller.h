// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_CONTROLLER_H_

#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

// Design doc for IntersectionObserver implementation:
//   https://docs.google.com/a/google.com/document/d/1hLK0eyT5_BzyNS4OkjsnoqqFQDYCbKfyBinj94OnLiQ

namespace blink {

class ExecutionContext;

class IntersectionObserverController
    : public GarbageCollected<IntersectionObserverController>,
      public ExecutionContextClient,
      public NameClient {
 public:
  explicit IntersectionObserverController(ExecutionContext*);
  ~IntersectionObserverController() override;

  void ScheduleIntersectionObserverForDelivery(IntersectionObserver&);

  // Immediately deliver all notifications for all observers for which
  // (observer->GetDeliveryBehavior() == behavior).
  void DeliverNotifications(IntersectionObserver::DeliveryBehavior behavior);

  // The flags argument is composed of values from
  // IntersectionObservation::ComputeFlags. They are dirty bits that control
  // whether an IntersectionObserver needs to do any work. The return value
  // communicates whether observer->trackVisibility() is true for any tracked
  // observer.
  bool ComputeIntersections(
      unsigned flags,
      LocalFrameUkmAggregator* metrics_aggregator,
      absl::optional<base::TimeTicks>& monotonic_time,
      gfx::Vector2dF accumulated_scroll_delta_since_last_update);

  // The second argument indicates whether the Element is a target of any
  // observers for which observer->trackVisibility() is true.
  void AddTrackedObserver(IntersectionObserver&);
  void AddTrackedObservation(IntersectionObservation&);
  void RemoveTrackedObserver(IntersectionObserver&);
  void RemoveTrackedObservation(IntersectionObservation&);

  bool NeedsOcclusionTracking() const { return needs_occlusion_tracking_; }

  void Trace(Visitor*) const override;
  const char* NameInHeapSnapshot() const override {
    return "IntersectionObserverController";
  }

  unsigned GetTrackedObserverCountForTesting() const {
    return tracked_explicit_root_observers_.size();
  }
  unsigned GetTrackedObservationCountForTesting() const {
    return tracked_implicit_root_observations_.size();
  }

  // Returns true if any IntersectionObservation has invalidated cached rects
  // since the last update.
  bool InvalidateCachedRectsIfNeeded();

 private:
  void PostTaskToDeliverNotifications();

 private:
  // IntersectionObserver's with a connected explicit root in this document.
  HeapHashSet<WeakMember<IntersectionObserver>>
      tracked_explicit_root_observers_;
  // IntersectionObservations with an implicit root and connected target in this
  // document.
  HeapHashSet<WeakMember<IntersectionObservation>>
      tracked_implicit_root_observations_;
  // IntersectionObservers for which this is the execution context of the
  // callback, and with unsent notifications.
  HeapHashSet<Member<IntersectionObserver>> pending_intersection_observers_;
  // This is 'true' if any tracked node is the target of an observer for
  // which observer->trackVisibility() is true.
  bool needs_occlusion_tracking_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_CONTROLLER_H_
