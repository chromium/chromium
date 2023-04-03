// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace content {

// Please keep features in alphabetical order.

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kADPFForBrowserIOThread,
             "kADPFForBrowserIOThread",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kNavigationUpdatesChildViewsVisibility,
             "NavigationUpdatesChildViewsVisibility",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kOnShowWithPageVisibility,
             "OnShowWithPageVisibility",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOptimizeImmHideCalls,
             "OptimizeImmHideCalls",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kConsolidatedIPCForProxyCreation,
             "ConsolidatedIPCForProxyCreation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kQueueNavigationsWhileWaitingForCommit,
             "QueueNavigationsWhileWaitingForCommit",
             base::FEATURE_DISABLED_BY_DEFAULT);

static constexpr base::FeatureParam<NavigationQueueingFeatureLevel>::Option
    kNavigationQueueingFeatureLevels[] = {
        {NavigationQueueingFeatureLevel::kNone, "none"},
        {NavigationQueueingFeatureLevel::kAvoidRedundantCancellations,
         "avoid-redundant"},
        {NavigationQueueingFeatureLevel::kFull, "full"}};
const base::FeatureParam<NavigationQueueingFeatureLevel>
    kNavigationQueueingFeatureLevelParam{
        &kQueueNavigationsWhileWaitingForCommit, "level",
        NavigationQueueingFeatureLevel::kAvoidRedundantCancellations,
        &kNavigationQueueingFeatureLevels};

NavigationQueueingFeatureLevel GetNavigationQueueingFeatureLevel() {
  if (base::FeatureList::IsEnabled(kQueueNavigationsWhileWaitingForCommit)) {
    return kNavigationQueueingFeatureLevelParam.Get();
  }
  return NavigationQueueingFeatureLevel::kNone;
}

bool ShouldAvoidRedundantNavigationCancellations() {
  return GetNavigationQueueingFeatureLevel() >=
         NavigationQueueingFeatureLevel::kAvoidRedundantCancellations;
}

bool ShouldQueueNavigationsWhenPendingCommitRFHExists() {
  return GetNavigationQueueingFeatureLevel() ==
         NavigationQueueingFeatureLevel::kFull;
}

BASE_FEATURE(kRestrictCanAccessDataForOriginToUIThread,
             "RestrictCanAccessDataForOriginToUIThread",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSpeculativeServiceWorkerStartup,
             "SpeculativeServiceWorkerStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kUserLevelMemoryPressureSignalOn4GbDevices,
             "UserLevelMemoryPressureSignalOn4GbDevices",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kUserLevelMemoryPressureSignalOn6GbDevices,
             "UserLevelMemoryPressureSignalOn6GbDevices",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {
constexpr base::TimeDelta kDefaultMinimumInterval = base::Minutes(10);
}  // namespace

// Minimum time interval between generated memory pressure signals.
base::TimeDelta MinimumIntervalOfUserLevelMemoryPressureSignalOn4GbDevices() {
  static const base::FeatureParam<base::TimeDelta> kMinimumInterval{
      &kUserLevelMemoryPressureSignalOn4GbDevices, "minimum_interval",
      kDefaultMinimumInterval};
  return kMinimumInterval.Get();
}

base::TimeDelta MinimumIntervalOfUserLevelMemoryPressureSignalOn6GbDevices() {
  static const base::FeatureParam<base::TimeDelta> kMinimumInterval{
      &kUserLevelMemoryPressureSignalOn6GbDevices, "minimum_interval",
      kDefaultMinimumInterval};
  return kMinimumInterval.Get();
}

#endif  // BUILDFLAG(IS_ANDROID)

// Please keep features in alphabetical order.

}  // namespace content
