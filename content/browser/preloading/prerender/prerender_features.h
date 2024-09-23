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

CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2EmbedderBlockedHosts);
CONTENT_EXPORT extern const base::FeatureParam<std::string>
    kPrerender2EmbedderBlockedHostsParam;

CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrerender2FallbackPrefetchSpecRules);

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

}  // namespace features

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_FEATURES_H_
