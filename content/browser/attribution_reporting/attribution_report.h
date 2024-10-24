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
    double randomized_response_rate;
    bool attributed_truthfully;

    friend bool operator==(const EventLevelData&,
                           const EventLevelData&) = default;
  };

  class CONTENT_EXPORT AggregatableData {
   public:
    AggregatableData(
        std::optional<attribution_reporting::SuitableOrigin>
            aggregation_coordinator_origin,
        attribution_reporting::AggregatableTriggerConfig,
        base::Time source_time,
        std::vector<blink::mojom::AggregatableReportHistogramContribution>
            contributions,
        std::optional<attribution_reporting::SuitableOrigin> source_origin);
    AggregatableData(const AggregatableData&);
    AggregatableData(AggregatableData&&);
    AggregatableData& operator=(const AggregatableData&);
    AggregatableData& operator=(AggregatableData&&);
    ~AggregatableData();

    // When updating the string, update the goldens and version history too, see
    // //content/test/data/attribution_reporting/aggregatable_report_goldens/README.md
    static constexpr char kVersion[] = "0.1";
    static constexpr char kVersionWithFlexibleContributionFiltering[] = "1.0";

    // Enum string identifying this API for use in reports.
    static constexpr char kApiIdentifier[] = "attribution-reporting";

    const std::optional<AggregatableReport>& assembled_report() const {
      return assembled_report_;
    }

    void SetAssembledReport(std::optional<AggregatableReport>);

    const std::optional<attribution_reporting::SuitableOrigin>&
    aggregation_coordinator_origin() const {
      return aggregation_coordinator_origin_;
    }

    const attribution_reporting::AggregatableTriggerConfig&
    aggregatable_trigger_config() const {
      return aggregatable_trigger_config_;
    }

    base::Time source_time() const { return source_time_; }

    const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
    contributions() const {
      return contributions_;
    }

    void SetContributions(
        std::vector<blink::mojom::AggregatableReportHistogramContribution>);

    const std::optional<attribution_reporting::SuitableOrigin>& source_origin()
        const {
      return source_origin_;
    }

    // Returns the sum of the contributions (values) across all buckets.
    base::CheckedNumeric<int64_t> BudgetRequired() const;

    bool is_null() const { return !source_origin_.has_value(); }

   private:
    // The report assembled by the aggregation service. If null, the report has
    // not been assembled yet.
    std::optional<AggregatableReport> assembled_report_;

    std::optional<attribution_reporting::SuitableOrigin>
        aggregation_coordinator_origin_;

    attribution_reporting::AggregatableTriggerConfig
        aggregatable_trigger_config_;

    base::Time source_time_;

    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions_;

    std::optional<attribution_reporting::SuitableOrigin> source_origin_;

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  using Data = absl::variant<EventLevelData, AggregatableData>;

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
                    attribution_reporting::SuitableOrigin reporting_origin,
                    std::optional<uint64_t> source_debug_key);
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

  Type GetReportType() const;

  std::optional<uint64_t> source_debug_key() const { return source_debug_key_; }

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

  std::optional<uint64_t> source_debug_key_;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_
