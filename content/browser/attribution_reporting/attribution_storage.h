// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_H_

#include <stdint.h>
#include <vector>

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class GUID;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

class StorableSource;
class AttributionTrigger;

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
  };

  // Storage delegate that can supplied to extend basic attribution storage
  // functionality like annotating reports.
  class Delegate {
   public:
    struct RateLimitConfig {
      base::TimeDelta time_window;
      int64_t max_contributions_per_window;
    };

    // Both bounds are inclusive.
    struct OfflineReportDelayConfig {
      base::TimeDelta min;
      base::TimeDelta max;
    };

    struct FakeReport {
      uint64_t trigger_data;
      base::Time report_time;
    };

    // Corresponds to `StoredSource::AttributionLogic` as follows:
    // `absl::nullopt` -> `StoredSource::AttributionLogic::kTruthfully`
    // empty vector -> `StoredSource::AttributionLogic::kNever`
    // non-empty vector -> `StoredSource::AttributionLogic::kFalsely`
    using RandomizedResponse = absl::optional<std::vector<FakeReport>>;

    virtual ~Delegate() = default;

    // Returns the time a report should be sent for a given trigger time and
    // its corresponding source.
    virtual base::Time GetReportTime(const CommonSourceInfo& source,
                                     base::Time trigger_time) const = 0;

    // This limit is used to determine if a source is allowed to schedule
    // a new report. When a source reaches this limit it is
    // marked inactive and no new reports will be created for it.
    // Sources will be checked against this limit after they schedule a new
    // report.
    virtual int GetMaxAttributionsPerSource(
        CommonSourceInfo::SourceType source_type) const = 0;

    // These limits are designed solely to avoid excessive disk / memory usage.
    // In particular, they do not correspond with any privacy parameters.
    // TODO(crbug.com/1082754): Consider replacing this functionality (and the
    // data deletion logic) with the quota system.
    //
    // Returns the maximum number of sources that can be in storage at any
    // time for a source top-level origin.
    virtual int GetMaxSourcesPerOrigin() const = 0;

    // Returns the maximum number of reports that can be in storage at any
    // time for an attribution top-level origin. Note that since reporting
    // origins are the actual entities that invoke attribution registration, we
    // could consider changing this limit to be keyed by an <attribution origin,
    // reporting origin> tuple.
    virtual int GetMaxAttributionsPerOrigin() const = 0;

    // Returns the maximum number of distinct attribution destinations that can
    // be in storage at any time for sources with the same <source site,
    // reporting origin>.
    virtual int GetMaxDestinationsPerSourceSiteReportingOrigin() const = 0;

    // Returns the rate limits for capping contributions per window.
    virtual RateLimitConfig GetRateLimits(
        AttributionType attribution_type) const = 0;

    // Returns the maximum frequency at which to delete expired sources.
    // Must be positive.
    virtual base::TimeDelta GetDeleteExpiredSourcesFrequency() const = 0;

    // Returns the maximum frequency at which to delete expired rate limits.
    // Must be positive.
    virtual base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const = 0;

    // Returns a new report ID.
    virtual base::GUID NewReportID() const = 0;

    // Delays reports that missed their report time, such as the browser not
    // being open, or internet being disconnected. This gives them a noisy
    // report time to help disassociate them from other reports. Returns null if
    // no delay should be applied, e.g. due to debug mode.
    virtual absl::optional<OfflineReportDelayConfig>
    GetOfflineReportDelayConfig() const = 0;

    // Shuffles reports to provide plausible deniability on the ordering of
    // reports that share the same |report_time|. This is important because
    // multiple conversions for the same impression share the same report time
    // if they are within the same reporting window, and we do not want to allow
    // ordering on their conversion metadata bits.
    virtual void ShuffleReports(
        std::vector<AttributionReport>& reports) const = 0;

    virtual RandomizedResponse GetRandomizedResponse(
        const CommonSourceInfo& source) const = 0;
  };

  struct CONTENT_EXPORT DeactivatedSource {
    enum class Reason {
      kReplacedByNewerSource,
      kReachedAttributionLimit,
    };

    DeactivatedSource(StoredSource source, Reason reason);
    ~DeactivatedSource();

    DeactivatedSource(const DeactivatedSource&);
    DeactivatedSource(DeactivatedSource&&);

    DeactivatedSource& operator=(const DeactivatedSource&);
    DeactivatedSource& operator=(DeactivatedSource&&);

    StoredSource source;
    Reason reason;
  };

  struct CONTENT_EXPORT StoreSourceResult {
    enum class Status {
      kSuccess,
      kInternalError,
      kInsufficientSourceCapacity,
      kInsufficientUniqueDestinationCapacity,
    };

    explicit StoreSourceResult(
        Status status,
        std::vector<DeactivatedSource> deactivated_sources = {});

    ~StoreSourceResult();

    StoreSourceResult(const StoreSourceResult&);
    StoreSourceResult(StoreSourceResult&&);

    StoreSourceResult& operator=(const StoreSourceResult&);
    StoreSourceResult& operator=(StoreSourceResult&&);

    Status status;
    std::vector<DeactivatedSource> deactivated_sources;
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
  virtual StoreSourceResult StoreSource(
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
            dropped_report_source_deactivation_reason = absl::nullopt,
        absl::optional<base::Time> report_time = absl::nullopt);
    ~CreateReportResult();

    CreateReportResult(const CreateReportResult&);
    CreateReportResult(CreateReportResult&&);

    CreateReportResult& operator=(const CreateReportResult&);
    CreateReportResult& operator=(CreateReportResult&&);

    Status status() const;

    const absl::optional<AttributionReport>& dropped_report() const;

    absl::optional<base::Time> report_time() const;

    absl::optional<DeactivatedSource> GetDeactivatedSource() const;

   private:
    Status status_;

    // Null unless `status` is `kSuccessDroppedLowerPriority`,
    // `kRateLimited`, `kPriorityTooLow`, or `kDroppedForNoise`.
    absl::optional<AttributionReport> dropped_report_;

    // Null unless `dropped_report_`'s source was deactivated.
    absl::optional<DeactivatedSource::Reason>
        dropped_report_source_deactivation_reason_;

    // Null unless `status` is `kSuccess` or `kSuccessDroppedLowerPriority`.
    absl::optional<base::Time> report_time_;
  };

  // Finds all stored sources matching a given `trigger`, and stores the
  // new associated report. Only active sources will receive new attributions.
  // Returns whether a new report has been scheduled/added to storage.
  virtual CreateReportResult MaybeCreateAndStoreReport(
      const AttributionTrigger& trigger) = 0;

  // Returns all of the reports that should be sent before
  // |max_report_time|. This call is logically const, and does not modify the
  // underlying storage. |limit| limits the number of reports to return; use
  // a negative number for no limit. Reports are shuffled before being returned.
  virtual std::vector<AttributionReport> GetAttributionsToReport(
      base::Time max_report_time,
      int limit = -1) = 0;

  // Returns the first report time strictly after `time`.
  virtual absl::optional<base::Time> GetNextReportTime(base::Time time) = 0;

  // Returns the reports with the given IDs. This call is logically const, and
  // does not modify the underlying storage.
  virtual std::vector<AttributionReport> GetReports(
      const std::vector<AttributionReport::EventLevelData::Id>& ids) = 0;

  // Returns all active sources in storage. Active sources are all
  // sources that can still convert. Sources that: are past expiry,
  // reached the attribution limit, or was marked inactive due to having
  // trigger and then superceded by a matching source should not be
  // returned. |limit| limits the number of sources to return; use
  // a negative number for no limit.
  virtual std::vector<StoredSource> GetActiveSources(int limit = -1) = 0;

  // Deletes the report with the given |report_id|. Returns
  // false if an error occurred.
  [[nodiscard]] virtual bool DeleteReport(
      AttributionReport::EventLevelData::Id report_id) = 0;

  // Updates the number of failures associated with the given report, and sets
  // its report time to the given value. Should be called after a transient
  // failure to send the report so that it is retried later.
  [[nodiscard]] virtual bool UpdateReportForSendFailure(
      AttributionReport::EventLevelData::Id report_id,
      base::Time new_report_time) = 0;

  // Adjusts the report time of all reports that should have been sent while the
  // browser was offline, according to
  // `AttributionStorage::Delegate::GetOfflineReportDelayConfig()`. If that
  // method returns null, no delay is applied. Otherwise, applies a random value
  // between `min_delay` and `max_delay`, both inclusive. Returns the new first
  // report time in storage, if any.
  virtual absl::optional<base::Time> AdjustOfflineReportTimes() = 0;

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
