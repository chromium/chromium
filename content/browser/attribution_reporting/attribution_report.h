// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom-forward.h"

class GURL;

namespace content {

class StoredSource;

// Class that contains all the data needed to serialize and send an attribution
// report. This class can represent multiple different types of reports.
class CONTENT_EXPORT AttributionReport {
 public:
  using Type = ::attribution_reporting::mojom::ReportType;

  using Id = base::StrongAlias<AttributionReport, int64_t>;

  // Struct that contains the data specific to the event-level report.
  struct CONTENT_EXPORT EventLevelData {
    EventLevelData(uint32_t trigger_data,
                   int64_t priority,
                   const StoredSource&);
    EventLevelData(const EventLevelData&);
    EventLevelData& operator=(const EventLevelData&);
    EventLevelData(EventLevelData&&);
    EventLevelData& operator=(EventLevelData&&);
    ~EventLevelData();

    // Data provided at trigger time by the attribution destination. Depending
    // on the source type, this contains the associated data in the trigger
    // redirect.
    uint32_t trigger_data;

    // Priority specified in conversion redirect.
    int64_t priority;

    attribution_reporting::SuitableOrigin source_origin;
    attribution_reporting::DestinationSet destinations;
    uint64_t source_event_id;
    attribution_reporting::mojom::SourceType source_type;
    std::optional<uint64_t> source_debug_key;
    double randomized_response_rate;
    bool attributed_truthfully;

    friend bool operator==(const EventLevelData&,
                           const EventLevelData&) = default;
  };

  struct CONTENT_EXPORT CommonAggregatableData {
    CommonAggregatableData(std::optional<attribution_reporting::SuitableOrigin>
                               aggregation_coordinator_origin,
                           attribution_reporting::AggregatableTriggerConfig);
    CommonAggregatableData(const CommonAggregatableData&);
    CommonAggregatableData(CommonAggregatableData&&);
    CommonAggregatableData& operator=(const CommonAggregatableData&);
    CommonAggregatableData& operator=(CommonAggregatableData&&);
    ~CommonAggregatableData();

    // When updating the string, update the goldens and version history too, see
    // //content/test/data/attribution_reporting/aggregatable_report_goldens/README.md
    static constexpr char kVersion[] = "0.1";
    static constexpr char kVersionWithFlexibleContributionFiltering[] = "1.0";

    // Enum string identifying this API for use in reports.
    static constexpr char kApiIdentifier[] = "attribution-reporting";

    // The report assembled by the aggregation service. If null, the report has
    // not been assembled yet.
    std::optional<AggregatableReport> assembled_report;

    std::optional<attribution_reporting::SuitableOrigin>
        aggregation_coordinator_origin;

    attribution_reporting::AggregatableTriggerConfig
        aggregatable_trigger_config;

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  // Struct that contains the data specific to the aggregatable report.
  struct CONTENT_EXPORT AggregatableAttributionData {
    AggregatableAttributionData(
        CommonAggregatableData,
        std::vector<blink::mojom::AggregatableReportHistogramContribution>
            contributions,
        const StoredSource&);
    AggregatableAttributionData(const AggregatableAttributionData&);
    AggregatableAttributionData& operator=(const AggregatableAttributionData&);
    AggregatableAttributionData(AggregatableAttributionData&&);
    AggregatableAttributionData& operator=(AggregatableAttributionData&&);
    ~AggregatableAttributionData();

    // Returns the sum of the contributions (values) across all buckets.
    base::CheckedNumeric<int64_t> BudgetRequired() const;

    CommonAggregatableData common_data;

    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions;

    base::Time source_time;
    std::optional<uint64_t> source_debug_key;
    attribution_reporting::SuitableOrigin source_origin;

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  struct CONTENT_EXPORT NullAggregatableData {
    NullAggregatableData(CommonAggregatableData,
                         base::Time fake_source_time);
    NullAggregatableData(const NullAggregatableData&);
    NullAggregatableData(NullAggregatableData&&);
    NullAggregatableData& operator=(const NullAggregatableData&);
    NullAggregatableData& operator=(NullAggregatableData&&);
    ~NullAggregatableData();

    CommonAggregatableData common_data;
    base::Time fake_source_time;

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  using Data = absl::variant<EventLevelData,
                             AggregatableAttributionData,
                             NullAggregatableData>;

  // Returns the minimum non-null time of `a` and `b`, or `std::nullopt` if
  // both are null.
  static std::optional<base::Time> MinReportTime(std::optional<base::Time> a,
                                                 std::optional<base::Time> b);

  AttributionReport(AttributionInfo attribution_info,
                    Id id,
                    base::Time report_time,
                    base::Time initial_report_time,
                    base::Uuid external_report_id,
                    int failed_send_attempts,
                    Data data,
                    attribution_reporting::SuitableOrigin reporting_origin);
  AttributionReport(const AttributionReport&);
  AttributionReport& operator=(const AttributionReport&);
  AttributionReport(AttributionReport&&);
  AttributionReport& operator=(AttributionReport&&);
  ~AttributionReport();

  // Returns the URL to which the report will be sent.
  GURL ReportURL(bool debug = false) const;

  base::Value::Dict ReportBody() const;

  const AttributionInfo& attribution_info() const { return attribution_info_; }

  Id id() const { return id_; }

  base::Time report_time() const { return report_time_; }

  base::Time initial_report_time() const { return initial_report_time_; }

  const base::Uuid& external_report_id() const { return external_report_id_; }

  int failed_send_attempts() const { return failed_send_attempts_; }

  const Data& data() const { return data_; }

  Data& data() { return data_; }

  Type GetReportType() const { return static_cast<Type>(data_.index()); }

  std::optional<uint64_t> GetSourceDebugKey() const;

  const attribution_reporting::SuitableOrigin& reporting_origin() const {
    return reporting_origin_;
  }

  // For null aggregatable reports, this is the same as
  // `AttributionInfo::context_origin` since there is no attributed source.
  const attribution_reporting::SuitableOrigin& GetSourceOrigin() const;

  void set_id(Id id) { id_ = id; }

  void set_report_time(base::Time report_time);

  bool CanDebuggingBeEnabled() const;

 private:
  // The attribution info.
  AttributionInfo attribution_info_;

  // Id assigned by storage to uniquely identify an attribution report.
  Id id_;

  // The time this conversion report should be sent.
  base::Time report_time_;

  // The originally calculated time the report should be sent.
  base::Time initial_report_time_;

  // External report ID for deduplicating reports received by the reporting
  // origin.
  base::Uuid external_report_id_;

  // Number of times the browser has tried and failed to send this report.
  int failed_send_attempts_;

  // Only one type of data may be stored at once.
  Data data_;

  attribution_reporting::SuitableOrigin reporting_origin_;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_
