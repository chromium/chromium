// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
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
#include "components/attribution_reporting/constants.h"
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
    SourceType source_type,
    const EventReportWindows& event_report_windows,
    int max_event_level_reports) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content::GetRandomizedResponseRate(
      GetNumStates(source_type, event_report_windows, max_event_level_reports),
      config_.event_level_limit.randomized_response_epsilon);
}

int64_t AttributionStorageDelegateImpl::GetNumStates(
    SourceType source_type,
    const EventReportWindows& event_report_windows,
    int max_event_level_reports) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetNumberOfStarsAndBarsSequences(
      /*num_stars=*/max_event_level_reports,
      /*num_bars=*/TriggerDataCardinality(source_type) *
          event_report_windows.end_times().size());
}

AttributionStorageDelegate::GetRandomizedResponseResult
AttributionStorageDelegateImpl::GetRandomizedResponse(
    SourceType source_type,
    const EventReportWindows& event_report_windows,
    int max_event_level_reports,
    base::Time source_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int64_t num_states =
      GetNumStates(source_type, event_report_windows, max_event_level_reports);

  const double rate = content::GetRandomizedResponseRate(
      num_states, config_.event_level_limit.randomized_response_epsilon);

  const double capacity = ComputeChannelCapacity(num_states, rate);

  if (capacity > GetMaxChannelCapacity(source_type)) {
    return base::unexpected(ExceedsChannelCapacityLimit());
  }

  switch (noise_mode_) {
    case AttributionNoiseMode::kDefault: {
      return RandomizedResponseData(
          rate, GenerateWithRate(rate)
                    ? absl::make_optional(GetRandomFakeReports(
                          source_type, event_report_windows,
                          max_event_level_reports, source_time, num_states))
                    : absl::nullopt);
    }
    case AttributionNoiseMode::kNone:
      return RandomizedResponseData(rate, absl::nullopt);
  }
}

std::vector<AttributionStorageDelegate::FakeReport>
AttributionStorageDelegateImpl::GetRandomFakeReports(
    SourceType source_type,
    const EventReportWindows& event_report_windows,
    int max_event_level_reports,
    base::Time source_time,
    int64_t num_states) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(noise_mode_, AttributionNoiseMode::kDefault);

  DCHECK_EQ(num_states, GetNumStates(source_type, event_report_windows,
                                     max_event_level_reports));

  const int64_t sequence_index =
      static_cast<int64_t>(base::RandGenerator(num_states));
  DCHECK_GE(sequence_index, 0);
  DCHECK_LE(sequence_index, kMaxNumCombinations);

  return GetFakeReportsForSequenceIndex(source_type, event_report_windows,
                                        max_event_level_reports, source_time,
                                        sequence_index);
}

std::vector<AttributionStorageDelegate::FakeReport>
AttributionStorageDelegateImpl::GetFakeReportsForSequenceIndex(
    SourceType source_type,
    const EventReportWindows& event_report_windows,
    int max_event_level_reports,
    base::Time source_time,
    int64_t random_stars_and_bars_sequence_index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(noise_mode_, AttributionNoiseMode::kDefault);

  const int trigger_data_cardinality = TriggerDataCardinality(source_type);

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

absl::optional<base::Time> AttributionStorageDelegateImpl::GetReportWindowTime(
    absl::optional<base::TimeDelta> declared_window,
    base::Time source_time) {
  if (!declared_window.has_value()) {
    return absl::nullopt;
  }

  return source_time + std::clamp(*declared_window,
                                  attribution_reporting::kMinReportWindow,
                                  attribution_reporting::kMaxSourceExpiry);
}

std::vector<AttributionStorageDelegate::NullAggregatableReport>
AttributionStorageDelegateImpl::GetNullAggregatableReports(
    const AttributionTrigger& trigger,
    base::Time trigger_time,
    absl::optional<base::Time> attributed_source_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::FeatureList::IsEnabled(
          attribution_reporting::features::
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

      static_assert(attribution_reporting::kMaxSourceExpiry == base::Days(30),
                    "update null reports rate");

      return GetNullAggregatableReportsForLookback(
          trigger, trigger_time, rounded_attributed_source_time,
          /*days_lookback=*/
          attribution_reporting::kMaxSourceExpiry.InDays(),
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
      end_times = {kDefaultNavigationReportWindow1,
                   kDefaultNavigationReportWindow2};
      break;
    case SourceType::kEvent:
      break;
  }

  absl::optional<EventReportWindows> event_report_windows =
      EventReportWindows::CreateWindowsAndTruncate(
          /*start_time=*/base::Days(0), std::move(end_times),
          /*expiry=*/last_report_window);
  DCHECK(event_report_windows.has_value());
  return event_report_windows.value();
}

}  // namespace content
