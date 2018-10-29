// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"

#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/intersection_geometry.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

namespace {

bool IsOccluded(const Element& element, const IntersectionGeometry& geometry) {
  DCHECK(RuntimeEnabledFeatures::IntersectionObserverV2Enabled());
  if (element.GetDocument()
          .GetFrame()
          ->LocalFrameRoot()
          .MayBeOccludedOrObscuredByRemoteAncestor()) {
    return true;
  }
  // TODO(layout-dev): This should hit-test the intersection rect, not the
  // target rect; it's not helpful to know that the portion of the target that
  // is clipped is also occluded. To do that, the intersection rect must be
  // mapped down to the local space of the target element.
  HitTestResult result(
      element.GetLayoutObject()->HitTestForOcclusion(geometry.TargetRect()));
  return result.InnerNode() && result.InnerNode() != &element;
}

}  // namespace

IntersectionObservation::IntersectionObservation(IntersectionObserver& observer,
                                                 Element& target)
    : observer_(observer),
      target_(&target),
      last_run_time_(-observer.GetEffectiveDelay()),
      last_is_visible_(false),
      // Note that the spec says the initial value of last_threshold_index_
      // should be -1, but since last_threshold_index_ is unsigned, we use a
      // different sentinel value.
      last_threshold_index_(kMaxThresholdIndex - 1) {}

void IntersectionObservation::Compute(unsigned flags) {
  DCHECK(Observer());
  if (!target_ || !observer_->RootIsValid() | !observer_->GetExecutionContext())
    return;
  if (flags &
      (observer_->RootIsImplicit() ? kImplicitRootObserversNeedUpdate
                                   : kExplicitRootObserversNeedUpdate)) {
    needs_update_ = true;
  }
  if (!needs_update_)
    return;
  DOMHighResTimeStamp timestamp = observer_->GetTimeStamp();
  if (timestamp == -1)
    return;
  if (timestamp - last_run_time_ < observer_->GetEffectiveDelay())
    return;
  last_run_time_ = timestamp;
  needs_update_ = 0;
  Vector<Length> root_margin(4);
  root_margin[0] = observer_->TopMargin();
  root_margin[1] = observer_->RightMargin();
  root_margin[2] = observer_->BottomMargin();
  root_margin[3] = observer_->LeftMargin();
  bool report_root_bounds =
      (flags & kReportImplicitRootBounds) || !observer_->RootIsImplicit();
  IntersectionGeometry geometry(observer_->root(), *Target(), root_margin,
                                report_root_bounds);
  geometry.ComputeGeometry();

  // Some corner cases for threshold index:
  //   - If target rect is zero area, because it has zero width and/or zero
  //     height,
  //     only two states are recognized:
  //     - 0 means not intersecting.
  //     - 1 means intersecting.
  //     No other threshold crossings are possible.
  //   - Otherwise:
  //     - If root and target do not intersect, the threshold index is 0.
  //     - If root and target intersect but the intersection has zero-area
  //       (i.e., they have a coincident edge or corner), we consider the
  //       intersection to have "crossed" a zero threshold, but not crossed
  //       any non-zero threshold.
  unsigned new_threshold_index;
  float new_visible_ratio;
  bool is_visible = false;
  if (geometry.DoesIntersect()) {
    if (geometry.TargetRect().IsEmpty()) {
      new_visible_ratio = 1;
    } else {
      float intersection_area =
          geometry.IntersectionRect().Size().Width().ToFloat() *
          geometry.IntersectionRect().Size().Height().ToFloat();
      float target_area = geometry.TargetRect().Size().Width().ToFloat() *
                          geometry.TargetRect().Size().Height().ToFloat();
      new_visible_ratio = intersection_area / target_area;
    }
    new_threshold_index =
        Observer()->FirstThresholdGreaterThan(new_visible_ratio);
    if (RuntimeEnabledFeatures::IntersectionObserverV2Enabled() &&
        Observer()->trackVisibility()) {
      is_visible = new_threshold_index > 0 &&
                   !Target()->GetLayoutObject()->HasDistortingVisualEffects() &&
                   !IsOccluded(*Target(), geometry);
    }
  } else {
    new_visible_ratio = 0;
    new_threshold_index = 0;
  }

  // TODO(tkent): We can't use CHECK_LT due to a compile error.
  CHECK(new_threshold_index < kMaxThresholdIndex - 1);

  if (last_threshold_index_ != new_threshold_index ||
      last_is_visible_ != is_visible) {
    FloatRect root_bounds(geometry.UnZoomedRootRect());
    FloatRect* root_bounds_pointer =
        report_root_bounds ? &root_bounds : nullptr;
    IntersectionObserverEntry* new_entry = new IntersectionObserverEntry(
        timestamp, new_visible_ratio, FloatRect(geometry.UnZoomedTargetRect()),
        root_bounds_pointer, FloatRect(geometry.UnZoomedIntersectionRect()),
        new_threshold_index > 0, is_visible, Target());
    entries_.push_back(new_entry);
    To<Document>(Observer()->GetExecutionContext())
        ->EnsureIntersectionObserverController()
        .ScheduleIntersectionObserverForDelivery(*Observer());
    SetLastThresholdIndex(new_threshold_index);
    SetWasVisible(is_visible);
  }
}

void IntersectionObservation::TakeRecords(
    HeapVector<Member<IntersectionObserverEntry>>& entries) {
  entries.AppendVector(entries_);
  entries_.clear();
}

void IntersectionObservation::Disconnect() {
  DCHECK(Observer());
  if (target_)
    Target()->EnsureIntersectionObserverData().RemoveObservation(*Observer());
  entries_.clear();
  observer_.Clear();
}

void IntersectionObservation::Trace(blink::Visitor* visitor) {
  visitor->Trace(observer_);
  visitor->Trace(entries_);
  visitor->Trace(target_);
}

}  // namespace blink
