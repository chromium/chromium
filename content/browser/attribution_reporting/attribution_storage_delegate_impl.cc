// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/attribution_features.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/combinatorics.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "services/network/public/cpp/trigger_verification.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
namespace content {

namespace {

using ::attribution_reporting::EventReportWindows;
using ::attribution_reporting::mojom::SourceType;

// The max possible number of state combinations given a valid input.
constexpr int64_t kMaxNumCombinations = 4191844505805495;

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

double binary_entropy(double p) {
  if (p == 0 || p == 1) {
    return 0;
  }

  return -p * log2(p) - (1 - p) * log2(1 - p);
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
                                     AttributionConfig()) {}

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
    const EventReportWindows& event_report_windows,
    base::Time source_time,
    base::Time trigger_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (delay_mode_) {
    case AttributionDelayMode::kDefault:
      return event_report_windows.ComputeReportTime(source_time, trigger_time);
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

void AttributionStorageDelegateImpl::ShuffleTriggerVerifications(
    std::vector<network::TriggerVerification>& verifications) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (noise_mode_) {
    case AttributionNoiseMode::kDefault:
      base::RandomShuffle(verifications.begin(), verifications.end());
      break;
    case AttributionNoiseMode::kNone:
      break;
  }
}

double AttributionStorageDelegateImpl::GetRandomizedResponseRate(
    const EventReportWindows& event_report_windows,
    SourceType source_type,
    int max_event_level_reports) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int64_t num_combinations = GetNumberOfStarsAndBarsSequences(
      /*num_stars=*/max_event_level_reports,
      /*num_bars=*/TriggerDataCardinality(source_type) *
          event_report_windows.end_times().size());

  double exp_epsilon =
      std::exp(config_.event_level_limit.randomized_response_epsilon);
  return num_combinations / (num_combinations - 1 + exp_epsilon);
}

AttributionStorageDelegate::RandomizedResponse
AttributionStorageDelegateImpl::GetRandomizedResponse(
    const CommonSourceInfo& source,
    const EventReportWindows& event_report_windows,
    base::Time source_time,
    int max_event_level_reports) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (noise_mode_) {
    case AttributionNoiseMode::kDefault: {
      double randomized_trigger_rate = GetRandomizedResponseRate(
          event_report_windows, source.source_type(), max_event_level_reports);
      return GenerateWithRate(randomized_trigger_rate)
                 ? absl::make_optional(GetRandomFakeReports(
                       source, event_report_windows, source_time,
                       max_event_level_reports))
                 : absl::nullopt;
    }
    case AttributionNoiseMode::kNone:
      return absl::nullopt;
  }
}

std::vector<AttributionStorageDelegate::FakeReport>
AttributionStorageDelegateImpl::GetRandomFakeReports(
    const CommonSourceInfo& source,
    const EventReportWindows& event_report_windows,
    base::Time source_time,
    int max_event_level_reports) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(noise_mode_, AttributionNoiseMode::kDefault);

  const int64_t num_combinations = GetNumberOfStarsAndBarsSequences(
      /*num_stars=*/max_event_level_reports,
      /*num_bars=*/TriggerDataCardinality(source.source_type()) *
          event_report_windows.end_times().size());

  const int64_t sequence_index =
      static_cast<int64_t>(base::RandGenerator(num_combinations));
  DCHECK_GE(sequence_index, 0);
  DCHECK_LE(sequence_index, kMaxNumCombinations);

  return GetFakeReportsForSequenceIndex(
      source, source_time, event_report_windows, max_event_level_reports,
      sequence_index);
}

