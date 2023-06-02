// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/combinatorics.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

using ::attribution_reporting::mojom::SourceType;

const base::FeatureParam<base::TimeDelta> kFirstNavigationReportWindowDeadline{
    &blink::features::kConversionMeasurement, "first_report_window_deadline",
    AttributionConfig::EventLevelLimit::kDefaultFirstReportWindowDeadline};

const base::FeatureParam<base::TimeDelta> kSecondNavigationReportWindowDeadline{
    &blink::features::kConversionMeasurement, "second_report_window_deadline",
    AttributionConfig::EventLevelLimit::kDefaultSecondReportWindowDeadline};

const base::FeatureParam<base::TimeDelta> kFirstEventReportWindowDeadline{
    &blink::features::kConversionMeasurement,
    "first_event_report_window_deadline",
    AttributionConfig::EventLevelLimit::kDefaultFirstReportWindowDeadline};

const base::FeatureParam<base::TimeDelta> kSecondEventReportWindowDeadline{
    &blink::features::kConversionMeasurement,
    "second_event_report_window_deadline",
    AttributionConfig::EventLevelLimit::kDefaultSecondReportWindowDeadline};

const base::FeatureParam<base::TimeDelta> kAggregateReportMinDelay{
    &blink::features::kConversionMeasurement, "aggregate_report_min_delay",
    AttributionConfig::AggregateLimit::kDefaultMinDelay};

const base::FeatureParam<base::TimeDelta> kAggregateReportDelaySpan{
    &blink::features::kConversionMeasurement, "aggregate_report_delay_span",
    AttributionConfig::AggregateLimit::kDefaultDelaySpan};

const base::FeatureParam<bool> kVTCEarlyReportingWindows(
    &blink::features::kConversionMeasurement,
    "vtc_early_reporting_windows",
    false);

std::vector<base::TimeDelta> GetVtcEarlyDeadlines(
    const AttributionConfig& config) {
  if (!kVTCEarlyReportingWindows.Get()) {
    return std::vector<base::TimeDelta>();
  }

  return std::vector<base::TimeDelta>{
      config.event_level_limit.first_event_report_window_deadline,
      config.event_level_limit.second_event_report_window_deadline};
}

base::Time GetClampedTime(base::TimeDelta time_delta, base::Time source_time) {
  constexpr base::TimeDelta kMinDeltaTime = base::Days(1);
  return source_time +
         std::clamp(time_delta, kMinDeltaTime, kDefaultAttributionSourceExpiry);
}

bool GenerateWithRate(double r) {
  DCHECK_GE(r, 0);
  DCHECK_LE(r, 1);
  return base::RandDouble() < r;
}

std::vector<AttributionStorageDelegate::NullAggregatableReport>
GetNullAggregatableReportsForLookback(
    const AttributionTrigger& trigger,
    base::Time trigger_time,
    absl::optional<base::Time> attributed_source_time,
    int days_lookback,
    double rate) {
  std::vector<AttributionStorageDelegate::NullAggregatableReport> reports;
  for (int i = 0; i <= days_lookback; i++) {
    base::Time fake_source_time = trigger_time - base::Days(i);
    if (attributed_source_time &&
        RoundDownToWholeDaySinceUnixEpoch(fake_source_time) ==
            *attributed_source_time) {
      continue;
    }

    if (GenerateWithRate(rate)) {
      reports.push_back(AttributionStorageDelegate::NullAggregatableReport{
          .fake_source_time = fake_source_time,
      });
    }
  }
  return reports;
}

}  // namespace

// static
std::unique_ptr<AttributionStorageDelegate>
AttributionStorageDelegateImpl::CreateForTesting(
    AttributionNoiseMode noise_mode,
    AttributionDelayMode delay_mode,
    const AttributionConfig& config) {
  return base::WrapUnique(
      new AttributionStorageDelegateImpl(noise_mode, delay_mode, config));
}

