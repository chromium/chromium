// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/features.h"

#include "base/command_line.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace scheduler {

namespace {

enum class PolicyOverride { NO_OVERRIDE, FORCE_DISABLE, FORCE_ENABLE };

bool g_intensive_wake_up_throttling_policy_override_cached_ = false;

// Returns the IntensiveWakeUpThrottling policy settings. This is checked once
// on first access and cached. Note that that this is *not* thread-safe!
PolicyOverride GetIntensiveWakeUpThrottlingPolicyOverride() {
  static PolicyOverride policy = PolicyOverride::NO_OVERRIDE;
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
    policy = PolicyOverride::FORCE_ENABLE;
  } else if (value == switches::kIntensiveWakeUpThrottlingPolicy_ForceDisable) {
    policy = PolicyOverride::FORCE_DISABLE;
  } else {
    // Necessary in testing configurations, as the policy can be parsed
    // repeatedly.
    policy = PolicyOverride::NO_OVERRIDE;
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
  if (policy != PolicyOverride::NO_OVERRIDE)
    return policy == PolicyOverride::FORCE_ENABLE;
  // Otherwise respect the base::Feature.
  return base::FeatureList::IsEnabled(features::kIntensiveWakeUpThrottling);
}

// If a policy override is specified then stick to the published defaults so
// that admins get consistent behaviour that clients can't override. Otherwise
// use the base::FeatureParams.

base::TimeDelta GetIntensiveWakeUpThrottlingDurationBetweenWakeUps() {
  DCHECK(IsIntensiveWakeUpThrottlingEnabled());

  // Controls the period during which at most 1 wake up from throttleable
  // TaskQueues in a page can take place.
  static const base::FeatureParam<int>
      kIntensiveWakeUpThrottling_DurationBetweenWakeUpsSeconds{
          &features::kIntensiveWakeUpThrottling,
          kIntensiveWakeUpThrottling_DurationBetweenWakeUpsSeconds_Name,
          kIntensiveWakeUpThrottling_DurationBetweenWakeUpsSeconds_Default};

  int seconds =
      kIntensiveWakeUpThrottling_DurationBetweenWakeUpsSeconds_Default;
  if (GetIntensiveWakeUpThrottlingPolicyOverride() ==
      PolicyOverride::NO_OVERRIDE) {
    seconds = kIntensiveWakeUpThrottling_DurationBetweenWakeUpsSeconds.Get();
  }
  return base::TimeDelta::FromSeconds(seconds);
}

base::TimeDelta GetIntensiveWakeUpThrottlingGracePeriod() {
  DCHECK(IsIntensiveWakeUpThrottlingEnabled());

  // Controls the time that elapses after a page is backgrounded before the
  // throttling policy takes effect.
  static const base::FeatureParam<int>
      kIntensiveWakeUpThrottling_GracePeriodSeconds{
          &features::kIntensiveWakeUpThrottling,
          features::kIntensiveWakeUpThrottling_GracePeriodSeconds_Name,
          kIntensiveWakeUpThrottling_GracePeriodSeconds_Default};

  int seconds = kIntensiveWakeUpThrottling_GracePeriodSeconds_Default;
  if (GetIntensiveWakeUpThrottlingPolicyOverride() ==
      PolicyOverride::NO_OVERRIDE) {
    seconds = kIntensiveWakeUpThrottling_GracePeriodSeconds.Get();
  }
  return base::TimeDelta::FromSeconds(seconds);
}

base::TimeDelta GetTimeToInhibitIntensiveThrottlingOnTitleOrFaviconUpdate() {
  DCHECK(IsIntensiveWakeUpThrottlingEnabled());

  constexpr int kDefaultSeconds = 3;

  static const base::FeatureParam<int> kFeatureParam{
      &features::kIntensiveWakeUpThrottling,
      "inhibit_seconds_on_title_or_favicon_update_seconds", kDefaultSeconds};

  int seconds = kDefaultSeconds;
  if (GetIntensiveWakeUpThrottlingPolicyOverride() ==
      PolicyOverride::NO_OVERRIDE) {
    seconds = kFeatureParam.Get();
  }

  return base::TimeDelta::FromSeconds(seconds);
}

bool CanIntensivelyThrottleLowNestingLevel() {
  DCHECK(IsIntensiveWakeUpThrottlingEnabled());

  static const base::FeatureParam<bool> kFeatureParam{
      &features::kIntensiveWakeUpThrottling,
      kIntensiveWakeUpThrottling_CanIntensivelyThrottleLowNestingLevel_Name,
      kIntensiveWakeUpThrottling_CanIntensivelyThrottleLowNestingLevel_Default};

  bool value =
      kIntensiveWakeUpThrottling_CanIntensivelyThrottleLowNestingLevel_Default;
  if (GetIntensiveWakeUpThrottlingPolicyOverride() ==
      PolicyOverride::NO_OVERRIDE) {
    value = kFeatureParam.Get();
  }

  return value;
}

}  // namespace scheduler
}  // namespace blink
