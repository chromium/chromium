// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_features.h"

#include "base/feature_list.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace features {

BASE_FEATURE(PrefetchTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

// 4MiB, 2**20 * 4.
const base::FeatureParam<int> kPrefetchReusableBodySizeLimit{
    &kPrefetchTesting, "kPrefetchReusableBodySizeLimit", 4194304};

BASE_FEATURE(PrefetchUseContentRefactor,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PrefetchNIKScope,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(PrefetchClientHints,
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<PrefetchClientHintsCrossSiteBehavior>::Option
    kPrefetchClientHintsCrossSiteBehaviorOptions[] = {
        {PrefetchClientHintsCrossSiteBehavior::kNone, "none"},
        {PrefetchClientHintsCrossSiteBehavior::kLowEntropy, "low_entropy"},
        {PrefetchClientHintsCrossSiteBehavior::kAll, "all"},
};
const base::FeatureParam<PrefetchClientHintsCrossSiteBehavior>
    kPrefetchClientHintsCrossSiteBehavior{
        &kPrefetchClientHints, "cross_site_behavior",
        PrefetchClientHintsCrossSiteBehavior::kLowEntropy,
        &kPrefetchClientHintsCrossSiteBehaviorOptions};

BASE_FEATURE(PrefetchStateContaminationMitigation,
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool>
    kPrefetchStateContaminationSwapsBrowsingContextGroup{
        &kPrefetchStateContaminationMitigation, "swaps_bcg", true};

BASE_FEATURE(PrefetchServiceWorkerNoFetchHandlerFix,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PrefetchNetworkPriorityForEmbedders,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PrefetchBumpNetworkPriorityAfterBeingServed,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PrefetchServiceWorker,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPrefetchServiceWorkerEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(kPrefetchServiceWorker) &&
         content::GetContentClient()->browser()->IsPrefetchWithServiceWorkerAllowed(
             browser_context);
}

BASE_FEATURE(PrefetchBrowsingDataRemoval,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PrefetchScheduler,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kPrefetchSchedulerProgressSyncBestEffort{
    &kPrefetchScheduler, "kPrefetchSchedulerProgressSyncBestEffort", true};

BASE_FEATURE(PrefetchSchedulerTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<size_t>
    kPrefetchSchedulerTestingActiveSetSizeLimitForBase{
        &kPrefetchSchedulerTesting,
        "kPrefetchSchedulerTestingActiveSetSizeLimitForBase", 1};
const base::FeatureParam<size_t>
    kPrefetchSchedulerTestingActiveSetSizeLimitForBurst{
        &kPrefetchSchedulerTesting,
        "kPrefetchSchedulerTestingActiveSetSizeLimitForBurst", 1};

BASE_FEATURE(PrefetchQueueingPartialFixWithoutScheduler,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PrefetchCanaryCheckerParams,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(PrefetchMultipleActiveSetSizeLimitForBase,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<size_t>
    kPrefetchMultipleActiveSetSizeLimitForBaseValue{
        &kPrefetchMultipleActiveSetSizeLimitForBase,
        "prefetch_multiple_active_set_size_limit_for_base_value", 2};
}  // namespace features
