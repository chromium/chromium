// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace content {

BASE_FEATURE(kAttributionReportExpiry,
             "AttributionReportExpiry",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAttributionReportNavigationBasedRetry,
             "AttributionReportNavigationBasedRetry",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<NavigationRetryAttempt>::Option
    kNavigationRetryAttemptOptions[] = {
        {NavigationRetryAttempt::kFirstRetry, "first_retry"},
        {NavigationRetryAttempt::kSecondRetry, "second_retry"},
        {NavigationRetryAttempt::kThirdRetry, "third_retry"}};

const base::FeatureParam<NavigationRetryAttempt>
    kAttributionReportNavigationRetryAttempt{
        &kAttributionReportNavigationBasedRetry, "navigation_retry_attempt",
        NavigationRetryAttempt::kThirdRetry, &kNavigationRetryAttemptOptions};

}  // namespace content
