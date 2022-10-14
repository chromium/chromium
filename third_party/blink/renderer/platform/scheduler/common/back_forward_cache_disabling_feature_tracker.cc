// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/back_forward_cache_disabling_feature_tracker.h"

#include "third_party/blink/renderer/platform/scheduler/common/thread_scheduler_base.h"

namespace blink {
namespace scheduler {

BackForwardCacheDisablingFeatureTracker::
    BackForwardCacheDisablingFeatureTracker(
        TraceableVariableController* tracing_controller,
        ThreadSchedulerBase* scheduler)
    : opted_out_from_back_forward_cache_{false,
                                         "FrameScheduler."
                                         "OptedOutFromBackForwardCache",
                                         tracing_controller,
                                         YesNoStateToString},
      scheduler_{scheduler} {}

void BackForwardCacheDisablingFeatureTracker::SetDelegate(
    FrameOrWorkerScheduler::Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
  // `delegate` might be nullptr on tests.
}

void BackForwardCacheDisablingFeatureTracker::Reset() {
  for (const auto& it : back_forward_cache_disabling_feature_counts_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "renderer.scheduler", "ActiveSchedulerTrackedFeature",
        TRACE_ID_LOCAL(reinterpret_cast<intptr_t>(this) ^
                       static_cast<int>(it.first)));
  }

  back_forward_cache_disabling_feature_counts_.clear();
  back_forward_cache_disabling_features_.reset();
  last_uploaded_bfcache_disabling_features_ = 0;
  non_sticky_features_and_js_locations_.clear();
  sticky_features_and_js_locations_.clear();
}

void BackForwardCacheDisablingFeatureTracker::AddFeatureInternal(
    SchedulingPolicy::Feature feature) {
  ++back_forward_cache_disabling_feature_counts_[feature];
  back_forward_cache_disabling_features_.set(static_cast<size_t>(feature));
  opted_out_from_back_forward_cache_ = true;

  NotifyDelegateAboutFeaturesAfterCurrentTask(
      BackForwardCacheDisablingFeatureTracker::TracingType::kBegin, feature);
}

void BackForwardCacheDisablingFeatureTracker::AddNonStickyFeature(
    SchedulingPolicy::Feature feature,
    std::unique_ptr<SourceLocation> source_location,
    FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle* handle) {
  AddFeatureInternal(feature);

  DCHECK(handle);
  non_sticky_features_and_js_locations_.push_back(
      handle->GetFeatureAndJSLocationBlockingBFCache());

  NotifyDelegateAboutFeaturesAfterCurrentTask(
      BackForwardCacheDisablingFeatureTracker::TracingType::kBegin, feature);
}

void BackForwardCacheDisablingFeatureTracker::AddStickyFeature(
    SchedulingPolicy::Feature feature,
    std::unique_ptr<SourceLocation> source_location) {
  AddFeatureInternal(feature);

  sticky_features_and_js_locations_.push_back(
      FeatureAndJSLocationBlockingBFCache(feature, source_location.get()));

  NotifyDelegateAboutFeaturesAfterCurrentTask(
      BackForwardCacheDisablingFeatureTracker::TracingType::kBegin, feature);
}

void BackForwardCacheDisablingFeatureTracker::Remove(
    FeatureAndJSLocationBlockingBFCache feature_and_js_location) {
  SchedulingPolicy::Feature feature = feature_and_js_location.Feature();

  DCHECK_GT(back_forward_cache_disabling_feature_counts_[feature], 0);
  auto it = back_forward_cache_disabling_feature_counts_.find(feature);
  if (it->second == 1) {
    back_forward_cache_disabling_feature_counts_.erase(it);
    back_forward_cache_disabling_features_.reset(static_cast<size_t>(feature));
  } else {
    --it->second;
  }
  opted_out_from_back_forward_cache_ =
      !back_forward_cache_disabling_feature_counts_.empty();

  wtf_size_t index =
      non_sticky_features_and_js_locations_.Find(feature_and_js_location);
  DCHECK(index != kNotFound);
  non_sticky_features_and_js_locations_.EraseAt(index);

  NotifyDelegateAboutFeaturesAfterCurrentTask(
      BackForwardCacheDisablingFeatureTracker::TracingType::kEnd, feature);
}

WTF::HashSet<SchedulingPolicy::Feature>
BackForwardCacheDisablingFeatureTracker::
    GetActiveFeaturesTrackedForBackForwardCacheMetrics() {
  WTF::HashSet<SchedulingPolicy::Feature> result;
  for (const auto& it : back_forward_cache_disabling_feature_counts_)
    result.insert(it.first);
  return result;
}

uint64_t BackForwardCacheDisablingFeatureTracker::
    GetActiveFeaturesTrackedForBackForwardCacheMetricsMask() const {
  auto result = back_forward_cache_disabling_features_.to_ullong();
  static_assert(static_cast<size_t>(SchedulingPolicy::Feature::kMaxValue) <
                    sizeof(result) * 8,
                "Number of the features should allow a bitmask to fit into "
                "64-bit integer");
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
  switch (tracing_type) {
    case TracingType::kBegin:
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
          "renderer.scheduler", "ActiveSchedulerTrackedFeature",
          TRACE_ID_LOCAL(reinterpret_cast<intptr_t>(this) ^
                         static_cast<int>(traced_feature)),
          "feature", FeatureToHumanReadableString(traced_feature));
      break;
    case TracingType::kEnd:
      TRACE_EVENT_NESTABLE_ASYNC_END0(
          "renderer.scheduler", "ActiveSchedulerTrackedFeature",
          TRACE_ID_LOCAL(reinterpret_cast<intptr_t>(this) ^
                         static_cast<int>(traced_feature)));
      break;
  }
}

void BackForwardCacheDisablingFeatureTracker::ReportFeaturesToDelegate() {
  feature_report_scheduled_ = false;

  uint64_t mask = GetActiveFeaturesTrackedForBackForwardCacheMetricsMask();
  if (mask == last_uploaded_bfcache_disabling_features_)
    return;
  last_uploaded_bfcache_disabling_features_ = mask;
  delegate_->UpdateBackForwardCacheDisablingFeatures(
      mask, non_sticky_features_and_js_locations_,
      sticky_features_and_js_locations_);
}

}  // namespace scheduler
}  // namespace blink
