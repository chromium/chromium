// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_FEATURES_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_FEATURES_H_

#include "base/feature_list.h"
#include "content/common/content_export.h"

namespace content {

// Controls whether third-party cookie eligibility should be queried before
// allowing debug mode to be used by a context. If enabled, any
// `enableDebugMode()` calls in a context that does not have third-party cookie
// eligibility will essentially have no effect. This feature has no effect on
// debug mode if `blink::features::kPrivateAggregationApiDebugModeEnabledAtAll`
// is disabled.
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kPrivateAggregationApiDebugModeRequires3pcEligibility);

// Controls whether contributions in a report with the same bucket and the same
// filtering ID should be merged into a single contribution before truncating
// (if necessary) and embedding into the report. Also drops contributions with a
// value of zero.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivateAggregationApiContributionMerging);

// Controls whether Protected Audience callers can make up to 100 contributions
// per report instead of 20. When enabled, reports for Protected Audience
// callers will be padded up to 100 contributions. This feature has no effect on
// Shared Storage callers.
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kPrivateAggregationApi100ContributionsForProtectedAudience);

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_FEATURES_H_
