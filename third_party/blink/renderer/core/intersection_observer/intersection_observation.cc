// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"

#include "base/debug/stack_trace.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_vector.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_geometry.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

#define CHECK_SKIPPED_UPDATE_ON_SCROLL DCHECK_IS_ON()

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
      last_run_time_(-observer.GetEffectiveDelay()) {}

int64_t IntersectionObservation::ComputeIntersection(
    unsigned compute_flags,
    gfx::Vector2dF accumulated_scroll_delta_since_last_update,
    absl::optional<base::TimeTicks>& monotonic_time,
    absl::optional<IntersectionGeometry::RootGeometry>& root_geometry) {
  DCHECK(Observer());
  cached_rects_.min_scroll_delta_to_update -=
      accumulated_scroll_delta_since_last_update;
  if (compute_flags &
      (observer_->RootIsImplicit() ? kImplicitRootObserversNeedUpdate
                                   : kExplicitRootObserversNeedUpdate)) {
    needs_update_ = true;
  }
  if (!ShouldCompute(compute_flags))
    return 0;
  if (!monotonic_time.has_value())
    monotonic_time = base::DefaultTickClock::GetInstance()->NowTicks();
  DOMHighResTimeStamp timestamp = observer_->GetTimeStamp(*monotonic_time);
  if (MaybeDelayAndReschedule(compute_flags, timestamp))
    return 0;

#if CHECK_SKIPPED_UPDATE_ON_SCROLL
  std::optional<IntersectionGeometry::CachedRects> cached_rects_backup;
#endif
  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled() &&
      cached_rects_.valid && cached_rects_.min_scroll_delta_to_update.x() > 0 &&
      cached_rects_.min_scroll_delta_to_update.y() > 0) {
#if CHECK_SKIPPED_UPDATE_ON_SCROLL
    cached_rects_backup.emplace(cached_rects_);
#else
    return 0;
#endif
  }

  unsigned geometry_flags = GetIntersectionGeometryFlags(compute_flags);
  IntersectionGeometry geometry(
      observer_->root(), *Target(), observer_->RootMargin(),
      observer_->thresholds(), observer_->TargetMargin(),
      observer_->ScrollMargin(), geometry_flags, root_geometry, &cached_rects_);

#if CHECK_SKIPPED_UPDATE_ON_SCROLL
  if (cached_rects_backup) {
    // A skipped update on scroll should generate the same result.
    cached_rects_ = cached_rects_backup.value();
    CHECK_EQ(last_threshold_index_, geometry.ThresholdIndex());
    CHECK_EQ(last_is_visible_, geometry.IsVisible());
    return 0;
  }
#endif

  ProcessIntersectionGeometry(geometry, timestamp);
  last_run_time_ = timestamp;
  needs_update_ = false;
  return geometry.DidComputeGeometry() ? 1 : 0;
}

gfx::Vector2dF IntersectionObservation::MinScrollDeltaToUpdate() const {
  if (cached_rects_.valid) {
    return cached_rects_.min_scroll_delta_to_update;
  }
  return gfx::Vector2dF();
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
    observer_data->RemoveObservation(*this);
    if (target_->isConnected()) {
      IntersectionObserverController* controller =
          target_->GetDocument().GetIntersectionObserverController();
      if (controller)
        controller->RemoveTrackedObservation(*this);
    }
  }
  entries_.clear();
  observer_.Clear();
}

bool IntersectionObservation::InvalidateCachedRectsIfNeeded() {
  DCHECK(RuntimeEnabledFeatures::IntersectionOptimizationEnabled());
  if (!cached_rects_.valid) {
    return true;
  }
  if (NeedsInvalidateCachedRects()) {
    InvalidateCachedRects();
    return true;
  }
  return false;
}

