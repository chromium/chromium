// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FEATURES_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/content_export.h"

namespace features {

// This feature was used to launch the prefetch migration from embedder layer to
// content/, and this work has finished and the old implemnetation was deleted.
// Now this flag is just for injecting parameters through field trials as an
// umberella feature.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchUseContentRefactor);

// If enabled, PrefetchContainer can be used for more than one navigation.
// https://crbug.com/1449360
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchReusable);

// The size limit of body size in bytes that can be reused in
// `kPrefetchReusable`.
CONTENT_EXPORT extern const base::FeatureParam<int>
    kPrefetchReusableBodySizeLimit;

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

// If enabled, prefetch requests may include X-Client-Data request header.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchXClientDataHeader);

// If enabled, then prefetch serving will apply mitigations if it may have been
// contaminated by cross-partition state.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchStateContaminationMitigation);

// If true, contaminated prefetches will also force a browsing context group
// swap.
CONTENT_EXPORT extern const base::FeatureParam<bool>
    kPrefetchStateContaminationSwapsBrowsingContextGroup;

// If explicitly disabled, prefetch proxy is not used.
BASE_DECLARE_FEATURE(kPrefetchProxy);

// If enabled, responses with an operative Cookie-Indices will not be used
// if the relevant cookie values have changed.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchCookieIndices);

// Does not enable any new behaviour; is only used to parameterize prefetch
// limit values (see content/browser/preloading/prefetch/prefetch_params.cc).
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchNewLimits);

// If enabled, use the new wait loop, which is driven by
// `PrefetchMatchResolver2` instead of `PrefetchService`.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrefetchNewWaitLoop);

}  // namespace features

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_FEATURES_H_
