// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_delegate.h"

#include "base/check.h"
#include "base/notreached.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom.h"

namespace content {

namespace {
using ::attribution_reporting::mojom::SourceType;
}  // namespace

AttributionStorageDelegate::AttributionStorageDelegate(
    const AttributionConfig& config)
    : config_(config) {
  DCHECK(config_.Validate());
}

int AttributionStorageDelegate::GetMaxAttributionsPerSource(
    SourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (source_type) {
    case SourceType::kNavigation:
      return config_.event_level_limit.max_attributions_per_navigation_source;
    case SourceType::kEvent:
      return config_.event_level_limit.max_attributions_per_event_source;
  }
}

int AttributionStorageDelegate::GetMaxSourcesPerOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return config_.max_sources_per_origin;
}

int AttributionStorageDelegate::GetMaxReportsPerDestination(
    attribution_reporting::mojom::ReportType report_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (report_type) {
    case attribution_reporting::mojom::ReportType::kEventLevel:
      return config_.event_level_limit.max_reports_per_destination;
    case attribution_reporting::mojom::ReportType::kAggregatableAttribution:
      return config_.aggregate_limit.max_reports_per_destination;
    case attribution_reporting::mojom::ReportType::kNullAggregatable:
      NOTREACHED();
      return 0;
  }
}

int AttributionStorageDelegate::GetMaxDestinationsPerSourceSiteReportingSite()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return config_.max_destinations_per_source_site_reporting_site;
}

AttributionConfig::RateLimitConfig AttributionStorageDelegate::GetRateLimits()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return config_.rate_limit;
}

int64_t AttributionStorageDelegate::GetAggregatableBudgetPerSource() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return config_.aggregate_limit.aggregatable_budget_per_source;
}

int AttributionStorageDelegate::GetMaxAggregatableReportsPerSource() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return config_.aggregate_limit.max_aggregatable_reports_per_source;
}

DestinationThrottler::Policy
AttributionStorageDelegate::GetDestinationThrottlerPolicy() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return config_.throttler_policy;
}

uint64_t AttributionStorageDelegate::SanitizeTriggerData(
    uint64_t trigger_data,
    SourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const uint64_t cardinality = TriggerDataCardinality(source_type);
  return trigger_data % cardinality;
}

uint64_t AttributionStorageDelegate::TriggerDataCardinality(
    SourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (source_type) {
    case SourceType::kNavigation:
      return config_.event_level_limit
          .navigation_source_trigger_data_cardinality;
    case SourceType::kEvent:
      return config_.event_level_limit.event_source_trigger_data_cardinality;
  }
}

}  // namespace content
