// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FEATURES_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/content_export.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace features {

// Controls params for tests of prefetch.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchTesting);

// The size limit of body size in bytes that can be reused in
// `PrefetchDataPipeTee`.
CONTENT_EXPORT extern const base::FeatureParam<int>
    kPrefetchReusableBodySizeLimit;

// This feature was used to launch the prefetch migration from embedder layer to
// content/, and this work has finished and the old implemnetation was deleted.
// Now this flag is just for injecting parameters through field trials as an
// umberella feature.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchUseContentRefactor);

// If enabled, navigational prefetch is scoped to the referring document's
// network isolation key instead of the old behavior of the referring document
// itself. See crbug.com/1502326
BASE_DECLARE_FEATURE(kPrefetchNIKScope);

// If enabled, prefetches may include client hints request headers.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchClientHints);

// This allows controlling the behavior of client hints with prefetches in case
// an unexpected issue arises with the planned behavior, or one is suspected and
// we want to debug more easily.
// TODO(crbug.com/41497015): Remove this control once a behavior is shipped and
// stabilized.
enum class PrefetchClientHintsCrossSiteBehavior {
  // Send no client hints cross-site.
  kNone,
  // Send only the "low-entropy" hints which are included by default.
  kLowEntropy,
  // Send all client hints that would normally be sent.
  kAll,
};
CONTENT_EXPORT extern const base::FeatureParam<
    PrefetchClientHintsCrossSiteBehavior>
    kPrefetchClientHintsCrossSiteBehavior;

// If enabled, then prefetch serving will apply mitigations if it may have been
// contaminated by cross-partition state.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchStateContaminationMitigation);

// If true, contaminated prefetches will also force a browsing context group
// swap.
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kPrefetchStateContaminationSwapsBrowsingContextGroup;

// Fix for prefetching a URL controlled by a ServiceWorker without fetch
// handler. Currently this stops prefetching for such cases
// (https://crbug.com/379076354).
// Even when `kPrefetchServiceWorker` is enabled, this is still effective for
// SW-ineligible prefetches.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchServiceWorkerNoFetchHandlerFix);

// Enabling this will apply net::RequestPriority::MEDIUM for prefetch
// requests triggered by embedders. See crbug.com/353628437 to track this issue.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchNetworkPriorityForEmbedders);

// Enabling this will bupm net::RequestPriority once after the running prefetch
// starts to be served.
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kPrefetchBumpNetworkPriorityAfterBeingServed);

// Allow prefetching ServiceWorker-controlled URLs.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchServiceWorker);
bool IsPrefetchServiceWorkerEnabled(content::BrowserContext* browser_context);

// Replace current prefetch queue with a new queue and scheduler, which allows
// prioritization, concurrent prefetches, bursting.
//
// For more details, see
// https://docs.google.com/document/d/1W0Nk3Nq6NaUXkBppOUC5zyNmhVqMjYShm1bydGYd9qc
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchScheduler);

// Call `PrefetchScheduler::Progress()` synchronously as much as possible.
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kPrefetchSchedulerProgressSyncBestEffort;

// Controls params for tests of `PrefetchScheduler`.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchSchedulerTesting);
CONTENT_EXPORT extern const base::FeatureParam<size_t>
    kPrefetchSchedulerTestingActiveSetSizeLimitForBase;
CONTENT_EXPORT extern const base::FeatureParam<size_t>
    kPrefetchSchedulerTestingActiveSetSizeLimitForBurst;

// Controls field trials parameters for prefetch canary checker.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchCanaryCheckerParams);

// Allows multiple base limit on `PrefetchScheduler`.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchMultipleActiveSetSizeLimitForBase);
CONTENT_EXPORT extern const base::FeatureParam<size_t>
    kPrefetchMultipleActiveSetSizeLimitForBaseValue;

// Kill switch, which enables reporting serving metrics of preloads.
// (crbug.com/360094997)
//
// TODO(crbug.com/360094997): Remove it after confirming stability.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPreloadServingMetrics);

// Kill switch for the modified failure/disconnect notifications around
// `PrefetchContainer`.
// TODO(crbug.com/400761083): Remove it after confirming stability.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchGracefulNotification);

// Kill switch for making the cancelling of `PrefetchStreamingURLLoader` async.
// TODO(crbug.com/400761083): Remove it after confirming stability.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchAsyncCancelOnCookiesChange);

}  // namespace features

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FEATURES_H_
