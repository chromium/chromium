// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_

#include <vector>

#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace attribution_reporting {
class AggregatableValues;
class AggregationKeys;
class FilterData;
}  // namespace attribution_reporting

namespace content {

class AggregatableHistogramContribution;
class AggregatableReportRequest;
class AttributionReport;

// Creates histograms from the specified source and trigger data.
CONTENT_EXPORT std::vector<AggregatableHistogramContribution>
CreateAggregatableHistogram(
    const attribution_reporting::FilterData& source_filter_data,
    AttributionSourceType,
    const attribution_reporting::AggregationKeys& keys,
    const attribution_reporting::AggregatableTriggerDataList&,
    const attribution_reporting::AggregatableValues&);

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
