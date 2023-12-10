// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_config.h"

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/attribution_reporting/features.h"

namespace content {

namespace {

const base::FeatureParam<int> kMaxReportingOriginsPerSiteParam{
    &attribution_reporting::features::kConversionMeasurement,
    "max_reporting_origins_per_source_reporting_site",
    AttributionConfig::RateLimitConfig::
        kDefaultMaxReportingOriginsPerSourceReportingSite};

const base::FeatureParam<base::TimeDelta> kAggregateReportMinDelay{
    &attribution_reporting::features::kConversionMeasurement,
    "aggregate_report_min_delay",
    AttributionConfig::AggregateLimit::kDefaultMinDelay};

const base::FeatureParam<base::TimeDelta> kAggregateReportDelaySpan{
    &attribution_reporting::features::kConversionMeasurement,
    "aggregate_report_delay_span",
    AttributionConfig::AggregateLimit::kDefaultDelaySpan};

const base::FeatureParam<double> kNavigationMaxInfoGain{
    &attribution_reporting::features::kConversionMeasurement,
    "navigation_max_info_gain",
    AttributionConfig::EventLevelLimit::kDefaultMaxNavigationInfoGain};

const base::FeatureParam<double> kEventMaxInfoGain{
    &attribution_reporting::features::kConversionMeasurement,
    "event_max_info_gain",
    AttributionConfig::EventLevelLimit::kDefaultMaxEventInfoGain};

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

  if (!destination_rate_limit.Validate()) {
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

AttributionConfig::EventLevelLimit::EventLevelLimit()
    : max_navigation_info_gain(kNavigationMaxInfoGain.Get()),
      max_event_info_gain(kEventMaxInfoGain.Get()) {}

AttributionConfig::EventLevelLimit::EventLevelLimit(const EventLevelLimit&) =
    default;
AttributionConfig::EventLevelLimit::EventLevelLimit(EventLevelLimit&&) =
    default;
AttributionConfig::EventLevelLimit::~EventLevelLimit() = default;

AttributionConfig::EventLevelLimit&
AttributionConfig::EventLevelLimit::operator=(const EventLevelLimit&) = default;
AttributionConfig::EventLevelLimit&
AttributionConfig::EventLevelLimit::operator=(EventLevelLimit&&) = default;

AttributionConfig::AggregateLimit::AggregateLimit()
    : min_delay(kAggregateReportMinDelay.Get()),
      delay_span(kAggregateReportDelaySpan.Get()) {
  if (min_delay.is_negative()) {
    min_delay = kDefaultMinDelay;
  }
  if (delay_span.is_negative()) {
    delay_span = kDefaultDelaySpan;
  }
}

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

  return true;
}

}  // namespace content
