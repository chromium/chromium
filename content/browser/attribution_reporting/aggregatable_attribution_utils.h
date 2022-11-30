// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace absl {
class uint128;
}  // namespace absl

namespace content {

class AggregatableHistogramContribution;
class AggregatableReportRequest;
class AttributionAggregationKeys;
class AttributionAggregatableTriggerData;
class AttributionAggregatableValues;
class AttributionFilterData;
class AttributionReport;

// Creates histograms from the specified source and trigger data.
CONTENT_EXPORT std::vector<AggregatableHistogramContribution>
CreateAggregatableHistogram(
    const AttributionFilterData& source_filter_data,
    const AttributionAggregationKeys& keys,
    const std::vector<AttributionAggregatableTriggerData>&
        aggregatable_trigger_data,
    const AttributionAggregatableValues& aggregatable_values);

// Returns a hex string representation of the 128-bit aggregatable key in big
// endian order.
CONTENT_EXPORT std::string HexEncodeAggregationKey(absl::uint128 value);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AssembleAggregatableReportStatus {
  kSuccess = 0,
  kAggregationServiceUnavailable = 1,
  kCreateRequestFailed = 2,
  kAssembleReportFailed = 3,
  kMaxValue = kAssembleReportFailed,
};

CONTENT_EXPORT absl::optional<AggregatableReportRequest>
CreateAggregatableReportRequest(const AttributionReport& report);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_
