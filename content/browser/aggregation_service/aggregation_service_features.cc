// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_features.h"

namespace content {

// Enables the Aggregation Service. See crbug.com/1207974.
const base::Feature kPrivacySandboxAggregationService = {
    "PrivacySandboxAggregationService", base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<std::string>
    kPrivacySandboxAggregationServiceTrustedServerUrlParam{
        &kPrivacySandboxAggregationService, "trusted_server_url",
        "https://server.example/.well-known/aggregation-service/keys.json"};

}  // namespace content
