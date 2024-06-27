// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_observation.h"

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

#define CHECK_SKIPPED_UPDATE_ON_SCROLL() DCHECK_IS_ON()

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
    : observer_(observer), target_(&target) {}

int64_t IntersectionObservation::ComputeIntersection(
    unsigned compute_flags,
    gfx::Vector2dF accumulated_scroll_delta_since_last_update,
    ComputeIntersectionsContext& context) {
  DCHECK(Observer());
  cached_rects_.min_scroll_delta_to_update -=
      accumulated_scroll_delta_since_last_update;

  // If we're processing post-layout deliveries only and we don't have a
  // post-layout delivery observer, then return early. Likewise, return if we
  // need to compute non-post-layout-delivery observations but the observer
  // behavior is post-layout.
  bool post_layout_delivery_only = compute_flags & kPostLayoutDeliveryOnly;
  bool is_post_layout_delivery_observer =
      Observer()->GetDeliveryBehavior() ==
      IntersectionObserver::kDeliverDuringPostLayoutSteps;
  if (post_layout_delivery_only != is_post_layout_delivery_observer) {
    return 0;
  }

  bool has_pending_update = needs_update_;
  if (compute_flags &
      (observer_->RootIsImplicit() ? kImplicitRootObserversNeedUpdate
                                   : kExplicitRootObserversNeedUpdate)) {
    needs_update_ = true;
  }

  if (!ShouldCompute(compute_flags)) {
    return 0;
  }
  if (MaybeDelayAndReschedule(compute_flags, context)) {
    return 0;
  }

  last_run_time_ = context.GetMonotonicTime();
  needs_update_ = false;

#if CHECK_SKIPPED_UPDATE_ON_SCROLL()
  std::optional<IntersectionGeometry::CachedRects> cached_rects_backup;
#endif
  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled() &&
      !has_pending_update && (compute_flags & kScrollAndVisibilityOnly) &&
      cached_rects_.min_scroll_delta_to_update.x() > 0 &&
      cached_rects_.min_scroll_delta_to_update.y() > 0) {
#if CHECK_SKIPPED_UPDATE_ON_SCROLL()
    cached_rects_backup.emplace(cached_rects_);
#else
    // This is equivalent to a full update.
    return 1;
#endif
  }

  unsigned geometry_flags = GetIntersectionGeometryFlags(compute_flags);
  // The policy for honoring margins is the same as that for reporting root
  // bounds, so this flag can be used for both.
  bool honor_margins =
      geometry_flags & IntersectionGeometry::kShouldReportRootBounds;
  Vector<Length> empty_margin;
  IntersectionGeometry geometry(
      observer_->root(), *Target(),
      honor_margins ? observer_->RootMargin() : empty_margin,
      observer_->thresholds(),
      honor_margins ? observer_->TargetMargin() : empty_margin,
      honor_margins ? observer_->ScrollMargin() : empty_margin, geometry_flags,
      context.GetRootGeometry(*observer_, compute_flags), &cached_rects_);

#if CHECK_SKIPPED_UPDATE_ON_SCROLL()
  if (cached_rects_backup) {
    // A skipped update on scroll should generate the same result.
    CHECK_EQ(last_threshold_index_, geometry.ThresholdIndex());
    CHECK_EQ(last_is_visible_, geometry.IsVisible());
    cached_rects_ = cached_rects_backup.value();
    return 1;
  }
#endif

  ProcessIntersectionGeometry(geometry, context);
  return geometry.DidComputeGeometry() ? 1 : 0;
}

void IntersectionObservation::ComputeIntersectionImmediately(
    ComputeIntersectionsContext& context) {
  ComputeIntersection(kImplicitRootObserversNeedUpdate |
                          kExplicitRootObserversNeedUpdate | kIgnoreDelay,
                      IntersectionGeometry::kInfiniteScrollDelta, context);
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

void IntersectionObservation::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  visitor->Trace(entries_);
  visitor->Trace(target_);
}

bool IntersectionObservation::CanUseCachedRectsForTesting(
    bool scroll_and_visibility_only) const {
  // This is to avoid the side effects of IntersectionGeometry.
  IntersectionGeometry::CachedRects cached_rects_copy = cached_rects_;

  std::optional<IntersectionGeometry::RootGeometry> root_geometry;
  IntersectionGeometry geometry(
      observer_->root(), *target_,
      /* root_margin */ {},
      /* thresholds */ {0},
      /* target_margin */ {},
      /* scroll_margin */ {},
      scroll_and_visibility_only
          ? IntersectionGeometry::kScrollAndVisibilityOnly
          : 0,
      root_geometry, &cached_rects_copy);

  return geometry.CanUseCachedRectsForTesting();
}

bool IntersectionObservation::ShouldCompute(unsigned flags) const {
  if (!target_ || !observer_->RootIsValid() ||
      !observer_->GetExecutionContext()) {
    return false;
  }
  if (!needs_update_) {
    return false;
  }
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
    ComputeIntersectionsContext& context) {
  if (flags & kIgnoreDelay) {
    return false;
  }
  if (last_run_time_.is_null()) {
    return false;
  }
  base::TimeDelta delay = observer_->GetEffectiveDelay() -
                          (context.GetMonotonicTime() - last_run_time_);
  if (delay.is_positive()) {
    if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
      context.UpdateNextRunDelay(delay);
    } else {
      // TODO(crbug.com/40873583): Handle the case that the frame becomes
      // throttled during the delay,
      TrackingDocument(this).View()->ScheduleAnimation(delay);
    }
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
  if (Observer()->IsInternal()) {
    // TODO(wangxianzhu): Let internal clients decide whether to respect
    // filters.
    geometry_flags |= IntersectionGeometry::kRespectFilters;
  }
  if (compute_flags & kScrollAndVisibilityOnly) {
    geometry_flags |= IntersectionGeometry::kScrollAndVisibilityOnly;
  }
  return geometry_flags;
}

void IntersectionObservation::ProcessIntersectionGeometry(
    const IntersectionGeometry& geometry,
    ComputeIntersectionsContext& context) {
  CHECK_LT(geometry.ThresholdIndex(), kNotFound);

  if (last_threshold_index_ != geometry.ThresholdIndex() ||
      last_is_visible_ != geometry.IsVisible()) {
    entries_.push_back(MakeGarbageCollected<IntersectionObserverEntry>(
        geometry, context.GetTimeStamp(*Observer()), Target()));
    Observer()->ReportUpdates(*this);
    last_threshold_index_ = geometry.ThresholdIndex();
    last_is_visible_ = geometry.IsVisible();
  }
}

}  // namespace blink
