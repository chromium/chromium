// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_FEATURES_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/content_export.h"

namespace content {

// Enables the Aggregation Service. See crbug.com/1207974.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivacySandboxAggregationService);
extern CONTENT_EXPORT const base::FeatureParam<std::string>
    kPrivacySandboxAggregationServiceTrustedServerUrlAwsParam;

// Enables filtering IDs. See crbug.com/330744610.
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kPrivacySandboxAggregationServiceFilteringIds);

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_FEATURES_H_
