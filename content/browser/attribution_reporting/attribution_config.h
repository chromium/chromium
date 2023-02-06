// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONFIG_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONFIG_H_

#include <stdint.h>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// See https://wicg.github.io/attribution-reporting-api/#vendor-specific-values
// for details.
struct CONTENT_EXPORT AttributionConfig {
  // Controls rate limits for the API.
  struct CONTENT_EXPORT RateLimitConfig {
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
    // site, reporting origin> in `time_window`.
    int64_t max_attributions = 100;

    // When adding new members, the corresponding `Validate()` definition and
    // `operator==()` definition in `attribution_interop_parser_unittest.cc`
    // should also be updated.
  };

  struct EventLevelLimit {
    // Returns true if this config is valid.
    [[nodiscard]] bool Validate() const;

    // Controls the valid range of trigger data.
    uint64_t navigation_source_trigger_data_cardinality = 8;
    uint64_t event_source_trigger_data_cardinality = 2;

    // Controls randomized response rates for the API: when a source is
    // registered, these rates are used to determine whether any subsequent
    // attributions for the source are handled truthfully, or whether the source
    // is immediately attributed with zero or more fake reports and real
    // attributions are dropped. Must be in the range [0, 1].
    double navigation_source_randomized_response_rate = .0024;
    double event_source_randomized_response_rate = .0000025;

    // Controls how many reports can be in the storage per attribution
    // destination.
    int max_reports_per_destination = 1024;

    // Controls how many times a single source can create an event-level report.
    int max_attributions_per_navigation_source = 3;
    int max_attributions_per_event_source = 1;

    // When adding new members, the corresponding `Validate()` definition and
    // `operator==()` definition in `attribution_interop_parser_unittest.cc`
    // should also be updated.
  };

  struct AggregateLimit {
    // Returns true if this config is valid.
    [[nodiscard]] bool Validate() const;

    // Controls how many reports can be in the storage per attribution
    // destination.
    int max_reports_per_destination = 1024;

    // Controls the maximum sum of the contributions (values) across all buckets
    // per source.
    // When updating the value, the corresponding BUDGET_PER_SOURCE value in
    // //content/browser/resources/attribution_reporting/attribution_internals.ts
    // should also be updated.
    int64_t aggregatable_budget_per_source = 65536;

    // Controls the report delivery time.
    base::TimeDelta min_delay = base::Minutes(10);
    base::TimeDelta delay_span = base::Minutes(50);

    // When adding new members, the corresponding `Validate()` definition and
    // `operator==()` definition in `attribution_interop_parser_unittest.cc`
    // should also be updated.
  };

  // Returns true if this config is valid.
  [[nodiscard]] bool Validate() const;

  // Controls how many sources can be in the storage per source origin.
  int max_sources_per_origin = 1024;

  // Controls the valid range of source event id. No limit if `absl::nullopt`.
  absl::optional<uint64_t> source_event_id_cardinality = absl::nullopt;

  // Controls the maximum number of distinct attribution destinations that can
  // be in storage at any time for sources with the same <source site, reporting
  // origin>.
  int max_destinations_per_source_site_reporting_origin = 100;

  RateLimitConfig rate_limit;
  EventLevelLimit event_level_limit;
  AggregateLimit aggregate_limit;

  // When adding new members, the corresponding `Validate()` definition and
  // `operator==()` definition in `attribution_interop_parser_unittest.cc`
  // should also be updated.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_CONFIG_H_
