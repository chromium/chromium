// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/test/configurable_storage_delegate.h"

#include <stdint.h>

#include <limits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

ConfigurableStorageDelegate::ConfigurableStorageDelegate()
    : AttributionStorageDelegate(AttributionConfig{
          .max_sources_per_origin = std::numeric_limits<int>::max(),
          .max_destinations_per_source_site_reporting_origin =
              std::numeric_limits<int>::max(),
          .rate_limit =
              {
                  .time_window = base::TimeDelta::Max(),
                  .max_source_registration_reporting_origins =
                      std::numeric_limits<int64_t>::max(),
                  .max_attribution_reporting_origins =
                      std::numeric_limits<int64_t>::max(),
                  .max_attributions = std::numeric_limits<int64_t>::max(),
              },
          .event_level_limit =
              {
                  .navigation_source_trigger_data_cardinality =
                      std::numeric_limits<uint64_t>::max(),
                  .event_source_trigger_data_cardinality =
                      std::numeric_limits<uint64_t>::max(),
                  .navigation_source_randomized_response_rate = 0,
                  .event_source_randomized_response_rate = 0,
                  .max_reports_per_destination =
                      std::numeric_limits<int>::max(),
                  .max_attributions_per_navigation_source =
                      std::numeric_limits<int>::max(),
                  .max_attributions_per_event_source =
                      std::numeric_limits<int>::max(),
              },
          .aggregate_limit =
              {
                  .max_reports_per_destination =
                      std::numeric_limits<int>::max(),
                  .aggregatable_budget_per_source =
                      std::numeric_limits<int64_t>::max(),
                  .min_delay = base::TimeDelta(),
                  .delay_span = base::TimeDelta(),
              },
      }) {}

ConfigurableStorageDelegate::~ConfigurableStorageDelegate() = default;

void ConfigurableStorageDelegate::DetachFromSequence() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

base::Time ConfigurableStorageDelegate::GetEventLevelReportTime(
    const StoredSource& source,
    base::Time trigger_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return source.common_info().source_time() + report_delay_;
}

base::Time ConfigurableStorageDelegate::GetAggregatableReportTime(
    base::Time trigger_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return trigger_time + report_delay_;
}

base::TimeDelta ConfigurableStorageDelegate::GetDeleteExpiredSourcesFrequency()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delete_expired_sources_frequency_;
}

base::TimeDelta
ConfigurableStorageDelegate::GetDeleteExpiredRateLimitsFrequency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delete_expired_rate_limits_frequency_;
}

base::GUID ConfigurableStorageDelegate::NewReportID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return DefaultExternalReportID();
}

absl::optional<AttributionStorageDelegate::OfflineReportDelayConfig>
ConfigurableStorageDelegate::GetOfflineReportDelayConfig() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return offline_report_delay_config_;
}

void ConfigurableStorageDelegate::ShuffleReports(
    std::vector<AttributionReport>& reports) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (reverse_reports_on_shuffle_) {
    base::ranges::reverse(reports);
  }
}

AttributionStorageDelegate::RandomizedResponse
ConfigurableStorageDelegate::GetRandomizedResponse(
    const CommonSourceInfo& source,
    base::Time event_report_window_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return randomized_response_;
}

base::Time ConfigurableStorageDelegate::GetExpiryTime(
    absl::optional<base::TimeDelta> declared_expiry,
    base::Time source_time,
    attribution_reporting::mojom::SourceType) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetExpiryTimeForTesting(
      declared_expiry.value_or(kDefaultAttributionSourceExpiry), source_time);
}

absl::optional<base::Time> ConfigurableStorageDelegate::GetReportWindowTime(
    absl::optional<base::TimeDelta> declared_window,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetReportWindowTimeForTesting(declared_window, source_time);
}

void ConfigurableStorageDelegate::set_max_attributions_per_source(int max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_.event_level_limit.max_attributions_per_navigation_source = max;
  config_.event_level_limit.max_attributions_per_event_source = max;
}

void ConfigurableStorageDelegate::set_max_sources_per_origin(int max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_.max_sources_per_origin = max;
}

void ConfigurableStorageDelegate::set_max_reports_per_destination(
    AttributionReport::Type report_type,
    int max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (report_type) {
    case AttributionReport::Type::kEventLevel:
      config_.event_level_limit.max_reports_per_destination = max;
      break;
    case AttributionReport::Type::kAggregatableAttribution:
      config_.aggregate_limit.max_reports_per_destination = max;
      break;
  }
}

void ConfigurableStorageDelegate::
    set_max_destinations_per_source_site_reporting_origin(int max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_.max_destinations_per_source_site_reporting_origin = max;
}

void ConfigurableStorageDelegate::set_aggregatable_budget_per_source(
    int64_t max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_.aggregate_limit.aggregatable_budget_per_source = max;
}

void ConfigurableStorageDelegate::set_rate_limits(
    AttributionConfig::RateLimitConfig c) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(c.Validate());
  config_.rate_limit = c;
}

void ConfigurableStorageDelegate::set_delete_expired_sources_frequency(
    base::TimeDelta frequency) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delete_expired_sources_frequency_ = frequency;
}

void ConfigurableStorageDelegate::set_delete_expired_rate_limits_frequency(
    base::TimeDelta frequency) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delete_expired_rate_limits_frequency_ = frequency;
}

void ConfigurableStorageDelegate::set_report_delay(
    base::TimeDelta report_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  report_delay_ = report_delay;
}

void ConfigurableStorageDelegate::set_offline_report_delay_config(
    absl::optional<OfflineReportDelayConfig> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  offline_report_delay_config_ = config;
}

void ConfigurableStorageDelegate::set_reverse_reports_on_shuffle(bool reverse) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reverse_reports_on_shuffle_ = reverse;
}

void ConfigurableStorageDelegate::set_randomized_response_rates(
    double navigation,
    double event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_.event_level_limit.navigation_source_randomized_response_rate =
      navigation;
  config_.event_level_limit.event_source_randomized_response_rate = event;
}

void ConfigurableStorageDelegate::set_randomized_response(
    RandomizedResponse randomized_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  randomized_response_ = std::move(randomized_response);
}

void ConfigurableStorageDelegate::set_trigger_data_cardinality(
    uint64_t navigation,
    uint64_t event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(navigation, 0u);
  DCHECK_GT(event, 0u);

  config_.event_level_limit.navigation_source_trigger_data_cardinality =
      navigation;
  config_.event_level_limit.event_source_trigger_data_cardinality = event;
}

}  // namespace content
