// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_BACK_FORWARD_CACHE_DISABLING_FEATURE_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_BACK_FORWARD_CACHE_DISABLING_FEATURE_TRACKER_H_

#include <bitset>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/platform/scheduler/common/tracing_helper.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {
namespace scheduler {

class ThreadSchedulerImpl;

// Keeps track of feature usage that disables back/forward cache.
//
// This tracks SchedulingPolicy::Feature values. See SchedulingPolicy::Feature
// for the list of features and the meaning of individual features.
//
// This class tracks features that are used on the renderer side, e.g.,
// IndexedDB transactions. The tracked info is sent to the browser side to be
// combined with the features that are used on the browser to be used to
// determine back-forward cache eligibility.
class PLATFORM_EXPORT BackForwardCacheDisablingFeatureTracker {
 public:
  // `tracing_controller` and `scheduler` must not be null and must outlive this
  // instance except for tests.
  BackForwardCacheDisablingFeatureTracker(
      TraceableVariableController* tracing_controller,
      ThreadSchedulerImpl* scheduler);

  // Sets the delegate to notify the feature usage update. This must be called
  // only once for initialization. `delegate` must not be null and must outlive
  // except for tests.
  void SetDelegate(FrameOrWorkerScheduler::Delegate* delegate);

  // Resets the feature-usage counters.
  void Reset();

  // Called when a usage of |feature| is added.
  void Add(SchedulingPolicy::Feature feature);

  // Called when one usage of |feature| is removed.
  void Remove(SchedulingPolicy::Feature feature);

  // Gets a hash set of feature usages for metrics.
  WTF::HashSet<SchedulingPolicy::Feature>
  GetActiveFeaturesTrackedForBackForwardCacheMetrics();

  // Gets a hash set of feature usages for metrics as a bitmap.
  uint64_t GetActiveFeaturesTrackedForBackForwardCacheMetricsMask() const;

  // Notifies the delegate about the change in the set of active features.
  // The scheduler calls this function when needed after each task finishes,
  // grouping multiple OnStartedUsingFeature/OnStoppedUsingFeature into
  // one call to the delegate (which is generally expected to upload them to
  // the browser process).
  // No calls will be issued to the delegate if the set of features didn't
  // change since the previous call.
  void ReportFeaturesToDelegate();

 private:
  enum class TracingType {
    kBegin,
    kEnd,
  };

  void NotifyDelegateAboutFeaturesAfterCurrentTask(
      TracingType tracing_type,
      SchedulingPolicy::Feature traced_feature);

  base::flat_map<SchedulingPolicy::Feature, int>
      back_forward_cache_disabling_feature_counts_{};
  std::bitset<static_cast<size_t>(SchedulingPolicy::Feature::kMaxValue) + 1>
      back_forward_cache_disabling_features_{};
  TraceableState<bool, TracingCategory::kInfo>
      opted_out_from_back_forward_cache_;

  // The last set of features passed to FrameOrWorkerScheduler::Delegate::
  // UpdateBackForwardCacheDisablingFeatures.
  uint64_t last_uploaded_bfcache_disabling_features_ = 0;
  bool feature_report_scheduled_ = false;

  FrameOrWorkerScheduler::Delegate* delegate_ = nullptr;
  ThreadSchedulerImpl* scheduler_;

  base::WeakPtrFactory<BackForwardCacheDisablingFeatureTracker> weak_factory_{
      this};
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_BACK_FORWARD_CACHE_DISABLING_FEATURE_TRACKER_H_
