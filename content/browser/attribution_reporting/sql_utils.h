// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_UTILS_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_UTILS_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/trigger_data_matching.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom-forward.h"

namespace attribution_reporting {
class AggregatableTriggerConfig;
class AggregationKeys;
class AttributionScopesData;
class EventReportWindows;
class FilterData;
class MaxEventLevelReports;
class SuitableOrigin;
class TriggerSpecs;
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

class StoredSource;

url::Origin DeserializeOrigin(std::string_view origin);

std::optional<attribution_reporting::mojom::SourceType> DeserializeSourceType(
    int val);

// Exposed for use with earlier DB migrations that only contained a subset of
// fields.
void SetReadOnlySourceData(const attribution_reporting::EventReportWindows*,
                           attribution_reporting::MaxEventLevelReports,
                           proto::AttributionReadOnlySourceData&);

std::string SerializeReadOnlySourceData(
    const attribution_reporting::TriggerSpecs&,
    double randomized_response_rate,
    attribution_reporting::mojom::TriggerDataMatching,
    bool debug_cookie_set,
    absl::uint128 aggregatable_debug_key_piece);

CONTENT_EXPORT std::optional<proto::AttributionReadOnlySourceData>
DeserializeReadOnlySourceDataAsProto(sql::Statement&, int col);

std::string SerializeFilterData(const attribution_reporting::FilterData&);

std::optional<attribution_reporting::FilterData> DeserializeFilterData(
    sql::Statement&,
    int col);

std::optional<attribution_reporting::TriggerSpecs> DeserializeTriggerSpecs(
    const proto::AttributionReadOnlySourceData&,
    attribution_reporting::mojom::SourceType,
    attribution_reporting::MaxEventLevelReports);

std::string SerializeAggregationKeys(
    const attribution_reporting::AggregationKeys&);

std::optional<attribution_reporting::AggregationKeys>
DeserializeAggregationKeys(sql::Statement&, int col);

std::string SerializeEventLevelReportMetadata(uint32_t trigger_data,
                                              int64_t priority);

std::string SerializeAggregatableReportMetadata(
    const std::optional<attribution_reporting::SuitableOrigin>&
        aggregation_coordinator_origin,
    const attribution_reporting::AggregatableTriggerConfig&,
    const std::vector<blink::mojom::AggregatableReportHistogramContribution>&);

std::string SerializeNullAggregatableReportMetadata(
    const std::optional<attribution_reporting::SuitableOrigin>&
        aggregation_coordinator_origin,
    const attribution_reporting::AggregatableTriggerConfig&,
    base::Time fake_source_time);

std::optional<int64_t> DeserializeEventLevelPriority(base::span<const uint8_t>);

std::optional<AttributionReport::EventLevelData>
DeserializeEventLevelReportMetadata(base::span<const uint8_t>,
                                    const StoredSource&);

std::optional<AttributionReport::AggregatableAttributionData>
DeserializeAggregatableReportMetadata(base::span<const uint8_t>,
                                      const StoredSource&);

std::optional<AttributionReport::NullAggregatableData>
    DeserializeNullAggregatableReportMetadata(base::span<const uint8_t>);

std::string SerializeAttributionScopesData(
    const attribution_reporting::AttributionScopesData&);

base::expected<std::optional<attribution_reporting::AttributionScopesData>,
               absl::monostate>
DeserializeAttributionScopesData(sql::Statement&, int col);

void DeduplicateSourceIds(std::vector<StoredSource::Id>&);

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_SQL_UTILS_H_
