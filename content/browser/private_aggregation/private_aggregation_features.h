// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_FEATURES_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_FEATURES_H_

#include "base/feature_list.h"

namespace content {

// Enables the Private Aggregation API. Note that this API also requires the
// `kPrivacySandboxAggregationService` to be enabled to successfully send
// reports.
extern const base::Feature kPrivateAggregationApi;

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_FEATURES_H_
