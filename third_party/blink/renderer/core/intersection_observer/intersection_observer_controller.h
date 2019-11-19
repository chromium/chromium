// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_CONTROLLER_H_

#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

// Design doc for IntersectionObserver implementation:
//   https://docs.google.com/a/google.com/document/d/1hLK0eyT5_BzyNS4OkjsnoqqFQDYCbKfyBinj94OnLiQ

namespace blink {

class Document;

class IntersectionObserverController
    : public GarbageCollected<IntersectionObserverController>,
      public ContextClient,
      public NameClient {
  USING_GARBAGE_COLLECTED_MIXIN(IntersectionObserverController);

 public:
  explicit IntersectionObserverController(Document*);
  virtual ~IntersectionObserverController();

  void ScheduleIntersectionObserverForDelivery(IntersectionObserver&);

  // Immediately deliver all notifications for all observers for which
  // (observer->GetDeliveryBehavior() == behavior).
  void DeliverNotifications(IntersectionObserver::DeliveryBehavior behavior);

  // The flags argument is composed of values from
  // IntersectionObservation::ComputeFlags. They are dirty bits that control
  // whether an IntersectionObserver needs to do any work. The return value
  // communicates whether observer->trackVisibility() is true for any tracked
  // observer.
  bool ComputeIntersections(unsigned flags);
  // The second argument indicates whether the Element is a target of any
  // observers for which observer->trackVisibility() is true.
  void AddTrackedElement(Element&, bool);
  void RemoveTrackedElement(Element&);
  bool NeedsOcclusionTracking() const { return needs_occlusion_tracking_; }

  void Trace(blink::Visitor*) override;
  const char* NameInHeapSnapshot() const override {
    return "IntersectionObserverController";
  }
  unsigned GetTrackedTargetCountForTesting() const {
    return tracked_elements_.size();
  }

 private:
  void PostTaskToDeliverNotifications();

 private:
  // Elements in this document which are the target of an IntersectionObserver
  // with implicit root; or the explicit root of an IntersectionObserver.
  HeapHashSet<WeakMember<Element>> tracked_elements_;
  // IntersectionObservers for which this is the execution context of the
  // callback.
  HeapHashSet<Member<IntersectionObserver>> pending_intersection_observers_;
  // This is 'true' if any tracked element is the target of an observer for
  // which observer->trackVisibility() is true.
  bool needs_occlusion_tracking_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVER_CONTROLLER_H_
