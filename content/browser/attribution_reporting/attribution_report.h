// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"
#include "content/browser/attribution_reporting/stored_source.h"
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

  using Id = base::StrongAlias<AttributionReport, int64_t>;

  // Struct that contains the data specific to the event-level report.
  struct CONTENT_EXPORT EventLevelData {
    EventLevelData(uint32_t trigger_data, int64_t priority, StoredSource);
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

    StoredSource source;

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  struct CONTENT_EXPORT CommonAggregatableData {
    CommonAggregatableData(absl::optional<attribution_reporting::SuitableOrigin>
                               aggregation_coordinator_origin,
                           absl::optional<std::string> verification_token,
                           attribution_reporting::AggregatableTriggerConfig);
    CommonAggregatableData();
    CommonAggregatableData(const CommonAggregatableData&);
    CommonAggregatableData(CommonAggregatableData&&);
    CommonAggregatableData& operator=(const CommonAggregatableData&);
    CommonAggregatableData& operator=(CommonAggregatableData&&);
    ~CommonAggregatableData();

    // When updating the string, update the goldens and version history too, see
    // //content/test/data/attribution_reporting/aggregatable_report_goldens/README.md
    static constexpr char kVersion[] = "0.1";

    // Enum string identifying this API for use in reports.
    static constexpr char kApiIdentifier[] = "attribution-reporting";

    // The report assembled by the aggregation service. If null, the report has
    // not been assembled yet.
    absl::optional<AggregatableReport> assembled_report;

    absl::optional<attribution_reporting::SuitableOrigin>
        aggregation_coordinator_origin;

    // A token that can be sent alongside the report to complete its
    // verification.
    absl::optional<std::string> verification_token;

    attribution_reporting::AggregatableTriggerConfig
        aggregatable_trigger_config;

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  // Struct that contains the data specific to the aggregatable report.
  struct CONTENT_EXPORT AggregatableAttributionData {
    AggregatableAttributionData(
        CommonAggregatableData,
        std::vector<AggregatableHistogramContribution> contributions,
        StoredSource);
    AggregatableAttributionData(const AggregatableAttributionData&);
    AggregatableAttributionData& operator=(const AggregatableAttributionData&);
    AggregatableAttributionData(AggregatableAttributionData&&);
    AggregatableAttributionData& operator=(AggregatableAttributionData&&);
    ~AggregatableAttributionData();

    // Returns the sum of the contributions (values) across all buckets.
    base::CheckedNumeric<int64_t> BudgetRequired() const;

    CommonAggregatableData common_data;

    // The historgram contributions.
    std::vector<AggregatableHistogramContribution> contributions;

    StoredSource source;

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  struct CONTENT_EXPORT NullAggregatableData {
    NullAggregatableData(CommonAggregatableData,
                         attribution_reporting::SuitableOrigin reporting_origin,
                         base::Time fake_source_time);
    NullAggregatableData(const NullAggregatableData&);
    NullAggregatableData(NullAggregatableData&&);
    NullAggregatableData& operator=(const NullAggregatableData&);
    NullAggregatableData& operator=(NullAggregatableData&&);
    ~NullAggregatableData();

    CommonAggregatableData common_data;
    attribution_reporting::SuitableOrigin reporting_origin;
    base::Time fake_source_time;

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  using Data = absl::variant<EventLevelData,
                             AggregatableAttributionData,
                             NullAggregatableData>;

  // Returns the minimum non-null time of `a` and `b`, or `absl::nullopt` if
  // both are null.
  static absl::optional<base::Time> MinReportTime(absl::optional<base::Time> a,
                                                  absl::optional<base::Time> b);

  AttributionReport(AttributionInfo attribution_info,
                    Id id,
                    base::Time report_time,
                    base::Time initial_report_time,
                    base::Uuid external_report_id,
                    int failed_send_attempts,
                    Data data);
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

  const AttributionInfo& attribution_info() const { return attribution_info_; }

  Id id() const { return id_; }

  base::Time report_time() const { return report_time_; }

  base::Time initial_report_time() const { return initial_report_time_; }

  const base::Uuid& external_report_id() const { return external_report_id_; }

  int failed_send_attempts() const { return failed_send_attempts_; }

  const Data& data() const { return data_; }

  Data& data() { return data_; }

  Type GetReportType() const { return static_cast<Type>(data_.index()); }

  const StoredSource* GetStoredSource() const;

  const attribution_reporting::SuitableOrigin& GetReportingOrigin() const;

  void set_id(Id id) { id_ = id; }

  void set_report_time(base::Time report_time);

  void set_external_report_id(base::Uuid external_report_id);

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

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_
