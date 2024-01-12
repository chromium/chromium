// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_UTILS_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/trigger_data_matching.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/common/content_export.h"

namespace attribution_reporting {
class AggregationKeys;
class EventReportWindows;
class FilterData;
class MaxEventLevelReports;
}  // namespace attribution_reporting

namespace sql {
class Statement;
}  // namespace sql

namespace url {
class Origin;
}  // namespace url

namespace content {

namespace proto {
class AttributionReadOnlySourceData;
}  // namespace proto

url::Origin DeserializeOrigin(const std::string& origin);

std::optional<attribution_reporting::mojom::SourceType> DeserializeSourceType(
    int val);

// Exposed for use with earlier DB migrations that only contained a subset of
// fields.
void SetReadOnlySourceData(const attribution_reporting::EventReportWindows&,
                           attribution_reporting::MaxEventLevelReports,
                           proto::AttributionReadOnlySourceData&);

std::string SerializeReadOnlySourceData(
    const attribution_reporting::EventReportWindows&,
    attribution_reporting::MaxEventLevelReports,
    double randomized_response_rate,
    attribution_reporting::mojom::TriggerDataMatching,
    bool debug_cookie_set);

CONTENT_EXPORT std::optional<proto::AttributionReadOnlySourceData>
DeserializeReadOnlySourceDataAsProto(sql::Statement&, int col);

std::string SerializeFilterData(const attribution_reporting::FilterData&);

std::optional<attribution_reporting::FilterData> DeserializeFilterData(
    sql::Statement&,
    int col);

std::optional<attribution_reporting::EventReportWindows>
DeserializeEventReportWindows(const proto::AttributionReadOnlySourceData&);

std::string SerializeAggregationKeys(
    const attribution_reporting::AggregationKeys&);

std::optional<attribution_reporting::AggregationKeys>
DeserializeAggregationKeys(sql::Statement&, int col);

std::string SerializeReportMetadata(const AttributionReport::EventLevelData&);

std::string SerializeReportMetadata(
    const AttributionReport::AggregatableAttributionData&);

std::string SerializeReportMetadata(
    const AttributionReport::NullAggregatableData&);

[[nodiscard]] bool DeserializeReportMetadata(base::span<const uint8_t>,
                                             uint32_t& trigger_data,
                                             int64_t& priority);

[[nodiscard]] bool DeserializeReportMetadata(
    base::span<const uint8_t>,
    AttributionReport::AggregatableAttributionData&);

[[nodiscard]] bool DeserializeReportMetadata(
    base::span<const uint8_t>,
    AttributionReport::NullAggregatableData&);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_UTILS_H_
