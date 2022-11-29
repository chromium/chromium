// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DEBUG_REPORT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DEBUG_REPORT_H_

#include <vector>

#include "base/values.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace content {

class AttributionTrigger;
class CreateReportResult;
class StorableSource;

// Class that contains all the data needed to serialize and send an attribution
// debug report.
class CONTENT_EXPORT AttributionDebugReport {
 public:
  enum class DataType {
    kSourceDestinationLimit,
    kSourceNoised,
    kSourceStorageLimit,
    kSourceUnknownError,
    kTriggerNoMatchingSource,
    kTriggerAttributionsPerSourceDestinationLimit,
    kTriggerNoMatchingFilterData,
    kTriggerReportingOriginLimit,
    kTriggerEventDeduplicated,
    kTriggerEventNoMatchingConfigurations,
    kTriggerEventNoise,
    kTriggerEventLowPriority,
    kTriggerEventExcessiveReports,
    kTriggerEventStorageLimit,
    kTriggerEventReportWindowPassed,
    kTriggerAggregateDeduplicated,
    kTriggerAggregateNoContributions,
    kTriggerAggregateInsufficientBudget,
    kTriggerAggregateStorageLimit,
    kTriggerAggregateReportWindowPassed,
    kTriggerUnknownError,
  };

  static absl::optional<AttributionDebugReport> Create(
      const StorableSource& source,
      bool is_debug_cookie_set,
      const AttributionStorage::StoreSourceResult& result);

  static absl::optional<AttributionDebugReport> Create(
      const AttributionTrigger& trigger,
      bool is_debug_cookie_set,
      const CreateReportResult& result);

  ~AttributionDebugReport();

  AttributionDebugReport(const AttributionDebugReport&) = delete;
  AttributionDebugReport& operator=(const AttributionDebugReport&) = delete;

  AttributionDebugReport(AttributionDebugReport&&);
  AttributionDebugReport& operator=(AttributionDebugReport&&);

  base::Value::List ReportBody() const;

  GURL ReportURL() const;

 private:
  class ReportData;

  AttributionDebugReport(
      std::vector<ReportData> report_data,
      attribution_reporting::SuitableOrigin reporting_origin);

  std::vector<ReportData> report_data_;
  attribution_reporting::SuitableOrigin reporting_origin_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DEBUG_REPORT_H_
