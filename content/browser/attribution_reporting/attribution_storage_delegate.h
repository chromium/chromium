// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_H_

#include <stdint.h>

#include <vector>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#include "content/browser/attribution_reporting/destination_throttler.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {
class TriggerVerification;
}  // namespace network

namespace content {

class AttributionReport;
class AttributionTrigger;
class CommonSourceInfo;
class StoredSource;

// Storage delegate that can supplied to extend basic attribution storage
// functionality like annotating reports.
class CONTENT_EXPORT AttributionStorageDelegate {
 public:
  // Both bounds are inclusive.
  struct OfflineReportDelayConfig {
    base::TimeDelta min;
    base::TimeDelta max;
  };

  struct FakeReport {
    uint64_t trigger_data;
    // A placeholder time created to align with `report_time`.
    base::Time trigger_time;
    base::Time report_time;
  };

  // Corresponds to `StoredSource::AttributionLogic` as follows:
  // `absl::nullopt` -> `StoredSource::AttributionLogic::kTruthfully`
  // empty vector -> `StoredSource::AttributionLogic::kNever`
  // non-empty vector -> `StoredSource::AttributionLogic::kFalsely`
  using RandomizedResponse = absl::optional<std::vector<FakeReport>>;

  struct NullAggregatableReport {
    base::Time fake_source_time;
  };

  explicit AttributionStorageDelegate(const AttributionConfig& config);

  virtual ~AttributionStorageDelegate() = default;

  // Returns the time an event-level report should be sent for a given trigger
  // time and its corresponding source.
  virtual base::Time GetEventLevelReportTime(const StoredSource& source,
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
  int GetMaxAttributionsPerSource(
      attribution_reporting::mojom::SourceType) const;

  // These limits are designed solely to avoid excessive disk / memory usage.
  // In particular, they do not correspond with any privacy parameters.
  //
  // Returns the maximum number of sources that can be in storage at any
  // time for a source top-level origin.
  int GetMaxSourcesPerOrigin() const;

  // Returns the maximum number of reports of the given type that can be in
  // storage at any time for a destination site. Note that since
  // reporting origins are the actual entities that invoke attribution
  // registration, we could consider changing this limit to be keyed by an
  // <attribution origin, reporting origin> tuple.
  int GetMaxReportsPerDestination(
      attribution_reporting::mojom::ReportType) const;

  // Returns the maximum number of distinct attribution destinations that can
  // be in storage at any time for sources with the same <source site,
  // reporting site>.
  int GetMaxDestinationsPerSourceSiteReportingSite() const;

  // Returns the rate limits for capping contributions per window.
  AttributionConfig::RateLimitConfig GetRateLimits() const;

  // Returns the maximum frequency at which to delete expired sources.
  // Must be positive.
  virtual base::TimeDelta GetDeleteExpiredSourcesFrequency() const = 0;

  // Returns the maximum frequency at which to delete expired rate limits.
  // Must be positive.
  virtual base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const = 0;

  // Returns a new report ID.
  virtual base::Uuid NewReportID() const = 0;

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

  // Shuffles trigger verifications to provide plausible deniability on the
  // ordering and use of verification tokens.
  virtual void ShuffleTriggerVerifications(
      std::vector<network::TriggerVerification>& verifications) = 0;

  // Returns the rate used to determine whether to randomize the response to a
  // source with the given source type and expiry deadline, as implemented by
  // `GetRandomizedResponse()`. Must be in the range [0, 1] and remain constant
  // for the lifetime of the delegate for calls with identical inputs.
  virtual double GetRandomizedResponseRate(
      attribution_reporting::mojom::SourceType,
      base::TimeDelta expiry_deadline) const = 0;

  // Returns a randomized response for the given source, consisting of zero or
  // more fake reports. Returns `absl::nullopt` to indicate that the response
  // should not be randomized.
  virtual RandomizedResponse GetRandomizedResponse(
      const CommonSourceInfo& source,
      base::Time source_time,
      base::Time event_report_window_time) = 0;

  virtual base::Time GetExpiryTime(
      absl::optional<base::TimeDelta> declared_expiry,
      base::Time source_time,
      attribution_reporting::mojom::SourceType) = 0;

  virtual absl::optional<base::Time> GetReportWindowTime(
      absl::optional<base::TimeDelta> declared_window,
      base::Time source_time) = 0;

  // Returns the maximum sum of the contributions (values) across all buckets
  // per source.
  int64_t GetAggregatableBudgetPerSource() const;

  int GetMaxAggregatableReportsPerSource() const;

  DestinationThrottler::Policy GetDestinationThrottlerPolicy() const;

  // Sanitizes `trigger_data` according to the data limits for `source_type`.
  uint64_t SanitizeTriggerData(uint64_t trigger_data,
                               attribution_reporting::mojom::SourceType) const;

  // Returns zero or more null aggregatable reports for the given trigger.
  virtual std::vector<NullAggregatableReport> GetNullAggregatableReports(
      const AttributionTrigger&,
      base::Time trigger_time,
      absl::optional<base::Time> attributed_source_time) const = 0;

 protected:
  uint64_t TriggerDataCardinality(
      attribution_reporting::mojom::SourceType) const;

  AttributionConfig config_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_H_
