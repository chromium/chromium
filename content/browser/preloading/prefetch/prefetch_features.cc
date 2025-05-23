// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "base/feature_list.h"

namespace features {

BASE_FEATURE(kPrefetchUseContentRefactor,
             "PrefetchUseContentRefactor",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchReusable,
             "PrefetchReusable",
             base::FEATURE_ENABLED_BY_DEFAULT);

// 4MiB, 2**20 * 4.
const base::FeatureParam<int> kPrefetchReusableBodySizeLimit{
    &kPrefetchReusable, "prefetch_reusable_body_size_limit", 4194304};

BASE_FEATURE(kPrefetchNIKScope,
             "PrefetchNIKScope",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchClientHints,
             "PrefetchClientHints",
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

BASE_FEATURE(kPrefetchStateContaminationMitigation,
             "PrefetchStateContaminationMitigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool>
    kPrefetchStateContaminationSwapsBrowsingContextGroup{
        &kPrefetchStateContaminationMitigation, "swaps_bcg", true};

BASE_FEATURE(kPrefetchProxy, "PrefetchProxy", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchCookieIndices,
             "PrefetchCookieIndices",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchNewLimits,
             "PrefetchNewLimits",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchServiceWorkerNoFetchHandlerFix,
             "PrefetchServiceWorkerNoFetchHandlerFix",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchNetworkPriorityForEmbedders,
             "PrefetchNetworkPriorityForEmbedders",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchBumpNetworkPriorityAfterBeingServed,
             "PrefetchBumpNetworkPriorityAfterBeingServed",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchServiceWorker,
             "PrefetchServiceWorker",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsPrefetchServiceWorkerEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(kPrefetchServiceWorker) &&
         content::GetContentClient()->browser()->IsPrefetchWithServiceWorkerAllowed(
             browser_context);
}

BASE_FEATURE(kPrefetchBrowsingDataRemoval,
             "PrefetchBrowsingDataRemoval",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchScheduler,
             "PrefetchScheduler",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kPrefetchSchedulerProgressSyncBestEffort{
    &kPrefetchScheduler, "kPrefetchSchedulerProgressSyncBestEffort", true};

BASE_FEATURE(kPrefetchSchedulerTesting,
             "PrefetchSchedulerTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<size_t>
    kPrefetchSchedulerTestingActiveSetSizeLimitForBase{
        &kPrefetchSchedulerTesting,
        "kPrefetchSchedulerTestingActiveSetSizeLimitForBase", 1};
const base::FeatureParam<size_t>
    kPrefetchSchedulerTestingActiveSetSizeLimitForBurst{
        &kPrefetchSchedulerTesting,
        "kPrefetchSchedulerTestingActiveSetSizeLimitForBurst", 1};

BASE_FEATURE(kPrefetchQueueingPartialFixWithoutScheduler,
             "PrefetchQueueingPartialFixWithoutScheduler",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
