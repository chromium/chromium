// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_features.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"

namespace features {

// Enables fallback from prerender to prefetch for Speculation Rules.
// See https://crbug.com/342089123 for more details.
//
// Effects:
//
// - Use code paths for prefetch/prerender integration. (The effect of
//   `kPrefetchPrerenderIntegration`).
// - Trigger prefetch ahead of prerender.
BASE_FEATURE(kPrerender2FallbackPrefetchSpecRules,
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool>
    kPrerender2FallbackPrefetchUseBlockUntilHeadTimetout{
        &kPrerender2FallbackPrefetchSpecRules,
        "kPrerender2FallbackPrefetchUseBlockUntilHeadTimetout", true};

constexpr base::FeatureParam<Prerender2FallbackPrefetchSchedulerPolicy>::Option
    kPrerender2FallbackPrefetchSchedulerPolicyOptios[] = {
        {Prerender2FallbackPrefetchSchedulerPolicy::kNotUse, "NotUse"},
        {Prerender2FallbackPrefetchSchedulerPolicy::kPrioritize, "Prioritize"},
        {Prerender2FallbackPrefetchSchedulerPolicy::kBurst, "Burst"},
};
const base::FeatureParam<Prerender2FallbackPrefetchSchedulerPolicy>
    kPrerender2FallbackPrefetchSchedulerPolicy{
        &kPrerender2FallbackPrefetchSpecRules,
        "kPrerender2FallbackPrefetchSchedulerPolicy",
        Prerender2FallbackPrefetchSchedulerPolicy::kNotUse,
        &kPrerender2FallbackPrefetchSchedulerPolicyOptios};

const base::FeatureParam<bool> kPrerender2FallbackUsePreloadServingMetrics{
    &kPrerender2FallbackPrefetchSpecRules,
    "kPrerender2FallbackUsePreloadServingMetrics", false};

BASE_FEATURE(kPrerender2NoVarySearch, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int>
    kPrerender2NoVarySearchWaitForHeadersTimeoutEagerPrerender{
        &kPrerender2NoVarySearch, "wait_for_headers_timeout_eager_prerender",
        1000};

const base::FeatureParam<int>
    kPrerender2NoVarySearchWaitForHeadersTimeoutModeratePrerender{
        &kPrerender2NoVarySearch, "wait_for_headers_timeout_moderate_prerender",
        0};

const base::FeatureParam<int>
    kPrerender2NoVarySearchWaitForHeadersTimeoutConservativePrerender{
        &kPrerender2NoVarySearch,
        "wait_for_headers_timeout_conservative_prerender", 0};

const base::FeatureParam<int>
    kPrerender2NoVarySearchWaitForHeadersTimeoutForEmbedders{
        &kPrerender2NoVarySearch, "wait_for_headers_timeout_embedders", 1000};

// If enabled, suppresses prerendering on slow network.
BASE_FEATURE(kSuppressesPrerenderingOnSlowNetwork,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Regarding how this number was chosen, see the design doc linked from
// crbug.com/350519234.
const base::FeatureParam<base::TimeDelta>
    kSuppressesPrerenderingOnSlowNetworkThreshold{
        &kSuppressesPrerenderingOnSlowNetwork,
        "slow_network_threshold_for_prerendering", base::Milliseconds(208)};

// If enabled, disallows non-trustworthy plaintext HTTP prerendering.
// See https://crbug.com/340895233 for more details.
BASE_FEATURE(kPrerender2DisallowNonTrustworthyHttp,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrerender2WarmUpCompositorForImmediate,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPrerender2WarmUpCompositorForNonImmediate,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool UsePrefetchPrerenderIntegration() {
  return base::FeatureList::IsEnabled(
             features::kPrerender2FallbackPrefetchSpecRules) ||
         base::FeatureList::IsEnabled(
             features::kPrefetchPrerenderIntegration) ||
         content::GetContentClient()
             ->browser()
             ->UsePrefetchPrerenderIntegration();
}

}  // namespace features
