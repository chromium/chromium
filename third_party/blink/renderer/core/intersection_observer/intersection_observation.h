
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVATION_H_

#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Element;
class IntersectionObserver;

// IntersectionObservation represents the result of calling
// IntersectionObserver::observe(target) for some target element; it tracks the
// intersection between a single target element and the IntersectionObserver's
// root.  It is an implementation-internal class without any exposed interface.
class IntersectionObservation final
    : public GarbageCollected<IntersectionObservation> {
 public:
  // Flags that drive the behavior of the ComputeIntersections() method. For an
  // explanation of implicit vs. explicit root, see intersection_observer.h.
  enum ComputeFlags {
    // If this bit is set, and observer_->RootIsImplicit() is true, then the
    // root bounds (i.e., size of the top document's viewport) should be
    // included in any IntersectionObserverEntry objects created by Compute().
    kReportImplicitRootBounds = 1 << 0,
    // If this bit is set, and observer_->RootIsImplicit() is false, then
    // Compute() should update the observation.
    kExplicitRootObserversNeedUpdate = 1 << 1,
    // If this bit is set, and observer_->RootIsImplicit() is true, then
    // Compute() should update the observation.
    kImplicitRootObserversNeedUpdate = 1 << 2,
    // If this bit is set, then the observer.delay parameter is ignored; i.e.,
    // the computation will run even if the previous run happened within the
    // delay parameter.
    kIgnoreDelay = 1 << 3,
  };

  IntersectionObservation(IntersectionObserver&, Element&);

  IntersectionObserver* Observer() const { return observer_.Get(); }
  Element* Target() const { return target_; }
  unsigned LastThresholdIndex() const { return last_threshold_index_; }
  void ComputeIntersection(unsigned flags);
  void ComputeIntersection(
      const IntersectionGeometry::RootGeometry& root_geometry,
      unsigned flags);
  void TakeRecords(HeapVector<Member<IntersectionObserverEntry>>&);
  void Disconnect();

  void Trace(blink::Visitor*);

 private:
  bool ShouldCompute(unsigned flags);
  unsigned GetIntersectionGeometryFlags(unsigned compute_flags) const;
  // Inspect the geometry to see if there has been a transition event; if so,
  // generate a notification and schedule it for delivery.
  void ProcessIntersectionGeometry(const IntersectionGeometry& geometry);
  void SetLastThresholdIndex(unsigned index) { last_threshold_index_ = index; }
  void SetWasVisible(bool last_is_visible) {
    last_is_visible_ = last_is_visible ? 1 : 0;
  }

  Member<IntersectionObserver> observer_;
  WeakMember<Element> target_;
  HeapVector<Member<IntersectionObserverEntry>> entries_;
  DOMHighResTimeStamp last_run_time_;

  unsigned last_is_visible_ : 1;
  unsigned needs_update_ : 1;
  unsigned last_threshold_index_ : 30;
  static const unsigned kMaxThresholdIndex = (unsigned)0x40000000;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVATION_H_
