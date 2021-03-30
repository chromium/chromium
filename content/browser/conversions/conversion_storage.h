// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_STORAGE_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_report.h"
#include "content/browser/conversions/storable_conversion.h"
#include "content/browser/conversions/storable_impression.h"
#include "url/origin.h"

namespace content {

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

    // New conversions will be sent through this callback for
    // pruning/modification before they are added to storage. This will be
    // called during the execution of
    // ConversionStorage::MaybeCreateAndStoreConversionReports(). |reports| will
    // contain a report for each matching impression for a given conversion
    // event. Each report will be pre-populated from storage with the conversion
    // event data.
    virtual void ProcessNewConversionReports(
        std::vector<ConversionReport>* reports) = 0;

    // This limit is used to determine if an impression is allowed to schedule
    // a new conversion reports. When an impression reaches this limit it is
    // marked inactive and no new conversion reports will be created for it.
    // Impressions will be checked against this limit after they schedule a new
    // report.
    virtual int GetMaxConversionsPerImpression() const = 0;

    // These limits are designed solely to avoid excessive disk / memory usage.
    // In particular, they do not correspond with any privacy parameters.
    // TODO(crbug.com/1082754): Consider replacing this functionality (and the
    // data deletion logic) with the quota system.
    //
    // Returns the maximum number of impressions that can be in storage at any
    // time for an impression top-level origin.
    virtual int GetMaxImpressionsPerOrigin() const = 0;
    //  Returns the maximum number of conversions that can be in storage at any
    //  time for a conversion top-level origin. Note that since reporting
    //  origins are the actual entities that invoke conversion registration, we
    //  could consider changing this limit to be keyed by a <conversion origin,
    //  reporting origin> tuple.
    virtual int GetMaxConversionsPerOrigin() const = 0;

    struct RateLimitConfig {
      base::TimeDelta time_window;
      int max_attributions_per_window;
    };

    // Returns the rate limits for capping attributions per window.
    virtual RateLimitConfig GetRateLimits() const = 0;
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

  // Finds all stored impressions matching a given |conversion|, and stores new
  // associated conversion reports. The delegate will receive a call
  // to Delegate::ProcessNewConversionReports() before the reports are added to
  // storage. Only active impressions will receive new conversions. Returns the
  // number of new conversion reports that have been scheduled/added to storage.
  virtual int MaybeCreateAndStoreConversionReports(
      const StorableConversion& conversion) = 0;

  // Returns all of the conversion reports that should be sent before
  // |max_report_time|. This call is logically const, and does not modify the
  // underlying storage.
  virtual std::vector<ConversionReport> GetConversionsToReport(
      base::Time max_report_time) = 0;

  // Returns all active impressions in storage. Active impressions are all
  // impressions that can still convert. Impressions that: are past expiry,
  // reached the conversion limit, or was marked inactive due to having
  // converted and then superceded by a matching impression should not be
  // returned.
  virtual std::vector<StorableImpression> GetActiveImpressions() = 0;

  // Deletes all impressions that have expired and have no pending conversion
  // reports. Returns the number of impressions that were deleted.
  virtual int DeleteExpiredImpressions() = 0;

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
