// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_FEATURES_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/content_export.h"

namespace features {

CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2NewLimitAndScheduler);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2AllowActivationInBackground);

CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2FallbackPrefetchSpecRules);

// This allows controlling the behavior of multiple use of `PrefetchContainer`
// if `kPrerender2FallbackPrefetchSpecRules` is enabled and `kPrefetchReusable`
// is disabled. If `kPrefetchReusable` is enabled, the feature flag has
// priority.
//
// TODO(crbug.com/373553133): Remove this control once a behavior is shipped and
// stabilized.
enum class Prerender2FallbackPrefetchReusablePolicy {
  // Do not use `PrefetchReusable` code path.
  kNotUse,
  // Use if a prefetch is started by prerender.
  kUseIfIsLikelyAheadOfPrerender,
  // Use always.
  kUseAlways,
};
CONTENT_EXPORT extern const base::FeatureParam<
    Prerender2FallbackPrefetchReusablePolicy>
    kPrerender2FallbackPrefetchReusablePolicy;

CONTENT_EXPORT extern const base::FeatureParam<size_t>
    kPrerender2FallbackBodySizeLimit;

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

// This feature was used to launch Prerender2 support for No-Vary-Search header.
// This work has finished and the old implementation was deleted. Now this flag
// is just for injecting parameters through field trials as an umberella
// feature.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2NoVarySearch);

// A field trial param that controls the timeout for waiting on headers
// during navigation for the prerender URL matched by No-Vary-Search hint before
// falling back to the default navigation path.
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

CONTENT_EXPORT bool UsePrefetchPrerenderIntegration();

}  // namespace features

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_FEATURES_H_
