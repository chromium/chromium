// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_H_

#include <stdint.h>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class GUID;
class Time;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

class StorableTrigger;

// This class provides an interface for persisting attribution data to
// disk, and performing queries on it. AttributionStorage should initialize
// itself. Calls to a AttributionStorage instance that failed to initialize
// properly should result in no-ops.
class AttributionStorage {
 public:
  // The type of attribution used for rate limiting calculations.
  enum class AttributionType {
    kNavigation = 0,
    kEvent = 1,
    kAggregate = 2,
  };

  // Storage delegate that can supplied to extend basic attribution storage
  // functionality like annotating reports.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the time a report should be sent for a given trigger time and
    // its corresponding source.
    virtual base::Time GetReportTime(const StorableSource& source,
                                     base::Time trigger_time) const
        WARN_UNUSED_RESULT = 0;

    // This limit is used to determine if a source is allowed to schedule
    // a new report. When a source reaches this limit it is
    // marked inactive and no new reports will be created for it.
    // Sources will be checked against this limit after they schedule a new
    // report.
    virtual int GetMaxAttributionsPerSource(
        StorableSource::SourceType source_type) const WARN_UNUSED_RESULT = 0;

    // These limits are designed solely to avoid excessive disk / memory usage.
    // In particular, they do not correspond with any privacy parameters.
    // TODO(crbug.com/1082754): Consider replacing this functionality (and the
    // data deletion logic) with the quota system.
    //
    // Returns the maximum number of sources that can be in storage at any
    // time for a source top-level origin.
    virtual int GetMaxSourcesPerOrigin() const WARN_UNUSED_RESULT = 0;

    // Returns the maximum number of reports that can be in storage at any
    // time for an attribution top-level origin. Note that since reporting
    // origins are the actual entities that invoke attribution registration, we
    // could consider changing this limit to be keyed by an <attribution origin,
    // reporting origin> tuple.
    virtual int GetMaxAttributionsPerOrigin() const WARN_UNUSED_RESULT = 0;

    // Returns the maximum number of distinct attribution destinations that can
    // be in storage at any time for event sources with a given
    // reporting origin.
    virtual int GetMaxAttributionDestinationsPerEventSource() const
        WARN_UNUSED_RESULT = 0;

    struct RateLimitConfig {
      base::TimeDelta time_window;
      int64_t max_contributions_per_window;
    };

    // Returns the rate limits for capping contributions per window.
    virtual RateLimitConfig GetRateLimits(
        AttributionType attribution_type) const WARN_UNUSED_RESULT = 0;

    // Returns random data for falsely attributed event sources. Only present on
    // the delegate interface so it can be overridden to return deterministic
    // data in tests. The data must be sanitized in the same way it would be for
    // `AttributionPolicy::GetNoisedEventSourceTriggerData()`.
    virtual uint64_t GetFakeEventSourceTriggerData() const
        WARN_UNUSED_RESULT = 0;

    // Returns the maximum frequency at which to delete expired sources.
    // Must be positive.
    virtual base::TimeDelta GetDeleteExpiredSourcesFrequency() const
        WARN_UNUSED_RESULT = 0;

    // Returns the maximum frequency at which to delete expired rate limits.
    // Must be positive.
    virtual base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const
        WARN_UNUSED_RESULT = 0;

