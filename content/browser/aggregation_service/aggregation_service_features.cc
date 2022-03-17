// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_features.h"

namespace content {

// Enables the Aggregation Service. See crbug.com/1207974.
const base::Feature kPrivacySandboxAggregationService = {
    "PrivacySandboxAggregationService", base::FEATURE_ENABLED_BY_DEFAULT};

const base::FeatureParam<std::string>
    kPrivacySandboxAggregationServiceTrustedServerUrlParam{
        &kPrivacySandboxAggregationService, "trusted_server_url",
        "https://publickeyservice.aws.privacysandboxservices.com/v1alpha/"
        "publicKeys"};

}  // namespace content
