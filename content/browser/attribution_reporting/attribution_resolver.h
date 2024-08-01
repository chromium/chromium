// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RESOLVER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RESOLVER_H_

#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/storage_partition.h"

namespace base {
class Time;
}  // namespace base

namespace content {

class AggregatableDebugReport;
class AttributionResolverDelegate;
class AttributionTrigger;
class CreateReportResult;
class StorableSource;
class StoreSourceResult;

struct ProcessAggregatableDebugReportResult;

// This class provides an interface for persisting attribution data to
// disk, and performing queries on it. AttributionResolver should initialize
// itself. Calls to a AttributionResolver instance that failed to initialize
// properly should result in no-ops.
class AttributionResolver {
 public:
  virtual ~AttributionResolver() = default;

  // When adding a new method, also add it to
  // AttributionResolverTest.StorageUsedAfterFailedInitialization_NoCrash.

  // Add |source| to storage. Two sources are considered
  // matching when they share a <reporting origin, attribution destination>
  // pair. When a source is stored, all matching sources that have already
  // converted are marked as inactive, and are no longer eligible for reporting.
  // Unconverted matching sources are not modified.
  virtual StoreSourceResult StoreSource(StorableSource source) = 0;

  // Finds all stored sources matching a given `trigger`, and creates a
  // new associated report. Only active sources will receive new attributions.
  // Returns whether a new report has been scheduled/added to storage.
  virtual CreateReportResult MaybeCreateAndStoreReport(AttributionTrigger) = 0;

  // Returns all of the reports that should be sent before
  // |max_report_time|. |limit| limits the number of reports to return; use
  // a negative number for no limit. Reports are shuffled before being returned. This call is logically const and does not mutate reports.
  virtual std::vector<AttributionReport> GetAttributionReports(
      base::Time max_report_time,
      int limit = -1) = 0;

  // Returns the first report time strictly after `time`.
  virtual std::optional<base::Time> GetNextReportTime(base::Time time) = 0;

  // Returns the report with the given ID. This call is logically const, and
  // does not mutate reports.
  virtual std::optional<AttributionReport> GetReport(AttributionReport::Id) = 0;

  // Returns all active sources in storage. Active sources are all
  // sources that can still convert. Sources that: are past expiry,
  // reached the attribution limit, or was marked inactive due to having
  // trigger and then superceded by a matching source should not be
  // returned. |limit| limits the number of sources to return; use
  // a negative number for no limit.
  virtual std::vector<StoredSource> GetActiveSources(int limit = -1) = 0;

  // Returns all distinct reporting origins for the
  // Browsing Data Model. Partial data will still be returned
  // in the event of an error.
  virtual std::set<AttributionDataModel::DataKey> GetAllDataKeys() = 0;

  // Deletes all data for storage keys matching the provided
  // reporting origin in the data key.
  virtual void DeleteByDataKey(const AttributionDataModel::DataKey&) = 0;

  // Deletes the report with the given |report_id|. Returns
  // false if an error occurred.
  [[nodiscard]] virtual bool DeleteReport(AttributionReport::Id report_id) = 0;

  // Updates the number of failures associated with the given report, and sets
  // its report time to the given value. Should be called after a transient
  // failure to send the report so that it is retried later.
  [[nodiscard]] virtual bool UpdateReportForSendFailure(
      AttributionReport::Id report_id,
      base::Time new_report_time) = 0;

  // Adjusts the report time of all reports that should have been sent while the
  // browser was offline, according to
  // `AttributionResolverDelegate::GetOfflineReportDelayConfig()`. If that
  // method returns null, no delay is applied. Otherwise, applies a random value
  // between `min_delay` and `max_delay`, both inclusive. Returns the new first
  // report time in storage, if any.
  virtual std::optional<base::Time> AdjustOfflineReportTimes() = 0;

  // Deletes all data in storage for reporting origins matching `filter`,
  // between `delete_begin` and `delete_end` time. More specifically, this:
  // 1. Deletes all sources within the time range. If any report is
  //    attributed to this source it is also deleted.
  // 2. Deletes all reports within the time range. All sources
  //    attributed to the report are also deleted.
  // 3. Deletes any rate limits matching `filter` or whose corresponding source
  //    was deleted.
  //
  // Note: if `filter` is null, it means that all reporting origins should
  // match.
  virtual void ClearData(base::Time delete_begin,
                         base::Time delete_end,
                         StoragePartition::StorageKeyMatcherFunction filter,
                         bool delete_rate_limit_data = true) = 0;

  // Processes the specified aggregatable debug report. Converts the report to
  // a null report if it's not allowed by the limits, otherwise adjusts the
  // limits. `remaining_budget` is set for source debug reports and
  // `std::nullopt` for trigger debug reports. `source_id` is set when there is
  // an associated source with the debug report, i.e. when the source is
  // successfully stored or when there is a matching source with the trigger.
  virtual ProcessAggregatableDebugReportResult ProcessAggregatableDebugReport(
      AggregatableDebugReport,
      std::optional<int> remaining_budget,
      std::optional<StoredSource::Id> source_id) = 0;

  virtual void SetDelegate(std::unique_ptr<AttributionResolverDelegate>) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RESOLVER_H_
