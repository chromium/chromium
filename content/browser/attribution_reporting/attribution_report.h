// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_

#include <stdint.h>

#include <vector>

#include "base/containers/enum_set.h"
#include "base/guid.h"
#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "base/values.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class GURL;

namespace content {

// Class that contains all the data needed to serialize and send an attribution
// report. This class can represent multiple different types of reports.
class CONTENT_EXPORT AttributionReport {
 public:
  enum class ReportType {
    kEventLevel = 0,
    kAggregatableAttribution = 1,
    kMinValue = kEventLevel,
    kMaxValue = kAggregatableAttribution,
  };

  using ReportTypes =
      base::EnumSet<ReportType, ReportType::kMinValue, ReportType::kMaxValue>;

  // Struct that contains the data specific to the event-level report.
  struct CONTENT_EXPORT EventLevelData {
    using Id = base::StrongAlias<EventLevelData, int64_t>;

    EventLevelData(uint64_t trigger_data,
                   int64_t priority,
                   double randomized_trigger_rate,
                   Id id);
    EventLevelData(const EventLevelData& other);
    EventLevelData& operator=(const EventLevelData& other);
    EventLevelData(EventLevelData&& other);
    EventLevelData& operator=(EventLevelData&& other);
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

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  // Struct that contains the data specific to the aggregatable report.
  struct CONTENT_EXPORT AggregatableAttributionData {
    using Id = base::StrongAlias<AggregatableAttributionData, int64_t>;

    AggregatableAttributionData(
        std::vector<AggregatableHistogramContribution> contributions,
        Id id,
        base::Time initial_report_time);
    AggregatableAttributionData(const AggregatableAttributionData&);
    AggregatableAttributionData& operator=(const AggregatableAttributionData&);
    AggregatableAttributionData(AggregatableAttributionData&&);
    AggregatableAttributionData& operator=(AggregatableAttributionData&&);
    ~AggregatableAttributionData();

    // Returns the sum of the contributions (values) across all buckets.
    base::CheckedNumeric<int64_t> BudgetRequired() const;

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

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  using Id = absl::variant<EventLevelData::Id, AggregatableAttributionData::Id>;

  static ReportType GetReportType(Id report_id) {
    return static_cast<ReportType>(report_id.index());
  }

  // Returns the minimum non-null time of `a` and `b`, or `absl::nullopt` if
  // both are null.
  static absl::optional<base::Time> MinReportTime(absl::optional<base::Time> a,
                                                  absl::optional<base::Time> b);

  AttributionReport(
      AttributionInfo attribution_info,
      base::Time report_time,
      base::GUID external_report_id,
      absl::variant<EventLevelData, AggregatableAttributionData> data);
  AttributionReport(const AttributionReport& other);
  AttributionReport& operator=(const AttributionReport& other);
  AttributionReport(AttributionReport&& other);
  AttributionReport& operator=(AttributionReport&& other);
  ~AttributionReport();

  // Returns the URL to which the report will be sent.
  GURL ReportURL(bool debug = false) const;

  base::Value::Dict ReportBody() const;

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

  ReportType GetReportType() const {
    return static_cast<ReportType>(data_.index());
  }

  void set_report_time(base::Time report_time);

  void set_failed_send_attempts(int failed_send_attempts);

  void SetExternalReportIdForTesting(base::GUID external_report_id);

 private:
  // The attribution info.
  AttributionInfo attribution_info_;

  // The time this conversion report should be sent.
  base::Time report_time_;

  // External report ID for deduplicating reports received by the reporting
  // origin.
  base::GUID external_report_id_;

  // Number of times the browser has tried and failed to send this report.
  int failed_send_attempts_ = 0;

  // Only one type of data may be stored at once.
  absl::variant<EventLevelData, AggregatableAttributionData> data_;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_
