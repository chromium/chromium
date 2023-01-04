// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_delegate.h"

#include "base/check.h"

namespace content {

AttributionStorageDelegate::AttributionStorageDelegate(
    const AttributionConfig& config)
    : config_(config) {
  DCHECK(config_.Validate());
}

int AttributionStorageDelegate::GetMaxAttributionsPerSource(
    AttributionSourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (source_type) {
    case AttributionSourceType::kNavigation:
      return config_.event_level_limit.max_attributions_per_navigation_source;
    case AttributionSourceType::kEvent:
      return config_.event_level_limit.max_attributions_per_event_source;
  }
}

int AttributionStorageDelegate::GetMaxSourcesPerOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return config_.max_sources_per_origin;
}

int AttributionStorageDelegate::GetMaxReportsPerDestination(
    AttributionReport::Type report_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (report_type) {
    case AttributionReport::Type::kEventLevel:
      return config_.event_level_limit.max_reports_per_destination;
    case AttributionReport::Type::kAggregatableAttribution:
      return config_.aggregate_limit.max_reports_per_destination;
  }
}

int AttributionStorageDelegate::GetMaxDestinationsPerSourceSiteReportingOrigin()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return config_.max_destinations_per_source_site_reporting_origin;
}

AttributionConfig::RateLimitConfig AttributionStorageDelegate::GetRateLimits()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return config_.rate_limit;
}

double AttributionStorageDelegate::GetRandomizedResponseRate(
    AttributionSourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (source_type) {
    case AttributionSourceType::kNavigation:
      return config_.event_level_limit
          .navigation_source_randomized_response_rate;
    case AttributionSourceType::kEvent:
      return config_.event_level_limit.event_source_randomized_response_rate;
  }
}

int64_t AttributionStorageDelegate::GetAggregatableBudgetPerSource() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return config_.aggregate_limit.aggregatable_budget_per_source;
}

uint64_t AttributionStorageDelegate::SanitizeTriggerData(
    uint64_t trigger_data,
    AttributionSourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const uint64_t cardinality = TriggerDataCardinality(source_type);
  return trigger_data % cardinality;
}

uint64_t AttributionStorageDelegate::SanitizeSourceEventId(
    uint64_t source_event_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!config_.source_event_id_cardinality) {
    return source_event_id;
  }

  return source_event_id % *config_.source_event_id_cardinality;
}

uint64_t AttributionStorageDelegate::TriggerDataCardinality(
    AttributionSourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (source_type) {
    case AttributionSourceType::kNavigation:
      return config_.event_level_limit
          .navigation_source_trigger_data_cardinality;
    case AttributionSourceType::kEvent:
      return config_.event_level_limit.event_source_trigger_data_cardinality;
  }
}

}  // namespace content
