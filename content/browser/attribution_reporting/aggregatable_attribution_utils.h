// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_

#include <optional>
#include <vector>
#include <unordered_map>

#include "base/metrics/histogram_functions.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "content/common/content_export.h"
#include "content/browser/attribution_reporting/partition.h"

namespace attribution_reporting {
class AggregatableTriggerData;
class AggregatableValues;
class AggregationKeys;
class FilterData;
}  // namespace attribution_reporting

namespace base {
class Time;
}  // namespace base

namespace content {

class AggregatableHistogramContribution;
class AggregatableReportRequest;
class AttributionReport;

// Creates histograms from the specified source and trigger data.
CONTENT_EXPORT std::vector<AggregatableHistogramContribution>
CreateAggregatableHistogram(
    const attribution_reporting::FilterData& source_filter_data,
    attribution_reporting::mojom::SourceType,
    const base::Time& source_time,
    const base::Time& trigger_time,
    const attribution_reporting::AggregationKeys& keys,
    const std::vector<attribution_reporting::AggregatableTriggerData>&,
    const attribution_reporting::AggregatableValues&);

CONTENT_EXPORT void AttributionLogicLastTouch(Partition& partition,
        const base::flat_map<std::string, std::vector<absl::uint128>>& trigger_keypieces_per_source);

CONTENT_EXPORT void AttributionLogicUniform(Partition& partition,
        const base::flat_map<std::string, std::vector<absl::uint128>>& trigger_keypieces_per_source);

CONTENT_EXPORT void CreateAggregatableHistogramM2M(
    Partition& partition,
    const std::vector<attribution_reporting::AggregatableTriggerData>& aggregatable_trigger_data);
  
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AssembleAggregatableReportStatus {
  kSuccess = 0,
  kAggregationServiceUnavailable = 1,
  kCreateRequestFailed = 2,
  kAssembleReportFailed = 3,
  kMaxValue = kAssembleReportFailed,
};

CONTENT_EXPORT std::optional<AggregatableReportRequest>
CreateAggregatableReportRequest(const AttributionReport& report);

CONTENT_EXPORT base::Time RoundDownToWholeDaySinceUnixEpoch(base::Time);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_AGGREGATABLE_ATTRIBUTION_UTILS_H_
