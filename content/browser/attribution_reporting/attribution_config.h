// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONFIG_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONFIG_H_

#include <stdint.h>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// See https://wicg.github.io/attribution-reporting-api/#vendor-specific-values
// for details.
struct CONTENT_EXPORT AttributionConfig {
  // Controls rate limits for the API.
  struct CONTENT_EXPORT RateLimitConfig {
    RateLimitConfig();
    ~RateLimitConfig();

    // Returns true if this config is valid.
    [[nodiscard]] bool Validate() const;

    // Controls the rate-limiting time window for attribution.
    base::TimeDelta time_window = base::Days(30);

    // Maximum number of distinct reporting origins that can register sources
    // for a given <source site, destination site> in `time_window`.
    int64_t max_source_registration_reporting_origins = 100;

    // Maximum number of distinct reporting origins that can create attributions
    // for a given <source site, destination site> in `time_window`.
    int64_t max_attribution_reporting_origins = 10;

    // Maximum number of attributions for a given <source site, destination
    // site, reporting site> in `time_window`.
    int64_t max_attributions = 100;

    static constexpr int kDefaultMaxReportingOriginsPerSourceReportingSite = 1;

    // Maximum number of distinct reporting origins for a given <source site,
    // reporting site> in `origins_per_site_window`.
    int max_reporting_origins_per_source_reporting_site =
        kDefaultMaxReportingOriginsPerSourceReportingSite;

    // Controls the time window for reporting origins per site limit.
    base::TimeDelta origins_per_site_window = base::Days(1);

    friend bool operator==(const RateLimitConfig&,
                           const RateLimitConfig&) = default;

    // When adding new members, the corresponding `Validate()` definition
    // should also be updated.
  };

  struct CONTENT_EXPORT EventLevelLimit {
    EventLevelLimit();

    EventLevelLimit(const EventLevelLimit&);
    EventLevelLimit(EventLevelLimit&&);
    ~EventLevelLimit();

    EventLevelLimit& operator=(const EventLevelLimit&);
    EventLevelLimit& operator=(EventLevelLimit&&);

    // Returns true if this config is valid.
    [[nodiscard]] bool Validate() const;

    // Controls how many reports can be in the storage per attribution
    // destination.
    int max_reports_per_destination = 1024;

    // Default constants for max info gain in bits per source type.
    // Rounded up to nearest e-5 digit.
    static constexpr double kDefaultMaxNavigationInfoGain = 11.46173;
    static constexpr double kDefaultMaxEventInfoGain = 6.5;

    // Controls the max number bits of information that can be associated with
    // a single a source.
    double max_navigation_info_gain = kDefaultMaxNavigationInfoGain;
    double max_event_info_gain = kDefaultMaxEventInfoGain;

    friend bool operator==(const EventLevelLimit&,
                           const EventLevelLimit&) = default;

    // When adding new members, the corresponding `Validate()` definition
    // should also be updated.
  };

  struct CONTENT_EXPORT AggregateLimit {
    AggregateLimit();

    // Returns true if this config is valid.
    [[nodiscard]] bool Validate() const;

    // Controls how many reports can be in the storage per attribution
    // destination.
    int max_reports_per_destination = 1024;

    // Default constants for the report delivery time to be used when declaring
    // field trial params.
    static constexpr base::TimeDelta kDefaultMinDelay = base::TimeDelta();
    static constexpr base::TimeDelta kDefaultDelaySpan = base::Minutes(10);

    // Controls the report delivery time.
    base::TimeDelta min_delay = kDefaultMinDelay;
    base::TimeDelta delay_span = kDefaultDelaySpan;

    double null_reports_rate_include_source_registration_time = .008;
    double null_reports_rate_exclude_source_registration_time = .05;

    int max_aggregatable_reports_per_source = 20;

    friend bool operator==(const AggregateLimit&,
                           const AggregateLimit&) = default;

    // When adding new members, the corresponding `Validate()` definition
    // should also be updated.
  };

  struct CONTENT_EXPORT DestinationRateLimit {
    // Returns true if this config is valid.
    [[nodiscard]] bool Validate() const;

    int max_total = 200;
    int max_per_reporting_site = 50;
    base::TimeDelta rate_limit_window = base::Minutes(1);

    friend bool operator==(const DestinationRateLimit&,
                           const DestinationRateLimit&) = default;

    // When adding new members, the corresponding `Validate()` definition
    // should also be updated.
  };

  AttributionConfig();

  AttributionConfig(const AttributionConfig&);
  AttributionConfig(AttributionConfig&&);
  ~AttributionConfig();

  AttributionConfig& operator=(const AttributionConfig&);
  AttributionConfig& operator=(AttributionConfig&&);

  // Returns true if this config is valid.
  [[nodiscard]] bool Validate() const;

  // Controls how many sources can be in the storage per source origin.
  int max_sources_per_origin = 4096;

  // Controls the maximum number of distinct attribution destinations that can
  // be in storage at any time for sources with the same <source site, reporting
  // site>.
  int max_destinations_per_source_site_reporting_site = 100;

  RateLimitConfig rate_limit;
  EventLevelLimit event_level_limit;
  AggregateLimit aggregate_limit;
  DestinationRateLimit destination_rate_limit;

  friend bool operator==(const AttributionConfig&,
                         const AttributionConfig&) = default;

  // When adding new members, the corresponding `Validate()` definition
  // should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONFIG_H_