AttributionStorageDelegateImpl::AttributionStorageDelegateImpl(
    AttributionNoiseMode noise_mode,
    AttributionDelayMode delay_mode)
    : AttributionStorageDelegateImpl(noise_mode,
                                     delay_mode,
                                     AttributionConfig()) {
  base::TimeDelta first_deadline = kFirstNavigationReportWindowDeadline.Get();
  base::TimeDelta second_deadline = kSecondNavigationReportWindowDeadline.Get();

  if (!first_deadline.is_negative() && first_deadline < second_deadline) {
    config_.event_level_limit.first_navigation_report_window_deadline =
        first_deadline;
    config_.event_level_limit.second_navigation_report_window_deadline =
        second_deadline;
  } else {
    LOG(WARNING)
        << "Invalid navigation reporting window deadline value(s) - "
        << "Reporting window deadlines should be non-negative "
        << "and the first deadline should be less than the second."
        << "Using default values: ["
        << AttributionConfig::EventLevelLimit::kDefaultFirstReportWindowDeadline
        << ", "
        << AttributionConfig::EventLevelLimit::
               kDefaultSecondReportWindowDeadline
        << "]";
  }

  first_deadline = kFirstEventReportWindowDeadline.Get();
  second_deadline = kSecondEventReportWindowDeadline.Get();

  if (!first_deadline.is_negative() && first_deadline < second_deadline) {
    config_.event_level_limit.first_event_report_window_deadline =
        first_deadline;
    config_.event_level_limit.second_event_report_window_deadline =
        second_deadline;
  } else {
    LOG(WARNING)
        << "Invalid VTC reporting window deadline value(s) - "
        << "Reporting window deadlines should be non-negative "
        << "and the first deadline should be less than the second."
        << "Using default values: ["
        << AttributionConfig::EventLevelLimit::kDefaultFirstReportWindowDeadline
        << ", "
        << AttributionConfig::EventLevelLimit::
               kDefaultSecondReportWindowDeadline
        << "]";
  }

  if (base::TimeDelta min_delay = kAggregateReportMinDelay.Get();
      !min_delay.is_negative()) {
    config_.aggregate_limit.min_delay = min_delay;
  } else {
    LOG(WARNING) << "Minimum aggregate delay declared negative, "
                 << "using default value: "
                 << AttributionConfig::AggregateLimit::kDefaultMinDelay;
  }

  if (base::TimeDelta delay_span = kAggregateReportDelaySpan.Get();
      !delay_span.is_negative()) {
    config_.aggregate_limit.delay_span = delay_span;
  } else {
    LOG(WARNING) << "Aggregate delay span declared negative, "
                 << "using default value: "
                 << AttributionConfig::AggregateLimit::kDefaultDelaySpan;
  }
}

AttributionStorageDelegateImpl::AttributionStorageDelegateImpl(
    AttributionNoiseMode noise_mode,
    AttributionDelayMode delay_mode,
    const AttributionConfig& config)
    : AttributionStorageDelegate(config),
      noise_mode_(noise_mode),
      delay_mode_(delay_mode) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AttributionStorageDelegateImpl::~AttributionStorageDelegateImpl() = default;

base::TimeDelta
AttributionStorageDelegateImpl::GetDeleteExpiredSourcesFrequency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Minutes(5);
}

base::TimeDelta
AttributionStorageDelegateImpl::GetDeleteExpiredRateLimitsFrequency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Minutes(5);
}

base::Time AttributionStorageDelegateImpl::GetEventLevelReportTime(
    const StoredSource& source,
    base::Time trigger_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const CommonSourceInfo& common_info = source.common_info();

  base::TimeDelta expiry_deadline =
      ExpiryDeadline(source.source_time(), source.event_report_window_time());
  switch (delay_mode_) {
    case AttributionDelayMode::kDefault:
      return ComputeReportTime(
          source.source_time(), trigger_time,
          EffectiveDeadlines(common_info.source_type(), expiry_deadline));
    case AttributionDelayMode::kNone:
      return trigger_time;
  }
}

base::Time AttributionStorageDelegateImpl::GetAggregatableReportTime(
    base::Time trigger_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (delay_mode_) {
    case AttributionDelayMode::kDefault:
      switch (noise_mode_) {
        case AttributionNoiseMode::kDefault:
          return trigger_time + config_.aggregate_limit.min_delay +
                 base::RandDouble() * config_.aggregate_limit.delay_span;
        case AttributionNoiseMode::kNone:
          return trigger_time + config_.aggregate_limit.min_delay +
                 config_.aggregate_limit.delay_span;
      }

    case AttributionDelayMode::kNone:
      return trigger_time;
  }
}

base::Uuid AttributionStorageDelegateImpl::NewReportID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Uuid::GenerateRandomV4();
}

absl::optional<AttributionStorageDelegate::OfflineReportDelayConfig>
AttributionStorageDelegateImpl::GetOfflineReportDelayConfig() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (noise_mode_ == AttributionNoiseMode::kDefault &&
      delay_mode_ == AttributionDelayMode::kDefault) {
    // Add uniform random noise in the range of [0, 1 minutes] to the report
    // time.
    // TODO(https://crbug.com/1075600): This delay is very conservative.
    // Consider increasing this delay once we can be sure reports are still
    // sent at reasonable times, and not delayed for many browser sessions due
    // to short session up-times.
    return OfflineReportDelayConfig{
        .min = base::Minutes(0),
        .max = base::Minutes(1),
    };
  }

  return absl::nullopt;
}

