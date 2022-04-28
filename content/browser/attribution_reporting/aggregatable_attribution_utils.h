// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "content/common/content_export.h"

namespace absl {
class uint128;
}  // namespace absl

namespace content {

class AggregatableHistogramContribution;
class AggregationService;
class AttributionAggregatableSource;
class AttributionAggregatableTrigger;
class AttributionFilterData;
class AttributionReport;

// Creates histograms from the specified source and trigger data.
CONTENT_EXPORT std::vector<AggregatableHistogramContribution>
CreateAggregatableHistogram(const AttributionFilterData& source_filter_data,
                            const AttributionAggregatableSource& source,
                            const AttributionAggregatableTrigger& trigger);

// Returns a hex string representation of the 128-bit aggregatable key in big
// endian order.
CONTENT_EXPORT std::string HexEncodeAggregatableKey(absl::uint128 value);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AssembleAggregatableReportStatus {
  kSuccess = 0,
  kAggregationServiceUnavailable = 1,
  kCreateRequestFailed = 2,
  kAssembleReportFailed = 3,
  kMaxValue = kAssembleReportFailed,
};

// Assembles the aggregatable report utilizing the aggregation service client.
CONTENT_EXPORT void AssembleAggregatableReport(
    AggregationService& aggregation_service,
    AttributionReport report,
    base::OnceCallback<void(AttributionReport,
                            AssembleAggregatableReportStatus)> callback);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_
