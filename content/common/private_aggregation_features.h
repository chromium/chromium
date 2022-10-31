// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PRIVATE_AGGREGATION_FEATURES_H_
#define CONTENT_COMMON_PRIVATE_AGGREGATION_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/content_export.h"

namespace content {

// Enables the Private Aggregation API. Note that this API also requires the
// `kPrivacySandboxAggregationService` to be enabled to successfully send
// reports.
CONTENT_EXPORT BASE_DECLARE_FEATURE(kPrivateAggregationApi);

// Selectively allows the JavaScript API to be disabled in just one of the
// contexts.
extern const base::FeatureParam<bool>
    kPrivateAggregationApiEnabledInSharedStorage;
extern const base::FeatureParam<bool> kPrivateAggregationApiEnabledInFledge;

}  // namespace content

#endif  // CONTENT_COMMON_PRIVATE_AGGREGATION_FEATURES_H_
