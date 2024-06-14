// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_config.h"

#include "base/time/time.h"

namespace content {

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

  if (!destination_rate_limit.Validate()) {
    return false;
  }

  if (!aggregatable_debug_rate_limit.Validate()) {
    return false;
  }

  return true;
}

AttributionConfig::RateLimitConfig::RateLimitConfig() = default;

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
  if (max_reports_per_destination <= 0) {
    return false;
  }

  return true;
}

bool AttributionConfig::AggregateLimit::Validate() const {
  if (max_reports_per_destination <= 0) {
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

AttributionConfig::EventLevelLimit::EventLevelLimit() = default;

AttributionConfig::EventLevelLimit::EventLevelLimit(const EventLevelLimit&) =
    default;
AttributionConfig::EventLevelLimit::EventLevelLimit(EventLevelLimit&&) =
    default;
AttributionConfig::EventLevelLimit::~EventLevelLimit() = default;

AttributionConfig::EventLevelLimit&
AttributionConfig::EventLevelLimit::operator=(const EventLevelLimit&) = default;
AttributionConfig::EventLevelLimit&
AttributionConfig::EventLevelLimit::operator=(EventLevelLimit&&) = default;

AttributionConfig::AggregateLimit::AggregateLimit() = default;

bool AttributionConfig::DestinationRateLimit::Validate() const {
  if (max_per_reporting_site <= 0) {
    return false;
  }

  if (max_total < max_per_reporting_site) {
    return false;
  }

  if (!rate_limit_window.is_positive()) {
    return false;
  }

  if (max_per_reporting_site_per_day <= 0) {
    return false;
  }

  return true;
}

bool AttributionConfig::AggregatableDebugRateLimit::Validate() const {
  if (max_budget_per_context_reporting_site <= 0) {
    return false;
  }

  if (max_budget_per_context_site < max_budget_per_context_reporting_site) {
    return false;
  }

  if (max_reports_per_source <= 0) {
    return false;
  }

  return true;
}

}  // namespace content
