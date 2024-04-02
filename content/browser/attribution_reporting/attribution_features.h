// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_FEATURES_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/common/content_export.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace content {

CONTENT_EXPORT BASE_DECLARE_FEATURE(kAttributionVerboseDebugReporting);

CONTENT_EXPORT BASE_DECLARE_FEATURE(kAttributionHeaderErrorDetails);

CONTENT_EXPORT BASE_DECLARE_FEATURE(kAttributionReportDeliveryRetryDelays);
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kAttributionReportDeliveryFirstRetryDelay;
CONTENT_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kAttributionReportDeliverySecondRetryDelay;

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_FEATURES_H_
