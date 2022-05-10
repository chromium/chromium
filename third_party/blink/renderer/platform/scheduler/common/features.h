// Copyright 2018 The Chromium Authors. All rights reserved.
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

const base::Feature kDedicatedWorkerThrottling{
    "BlinkSchedulerWorkerThrottling", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBestEffortPriorityForFindInPage{
    "BlinkSchedulerBestEffortPriorityForFindInPage",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of background (with no audio) pages'
// task queues to low priority.
const base::Feature kLowPriorityForBackgroundPages{
    "BlinkSchedulerLowPriorityForBackgroundPages",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of background (with no audio) pages'
// task queues to best effort.
const base::Feature kBestEffortPriorityForBackgroundPages{
    "BlinkSchedulerBestEffortPriorityForBackgroundPages",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of sub-frame task queues to low
// priority.
const base::Feature kLowPriorityForSubFrame{
    "BlinkSchedulerLowPriorityForSubFrame", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of throttleable task queues to
// low priority.
const base::Feature kLowPriorityForThrottleableTask{
    "BlinkSchedulerLowPriorityForThrottleableTask",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of sub-frame throttleable
// task queues to low priority.
const base::Feature kLowPriorityForSubFrameThrottleableTask{
    "BlinkSchedulerLowPriorityForSubFrameThrottleableTask",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of hidden frame task queues to
// low priority.
const base::Feature kLowPriorityForHiddenFrame{
    "BlinkSchedulerLowPriorityForHiddenFrame",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Used along with |kLowPriorityForHiddenFrame|,
// |kLowPriorityForSubFrameThrottleableTask|, |kLowPriorityForThrottleableTask|,
// |kLowPriorityForSubFrame| to enable one of these experiments only during the
// load use case.
const base::Feature kFrameExperimentOnlyWhenLoading{
    "BlinkSchedulerFrameExperimentOnlyWhenLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of an ad frame to low priority.
const base::Feature kLowPriorityForAdFrame{
    "BlinkSchedulerLowPriorityForAdFrame", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of an ad frame to best effort priority.
const base::Feature kBestEffortPriorityForAdFrame{
    "BlinkSchedulerBestEffortPriorityForAdFrame",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Used along with |kLowPriorityForAdFrame| or |kBestEffortPriorityForAdFrame|
// to enable one of these experiments only during the load use case.
const base::Feature kAdFrameExperimentOnlyWhenLoading{
    "BlinkSchedulerAdFrameExperimentOnlyWhenLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables using a resource's fetch priority to determine the priority of the
// resource's loading tasks posted to blink's scheduler.
const base::Feature kUseResourceFetchPriority{
    "BlinkSchedulerResourceFetchPriority", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables using a resource's fetch priority to determine the priority of the
// resource's loading tasks posted to blink's scheduler only for resources
// requested during the loading phase.
const base::Feature kUseResourceFetchPriorityOnlyWhenLoading{
    "BlinkSchedulerResourceFetchPriorityOnlyWhenLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of cross-origin task queues to
// low priority.
const base::Feature kLowPriorityForCrossOrigin{
    "BlinkSchedulerLowPriorityForCrossOrigin",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the priority of cross-origin task queues to
// low priority during loading only.
const base::Feature kLowPriorityForCrossOriginOnlyWhenLoading{
    "BlinkSchedulerLowPriorityForCrossOriginOnlyWhenLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Prioritizes loading and compositing tasks while loading.
const base::Feature kPrioritizeCompositingAndLoadingDuringEarlyLoading{
    "PrioritizeCompositingAndLoadingDuringEarlyLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Prioritizes one BeginMainFrame after input.
const base::Feature kPrioritizeCompositingAfterInput{
    "PrioritizeCompositingAfterInput", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable setting high priority database task type from field trial parameters.
const base::Feature kHighPriorityDatabaseTaskType{
    "HighPriorityDatabaseTaskType", base::FEATURE_DISABLED_BY_DEFAULT};

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
constexpr int kIntensiveWakeUpThrottling_GracePeriodSeconds_Loaded = 10;

// If enabled, the grace period of features::kIntensiveWakeUpThrottling will be
// |kIntensiveWakeUpThrottling_GracePeriodSeconds_Loaded| when a background page
// is loaded.
const base::Feature kQuickIntensiveWakeUpThrottlingAfterLoading{
    "QuickIntensiveWakeUpThrottlingAfterLoading",
    base::FEATURE_DISABLED_BY_DEFAULT};

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

// If enabled, base::ThreadTaskRunnerHandle::Get() and
// base::SequencedTaskRunnerHandle::Get() returns the current active
// per-ASG task runner instead of the per-thread task runner.
const base::Feature kMbiOverrideTaskRunnerHandle{
    "MbiOverrideTaskRunnerHandle", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, per-AgentGroupScheduler CompositorTaskRunner will be used instead
// of per-MainThreadScheduler CompositorTaskRunner.
const base::Feature kMbiCompositorTaskRunnerPerAgentSchedulingGroup{
    "MbiCompositorTaskRunnerPerAgentSchedulingGroup",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Interval between Javascript timer wake ups when the "ThrottleForegroundTimers"
// feature is enabled.
PLATFORM_EXPORT base::TimeDelta GetForegroundTimersThrottledWakeUpInterval();

// Deprioritizes JS timer tasks during a particular phase of page loading.
PLATFORM_EXPORT extern const base::Feature
    kDeprioritizeDOMTimersDuringPageLoading;

// The phase in which we deprioritize JS timer tasks.
enum class DeprioritizeDOMTimersPhase {
  // Until the DOMContentLoaded event is fired.
  kOnDOMContentLoaded,
  // Until First Contentful Paint is reached.
  kFirstContentfulPaint,
  // Until the load event is fired.
  kOnLoad,
};

PLATFORM_EXPORT extern const base::FeatureParam<
    DeprioritizeDOMTimersPhase>::Option kDeprioritizeDOMTimersPhaseOptions[];

PLATFORM_EXPORT extern const base::FeatureParam<DeprioritizeDOMTimersPhase>
    kDeprioritizeDOMTimersPhase;

// Killswitch for prioritizing cross-process postMessage forwarding.
//
// TODO(crbug.com/1212894): Remove after M95.
const base::Feature kDisablePrioritizedPostMessageForwarding{
    "DisablePrioritizedPostMessageForwarding",
    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_FEATURES_H_