    // Returns a new report ID.
    virtual base::GUID NewReportID() const WARN_UNUSED_RESULT = 0;
  };

  struct CONTENT_EXPORT DeactivatedSource {
    enum class Reason {
      kReplacedByNewerSource,
      kReachedAttributionLimit,
    };

    DeactivatedSource(StorableSource source, Reason reason);
    ~DeactivatedSource();

    DeactivatedSource(const DeactivatedSource&);
    DeactivatedSource(DeactivatedSource&&);

    DeactivatedSource& operator=(const DeactivatedSource&);
    DeactivatedSource& operator=(DeactivatedSource&&);

    StorableSource source;
    Reason reason;
  };

  virtual ~AttributionStorage() = default;

  // When adding a new method, also add it to
  // AttributionStorageTest.StorageUsedAfterFailedInitilization_FailsSilently.

  // Add |source| to storage. Two sources are considered
  // matching when they share a <reporting origin, attribution destination>
  // pair. When a source is stored, all matching sources that have already
  // converted are marked as inactive, and are no longer eligible for reporting.
  // Unconverted matching sources are not modified.
  // Returns at most `deactivated_source_return_limit` deactivated sources, to
  // put an upper bound on memory usage; use a negative number for no limit.
  virtual std::vector<DeactivatedSource> StoreSource(
      const StorableSource& source,
      int deactivated_source_return_limit = -1) = 0;

  class CONTENT_EXPORT CreateReportResult {
   public:
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    enum class Status {
      kSuccess = 0,
      // The report was stored successfully, but it replaced an existing report
      // with a lower priority.
      kSuccessDroppedLowerPriority = 1,
      kInternalError = 2,
      kNoCapacityForConversionDestination = 3,
      kNoMatchingImpressions = 4,
      kDeduplicated = 5,
      kRateLimited = 6,
      kPriorityTooLow = 7,
      kDroppedForNoise = 8,
      kMaxValue = kDroppedForNoise,
    };

    explicit CreateReportResult(
        Status status,
        absl::optional<AttributionReport> dropped_report = absl::nullopt,
        absl::optional<DeactivatedSource::Reason>
            dropped_report_source_deactivation_reason = absl::nullopt);
    ~CreateReportResult();

    CreateReportResult(const CreateReportResult&);
    CreateReportResult(CreateReportResult&&);

    CreateReportResult& operator=(const CreateReportResult&);
    CreateReportResult& operator=(CreateReportResult&&);

    Status status() const;

    const absl::optional<AttributionReport>& dropped_report() const;

    absl::optional<DeactivatedSource> GetDeactivatedSource() const;

   private:
    Status status_;

    // Null unless `status` is `kSuccessDroppedLowerPriority`,
    // `kPriorityTooLow`, or `kDroppedForNoise`.
    absl::optional<AttributionReport> dropped_report_;

    // Null unless `dropped_report_`'s source was deactivated.
    absl::optional<DeactivatedSource::Reason>
        dropped_report_source_deactivation_reason_;
  };

  // Finds all stored sources matching a given `trigger`, and stores the
  // new associated report. Only active sources will receive new attributions.
  // Returns whether a new report has been scheduled/added to storage.
  virtual CreateReportResult MaybeCreateAndStoreReport(
      const StorableTrigger& trigger) = 0;

  // Returns all of the reports that should be sent before
  // |max_report_time|. This call is logically const, and does not modify the
  // underlying storage. |limit| limits the number of reports to return; use
  // a negative number for no limit.
  virtual std::vector<AttributionReport> GetAttributionsToReport(
      base::Time max_report_time,
      int limit = -1) WARN_UNUSED_RESULT = 0;

  // Returns all active sources in storage. Active sources are all
  // sources that can still convert. Sources that: are past expiry,
  // reached the attribution limit, or was marked inactive due to having
  // trigger and then superceded by a matching source should not be
  // returned. |limit| limits the number of sources to return; use
  // a negative number for no limit.
  virtual std::vector<StorableSource> GetActiveSources(int limit = -1)
      WARN_UNUSED_RESULT = 0;

  // Deletes the report with the given |report_id|. Returns
  // false if an error occurred.
  virtual bool DeleteReport(AttributionReport::Id report_id) = 0;

  // Updates the number of failures associated with the given report, and sets
  // its report time to the given value. Should be called after a transient
  // failure to send the report so that it is retried later.
  virtual bool UpdateReportForSendFailure(AttributionReport::Id report_id,
                                          base::Time new_report_time) = 0;

  // Deletes all data in storage for URLs matching |filter|, between
  // |delete_begin| and |delete_end| time. More specifically, this:
  // 1. Deletes all sources within the time range. If any report is
  //    attributed to this source it is also deleted.
  // 2. Deletes all reports within the time range. All sources
  //    attributed to the report are also deleted.
  //
  // Note: if |filter| is null, it means that all Origins should match.
  virtual void ClearData(
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const url::Origin& origin)> filter) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_H_
