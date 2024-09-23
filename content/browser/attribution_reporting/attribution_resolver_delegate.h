// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RESOLVER_DELEGATE_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RESOLVER_DELEGATE_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/source_registration_time_config.mojom-forward.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#include "content/common/content_export.h"

namespace attribution_reporting {
class AttributionScopesData;
class EventLevelEpsilon;
class EventReportWindows;
class TriggerSpecs;
}  // namespace attribution_reporting

namespace base {
class Uuid;
}  // namespace base

namespace content {

class AttributionReport;

// Resolver delegate that can supplied to extend basic attribution storage
// functionality like annotating reports. Users and subclasses must NOT assume
// that the delegate has the same lifetime as the `AttributionManager` or
// `AttributionResolver` classes.
class CONTENT_EXPORT AttributionResolverDelegate {
 public:
  // Both bounds are inclusive.
  struct OfflineReportDelayConfig {
    base::TimeDelta min;
    base::TimeDelta max;
  };

  explicit AttributionResolverDelegate(const AttributionConfig& config);

  virtual ~AttributionResolverDelegate();

  AttributionResolverDelegate(const AttributionResolverDelegate&) = delete;
  AttributionResolverDelegate& operator=(const AttributionResolverDelegate&) =
      delete;

  AttributionResolverDelegate(AttributionResolverDelegate&&) = delete;
  AttributionResolverDelegate& operator=(AttributionResolverDelegate&&) = delete;

  // Returns the time an event-level report should be sent for a given trigger
  // time and its corresponding source.
  virtual base::Time GetEventLevelReportTime(
      const attribution_reporting::EventReportWindows& event_report_windows,
      base::Time source_time,
      base::Time trigger_time) const = 0;

  // Returns the time an aggregatable report should be sent for a given trigger
  // time.
  virtual base::Time GetAggregatableReportTime(
      base::Time trigger_time) const = 0;

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
  const AttributionConfig::RateLimitConfig& GetRateLimits() const;

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
  virtual std::optional<OfflineReportDelayConfig> GetOfflineReportDelayConfig()
      const = 0;

  // Shuffles reports to provide plausible deniability on the ordering of
  // reports that share the same |report_time|. This is important because
  // multiple conversions for the same impression share the same report time
  // if they are within the same reporting window, and we do not want to allow
  // ordering on their conversion metadata bits.
  virtual void ShuffleReports(std::vector<AttributionReport>& reports) = 0;

  // Returns the rate used to determine whether to randomize the response to a
  // source with the given trigger specs, as implemented by
  // `GetRandomizedResponse()`. Must be in the range [0, 1] and remain constant
  // for the lifetime of the delegate for calls with identical inputs.
  virtual std::optional<double> GetRandomizedResponseRate(
      const attribution_reporting::TriggerSpecs&,
      attribution_reporting::EventLevelEpsilon) const = 0;

  using GetRandomizedResponseResult =
      base::expected<attribution_reporting::RandomizedResponseData,
                     attribution_reporting::RandomizedResponseError>;

  // Returns a randomized response for the given source, consisting of zero or
  // more fake reports. Returns an error if the channel capacity exceeds the
  // limit.
  virtual GetRandomizedResponseResult GetRandomizedResponse(
      attribution_reporting::mojom::SourceType,
      const attribution_reporting::TriggerSpecs&,
      attribution_reporting::EventLevelEpsilon,
      const std::optional<attribution_reporting::AttributionScopesData>&) = 0;

  int GetMaxAggregatableReportsPerSource() const;

  AttributionConfig::DestinationRateLimit GetDestinationRateLimit() const;

  AttributionConfig::AggregatableDebugRateLimit GetAggregatableDebugRateLimit()
      const;

  virtual bool GenerateNullAggregatableReportForLookbackDay(
      int lookback_day,
      attribution_reporting::mojom::SourceRegistrationTimeConfig) const = 0;

  const AttributionConfig& config() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return config_;
  }

 protected:
  AttributionConfig config_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_RESOLVER_DELEGATE_H_
