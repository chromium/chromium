// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_features.h"

#include "base/feature_list.h"

namespace content {

BASE_FEATURE(kPrivateAggregationApiDebugModeRequires3pcEligibility,
             "PrivateAggregationApiDebugModeRequires3pcEligibility",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrivateAggregationApiContributionMerging,
             "PrivateAggregationApiContributionMerging",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrivateAggregationApi100ContributionsForProtectedAudience,
             "PrivateAggregationApi100ContributionsForProtectedAudience",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace content
