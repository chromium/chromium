// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_H_

#include <stdint.h>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "content/browser/conversions/storable_impression.h"

namespace base {
class Time;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace content {

class StorableConversion;

struct ConversionReport;

// This class provides an interface for persisting impression/conversion data to
// disk, and performing queries on it. ConversionStorage should initialize
// itself. Calls to a ConversionStorage instance that failed to initialize
// properly should result in no-ops.
class ConversionStorage {
 public:
  // Storage delegate that can supplied to extend basic conversion storage
  // functionality like annotating conversion reports.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // New conversion reports will be sent through this callback to determine
    // their report time before they are added to storage. This will be called
    // during the execution of
    // `ConversionStorage::MaybeCreateAndStoreConversionReport()`.
    // The report will be pre-populated from storage with the conversion
    // event data.
    virtual base::Time GetReportTime(const ConversionReport& report) const
        WARN_UNUSED_RESULT = 0;

    // This limit is used to determine if an impression is allowed to schedule
    // a new conversion reports. When an impression reaches this limit it is
    // marked inactive and no new conversion reports will be created for it.
    // Impressions will be checked against this limit after they schedule a new
    // report.
    virtual int GetMaxConversionsPerImpression(
        StorableImpression::SourceType source_type) const
        WARN_UNUSED_RESULT = 0;

    // These limits are designed solely to avoid excessive disk / memory usage.
    // In particular, they do not correspond with any privacy parameters.
    // TODO(crbug.com/1082754): Consider replacing this functionality (and the
    // data deletion logic) with the quota system.
    //
    // Returns the maximum number of impressions that can be in storage at any
    // time for an impression top-level origin.
    virtual int GetMaxImpressionsPerOrigin() const WARN_UNUSED_RESULT = 0;

    //  Returns the maximum number of conversions that can be in storage at any
    //  time for a conversion top-level origin. Note that since reporting
    //  origins are the actual entities that invoke conversion registration, we
    //  could consider changing this limit to be keyed by a <conversion origin,
    //  reporting origin> tuple.
    virtual int GetMaxConversionsPerOrigin() const WARN_UNUSED_RESULT = 0;

    // Returns the maximum number of distinct conversion destinations that can
    // be in storage at any time for event-source impressions with a given
    // reporting origin.
    virtual int GetMaxAttributionDestinationsPerEventSource() const
        WARN_UNUSED_RESULT = 0;

    struct RateLimitConfig {
      base::TimeDelta time_window;
      int max_attributions_per_window;
    };

    // Returns the rate limits for capping attributions per window.
    virtual RateLimitConfig GetRateLimits() const WARN_UNUSED_RESULT = 0;

    // Selects how to handle the given impression; may involve RNG or other
    // dynamic criteria.
    virtual StorableImpression::AttributionLogic SelectAttributionLogic(
        const StorableImpression& impression) const WARN_UNUSED_RESULT = 0;

    // Returns random data for falsely attributed event sources. Only present on
    // the delegate interface so it can be overridden to return deterministic
    // data in tests. The data must be sanitized in the same way it would be for
    // `ConversionPolicy::GetNoisedEventSourceTriggerData()`.
    virtual uint64_t GetFakeEventSourceTriggerData() const
        WARN_UNUSED_RESULT = 0;

    // Returns the maximum frequency at which to delete expired impressions.
    // Must be positive.
    virtual base::TimeDelta GetDeleteExpiredImpressionsFrequency() const
        WARN_UNUSED_RESULT = 0;

    // Returns the maximum frequency at which to delete expired rate limits.
    // Must be positive.
    virtual base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const
        WARN_UNUSED_RESULT = 0;
  };
  virtual ~ConversionStorage() = default;

  // When adding a new method, also add it to
  // ConversionStorageTest.StorageUsedAfterFailedInitilization_FailsSilently.

  // Add |impression| to storage. Two impressions are considered
  // matching when they share a <reporting_origin, conversion_origin> pair. When
  // an impression is stored, all matching impressions that have
  // already converted are marked as inactive, and are no longer eligible for
  // reporting. Unconverted matching impressions are not modified.
  virtual void StoreImpression(const StorableImpression& impression) = 0;

  // Finds all stored impressions matching a given `conversion`, and stores the
  // new associated conversion report. The delegate will receive a call to
  // `Delegate::ProcessNewConversionReports()` before the report is added to
  // storage. Only active impressions will receive new conversions. Returns
  // whether a new conversion report has been scheduled/added to storage.
  virtual bool MaybeCreateAndStoreConversionReport(
      const StorableConversion& conversion) = 0;

  // Returns all of the conversion reports that should be sent before
  // |max_report_time|. This call is logically const, and does not modify the
  // underlying storage. |limit| limits the number of conversions to return; use
  // a negative number for no limit.
  virtual std::vector<ConversionReport> GetConversionsToReport(
      base::Time max_report_time,
      int limit = -1) WARN_UNUSED_RESULT = 0;

  // Returns all active impressions in storage. Active impressions are all
  // impressions that can still convert. Impressions that: are past expiry,
  // reached the conversion limit, or was marked inactive due to having
  // converted and then superceded by a matching impression should not be
  // returned. |limit| limits the number of impressions to return; use
  // a negative number for no limit.
  virtual std::vector<StorableImpression> GetActiveImpressions(int limit = -1)
      WARN_UNUSED_RESULT = 0;

  // Deletes the conversion report with the given |conversion_id|. Returns
  // whether the deletion was successful.
  virtual bool DeleteConversion(int64_t conversion_id) = 0;

  // Deletes all data in storage for URLs matching |filter|, between
  // |delete_begin| and |delete_end| time. More specifically, this:
  // 1. Deletes all impressions within the time range. If any conversion is
  //    attributed to this impression it is also deleted.
  // 2. Deletes all conversions within the time range. All impressions
  //    attributed to the conversion are also deleted.
  //
  // Note: if |filter| is null, it means that all Origins should match.
  virtual void ClearData(
      base::Time delete_begin,
      base::Time delete_end,
      base::RepeatingCallback<bool(const url::Origin& origin)> filter) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_H_
