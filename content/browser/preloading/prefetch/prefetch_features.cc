// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace features {

BASE_FEATURE(kPrefetchTesting, base::FEATURE_DISABLED_BY_DEFAULT);

// 4MiB, 2**20 * 4.
const base::FeatureParam<int> kPrefetchReusableBodySizeLimit{
    &kPrefetchTesting, "kPrefetchReusableBodySizeLimit", 4194304};

BASE_FEATURE(kPrefetchUseContentRefactor, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchNIKScope, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchClientHints, base::FEATURE_ENABLED_BY_DEFAULT);

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
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool>
    kPrefetchStateContaminationSwapsBrowsingContextGroup{
        &kPrefetchStateContaminationMitigation, "swaps_bcg", true};

BASE_FEATURE(kPrefetchNetworkPriorityForEmbedders,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchBumpNetworkPriorityAfterBeingServed,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchServiceWorker, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPrefetchServiceWorkerEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(kPrefetchServiceWorker) &&
         content::GetContentClient()
             ->browser()
             ->IsPrefetchWithServiceWorkerAllowed(browser_context);
}

BASE_FEATURE(kPrefetchSchedulerTesting, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<size_t>
    kPrefetchSchedulerTestingActiveSetSizeLimitForBase{
        &kPrefetchSchedulerTesting,
        "kPrefetchSchedulerTestingActiveSetSizeLimitForBase", 1};
const base::FeatureParam<size_t>
    kPrefetchSchedulerTestingActiveSetSizeLimitForBurst{
        &kPrefetchSchedulerTesting,
        "kPrefetchSchedulerTestingActiveSetSizeLimitForBurst", 1};

BASE_FEATURE(kPrefetchCanaryCheckerParams, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPrefetchMultipleActiveSetSizeLimitForBase,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<size_t>
    kPrefetchMultipleActiveSetSizeLimitForBaseValue{
        &kPrefetchMultipleActiveSetSizeLimitForBase,
        "prefetch_multiple_active_set_size_limit_for_base_value", 2};
#else
BASE_FEATURE(kPrefetchMultipleActiveSetSizeLimitForBase,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<size_t>
    kPrefetchMultipleActiveSetSizeLimitForBaseValue{
        &kPrefetchMultipleActiveSetSizeLimitForBase,
        "prefetch_multiple_active_set_size_limit_for_base_value", 3};
#endif

BASE_FEATURE(kPrefetchEagerLimit, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<size_t> kMaxNumberOfEagerPrefetchesPerPage{
    &kPrefetchEagerLimit, "max_number_of_eager_prefetches_per_page", 2};

BASE_FEATURE(kPrefetchModerateLimit, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<size_t> kMaxNumberOfModeratePrefetchesPerPage{
    &kPrefetchModerateLimit, "max_number_of_moderate_prefetches_per_page", 2};

BASE_FEATURE(kPrefetchOffTheMainThreadForceForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchCancelUnrelatedPrefetch,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrefetchAsyncPrefetchHandleCallback,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
