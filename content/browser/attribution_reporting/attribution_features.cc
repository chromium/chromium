// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_features.h"

#include "base/feature_list.h"
#include "base/time/time.h"

namespace content {

BASE_FEATURE(kAttributionVerboseDebugReporting,
             "AttributionVerboseDebugReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAttributionReportDeliveryRetryDelays,
             "AttributionReportDeliveryRetryDelays",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta>
    kAttributionReportDeliveryFirstRetryDelay{
        &kAttributionReportDeliveryRetryDelays, "first_retry_delay",
        base::Minutes(5)};
const base::FeatureParam<base::TimeDelta>
    kAttributionReportDeliverySecondRetryDelay{
        &kAttributionReportDeliveryRetryDelays, "second_retry_delay",
        base::Minutes(15)};

}  // namespace content
