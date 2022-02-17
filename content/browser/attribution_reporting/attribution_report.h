// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_

#include <stdint.h>

#include "base/guid.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "content/browser/attribution_reporting/aggregatable_attribution.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class GURL;

namespace base {
class Value;
}  // namespace base

namespace content {

// Class that contains all the data needed to serialize and send an attribution
// report. This class can represent multiple different types of reports.
class CONTENT_EXPORT AttributionReport {
 public:
  // Struct that contains the data specific to the event-level report.
  struct CONTENT_EXPORT EventLevelData {
    using Id = base::StrongAlias<EventLevelData, int64_t>;

    EventLevelData(uint64_t trigger_data,
                   int64_t priority,
                   absl::optional<Id> id);
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

    // Id assigned by storage to uniquely identify a completed conversion. If
    // null, an ID has not been assigned yet.
    absl::optional<Id> id;

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  // Struct that contains the data specific to the aggregatable report.
  struct CONTENT_EXPORT AggregatableContributionData {
    using Id = base::StrongAlias<AggregatableContributionData, int64_t>;

    AggregatableContributionData(HistogramContribution contribution,
                                 absl::optional<Id> id);
    AggregatableContributionData(const AggregatableContributionData& other);
    AggregatableContributionData& operator=(
        const AggregatableContributionData& other);
    AggregatableContributionData(AggregatableContributionData&& other);
    AggregatableContributionData& operator=(
        AggregatableContributionData&& other);
    ~AggregatableContributionData();

    // The historgram contribution.
    HistogramContribution contribution;

    // Id assigned by storage to uniquely identify an aggregatable contribution.
    // If null, an ID has not been assigned yet.
    absl::optional<Id> id;

    // When adding new members, the corresponding `operator==()` definition in
    // `attribution_test_utils.h` should also be updated.
  };

  using Id =
      absl::variant<EventLevelData::Id, AggregatableContributionData::Id>;

  AttributionReport(
      AttributionInfo attribution_info,
      base::Time report_time,
      base::GUID external_report_id,
      absl::variant<EventLevelData, AggregatableContributionData> data);
  AttributionReport(const AttributionReport& other);
  AttributionReport& operator=(const AttributionReport& other);
  AttributionReport(AttributionReport&& other);
  AttributionReport& operator=(AttributionReport&& other);
  ~AttributionReport();

  // Returns the URL to which the report will be sent.
  GURL ReportURL() const;

  base::Value ReportBody() const;

  absl::optional<Id> ReportId() const;

  const AttributionInfo& attribution_info() const { return attribution_info_; }

  base::Time report_time() const { return report_time_; }

  const base::GUID& external_report_id() const { return external_report_id_; }

  int failed_send_attempts() const { return failed_send_attempts_; }

  const absl::variant<EventLevelData, AggregatableContributionData>& data()
      const {
    return data_;
  }

  absl::variant<EventLevelData, AggregatableContributionData>& data() {
    return data_;
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
  absl::variant<EventLevelData, AggregatableContributionData> data_;

  // When adding new members, the corresponding `operator==()` definition in
  // `attribution_test_utils.h` should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_REPORT_H_
