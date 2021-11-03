// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/back_forward_cache_disabling_feature_tracker.h"

namespace blink {
namespace scheduler {

BackForwardCacheDisablingFeatureTracker::
    BackForwardCacheDisablingFeatureTracker(
        TraceableVariableController* tracing_controller)
    : opted_out_from_back_forward_cache_{
          false, "FrameScheduler.OptedOutFromBackForwardCache",
          tracing_controller, YesNoStateToString} {}

void BackForwardCacheDisablingFeatureTracker::Reset() {
  for (const auto& it : back_forward_cache_disabling_feature_counts_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "renderer.scheduler", "ActiveSchedulerTrackedFeature",
        TRACE_ID_LOCAL(reinterpret_cast<intptr_t>(this) ^
                       static_cast<int>(it.first)));
  }

  back_forward_cache_disabling_feature_counts_.clear();
  back_forward_cache_disabling_features_.reset();
}

void BackForwardCacheDisablingFeatureTracker::Add(
    SchedulingPolicy::Feature feature) {
  ++back_forward_cache_disabling_feature_counts_[feature];
  back_forward_cache_disabling_features_.set(static_cast<size_t>(feature));
  opted_out_from_back_forward_cache_ = true;
}

void BackForwardCacheDisablingFeatureTracker::Remove(
    SchedulingPolicy::Feature feature) {
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

}  // namespace scheduler
}  // namespace blink
