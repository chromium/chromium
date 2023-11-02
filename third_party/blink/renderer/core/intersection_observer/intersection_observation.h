// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVATION_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_geometry.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class Element;
class IntersectionObserver;
class IntersectionObserverEntry;

// IntersectionObservation represents the result of calling
// IntersectionObserver::observe(target) for some target element; it tracks the
// intersection between a single target element and the IntersectionObserver's
// root.  It is an implementation-internal class without any exposed interface.
class CORE_EXPORT IntersectionObservation final
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
    // If this bit is set, it indicates that at least one LocalFrameView
    // ancestor is detached from the LayoutObject tree of its parent. Usually,
    // this is unnecessary -- if an ancestor FrameView is detached, then all
    // descendant frames are detached. There is, however, at least one exception
    // to this rule; see crbug.com/749737 for details.
    kAncestorFrameIsDetachedFromLayout = 1 << 3,
    // If this bit is set, then the observer.delay parameter is ignored; i.e.,
    // the computation will run even if the previous run happened within the
    // delay parameter.
    kIgnoreDelay = 1 << 4,
    // If this bit is set, we can skip tracking the sticky frame during
    // UpdateViewportIntersectionsForSubtree.
    kCanSkipStickyFrameTracking = 1 << 5,
    // If this bit is set, we only process intersection observations that
    // require post-layout delivery.
    kPostLayoutDeliveryOnly = 1 << 6,
    // If this is set, the overflow clip edge is used.
    kUseOverflowClipEdge = 1 << 7,
  };

  IntersectionObservation(IntersectionObserver&, Element&);

  IntersectionObserver* Observer() const { return observer_.Get(); }
  Element* Target() const { return target_; }
  unsigned LastThresholdIndex() const { return last_threshold_index_; }
  // Returns 1 if the geometry was recalculated, otherwise 0. This could be a
  // bool, but int64_t matches IntersectionObserver::ComputeIntersections().
  int64_t ComputeIntersection(unsigned flags,
                              absl::optional<base::TimeTicks>& monotonic_time);
  int64_t ComputeIntersection(
      const IntersectionGeometry::RootGeometry& root_geometry,
      unsigned flags,
      absl::optional<base::TimeTicks>& monotonic_time);
  void TakeRecords(HeapVector<Member<IntersectionObserverEntry>>&);
  void Disconnect();
  void InvalidateCachedRects();

  void Trace(Visitor*) const;

  bool CanUseCachedRectsForTesting() const { return CanUseCachedRects(); }

 private:
  bool ShouldCompute(unsigned flags) const;
  bool MaybeDelayAndReschedule(unsigned flags, DOMHighResTimeStamp timestamp);
  bool CanUseCachedRects() const;
  unsigned GetIntersectionGeometryFlags(unsigned compute_flags) const;
  // Inspect the geometry to see if there has been a transition event; if so,
  // generate a notification and schedule it for delivery.
  void ProcessIntersectionGeometry(const IntersectionGeometry& geometry,
                                   DOMHighResTimeStamp timestamp);
  void SetLastThresholdIndex(unsigned index) { last_threshold_index_ = index; }
  void SetWasVisible(bool last_is_visible) {
    last_is_visible_ = last_is_visible ? 1 : 0;
  }

  Member<IntersectionObserver> observer_;
  WeakMember<Element> target_;
  HeapVector<Member<IntersectionObserverEntry>> entries_;
  DOMHighResTimeStamp last_run_time_;

  std::unique_ptr<IntersectionGeometry::CachedRects> cached_rects_;

  unsigned last_is_visible_ : 1;
  unsigned needs_update_ : 1;
  unsigned last_threshold_index_ : 30;
  static const unsigned kMaxThresholdIndex = static_cast<unsigned>(0x40000000);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INTERSECTION_OBSERVER_INTERSECTION_OBSERVATION_H_
