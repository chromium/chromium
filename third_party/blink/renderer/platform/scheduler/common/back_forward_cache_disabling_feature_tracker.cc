// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/back_forward_cache_disabling_feature_tracker.h"

#include "third_party/blink/renderer/platform/scheduler/common/thread_scheduler_base.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

namespace blink {
namespace scheduler {

BackForwardCacheDisablingFeatureTracker::
    BackForwardCacheDisablingFeatureTracker(
        TraceableVariableController* tracing_controller,
        perfetto::Track parent_track,
        ThreadSchedulerBase* scheduler)
    : parent_track_(parent_track),
      opted_out_from_back_forward_cache_{
          false,
          perfetto::NamedTrack::FromPointer("FrameScheduler."
                                            "OptedOutFromBackForwardCache",
                                            this,
                                            parent_track_),
          tracing_controller, YesNoStateToString},
      scheduler_{scheduler} {}

void BackForwardCacheDisablingFeatureTracker::SetDelegate(
    FrameOrWorkerScheduler::Delegate* delegate) {
  // This function is only called when initializing. `delegate_` should be
  // nullptr at first.
  DCHECK(!delegate_);
  // `delegate` can be nullptr for tests.
  if (delegate) {
    delegate_ = (*delegate).AsWeakPtr();
  }
}

void BackForwardCacheDisablingFeatureTracker::Reset() {
  for (const auto& it : back_forward_cache_disabling_feature_counts_) {
    auto track = GetTrackForFeature(it.first);
    TRACE_EVENT_END("renderer.scheduler", track);
  }

  back_forward_cache_disabling_feature_counts_.clear();
  non_sticky_features_and_js_locations_.Clear();
  sticky_features_and_js_locations_.Clear();
  last_reported_non_sticky_.Clear();
  last_reported_sticky_.Clear();
}

void BackForwardCacheDisablingFeatureTracker::AddFeatureInternal(
    SchedulingPolicy::Feature feature) {
  if (back_forward_cache_disabling_feature_counts_[feature]++ == 0) {
    auto track = GetTrackForFeature(feature);
    TRACE_EVENT_BEGIN(
        "renderer.scheduler",
        perfetto::StaticString(FeatureToHumanReadableString(feature)), track);
  }
  opted_out_from_back_forward_cache_ = true;

  NotifyDelegateAboutFeaturesAfterCurrentTask(
      BackForwardCacheDisablingFeatureTracker::TracingType::kBegin, feature);
}

perfetto::NamedTrack
BackForwardCacheDisablingFeatureTracker::GetTrackForFeature(
    SchedulingPolicy::Feature traced_feature) const {
  return perfetto::NamedTrack(
      "ActiveSchedulerTrackedFeature",
      reinterpret_cast<intptr_t>(this) ^ static_cast<int>(traced_feature),
      parent_track_);
}

void BackForwardCacheDisablingFeatureTracker::AddNonStickyFeature(
    SchedulingPolicy::Feature feature,
    SourceLocation* source_location,
    FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle* handle) {
  DCHECK(!scheduler::IsFeatureSticky(feature));
  AddFeatureInternal(feature);

  DCHECK(handle);
  non_sticky_features_and_js_locations_.MaybeAdd(
      handle->GetFeatureAndJSLocationBlockingBFCache());

  NotifyDelegateAboutFeaturesAfterCurrentTask(
      BackForwardCacheDisablingFeatureTracker::TracingType::kBegin, feature);
}

void BackForwardCacheDisablingFeatureTracker::AddStickyFeature(
    SchedulingPolicy::Feature feature,
    SourceLocation* source_location) {
  DCHECK(scheduler::IsFeatureSticky(feature));
  AddFeatureInternal(feature);

  sticky_features_and_js_locations_.MaybeAdd(
      FeatureAndJSLocationBlockingBFCache(feature, source_location));

  NotifyDelegateAboutFeaturesAfterCurrentTask(
      BackForwardCacheDisablingFeatureTracker::TracingType::kBegin, feature);
}

void BackForwardCacheDisablingFeatureTracker::Remove(
    FeatureAndJSLocationBlockingBFCache feature_and_js_location) {
  SchedulingPolicy::Feature feature = feature_and_js_location.Feature();

  DCHECK_GT(back_forward_cache_disabling_feature_counts_[feature], 0);
  auto it = back_forward_cache_disabling_feature_counts_.find(feature);
  if (it->second == 1) {
    TRACE_EVENT_END("renderer.scheduler", GetTrackForFeature(feature));
    back_forward_cache_disabling_feature_counts_.erase(it);
  } else {
    --it->second;
  }
  opted_out_from_back_forward_cache_ =
      !back_forward_cache_disabling_feature_counts_.empty();

  non_sticky_features_and_js_locations_.Erase(feature_and_js_location);

  NotifyDelegateAboutFeaturesAfterCurrentTask(
      BackForwardCacheDisablingFeatureTracker::TracingType::kEnd, feature);
}

HashSet<SchedulingPolicy::Feature> BackForwardCacheDisablingFeatureTracker::
    GetActiveFeaturesTrackedForBackForwardCacheMetrics() {
  HashSet<SchedulingPolicy::Feature> result;
  for (const auto& it : back_forward_cache_disabling_feature_counts_) {
    result.insert(it.first);
  }
  return result;
}

BFCacheBlockingFeatureAndLocations& BackForwardCacheDisablingFeatureTracker::
    GetActiveNonStickyFeaturesTrackedForBackForwardCache() {
  return non_sticky_features_and_js_locations_;
}

const BFCacheBlockingFeatureAndLocations&
BackForwardCacheDisablingFeatureTracker::
    GetActiveStickyFeaturesTrackedForBackForwardCache() const {
  return sticky_features_and_js_locations_;
}

void BackForwardCacheDisablingFeatureTracker::
    NotifyDelegateAboutFeaturesAfterCurrentTask(
        TracingType tracing_type,
        SchedulingPolicy::Feature traced_feature) {
  if (delegate_ && scheduler_ && !feature_report_scheduled_) {
    // To avoid IPC flooding by updating multiple features in one task, upload
    // the tracked feature as one IPC after the current task finishes.
    scheduler_->ExecuteAfterCurrentTask(base::BindOnce(
        &BackForwardCacheDisablingFeatureTracker::ReportFeaturesToDelegate,
        weak_factory_.GetWeakPtr()));
  }
}

void BackForwardCacheDisablingFeatureTracker::ReportFeaturesToDelegate() {
  feature_report_scheduled_ = false;

  if (non_sticky_features_and_js_locations_ == last_reported_non_sticky_ &&
      sticky_features_and_js_locations_ == last_reported_sticky_) {
    return;
  }
  last_reported_non_sticky_ = non_sticky_features_and_js_locations_;
  last_reported_sticky_ = sticky_features_and_js_locations_;
  FrameOrWorkerScheduler::Delegate::BlockingDetails details(
      non_sticky_features_and_js_locations_, sticky_features_and_js_locations_);

  // Check if the delegate still exists. This check is necessary because
  // `FrameOrWorkerScheduler::Delegate` might be destroyed and thus `delegate_`
  // might be gone when `ReportFeaturesToDelegate() is executed.
  if (delegate_) {
    delegate_->UpdateBackForwardCacheDisablingFeatures(details);
  }
}

}  // namespace scheduler
}  // namespace blink
