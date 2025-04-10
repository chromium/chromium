// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_features.h"

#include "content/public/common/content_features.h"

namespace features {

// Allows activation in background tab. For now, this is used only on web
// platform tests on macOS to run activation with target hint tests that have
// race conditions between visibility change and activation start on a prerender
// WebContents. Note that this issue does not happen on browser_tests, so this
// could be specific to WPT setup.
// TODO(crbug.com/40249964): Allow activation in background by default.
BASE_FEATURE(kPrerender2AllowActivationInBackground,
             "Prerender2AllowActivationInBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables fallback from prerender to prefetch for Speculation Rules.
// See https://crbug.com/342089123 for more details.
//
// Effects:
//
// - Use code paths for prefetch/prerender integration. (The effect of
//   `kPrefetchPrerenderIntegration`).
// - Trigger prefetch ahead of prerender.
BASE_FEATURE(kPrerender2FallbackPrefetchSpecRules,
             "Prerender2FallbackPrefetchSpecRules",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<Prerender2FallbackPrefetchReusablePolicy>::Option
    kPrerender2FallbackPrefetchReusablePolicyOptions[] = {
        {Prerender2FallbackPrefetchReusablePolicy::kNotUse, "NotUse"},
        {Prerender2FallbackPrefetchReusablePolicy::
             kUseIfIsLikelyAheadOfPrerender,
         "UseIfIsLikelyAheadOfPrerender"},
        {Prerender2FallbackPrefetchReusablePolicy::kUseAlways, "UseAlways"},
};
const base::FeatureParam<Prerender2FallbackPrefetchReusablePolicy>
    kPrerender2FallbackPrefetchReusablePolicy{
        &kPrerender2FallbackPrefetchSpecRules,
        "kPrerender2FallbackPrefetchReusablePolicy",
        Prerender2FallbackPrefetchReusablePolicy::kNotUse,
        &kPrerender2FallbackPrefetchReusablePolicyOptions};

const base::FeatureParam<size_t> kPrerender2FallbackBodySizeLimit{
    &kPrerender2FallbackPrefetchSpecRules, "kPrerender2FallbackBodySizeLimit",
    65536};

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

BASE_FEATURE(kPrerender2NoVarySearch,
             "Prerender2NoVarySearch",
             base::FEATURE_ENABLED_BY_DEFAULT);

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
             "SuppressesPrerenderingOnSlowNetwork",
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
             "Prerender2DisallowNonTrustworthyHttp",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool UsePrefetchPrerenderIntegration() {
  return base::FeatureList::IsEnabled(
             features::kPrerender2FallbackPrefetchSpecRules) ||
         base::FeatureList::IsEnabled(features::kPrefetchPrerenderIntegration);
}

}  // namespace features