std::vector<AttributionStorageDelegate::FakeReport>
AttributionStorageDelegateImpl::GetFakeReportsForSequenceIndex(
    const CommonSourceInfo& source,
    base::Time source_time,
    const EventReportWindows& event_report_windows,
    int max_event_level_reports,
    int64_t random_stars_and_bars_sequence_index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(noise_mode_, AttributionNoiseMode::kDefault);

  const int trigger_data_cardinality =
      TriggerDataCardinality(source.source_type());

  const std::vector<int> bars_preceding_each_star =
      GetBarsPrecedingEachStar(GetStarIndices(
          /*num_stars=*/max_event_level_reports,
          /*num_bars=*/trigger_data_cardinality *
              event_report_windows.end_times().size(),
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

    base::Time report_time = event_report_windows.ReportTimeAtWindow(
        source_time, /*window_index=*/result.quot);
    // The last trigger time will always fall within a report window, no matter
    // the report window's start time.
    base::Time trigger_time = LastTriggerTimeForReportTime(report_time);

    DCHECK_EQ(event_report_windows.ComputeReportTime(source_time, trigger_time),
              report_time);

    fake_reports.push_back({
        .trigger_data = static_cast<uint64_t>(trigger_data),
        .trigger_time = trigger_time,
        .report_time = report_time,
    });
  }
  DCHECK_LE(fake_reports.size(), static_cast<size_t>(max_event_level_reports));
  return fake_reports;
}

double AttributionStorageDelegateImpl::ComputeChannelCapacity(
    const CommonSourceInfo& source,
    const attribution_reporting::EventReportWindows& event_report_windows,
    base::Time source_time,
    int max_event_level_reports) {
  double randomized_trigger_rate = GetRandomizedResponseRate(
      event_report_windows, source.source_type(), max_event_level_reports);
  const int64_t num_states = GetNumberOfStarsAndBarsSequences(
      /*num_stars=*/max_event_level_reports,
      /*num_bars=*/TriggerDataCardinality(source.source_type()) *
          event_report_windows.end_times().size());

  // This computes the channel capacity of a qary-symmetric channel with error
  // probability p. See more info at
  // https://wicg.github.io/attribution-reporting-api/#computing-channel-capacity
  double p = randomized_trigger_rate * (num_states - 1) / num_states;
  return log2(num_states) - binary_entropy(p) - p * log2(num_states - 1);
}

base::Time AttributionStorageDelegateImpl::GetExpiryTime(
    absl::optional<base::TimeDelta> declared_expiry,
    base::Time source_time,
    attribution_reporting::mojom::SourceType source_type) {
  base::TimeDelta expiry =
      declared_expiry.value_or(kDefaultAttributionSourceExpiry);

  if (source_type == attribution_reporting::mojom::SourceType::kEvent) {
    expiry = expiry.RoundToMultiple(base::Days(1));
  }

  return source_time +
         std::clamp(expiry, base::Days(1), kDefaultAttributionSourceExpiry);
}

absl::optional<base::Time> AttributionStorageDelegateImpl::GetReportWindowTime(
    absl::optional<base::TimeDelta> declared_window,
    base::Time source_time) {
  if (!declared_window.has_value()) {
    return absl::nullopt;
  }

  return source_time + std::clamp(*declared_window, base::Hours(1),
                                  kDefaultAttributionSourceExpiry);
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

EventReportWindows AttributionStorageDelegateImpl::GetDefaultEventReportWindows(
    SourceType source_type,
    base::TimeDelta last_report_window) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<base::TimeDelta> end_times;
  switch (source_type) {
    case SourceType::kNavigation:
      end_times = {
          config_.event_level_limit.first_navigation_report_window_deadline,
          config_.event_level_limit.second_navigation_report_window_deadline};
      break;
    case SourceType::kEvent:
      if (kVTCEarlyReportingWindows.Get()) {
        end_times = {
            config_.event_level_limit.first_event_report_window_deadline,
            config_.event_level_limit.second_event_report_window_deadline};
      }
      break;
  }

  absl::optional<EventReportWindows> event_report_windows =
      EventReportWindows::CreateAndTruncate(
          /*start_time=*/base::Days(0), std::move(end_times),
          /*expiry=*/last_report_window);
  DCHECK(event_report_windows.has_value());
  return event_report_windows.value();
}

}  // namespace content
