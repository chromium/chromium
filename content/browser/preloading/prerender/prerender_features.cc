// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prerender/prerender_features.h"

#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"

namespace features {

const base::FeatureParam<bool>
    kPrerender2FallbackPrefetchUseBlockUntilHeadTimetout{
        &kPrerender2FallbackPrefetchSpecRules,
        "kPrerender2FallbackPrefetchUseBlockUntilHeadTimetout", false};

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
        Prerender2FallbackPrefetchSchedulerPolicy::kBurst,
        &kPrerender2FallbackPrefetchSchedulerPolicyOptios};

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

// If enabled, disallows non-trustworthy plaintext HTTP prerendering.
// See https://crbug.com/340895233 for more details.
BASE_FEATURE(kPrerender2DisallowNonTrustworthyHttp,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrerender2WarmUpCompositorForImmediate,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPrerender2WarmUpCompositorForNonImmediate,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrerenderUntilScriptUpgrade, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrerenderUntilScriptProcessReuse,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrerender2ReuseInitiatorProcess,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<std::string>
    kPrerender2ReuseInitiatorProcessActionType{
        &kPrerender2ReuseInitiatorProcess, "prerender_action_type",
        "prerender-until-script"};

const base::FeatureParam<std::string> kPrerender2ReuseInitiatorProcessEagerness{
    &kPrerender2ReuseInitiatorProcess, "eagerness", "moderate"};

const base::FeatureParam<int> kPrerender2ReuseInitiatorProcessMaxReuseCount{
    &kPrerender2ReuseInitiatorProcess, "max_reuse_count", 2};

const base::FeatureParam<bool> kPrerender2CrossOriginIframesNesting{
    &blink::features::kPrerender2CrossOriginIframes, "nesting", true};

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