void AttributionStorageDelegateImpl::ShuffleReports(
    std::vector<AttributionReport>& reports) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (noise_mode_) {
    case AttributionNoiseMode::kDefault:
      base::RandomShuffle(reports.begin(), reports.end());
      break;
    case AttributionNoiseMode::kNone:
      break;
  }
}

double AttributionStorageDelegateImpl::GetRandomizedResponseRate(
    SourceType source_type,
    base::TimeDelta expiry_deadline) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int num_combinations = GetNumberOfStarsAndBarsSequences(
      /*num_stars=*/GetMaxAttributionsPerSource(source_type),
      /*num_bars=*/TriggerDataCardinality(source_type) *
          EffectiveDeadlines(source_type, expiry_deadline).size());

  double exp_epsilon =
      std::exp(config_.event_level_limit.randomized_response_epsilon);
  return num_combinations / (num_combinations - 1 + exp_epsilon);
}

AttributionStorageDelegate::RandomizedResponse
AttributionStorageDelegateImpl::GetRandomizedResponse(
    const CommonSourceInfo& source,
    base::Time source_time,
    base::Time event_report_window_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (noise_mode_) {
    case AttributionNoiseMode::kDefault: {
      double randomized_trigger_rate = GetRandomizedResponseRate(
          source.source_type(),
          ExpiryDeadline(source_time, event_report_window_time));
      return GenerateWithRate(randomized_trigger_rate)
                 ? absl::make_optional(GetRandomFakeReports(
                       source, source_time, event_report_window_time))
                 : absl::nullopt;
    }
    case AttributionNoiseMode::kNone:
      return absl::nullopt;
  }
}

std::vector<AttributionStorageDelegate::FakeReport>
AttributionStorageDelegateImpl::GetRandomFakeReports(
    const CommonSourceInfo& source,
    base::Time source_time,
    base::Time event_report_window_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(noise_mode_, AttributionNoiseMode::kDefault);

  std::vector<base::TimeDelta> deadlines =
      EffectiveDeadlines(source.source_type(),
                         ExpiryDeadline(source_time, event_report_window_time));

  const int num_combinations = GetNumberOfStarsAndBarsSequences(
      /*num_stars=*/GetMaxAttributionsPerSource(source.source_type()),
      /*num_bars=*/TriggerDataCardinality(source.source_type()) *
          deadlines.size());

  // Subtract 1 because `AttributionRandomGenerator::RandInt()` is inclusive.
  const int sequence_index = base::RandInt(0, num_combinations - 1);

  return GetFakeReportsForSequenceIndex(source, source_time, deadlines,
                                        sequence_index);
}

std::vector<AttributionStorageDelegate::FakeReport>
AttributionStorageDelegateImpl::GetFakeReportsForSequenceIndex(
    const CommonSourceInfo& source,
    base::Time source_time,
    const std::vector<base::TimeDelta>& deadlines,
    int random_stars_and_bars_sequence_index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(noise_mode_, AttributionNoiseMode::kDefault);

  const int trigger_data_cardinality =
      TriggerDataCardinality(source.source_type());

  const std::vector<int> bars_preceding_each_star =
      GetBarsPrecedingEachStar(GetStarIndices(
          /*num_stars=*/GetMaxAttributionsPerSource(source.source_type()),
          /*num_bars=*/trigger_data_cardinality * deadlines.size(),
          /*sequence_index=*/random_stars_and_bars_sequence_index));

  std::vector<FakeReport> fake_reports;

  // an output state is uniquely determined by an ordering of c stars and w*d
  // bars, where:
  // w = the number of reporting windows
  // c = the maximum number of reports for a source
  // d = the trigger data cardinality for a source
  for (int num_bars : bars_preceding_each_star) {
    if (num_bars == 0) {
      continue;
    }

    auto result = std::div(num_bars - 1, trigger_data_cardinality);

    const int trigger_data = result.rem;
    DCHECK_GE(trigger_data, 0);
    DCHECK_LT(trigger_data, trigger_data_cardinality);

    base::Time report_time = ReportTimeAtWindow(source_time, deadlines,
                                                /*window_index=*/result.quot);
    base::Time trigger_time = LastTriggerTimeForReportTime(report_time);

    DCHECK_EQ(ComputeReportTime(source_time, trigger_time, deadlines),
              report_time);

    fake_reports.push_back({
        .trigger_data = static_cast<uint64_t>(trigger_data),
        .trigger_time = trigger_time,
        .report_time = report_time,
    });
  }
  return fake_reports;
}

