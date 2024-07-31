// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/numerics/checked_math.h"
#include "base/values.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom-forward.h"

namespace attribution_reporting {
class AggregatableTriggerData;
class AggregatableValues;
class AggregationKeys;
class FilterData;
}  // namespace attribution_reporting

namespace base {
class Time;
}  // namespace base

namespace net {
class SchemefulSite;
}  // namespace net

namespace content {

class AggregatableReportRequest;
class AttributionReport;

// Creates histograms from the specified source and trigger data.
CONTENT_EXPORT
std::vector<blink::mojom::AggregatableReportHistogramContribution>
CreateAggregatableHistogram(
    const attribution_reporting::FilterData& source_filter_data,
    attribution_reporting::mojom::SourceType,
    const base::Time& source_time,
    const base::Time& trigger_time,
    const attribution_reporting::AggregationKeys& keys,
    const std::vector<attribution_reporting::AggregatableTriggerData>&,
    const std::vector<attribution_reporting::AggregatableValues>&);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(AssembleAggregatableReportStatus)
enum class AssembleAggregatableReportStatus {
  kSuccess = 0,
  kAggregationServiceUnavailable = 1,
  kCreateRequestFailed = 2,
  kAssembleReportFailed = 3,
  kMaxValue = kAssembleReportFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/attribution_reporting/enums.xml:ConversionAssembleAggregatableReportStatus)

CONTENT_EXPORT std::optional<AggregatableReportRequest>
CreateAggregatableReportRequest(const AttributionReport& report);

base::CheckedNumeric<int64_t> GetTotalAggregatableValues(
    const std::vector<blink::mojom::AggregatableReportHistogramContribution>&);

void SetAttributionDestination(base::Value::Dict&,
                               const net::SchemefulSite& destination);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_