bool IntersectionObservation::NeedsInvalidateCachedRects() const {
  DCHECK(cached_rects_.valid);
  if (observer_->trackVisibility()) {
    return true;
  }
  const LayoutObject* target_object =
      IntersectionGeometry::GetTargetLayoutObject(*target_);
  if (!target_object ||
      !IntersectionGeometry::CanUseGeometryMapper(*target_object)) {
    return true;
  }
  const LayoutObject* root_object = nullptr;
  PropertyTreeStateOrAlias root_state = PropertyTreeState::Root();
  if (!observer_->RootIsImplicit()) {
    root_object =
        IntersectionGeometry::GetExplicitRootLayoutObject(*observer_->root());
    if (!root_object || root_object == target_object) {
      return true;
    }
    const auto* root_property_container =
        root_object->GetPropertyContainer(nullptr);
    if (!root_property_container) {
      return true;
    }
    root_state = root_property_container->FirstFragment().ContentsProperties();
  }
  PropertyTreeStateOrAlias target_state = PropertyTreeState::Uninitialized();
  LayoutObject::AncestorSkipInfo skip_info(root_object);
  if (!target_object->GetPropertyContainer(&skip_info, &target_state)) {
    return true;
  }
  return target_state.ChangedExceptScrollAndEffect(
      PaintPropertyChangeType::kChangedOnlyCompositedValues,
      root_state.Unalias());
}

void IntersectionObservation::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  visitor->Trace(entries_);
  visitor->Trace(target_);
}

bool IntersectionObservation::CanUseCachedRectsForTesting() const {
  // This is to avoid the side effects of IntersectionGeometry.
  IntersectionGeometry::CachedRects cached_rects_copy = cached_rects_;

  absl::optional<IntersectionGeometry::RootGeometry> root_geometry;
  IntersectionGeometry geometry(observer_->root(), *target_,
                                /* root_margin */ {},
                                /* thresholds */ {0},
                                /* target_margin */ {},
                                /* scroll_margin */ {},
                                /* flags */ 0, root_geometry,
                                &cached_rects_copy);

  return geometry.CanUseCachedRectsForTesting();
}

bool IntersectionObservation::ShouldCompute(unsigned flags) const {
  if (!target_ || !observer_->RootIsValid() ||
      !observer_->GetExecutionContext())
    return false;
  // If we're processing post-layout deliveries only and we don't have a
  // post-layout delivery observer, then return early. Likewise, return if we
  // need to compute non-post-layout-delivery observations but the observer
  // behavior is post-layout.
  bool post_layout_delivery_only = flags & kPostLayoutDeliveryOnly;
  bool is_post_layout_delivery_observer =
      Observer()->GetDeliveryBehavior() ==
      IntersectionObserver::kDeliverDuringPostLayoutSteps;
  if (post_layout_delivery_only != is_post_layout_delivery_observer)
    return false;
  if (!needs_update_)
    return false;
  if (target_->isConnected() && target_->GetDocument().GetFrame() &&
      Observer()->trackVisibility()) {
    mojom::blink::FrameOcclusionState occlusion_state =
        target_->GetDocument().GetFrame()->GetOcclusionState();
    // If we're tracking visibility, and we don't have occlusion information
    // from our parent frame, then postpone computing intersections until a
    // later lifecycle when the occlusion information is known.
    if (occlusion_state == mojom::blink::FrameOcclusionState::kUnknown)
      return false;
  }
  return true;
}

bool IntersectionObservation::MaybeDelayAndReschedule(
    unsigned flags,
    DOMHighResTimeStamp timestamp) {
  if (timestamp == -1)
    return true;
  base::TimeDelta delay = base::Milliseconds(observer_->GetEffectiveDelay() -
                                             (timestamp - last_run_time_));
  if (!(flags & kIgnoreDelay) && delay.is_positive()) {
    TrackingDocument(this).View()->ScheduleAnimation(delay);
    return true;
  }
  return false;
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
  if (Observer()->UseOverflowClipEdge())
    geometry_flags |= IntersectionGeometry::kUseOverflowClipEdge;
  return geometry_flags;
}

void IntersectionObservation::ProcessIntersectionGeometry(
    const IntersectionGeometry& geometry,
    DOMHighResTimeStamp timestamp) {
  CHECK_LT(geometry.ThresholdIndex(), kNotFound);

  if (last_threshold_index_ != geometry.ThresholdIndex() ||
      last_is_visible_ != geometry.IsVisible()) {
    entries_.push_back(MakeGarbageCollected<IntersectionObserverEntry>(
        geometry, timestamp, Target()));
    Observer()->ReportUpdates(*this);
    last_threshold_index_ = geometry.ThresholdIndex();
    last_is_visible_ = geometry.IsVisible();
  }
}

}  // namespace blink
