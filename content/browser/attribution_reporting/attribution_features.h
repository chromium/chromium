// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_FEATURES_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/content_export.h"

namespace content {

CONTENT_EXPORT BASE_DECLARE_FEATURE(kAttributionReportExpiry);

// Feature flag that controls whether reports that fail a delivery follow the
// navigation-based retry system, where the last retry for a report is only
// attempted when a new navigation successfully commits.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kAttributionReportNavigationBasedRetry);

// Feature param that controls the send attempt number to conduct a
// navigation-based retry on. An enum is used to make clear that the retry
// number cannot exceed 3. Tied to `kAttributionReportNavigationBasedRetry`.
enum class NavigationRetryAttempt {
  kFirstRetry = 1,
  kSecondRetry = 2,
  kThirdRetry = 3,
};

CONTENT_EXPORT extern const base::FeatureParam<NavigationRetryAttempt>
    kAttributionReportNavigationRetryAttempt;

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_FEATURES_H_
