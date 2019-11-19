// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"

#include "third_party/blink/renderer/core/dom/element_rare_data.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_geometry.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

namespace {

Document& TrackingDocument(const IntersectionObservation* observation) {
  if (observation->Observer()->RootIsImplicit())
    return observation->Target()->GetDocument();
  return (observation->Observer()->root()->GetDocument());
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

void IntersectionObservation::ComputeIntersection(
    const IntersectionGeometry::RootGeometry& root_geometry,
    unsigned compute_flags) {
  if (!ShouldCompute(compute_flags))
    return;
  DCHECK(observer_->root());
  unsigned geometry_flags = GetIntersectionGeometryFlags(compute_flags);
  IntersectionGeometry geometry(root_geometry, *observer_->root(), *Target(),
                                observer_->thresholds(), geometry_flags);
  ProcessIntersectionGeometry(geometry);
}

void IntersectionObservation::ComputeIntersection(unsigned compute_flags) {
  if (!ShouldCompute(compute_flags))
    return;
  unsigned geometry_flags = GetIntersectionGeometryFlags(compute_flags);
  IntersectionGeometry geometry(observer_->root(), *Target(),
                                observer_->RootMargin(),
                                observer_->thresholds(), geometry_flags);
  ProcessIntersectionGeometry(geometry);
}

void IntersectionObservation::TakeRecords(
    HeapVector<Member<IntersectionObserverEntry>>& entries) {
  entries.AppendVector(entries_);
  entries_.clear();
}

void IntersectionObservation::Disconnect() {
  DCHECK(Observer());
  if (target_) {
    DCHECK(target_->IntersectionObserverData());
    ElementIntersectionObserverData* observer_data =
        target_->IntersectionObserverData();
    observer_data->RemoveObservation(*Observer());
    // We track connected elements that are either the root of an explicit root
    // observer, or the target of an implicit root observer. If the target was
    // previously being tracked, but no longer needs to be tracked, then remove
    // it.
    if (target_->isConnected() && Observer()->RootIsImplicit() &&
        !observer_data->IsTargetOfImplicitRootObserver() &&
        !observer_data->IsRoot()) {
      IntersectionObserverController* controller =
          target_->GetDocument().GetIntersectionObserverController();
      if (controller)
        controller->RemoveTrackedElement(*target_);
    }
  }
  entries_.clear();
  observer_.Clear();
}

void IntersectionObservation::Trace(blink::Visitor* visitor) {
  visitor->Trace(observer_);
  visitor->Trace(entries_);
  visitor->Trace(target_);
}

bool IntersectionObservation::ShouldCompute(unsigned flags) {
  DCHECK(Observer());
  if (!target_ || !observer_->RootIsValid() | !observer_->GetExecutionContext())
    return false;
  if (flags &
      (observer_->RootIsImplicit() ? kImplicitRootObserversNeedUpdate
                                   : kExplicitRootObserversNeedUpdate)) {
    needs_update_ = true;
  }
  if (!needs_update_)
    return false;
  DOMHighResTimeStamp timestamp = observer_->GetTimeStamp();
  if (timestamp == -1)
    return false;
  base::TimeDelta delay = base::TimeDelta::FromMilliseconds(
      observer_->GetEffectiveDelay() - (timestamp - last_run_time_));
  if (!(flags & kIgnoreDelay) && delay > base::TimeDelta()) {
    TrackingDocument(this).View()->ScheduleAnimation(delay);
    return false;
  }
  if (target_->isConnected() && Observer()->trackVisibility()) {
    FrameOcclusionState occlusion_state =
        target_->GetDocument().GetFrame()->GetOcclusionState();
    // If we're tracking visibility, and we don't have occlusion information
    // from our parent frame, then postpone computing intersections until a
    // later lifecycle when the occlusion information is known.
    if (occlusion_state == FrameOcclusionState::kUnknown)
      return false;
  }
  last_run_time_ = timestamp;
  needs_update_ = false;
  return true;
}

unsigned IntersectionObservation::GetIntersectionGeometryFlags(
    unsigned compute_flags) const {
  bool report_root_bounds = observer_->AlwaysReportRootBounds() ||
                            (compute_flags & kReportImplicitRootBounds) ||
                            !observer_->RootIsImplicit();
  unsigned geometry_flags = IntersectionGeometry::kShouldConvertToCSSPixels;
  if (report_root_bounds)
    geometry_flags |= IntersectionGeometry::kShouldReportRootBounds;
  if (Observer()->trackVisibility())
    geometry_flags |= IntersectionGeometry::kShouldComputeVisibility;
  if (Observer()->trackFractionOfRoot())
    geometry_flags |= IntersectionGeometry::kShouldTrackFractionOfRoot;
  return geometry_flags;
}

void IntersectionObservation::ProcessIntersectionGeometry(
    const IntersectionGeometry& geometry) {
  // TODO(tkent): We can't use CHECK_LT due to a compile error.
  CHECK(geometry.ThresholdIndex() < kMaxThresholdIndex - 1);

  if (last_threshold_index_ != geometry.ThresholdIndex() ||
      last_is_visible_ != geometry.IsVisible()) {
    entries_.push_back(MakeGarbageCollected<IntersectionObserverEntry>(
        geometry, last_run_time_, Target()));
    Observer()->SetNeedsDelivery();
    SetLastThresholdIndex(geometry.ThresholdIndex());
    SetWasVisible(geometry.IsVisible());
  }
}

}  // namespace blink
