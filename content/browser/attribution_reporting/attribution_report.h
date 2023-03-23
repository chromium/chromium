// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/guid.h"
#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "base/values.h"
#include "components/aggregation_service/aggregation_service.mojom-forward.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class GURL;

namespace net {
class HttpRequestHeaders;
}  // namespace net

namespace content {

// Class that contains all the data needed to serialize and send an attribution
// report. This class can represent multiple different types of reports.
class CONTENT_EXPORT AttributionReport {
 public:
  using Type = ::attribution_reporting::mojom::ReportType;

  // Struct that contains the data specific to the event-level report.
  struct CONTENT_EXPORT EventLevelData {
    using Id = base::StrongAlias<EventLevelData, int64_t>;

    EventLevelData(uint64_t trigger_data,
                   int64_t priority,
                   double randomized_trigger_rate,
                   Id id,
                   base::Time initial_report_time);
    EventLevelData(const EventLevelData&);
    EventLevelData& operator=(const EventLevelData&);
    EventLevelData(EventLevelData&&);
    EventLevelData& operator=(EventLevelData&&);
    ~EventLevelData();

    // Data provided at trigger time by the attribution destination. Depending
    // on the source type, this contains the associated data in the trigger
    // redirect.
    uint64_t trigger_data;

    // Priority specified in conversion redirect.
    int64_t priority;

    // Randomized trigger rate used at the time this report's source was
    // registered.
    double randomized_trigger_rate;

    // Id assigned by storage to uniquely identify a completed conversion.
    Id id;

    // The initial report time scheduled by the browser.
    // TODO(tquintanilla): Move to top level with aggregatable equivalent.
    base::Time initial_report_time;

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  // Struct that contains the data specific to the aggregatable report.
  struct CONTENT_EXPORT AggregatableAttributionData {
    using Id = base::StrongAlias<AggregatableAttributionData, int64_t>;

    AggregatableAttributionData(
        std::vector<AggregatableHistogramContribution> contributions,
        Id id,
        base::Time initial_report_time,
        ::aggregation_service::mojom::AggregationCoordinator
            aggregation_coordinator,
        absl::optional<std::string> attestation_token);
    AggregatableAttributionData(const AggregatableAttributionData&);
    AggregatableAttributionData& operator=(const AggregatableAttributionData&);
    AggregatableAttributionData(AggregatableAttributionData&&);
    AggregatableAttributionData& operator=(AggregatableAttributionData&&);
    ~AggregatableAttributionData();

    // Returns the sum of the contributions (values) across all buckets.
    base::CheckedNumeric<int64_t> BudgetRequired() const;

    // When updating the string, update the goldens and version history too, see
    // //content/test/data/attribution_reporting/aggregatable_report_goldens/README.md
    static constexpr char kVersion[] = "0.1";

    // Enum string identifying this API for use in reports.
    static constexpr char kApiIdentifier[] = "attribution-reporting";

    // The historgram contributions.
    std::vector<AggregatableHistogramContribution> contributions;

    // Id assigned by storage to uniquely identify an aggregatable contribution.
    Id id;

    // The report assembled by the aggregation service. If null, the report has
    // not been assembled yet.
    absl::optional<AggregatableReport> assembled_report;

    // The initial report time scheduled by the browser.
    base::Time initial_report_time;

    // A token that can be sent alongside the report to complete trigger
    // attestation.
    absl::optional<std::string> attestation_token;

    ::aggregation_service::mojom::AggregationCoordinator
        aggregation_coordinator;

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  using Id = absl::variant<EventLevelData::Id, AggregatableAttributionData::Id>;

  static Type GetReportType(Id report_id) {
    return static_cast<Type>(report_id.index());
  }

  // Returns the minimum non-null time of `a` and `b`, or `absl::nullopt` if
  // both are null.
  static absl::optional<base::Time> MinReportTime(absl::optional<base::Time> a,
                                                  absl::optional<base::Time> b);

  AttributionReport(
      AttributionInfo attribution_info,
      base::Time report_time,
      base::GUID external_report_id,
      int failed_send_attempts,
      absl::variant<EventLevelData, AggregatableAttributionData> data);
  AttributionReport(const AttributionReport&);
  AttributionReport& operator=(const AttributionReport&);
  AttributionReport(AttributionReport&&);
  AttributionReport& operator=(AttributionReport&&);
  ~AttributionReport();

  // Returns the URL to which the report will be sent.
  GURL ReportURL(bool debug = false) const;

  base::Value::Dict ReportBody() const;

  // Populate additional headers that should be sent alongside the report.
  void PopulateAdditionalHeaders(net::HttpRequestHeaders&) const;

  Id ReportId() const;

  const AttributionInfo& attribution_info() const { return attribution_info_; }

  base::Time report_time() const { return report_time_; }

  const base::GUID& external_report_id() const { return external_report_id_; }

  int failed_send_attempts() const { return failed_send_attempts_; }

  const absl::variant<EventLevelData, AggregatableAttributionData>& data()
      const {
    return data_;
  }

  absl::variant<EventLevelData, AggregatableAttributionData>& data() {
    return data_;
  }

  Type GetReportType() const { return static_cast<Type>(data_.index()); }

  void set_report_time(base::Time report_time);

  void SetExternalReportIdForTesting(base::GUID external_report_id);

  base::Time OriginalReportTime() const;

 private:
  // The attribution info.
  AttributionInfo attribution_info_;

  // The time this conversion report should be sent.
  base::Time report_time_;

  // External report ID for deduplicating reports received by the reporting
  // origin.
  base::GUID external_report_id_;

  // Number of times the browser has tried and failed to send this report.
  int failed_send_attempts_;

  // Only one type of data may be stored at once.
  absl::variant<EventLevelData, AggregatableAttributionData> data_;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_
