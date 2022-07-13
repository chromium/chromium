// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_ATTRIBUTION_CONFIG_H_
#define CONTENT_PUBLIC_TEST_ATTRIBUTION_CONFIG_H_

#include <stdint.h>

#include <limits>

#include "base/time/time.h"
#include "content/public/browser/attribution_reporting.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// See https://wicg.github.io/attribution-reporting-api/#vendor-specific-values
// for details.
struct AttributionConfig {
  struct EventLevelLimit {
    // Returns true if this config is valid.
    [[nodiscard]] bool Validate() const;

    // Controls the valid range of trigger data.
    uint64_t navigation_source_trigger_data_cardinality =
        std::numeric_limits<uint64_t>::max();
    uint64_t event_source_trigger_data_cardinality =
        std::numeric_limits<uint64_t>::max();

    // Controls randomized response rates for the API: when a source is
    // registered, these rates are used to determine whether any subsequent
    // attributions for the source are handled truthfully, or whether the source
    // is immediately attributed with zero or more fake reports and real
    // attributions are dropped. Must be in the range [0, 1].
    double navigation_source_randomized_response_rate = 0;
    double event_source_randomized_response_rate = 0;

    // Controls how many reports can be in the storage per attribution
    // destination.
    int max_reports_per_destination = std::numeric_limits<int>::max();

    // Controls how many times a single source can create an event-level report.
    int max_attributions_per_navigation_source =
        std::numeric_limits<int>::max();
    int max_attributions_per_event_source = std::numeric_limits<int>::max();

    // When adding new members, the corresponding `Validate()` definition and
    // `operator==()` definition in `attribution_interop_parser_unittest.cc`
    // should also be updated.
  };

  struct AggregateLimit {
    // Returns true if this config is valid.
    [[nodiscard]] bool Validate() const;

    // Controls how many reports can be in the storage per attribution
    // destination.
    int max_reports_per_destination = std::numeric_limits<int>::max();

    // Controls the maximum sum of the contributions (values) across all buckets
    // per source.
    int64_t aggregatable_budget_per_source =
        std::numeric_limits<int64_t>::max();

    // Controls the report delivery time.
    base::TimeDelta min_delay;
    base::TimeDelta delay_span;

    // When adding new members, the corresponding `Validate()` definition and
    // `operator==()` definition in `attribution_interop_parser_unittest.cc`
    // should also be updated.
  };

  static const AttributionConfig kDefault;

  // Returns true if this config is valid.
  [[nodiscard]] bool Validate() const;

  // Controls how many sources can be in the storage per source origin.
  int max_sources_per_origin = std::numeric_limits<int>::max();

  // Controls the valid range of source event id. No limit if `absl::nullopt`.
  absl::optional<uint64_t> source_event_id_cardinality;

  // Controls the maximum number of distinct attribution destinations that can
  // be in storage at any time for sources with the same <source site, reporting
  // origin>.
  int max_destinations_per_source_site_reporting_origin =
      std::numeric_limits<int>::max();

  AttributionRateLimitConfig rate_limit;
  EventLevelLimit event_level_limit;
  AggregateLimit aggregate_limit;

  // When adding new members, the corresponding `Validate()` definition and
  // `operator==()` definition in `attribution_interop_parser_unittest.cc`
  // should also be updated.
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_ATTRIBUTION_CONFIG_H_
