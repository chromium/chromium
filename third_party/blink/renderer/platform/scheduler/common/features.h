// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {
namespace scheduler {

const base::Feature kHighPriorityInputOnMainThread{
    "BlinkSchedulerHighPriorityInput", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHighPriorityInputOnCompositorThread{
    "BlinkSchedulerHighPriorityInputOnCompositorThread",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDedicatedWorkerThrottling{
    "BlinkSchedulerWorkerThrottling", base::FEATURE_DISABLED_BY_DEFAULT};

// COMPOSITING PRIORITY EXPERIMENT CONTROLS

// Enables experiment to increase priority of the compositing tasks during
// input handling. Other features in this section do not have any effect
// when this feature is disabled.
const base::Feature kPrioritizeCompositingAfterInput{
    "BlinkSchedulerPrioritizeCompositingAfterInput",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Use kHighestPriority for compositing tasks during the experiment.
// kHighPriority is used otherwise.
const base::Feature kHighestPriorityForCompositingAfterInput{
    "BlinkSchedulerHighestPriorityForCompostingAfterInput",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, MainFrameSchedulerImpl::OnRequestMainFrameForInput is used as
// triggering signal for the experiment. If disabled, the presence of an input
// task is used as trigger.
const base::Feature kUseExplicitSignalForTriggeringCompositingPrioritization{
    "BlinkSchedulerUseExplicitSignalForTriggeringCompositingPrioritization",
    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the increased priority continues until we get the appropriate
// number of WillBeginMainFrame signals. If disabled, the priority is increased
// for the fixed number of compositing tasks.
const base::Feature kUseWillBeginMainFrameForCompositingPrioritization{
    "BlinkSchedulerUseWillBeginMainFrameForCompositingPrioritization",
    base::FEATURE_DISABLED_BY_DEFAULT};

// LOAD PRIORITY EXPERIMENT CONTROLS

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

// Enable setting throttleable and freezable task types from field trial
// parameters.
const base::Feature kThrottleAndFreezeTaskTypes{
    "ThrottleAndFreezeTaskTypes", base::FEATURE_DISABLED_BY_DEFAULT};

// Parameters for |kThrottleAndFreezeTaskTypes|.
extern const char PLATFORM_EXPORT kThrottleableTaskTypesListParam[];
extern const char PLATFORM_EXPORT kFreezableTaskTypesListParam[];

// https://crbug.com/874836: Experiment-controlled removal of input heuristics.
// Expensive task blocking as a part of input handling heuristics, so disabling
// input heuristics implicitly disables expensive task blocking. Expensive task
// blocking is tested separately as it's less risky. Touchstart and
// non-touchstart input heuristics are separated because non-touchstart are
// seen as less ricky.
const base::Feature kDisableExpensiveTaskBlocking{
    "BlinkSchedulerDisableExpensiveTaskBlocking",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kDisableNonTouchstartInputHeuristics{
    "BlinkSchedulerDisableNonTouchstartInputHeuristics",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kDisableTouchstartInputHeuristics{
    "BlinkSchedulerDisableTouchstartInputHeuristics",
    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_COMMON_FEATURES_H_
