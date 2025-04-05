// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "base/feature_list.h"

namespace features {

BASE_FEATURE(kPrefetchUseContentRefactor,
             "PrefetchUseContentRefactor",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchReusable,
             "PrefetchReusable",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

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

BASE_FEATURE(kPrefetchBrowsingDataRemoval,
             "PrefetchBrowsingDataRemoval",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchScheduler,
             "PrefetchScheduler",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

}  // namespace features
