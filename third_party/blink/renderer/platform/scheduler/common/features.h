// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {
namespace scheduler {

BASE_FEATURE(kDedicatedWorkerThrottling,
             "BlinkSchedulerWorkerThrottling",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBestEffortPriorityForFindInPage,
             "BlinkSchedulerBestEffortPriorityForFindInPage",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the priority of background (with no audio) pages'
// task queues to low priority.
BASE_FEATURE(kLowPriorityForBackgroundPages,
             "BlinkSchedulerLowPriorityForBackgroundPages",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the priority of background (with no audio) pages'
// task queues to best effort.
BASE_FEATURE(kBestEffortPriorityForBackgroundPages,
             "BlinkSchedulerBestEffortPriorityForBackgroundPages",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the priority of sub-frame task queues to low
// priority.
BASE_FEATURE(kLowPriorityForSubFrame,
             "BlinkSchedulerLowPriorityForSubFrame",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the priority of throttleable task queues to
// low priority.
BASE_FEATURE(kLowPriorityForThrottleableTask,
             "BlinkSchedulerLowPriorityForThrottleableTask",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the priority of sub-frame throttleable
// task queues to low priority.
BASE_FEATURE(kLowPriorityForSubFrameThrottleableTask,
             "BlinkSchedulerLowPriorityForSubFrameThrottleableTask",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the priority of hidden frame task queues to
// low priority.
BASE_FEATURE(kLowPriorityForHiddenFrame,
             "BlinkSchedulerLowPriorityForHiddenFrame",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the priority of an ad frame to low priority.
BASE_FEATURE(kLowPriorityForAdFrame,
             "BlinkSchedulerLowPriorityForAdFrame",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the priority of an ad frame to best effort priority.
BASE_FEATURE(kBestEffortPriorityForAdFrame,
             "BlinkSchedulerBestEffortPriorityForAdFrame",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables setting the priority of cross-origin task queues to
// low priority.
BASE_FEATURE(kLowPriorityForCrossOrigin,
             "BlinkSchedulerLowPriorityForCrossOrigin",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Prioritizes loading and compositing tasks while loading.
BASE_FEATURE(kPrioritizeCompositingAndLoadingDuringEarlyLoading,
             "PrioritizeCompositingAndLoadingDuringEarlyLoading",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Prioritizes one BeginMainFrame after input.
BASE_FEATURE(kPrioritizeCompositingAfterInput,
             "PrioritizeCompositingAfterInput",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable setting high priority database task type from field trial parameters.
BASE_FEATURE(kHighPriorityDatabaseTaskType,
             "HighPriorityDatabaseTaskType",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When features::kIntensiveWakeUpThrottling is enabled, wake ups from timers
// with a high nesting level are limited to 1 per minute on a page that has been
// backgrounded for GetIntensiveWakeUpThrottlingGracePeriod().
//
// Intensive wake up throttling is enforced in addition to other throttling
// mechanisms:
//  - 1 wake up per second in a background page or hidden cross-origin frame
//  - 1% CPU time in a page that has been backgrounded for 10 seconds
//
// Feature tracking bug: https://crbug.com/1075553
//
// Note that features::kIntensiveWakeUpThrottling should not be read from;
// rather the provided accessors should be used, which also take into account
// the managed policy override of the feature.
//
// Parameter name and default values, exposed for testing.
constexpr int kIntensiveWakeUpThrottling_GracePeriodSeconds_Default = 5 * 60;
constexpr int kIntensiveWakeUpThrottling_GracePeriodSecondsLoaded_Default = 60;

// Exposed so that multiple tests can tinker with the policy override.
PLATFORM_EXPORT void
ClearIntensiveWakeUpThrottlingPolicyOverrideCacheForTesting();
// Determines if the feature is enabled, taking into account base::Feature
// settings and policy overrides.
PLATFORM_EXPORT bool IsIntensiveWakeUpThrottlingEnabled();
// Grace period after hiding a page during which there is no intensive wake up
// throttling for the kIntensiveWakeUpThrottling feature.
// |loading| is the loading state of the page, used to determine if the grace
// period should be overwritten when kQuickIntensiveWakeUpThrottlingAfterLoading
// is enabled.
PLATFORM_EXPORT base::TimeDelta GetIntensiveWakeUpThrottlingGracePeriod(
    bool loading);

// If enabled, base::SingleThreadTaskRunner::GetCurrentDefault() and
// base::SequencedTaskRunner::GetCurrentDefault() returns the current active
// per-ASG task runner instead of the per-thread task runner.
BASE_FEATURE(kMbiOverrideTaskRunnerHandle,
             "MbiOverrideTaskRunnerHandle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, per-AgentGroupScheduler CompositorTaskRunner will be used instead
// of per-MainThreadScheduler CompositorTaskRunner.
BASE_FEATURE(kMbiCompositorTaskRunnerPerAgentSchedulingGroup,
             "MbiCompositorTaskRunnerPerAgentSchedulingGroup",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature to experiment with different values for: "prioritize main thread
// compositing tasks if we haven't done a main frame in this many milliseconds."
PLATFORM_EXPORT BASE_DECLARE_FEATURE(kPrioritizeCompositingAfterDelayTrials);

// Interval between Javascript timer wake ups when the "ThrottleForegroundTimers"
// feature is enabled.
PLATFORM_EXPORT base::TimeDelta GetForegroundTimersThrottledWakeUpInterval();

// Finch flag for preventing rendering starvation during threaded scrolling.
// With this feature enabled, the existing delay-based rendering anti-starvation
// applies, and the compositor task queue priority is controlled with the
// `kCompositorTQPolicyDuringThreadedScroll` `FeatureParam`.
PLATFORM_EXPORT BASE_DECLARE_FEATURE(kThreadedScrollPreventRenderingStarvation);

enum class CompositorTQPolicyDuringThreadedScroll {
  // Compositor TQ has low priority, delay-based anti-starvation does not apply.
  // This is the current behavior and it isn't exposed through
  // `kCompositorTQPolicyDuringThreadedScrollOptions`; this exists to simplify
  // the relayed policy logic.
  kLowPriorityAlways,
  // Compositor TQ has low priority, delay-based anti-starvation applies.
  kLowPriorityWithAntiStarvation,
  // Compositor TQ has normal priority, delay-based anti-starvation applies.
  kNormalPriorityWithAntiStarvation,
  // Compositor TQ has very high priority. Note that this is the same priority
  // as used by the delay-based anti-starvation logic.
  kVeryHighPriorityAlways,
};

PLATFORM_EXPORT extern const base::FeatureParam<
    CompositorTQPolicyDuringThreadedScroll>::Option
    kCompositorTQPolicyDuringThreadedScrollOptions[];

PLATFORM_EXPORT extern const base::FeatureParam<
    CompositorTQPolicyDuringThreadedScroll>
    kCompositorTQPolicyDuringThreadedScroll;

BASE_FEATURE(kRejectedPromisesPerWindowAgent,
             "BlinkSchedulerRejectedPromisesPerWindowAgent",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMicrotaskQueuePerWindowAgent,
             "BlinkSchedulerMicroTaskQueuePerWindowAgent",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMicrotaskQueuePerPaintWorklet,
             "BlinkSchedulerMicroTaskQueuePerPaintWorklet",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMicrotaskQueuePerAnimationWorklet,
             "BlinkSchedulerMicroTaskQueuePerAnimationWorklet",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMicrotaskQueuePerAudioWorklet,
             "BlinkSchedulerMicroTaskQueuePerAudioWorklet",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMicrotaskQueuePerWorkerAgent,
             "BlinkSchedulerMicroTaskQueuePerWorkerAgent",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_FEATURES_H_
