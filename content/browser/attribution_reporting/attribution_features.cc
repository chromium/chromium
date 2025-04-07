// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace content {

BASE_FEATURE(kAttributionReportDeliveryOnNewNavigation,
             "AttributionReportDeliveryOnNewNavigation",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta>
    kAttributionReportingNavigationForReportDeliveryWindow{
        &kAttributionReportDeliveryOnNewNavigation, "navigation_window",
        base::Minutes(2)};

BASE_FEATURE(kAttributionReportExpiry,
             "AttributionReportExpiry",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace content
