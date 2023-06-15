// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_config.h"

#include <cmath>

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/destination_throttler.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

const base::FeatureParam<int> kMaxReportingOriginsPerSiteParam{
    &blink::features::kConversionMeasurement,
    "max_reporting_origins_per_source_reporting_site",
    AttributionConfig::RateLimitConfig::
        kDefaultMaxReportingOriginsPerSourceReportingSite};

const base::FeatureParam<int> kMaxAttributionsPerEventSourceParam{
    &blink::features::kConversionMeasurement,
    "max_attributions_per_event_source",
    AttributionConfig::EventLevelLimit::kDefaultMaxAttributionsPerEventSource};

}  // namespace

bool AttributionConfig::Validate() const {
  if (max_sources_per_origin <= 0) {
    return false;
  }

  if (max_destinations_per_source_site_reporting_site <= 0) {
    return false;
  }

  if (!rate_limit.Validate()) {
    return false;
  }

  if (!event_level_limit.Validate()) {
    return false;
  }

  if (!aggregate_limit.Validate()) {
    return false;
  }

  if (!throttler_policy.Validate()) {
    return false;
  }

  return true;
}

AttributionConfig::RateLimitConfig::RateLimitConfig()
    : max_reporting_origins_per_source_reporting_site(
          kMaxReportingOriginsPerSiteParam.Get()) {
  if (max_reporting_origins_per_source_reporting_site <= 0) {
    max_reporting_origins_per_source_reporting_site =
        kDefaultMaxReportingOriginsPerSourceReportingSite;
  }
}

AttributionConfig::RateLimitConfig::~RateLimitConfig() = default;

bool AttributionConfig::RateLimitConfig::Validate() const {
  if (time_window <= base::TimeDelta()) {
    return false;
  }

  if (max_source_registration_reporting_origins <= 0) {
    return false;
  }

  if (max_attribution_reporting_origins <= 0) {
    return false;
  }

  if (max_attributions <= 0) {
    return false;
  }

  if (max_reporting_origins_per_source_reporting_site <= 0) {
    return false;
  }

  if (!origins_per_site_window.is_positive()) {
    return false;
  }

  return true;
}

bool AttributionConfig::EventLevelLimit::Validate() const {
  if (navigation_source_trigger_data_cardinality == 0u) {
    return false;
  }

  if (event_source_trigger_data_cardinality == 0u) {
    return false;
  }

  if (max_reports_per_destination <= 0) {
    return false;
  }

  if (max_attributions_per_navigation_source <= 0) {
    return false;
  }

  if (max_attributions_per_event_source <= 0) {
    return false;
  }

  if (randomized_response_epsilon < 0 ||
      std::isnan(randomized_response_epsilon)) {
    return false;
  }

  if (first_navigation_report_window_deadline < base::TimeDelta() ||
      second_navigation_report_window_deadline <=
          first_navigation_report_window_deadline) {
    return false;
  }

  if (first_event_report_window_deadline < base::TimeDelta() ||
      second_event_report_window_deadline <=
          first_event_report_window_deadline) {
    return false;
  }

  return true;
}

bool AttributionConfig::AggregateLimit::Validate() const {
  if (max_reports_per_destination <= 0) {
    return false;
  }

  if (aggregatable_budget_per_source <= 0) {
    return false;
  }

  if (min_delay < base::TimeDelta()) {
    return false;
  }

  if (delay_span < base::TimeDelta()) {
    return false;
  }

  if (null_reports_rate_include_source_registration_time < 0 ||
      null_reports_rate_include_source_registration_time > 1) {
    return false;
  }

  if (null_reports_rate_exclude_source_registration_time < 0 ||
      null_reports_rate_exclude_source_registration_time > 1) {
    return false;
  }

  if (max_aggregatable_reports_per_source <= 0) {
    return false;
  }

  return true;
}

AttributionConfig::AttributionConfig() = default;
AttributionConfig::AttributionConfig(const AttributionConfig&) = default;
AttributionConfig::AttributionConfig(AttributionConfig&&) = default;
AttributionConfig::~AttributionConfig() = default;

AttributionConfig& AttributionConfig::operator=(const AttributionConfig&) =
    default;
AttributionConfig& AttributionConfig::operator=(AttributionConfig&&) = default;

AttributionConfig::EventLevelLimit::EventLevelLimit()
    : max_attributions_per_event_source(
          kMaxAttributionsPerEventSourceParam.Get()) {
  if (max_attributions_per_event_source <= 0) {
    max_attributions_per_event_source = kDefaultMaxAttributionsPerEventSource;
  }
}

AttributionConfig::EventLevelLimit::EventLevelLimit(const EventLevelLimit&) =
    default;
AttributionConfig::EventLevelLimit::EventLevelLimit(EventLevelLimit&&) =
    default;
AttributionConfig::EventLevelLimit::~EventLevelLimit() = default;

AttributionConfig::EventLevelLimit&
AttributionConfig::EventLevelLimit::operator=(const EventLevelLimit&) = default;
AttributionConfig::EventLevelLimit&
AttributionConfig::EventLevelLimit::operator=(EventLevelLimit&&) = default;

}  // namespace content
