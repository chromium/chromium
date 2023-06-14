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
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "services/network/public/cpp/trigger_verification.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

ConfigurableStorageDelegate::ConfigurableStorageDelegate()
    : AttributionStorageDelegate(
          AttributionConfigWith([](AttributionConfig& c) {
            c.max_sources_per_origin = std::numeric_limits<int>::max(),
            c.max_destinations_per_source_site_reporting_site =
                std::numeric_limits<int>::max();
            c.rate_limit =
                RateLimitWith([](AttributionConfig::RateLimitConfig& r) {
                  r.time_window = base::TimeDelta::Max();
                  r.max_source_registration_reporting_origins =
                      std::numeric_limits<int64_t>::max();
                  r.max_attribution_reporting_origins =
                      std::numeric_limits<int64_t>::max();
                  r.max_attributions = std::numeric_limits<int64_t>::max();
                  r.max_reporting_origins_per_source_reporting_site =
                      std::numeric_limits<int>::max();
                }),
            c.event_level_limit =
                EventLevelLimitWith([](AttributionConfig::EventLevelLimit& e) {
                  e.navigation_source_trigger_data_cardinality =
                      std::numeric_limits<uint64_t>::max();
                  e.event_source_trigger_data_cardinality =
                      std::numeric_limits<uint64_t>::max();
                  e.randomized_response_epsilon =
                      std::numeric_limits<double>::infinity();
                  e.max_reports_per_destination =
                      std::numeric_limits<int>::max();
                  e.max_attributions_per_navigation_source =
                      std::numeric_limits<int>::max();
                  e.max_attributions_per_event_source =
                      std::numeric_limits<int>::max();
                });
            c.aggregate_limit = {
                .max_reports_per_destination = std::numeric_limits<int>::max(),
                .aggregatable_budget_per_source =
                    std::numeric_limits<int64_t>::max(),
                .min_delay = base::TimeDelta(),
                .delay_span = base::TimeDelta(),
            };
          })) {}

ConfigurableStorageDelegate::~ConfigurableStorageDelegate() = default;

void ConfigurableStorageDelegate::DetachFromSequence() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

base::Time ConfigurableStorageDelegate::GetEventLevelReportTime(
    const StoredSource& source,
    base::Time trigger_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return source.source_time() + report_delay_;
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

base::Uuid ConfigurableStorageDelegate::NewReportID() const {
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

void ConfigurableStorageDelegate::ShuffleTriggerVerifications(
    std::vector<network::TriggerVerification>& verifications) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (reverse_verifications_on_shuffle_) {
    base::ranges::reverse(verifications);
  }
}

double ConfigurableStorageDelegate::GetRandomizedResponseRate(
    attribution_reporting::mojom::SourceType,
    base::TimeDelta expiry_deadline) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return randomized_response_rate_;
}

AttributionStorageDelegate::RandomizedResponse
ConfigurableStorageDelegate::GetRandomizedResponse(
    const CommonSourceInfo& source,
    base::Time source_time,
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

std::vector<AttributionStorageDelegate::NullAggregatableReport>
ConfigurableStorageDelegate::GetNullAggregatableReports(
    const AttributionTrigger& trigger,
    base::Time trigger_time,
    absl::optional<base::Time> attributed_source_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return null_aggregatable_reports_;
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
    case AttributionReport::Type::kNullAggregatable:
      NOTREACHED();
      break;
  }
}

void ConfigurableStorageDelegate::
    set_max_destinations_per_source_site_reporting_site(int max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_.max_destinations_per_source_site_reporting_site = max;
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
void ConfigurableStorageDelegate::set_reverse_verifications_on_shuffle(
    bool reverse) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reverse_verifications_on_shuffle_ = reverse;
}

void ConfigurableStorageDelegate::set_randomized_response_rate(double rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  randomized_response_rate_ = rate;
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

void ConfigurableStorageDelegate::set_null_aggregatable_reports(
    std::vector<NullAggregatableReport> null_aggregatable_reports) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  null_aggregatable_reports_ = std::move(null_aggregatable_reports);
}

}  // namespace content
