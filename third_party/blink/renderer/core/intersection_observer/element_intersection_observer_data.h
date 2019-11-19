// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_ELEMENT_INTERSECTION_OBSERVER_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_ELEMENT_INTERSECTION_OBSERVER_DATA_H_

#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class IntersectionObservation;
class IntersectionObserver;

class ElementIntersectionObserverData final
    : public GarbageCollected<ElementIntersectionObserverData>,
      public NameClient {
 public:
  ElementIntersectionObserverData();

  IntersectionObservation* GetObservationFor(IntersectionObserver&);
  void AddObservation(IntersectionObservation&);
  void AddObserver(IntersectionObserver&);
  void RemoveObservation(IntersectionObserver&);
  bool IsTarget() const { return !intersection_observations_.IsEmpty(); }
  bool IsTargetOfImplicitRootObserver() const;
  bool IsRoot() const { return !intersection_observers_.IsEmpty(); }
  // Run the IntersectionObserver algorithm for all observations for which this
  // element is target.
  bool ComputeIntersectionsForTarget(unsigned flags);
  // Run the IntersectionObserver algorithm for all implicit-root observations
  // for which this element is target; and all explicit-root observers for which
  // this element is root. Returns true if any observer needs occlusion
  // tracking.
  bool ComputeIntersectionsForLifecycleUpdate(unsigned flags);
  bool NeedsOcclusionTracking() const;

  void Trace(blink::Visitor*);
  const char* NameInHeapSnapshot() const override {
    return "ElementIntersectionObserverData";
  }

 private:
  // IntersectionObservations for which the Node owning this data is target.
  HeapHashMap<Member<IntersectionObserver>, Member<IntersectionObservation>>
      intersection_observations_;
  // IntersectionObservers for which the Node owning this data is root.
  // Weak because once an observer is unreachable from javascript and has no
  // active observations, it should be allowed to die.
  HeapHashSet<WeakMember<IntersectionObserver>> intersection_observers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_ELEMENT_INTERSECTION_OBSERVER_DATA_H_