base::Time AttributionStorageDelegateImpl::GetExpiryTime(
    absl::optional<base::TimeDelta> declared_expiry,
    base::Time source_time,
    attribution_reporting::mojom::SourceType source_type) {
  // Default to the maximum expiry time.
  base::TimeDelta expiry =
      declared_expiry.value_or(kDefaultAttributionSourceExpiry);

  // Expiry time for event sources must be a whole number of days.
  if (source_type == attribution_reporting::mojom::SourceType::kEvent) {
    expiry = expiry.RoundToMultiple(base::Days(1));
  }

  // If the impression specified its own expiry, clamp it to the minimum and
  // maximum.
  return GetClampedTime(expiry, source_time);
}

absl::optional<base::Time> AttributionStorageDelegateImpl::GetReportWindowTime(
    absl::optional<base::TimeDelta> declared_window,
    base::Time source_time) {
  // If the impression specified its own window, clamp it to the minimum and
  // maximum.
  return declared_window.has_value()
             ? absl::make_optional(
                   GetClampedTime(declared_window.value(), source_time))
             : absl::nullopt;
}

std::vector<base::TimeDelta> AttributionStorageDelegateImpl::EffectiveDeadlines(
    SourceType source_type,
    base::TimeDelta expiry_deadline) const {
  std::vector<base::TimeDelta> deadlines = EarlyDeadlines(source_type);
  while (deadlines.size() > 0 && deadlines.back() >= expiry_deadline) {
    deadlines.pop_back();
  }
  deadlines.push_back(expiry_deadline);
  return deadlines;
}

std::vector<base::TimeDelta> AttributionStorageDelegateImpl::EarlyDeadlines(
    SourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (source_type) {
    case SourceType::kNavigation:
      return std::vector<base::TimeDelta>{
          config_.event_level_limit.first_navigation_report_window_deadline,
          config_.event_level_limit.second_navigation_report_window_deadline};
    case SourceType::kEvent:
      return GetVtcEarlyDeadlines(config_);
  }
}

base::Time AttributionStorageDelegateImpl::ReportTimeAtWindow(
    base::Time source_time,
    const std::vector<base::TimeDelta>& deadlines,
    int window_index) const {
  DCHECK_GE(window_index, 0);
  DCHECK_LT(static_cast<size_t>(window_index), deadlines.size());
  return ReportTimeFromDeadline(source_time, deadlines[window_index]);
}

std::vector<AttributionStorageDelegate::NullAggregatableReport>
AttributionStorageDelegateImpl::GetNullAggregatableReports(
    const AttributionTrigger& trigger,
    base::Time trigger_time,
    absl::optional<base::Time> attributed_source_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::FeatureList::IsEnabled(
          attribution_reporting::
              kAttributionReportingNullAggregatableReports)) {
    return {};
  }

  switch (noise_mode_) {
    case AttributionNoiseMode::kDefault:
      return GetNullAggregatableReportsImpl(trigger, trigger_time,
                                            attributed_source_time);
    case AttributionNoiseMode::kNone:
      return {};
  }
}

std::vector<AttributionStorageDelegate::NullAggregatableReport>
AttributionStorageDelegateImpl::GetNullAggregatableReportsImpl(
    const AttributionTrigger& trigger,
    base::Time trigger_time,
    absl::optional<base::Time> attributed_source_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // See spec
  // https://wicg.github.io/attribution-reporting-api/#generate-null-reports.

  switch (trigger.registration().source_registration_time_config) {
    case attribution_reporting::mojom::SourceRegistrationTimeConfig::kInclude: {
      absl::optional<base::Time> rounded_attributed_source_time;
      if (attributed_source_time) {
        rounded_attributed_source_time =
            RoundDownToWholeDaySinceUnixEpoch(*attributed_source_time);
      }

      static_assert(kDefaultAttributionSourceExpiry == base::Days(30),
                    "update null reports rate");

      return GetNullAggregatableReportsForLookback(
          trigger, trigger_time, rounded_attributed_source_time,
          /*days_lookback=*/
          kDefaultAttributionSourceExpiry.RoundToMultiple(base::Days(1))
              .InDays(),
          config_.aggregate_limit
              .null_reports_rate_include_source_registration_time);
    }
    case attribution_reporting::mojom::SourceRegistrationTimeConfig::kExclude: {
      const bool has_real_report = attributed_source_time.has_value();
      if (has_real_report) {
        return {};
      }

      return GetNullAggregatableReportsForLookback(
          trigger, trigger_time, attributed_source_time, /*days_lookback=*/0,
          config_.aggregate_limit
              .null_reports_rate_exclude_source_registration_time);
    }
  }
}

}  // namespace content
