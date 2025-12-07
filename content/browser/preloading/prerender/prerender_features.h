// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_FEATURES_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/content_export.h"

namespace features {

CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2FallbackPrefetchSpecRules);

// Controls whether `PrefetchMatchResolver` use timeout for prefetch ahead of
// prerender. We are going not to use timeout as it makes prerender fail. For
// more details, see
// https://docs.google.com/document/d/1ZP7lYrtqZL9jC2xXieNY_UBMJL1sCrfmzTB8K6v4sD4/edit?resourcekey=0-fkbeQhkT3PhBb9FnnPgnZA&tab=t.wphan8fb23kr
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kPrerender2FallbackPrefetchUseBlockUntilHeadTimetout;

enum class Prerender2FallbackPrefetchSchedulerPolicy {
  // Do not use `PrefetchScheduler` code path.
  kNotUse,
  // Prioritize prefetch ahead of prerender.
  kPrioritize,
  // Burst prefetch ahead of prerender.
  kBurst,
};
CONTENT_EXPORT extern const base::FeatureParam<
    Prerender2FallbackPrefetchSchedulerPolicy>
    kPrerender2FallbackPrefetchSchedulerPolicy;

// If enabled, `PreloadServingMetrics` is collected and metrics are rerpoted.
// For more details, see `PreloadServingMetrics`.
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kPrerender2FallbackUsePreloadServingMetrics;

// This feature was used to launch Prerender2 support for No-Vary-Search header.
// This work has finished and the old implementation was deleted. Now this flag
// is just for injecting parameters through field trials as an umberella
// feature.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2NoVarySearch);

// A set of trial parameters that controls the timeout for waiting on headers
// during navigation for the prerender URL matched by No-Vary-Search hint before
// falling back to the default navigation path.

// This is actually for "immediate"; see https://crbug.com/40287486.
CONTENT_EXPORT extern const base::FeatureParam<int>
    kPrerender2NoVarySearchWaitForHeadersTimeoutEagerPrerender;
CONTENT_EXPORT extern const base::FeatureParam<int>
    kPrerender2NoVarySearchWaitForHeadersTimeoutModeratePrerender;
CONTENT_EXPORT extern const base::FeatureParam<int>
    kPrerender2NoVarySearchWaitForHeadersTimeoutConservativePrerender;
CONTENT_EXPORT extern const base::FeatureParam<int>
    kPrerender2NoVarySearchWaitForHeadersTimeoutForEmbedders;

// If enabled, suppresses prerendering on slow network.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kSuppressesPrerenderingOnSlowNetwork);

CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kSuppressesPrerenderingOnSlowNetworkThreshold;

CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2DisallowNonTrustworthyHttp);

// If enabled, requests the compositor warm-up (crbug.com/41496019) for
// Immediate/non-Immediate Speculation Rules prerenders.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2WarmUpCompositorForImmediate);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2WarmUpCompositorForNonImmediate);

CONTENT_EXPORT bool UsePrefetchPrerenderIntegration();
}  // namespace features

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_FEATURES_H_
