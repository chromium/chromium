// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORTING_CONSTANTS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORTING_CONSTANTS_H_

#include <stdint.h>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

constexpr int kAttributionMaxSourcesPerOrigin = 1024;

constexpr absl::optional<uint64_t> kAttributionSourceEventIdCardinality =
    absl::nullopt;

constexpr int kAttributionMaxDestinationsPerSourceSiteReportingOrigin = 100;

constexpr uint64_t kAttributionNavigationSourceTriggerDataCardinality = 8;

constexpr uint64_t kAttributionEventSourceTriggerDataCardinality = 2;

constexpr double kAttributionNavigationSourceRandomizedResponseRate = .0024;

constexpr double kAttributionEventSourceRandomizedResponseRate = .0000025;

constexpr int kAttributionMaxReportsPerDestination = 1024;

constexpr int kMaxAttributionsPerNavigationSource = 3;

constexpr int kMaxAttributionsPerEventSource = 1;

constexpr int64_t kAttributionAggregatableBudgetPerSource = 65536;

constexpr base::TimeDelta kAttributionAggregatableReportMinDelay =
    base::Minutes(10);

constexpr base::TimeDelta kAttributionAggregatableReportDelaySpan =
    base::Minutes(50);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORTING_CONSTANTS_H_
