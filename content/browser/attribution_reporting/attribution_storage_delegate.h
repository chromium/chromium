// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_H_

#include <stdint.h>
#include <vector>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class GUID;
}  // namespace base

namespace content {

class AttributionReport;
class CommonSourceInfo;

// Storage delegate that can supplied to extend basic attribution storage
// functionality like annotating reports.
class AttributionStorageDelegate {
 public:
  struct RateLimitConfig {
    base::TimeDelta time_window;

    // Maximum number of distinct reporting origins that can register sources
    // for a given <source site, destination site> in `time_window`.
    int64_t max_source_registration_reporting_origins;

    // Maximum number of distinct reporting origins that can create attributions
    // for a given <source site, destination site> in `time_window`.
    int64_t max_attribution_reporting_origins;

    // Maximum number of attributions for a given <source site, destination
    // site, reporting origin> in `time_window`.
    int64_t max_attributions;
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

  virtual ~AttributionStorageDelegate() = default;

  // Returns the time an event-level report should be sent for a given trigger
  // time and its corresponding source.
  virtual base::Time GetEventLevelReportTime(const CommonSourceInfo& source,
                                             base::Time trigger_time) const = 0;

  // Returns the time an aggregatable report should be sent for a given trigger
  // time.
  virtual base::Time GetAggregatableReportTime(
      base::Time trigger_time) const = 0;

  // This limit is used to determine if a source is allowed to schedule
  // a new report. When a source reaches this limit it is
  // marked inactive and no new reports will be created for it.
  // Sources will be checked against this limit after they schedule a new
  // report.
  virtual int GetMaxAttributionsPerSource(
      AttributionSourceType source_type) const = 0;

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
  virtual RateLimitConfig GetRateLimits() const = 0;

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
  virtual absl::optional<OfflineReportDelayConfig> GetOfflineReportDelayConfig()
      const = 0;

  // Shuffles reports to provide plausible deniability on the ordering of
  // reports that share the same |report_time|. This is important because
  // multiple conversions for the same impression share the same report time
  // if they are within the same reporting window, and we do not want to allow
  // ordering on their conversion metadata bits.
  virtual void ShuffleReports(std::vector<AttributionReport>& reports) = 0;

  // Returns the rate used to determine whether to randomize the response to a
  // source with the given source type, as implemented by
  // `GetRandomizedResponse()`. Must be in the range [0, 1] and remain constant
  // for the lifetime of the delegate.
  virtual double GetRandomizedResponseRate(AttributionSourceType) const = 0;

  // Returns a randomized response for the given source, consisting of zero or
  // more fake reports. Returns `absl::nullopt` to indicate that the response
  // should not be randomized.
  virtual RandomizedResponse GetRandomizedResponse(
      const CommonSourceInfo& source) = 0;

  // Returns the maximum sum of the contributions (values) across all buckets
  // per source.
  virtual int64_t GetAggregatableBudgetPerSource() const = 0;

  // Sanitizes `trigger_data` according to the data limits for `source_type`.
  virtual uint64_t SanitizeTriggerData(
      uint64_t trigger_data,
      AttributionSourceType source_type) const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_H_
