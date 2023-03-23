// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"

#include <stdint.h>

#include <cstdlib>
#include <utility>

#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/combinatorics.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {

const base::FeatureParam<base::TimeDelta> kAggregateReportMinDelay{
    &blink::features::kConversionMeasurement, "aggregate_report_min_delay",
    AttributionConfig::AggregateLimit::kDefaultMinDelay};

const base::FeatureParam<base::TimeDelta> kAggregateReportDelaySpan{
    &blink::features::kConversionMeasurement, "aggregate_report_delay_span",
    AttributionConfig::AggregateLimit::kDefaultDelaySpan};

base::Time GetClampedTime(base::TimeDelta time_delta, base::Time source_time) {
  constexpr base::TimeDelta kMinDeltaTime = base::Days(1);
  return source_time + base::clamp(time_delta, kMinDeltaTime,
                                   kDefaultAttributionSourceExpiry);
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
  // TODO(tquintanilla): Investigate techniques to valid these params.
  config_.aggregate_limit.min_delay = kAggregateReportMinDelay.Get();
  config_.aggregate_limit.delay_span = kAggregateReportDelaySpan.Get();
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

  switch (delay_mode_) {
    case AttributionDelayMode::kDefault:
      return ComputeReportTime(source.common_info(),
                               source.event_report_window_time(), trigger_time);
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

base::GUID AttributionStorageDelegateImpl::NewReportID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::GUID::GenerateRandomV4();
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

AttributionStorageDelegate::RandomizedResponse
AttributionStorageDelegateImpl::GetRandomizedResponse(
    const CommonSourceInfo& source,
    base::Time event_report_window_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (noise_mode_) {
    case AttributionNoiseMode::kDefault: {
      double randomized_trigger_rate =
          GetRandomizedResponseRate(source.source_type());
      DCHECK_GE(randomized_trigger_rate, 0);
      DCHECK_LE(randomized_trigger_rate, 1);

      return base::RandDouble() < randomized_trigger_rate
                 ? absl::make_optional(
                       GetRandomFakeReports(source, event_report_window_time))
                 : absl::nullopt;
    }
    case AttributionNoiseMode::kNone:
      return absl::nullopt;
  }
}

std::vector<AttributionStorageDelegate::FakeReport>
AttributionStorageDelegateImpl::GetRandomFakeReports(
    const CommonSourceInfo& source,
    base::Time event_report_window_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(noise_mode_, AttributionNoiseMode::kDefault);

  const int num_combinations = GetNumberOfStarsAndBarsSequences(
      /*num_stars=*/GetMaxAttributionsPerSource(source.source_type()),
      /*num_bars=*/TriggerDataCardinality(source.source_type()) *
          NumReportWindows(source.source_type()));

  // Subtract 1 because `AttributionRandomGenerator::RandInt()` is inclusive.
  const int sequence_index = base::RandInt(0, num_combinations - 1);

  return GetFakeReportsForSequenceIndex(source, event_report_window_time,
                                        sequence_index);
}

std::vector<AttributionStorageDelegate::FakeReport>
AttributionStorageDelegateImpl::GetFakeReportsForSequenceIndex(
    const CommonSourceInfo& source,
    base::Time event_report_window_time,
    int random_stars_and_bars_sequence_index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(noise_mode_, AttributionNoiseMode::kDefault);

  const int trigger_data_cardinality =
      TriggerDataCardinality(source.source_type());

  const std::vector<int> bars_preceding_each_star =
      GetBarsPrecedingEachStar(GetStarIndices(
          /*num_stars=*/GetMaxAttributionsPerSource(source.source_type()),
          /*num_bars=*/trigger_data_cardinality *
              NumReportWindows(source.source_type()),
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

    base::Time report_time = ReportTimeAtWindow(
        source, event_report_window_time, /*window_index=*/result.quot);
    base::Time trigger_time = LastTriggerTimeForReportTime(report_time);

    DCHECK_EQ(ComputeReportTime(source, event_report_window_time, trigger_time),
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

}  // namespace content
