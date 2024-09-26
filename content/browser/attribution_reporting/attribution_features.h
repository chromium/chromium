// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_FEATURES_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_FEATURES_H_

#include "base/feature_list.h"
#include "content/common/content_export.h"

namespace content {

CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kAttributionReportDeliveryThirdRetryAttempt);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_FEATURES_H_
