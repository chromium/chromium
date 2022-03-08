// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_FEATURES_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace content {

// Enables the Aggregation Service. See crbug.com/1207974.
extern const base::Feature kPrivacySandboxAggregationService;
extern const base::FeatureParam<std::string>
    kPrivacySandboxAggregationServiceTrustedServerUrlParam;

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_FEATURES_H_
