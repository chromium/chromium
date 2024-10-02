// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/features.h"

#include "base/command_line.h"
#include "components/miracle_parameter/common/public/miracle_parameter.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"

namespace blink {
namespace scheduler {

namespace {

enum class PolicyOverride { kNoOverride, kForceDisable, kForceEnable };

bool g_intensive_wake_up_throttling_policy_override_cached_ = false;

// Returns the IntensiveWakeUpThrottling policy settings. This is checked once
// on first access and cached. Note that that this is *not* thread-safe!
PolicyOverride GetIntensiveWakeUpThrottlingPolicyOverride() {
  static PolicyOverride policy = PolicyOverride::kNoOverride;
  if (g_intensive_wake_up_throttling_policy_override_cached_)
    return policy;

  // Otherwise, check the command-line. Only values of "0" and "1" are valid,
  // anything else is ignored (and allows the base::Feature to control the
  // feature). This slow path will only be hit once per renderer process.
  g_intensive_wake_up_throttling_policy_override_cached_ = true;
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kIntensiveWakeUpThrottlingPolicy);
  if (value == switches::kIntensiveWakeUpThrottlingPolicy_ForceEnable) {
    policy = PolicyOverride::kForceEnable;
  } else if (value == switches::kIntensiveWakeUpThrottlingPolicy_ForceDisable) {
    policy = PolicyOverride::kForceDisable;
  } else {
    // Necessary in testing configurations, as the policy can be parsed
    // repeatedly.
    policy = PolicyOverride::kNoOverride;
  }

  return policy;
}

}  // namespace

void ClearIntensiveWakeUpThrottlingPolicyOverrideCacheForTesting() {
  g_intensive_wake_up_throttling_policy_override_cached_ = false;
}

bool IsIntensiveWakeUpThrottlingEnabled() {
  // If policy is present then respect it.
  auto policy = GetIntensiveWakeUpThrottlingPolicyOverride();
  if (policy != PolicyOverride::kNoOverride)
    return policy == PolicyOverride::kForceEnable;
  // Otherwise respect the base::Feature.
  return base::FeatureList::IsEnabled(features::kIntensiveWakeUpThrottling);
}

// If a policy override is specified then stick to the published defaults so
// that admins get consistent behaviour that clients can't override. Otherwise
// use the base::FeatureParams.

base::TimeDelta GetIntensiveWakeUpThrottlingGracePeriod(bool loading) {
  // Controls the time that elapses after a page is backgrounded before the
  // throttling policy takes effect.
  static const base::FeatureParam<int>
      kIntensiveWakeUpThrottling_GracePeriodSeconds{
          &features::kIntensiveWakeUpThrottling,
          features::kIntensiveWakeUpThrottling_GracePeriodSeconds_Name,
          kIntensiveWakeUpThrottling_GracePeriodSeconds_Default};

  // Controls the grace period for loaded pages.
  static const base::FeatureParam<int>
      kIntensiveWakeUpThrottling_GracePeriodSeconds_Loaded{
          &features::kQuickIntensiveWakeUpThrottlingAfterLoading,
          "grace_period_seconds_loaded",
          kIntensiveWakeUpThrottling_GracePeriodSecondsLoaded_Default};

  int seconds = kIntensiveWakeUpThrottling_GracePeriodSeconds_Default;
  if (GetIntensiveWakeUpThrottlingPolicyOverride() ==
      PolicyOverride::kNoOverride) {
    seconds = kIntensiveWakeUpThrottling_GracePeriodSeconds.Get();
    if (!loading && base::FeatureList::IsEnabled(
                        features::kQuickIntensiveWakeUpThrottlingAfterLoading))
      seconds = kIntensiveWakeUpThrottling_GracePeriodSeconds_Loaded.Get();
  }
  return base::Seconds(seconds);
}

// TODO(crbug.com/1475915): convert this param value to TimeDelta instead of int
// after the experiment.
MIRACLE_PARAMETER_FOR_INT(
    GetLoadingPhaseBufferTimeAfterFirstMeaningfulPaintMillis,
    features::kLoadingPhaseBufferTimeAfterFirstMeaningfulPaint,
    "LoadingPhaseBufferTimeAfterFirstMeaningfulPaintMillis",
    0)

base::TimeDelta GetLoadingPhaseBufferTimeAfterFirstMeaningfulPaint() {
  return base::Milliseconds(
      GetLoadingPhaseBufferTimeAfterFirstMeaningfulPaintMillis());
}

base::TimeDelta GetThreadedScrollRenderingStarvationThreshold() {
  static const base::FeatureParam<int>
      kThreadedScrollRenderingStarvationThreshold{
          &features::kThreadedScrollPreventRenderingStarvation, "threshold_ms",
          100};
  if (base::FeatureList::IsEnabled(
          features::kThreadedScrollPreventRenderingStarvation)) {
    return base::Milliseconds(
        kThreadedScrollRenderingStarvationThreshold.Get());
  }
  return base::TimeDelta::Max();
}

BASE_FEATURE(kPrioritizeCompositingAfterDelayTrials,
             "PrioritizeCompositingAfterDelayTrials",
             base::FEATURE_DISABLED_BY_DEFAULT);


}  // namespace scheduler
}  // namespace blink
