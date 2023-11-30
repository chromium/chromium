// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <iterator>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/event_level_epsilon.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/privacy_math.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "services/network/public/cpp/trigger_verification.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

using ::attribution_reporting::mojom::SourceType;

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
    const attribution_reporting::EventReportWindows& event_report_windows,
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
    const attribution_reporting::TriggerSpecs& trigger_specs,
    attribution_reporting::MaxEventLevelReports max_event_level_reports,
    attribution_reporting::EventLevelEpsilon epsilon) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return content::GetRandomizedResponseRate(
      GetNumStates(trigger_specs, max_event_level_reports), epsilon);
}

AttributionStorageDelegate::GetRandomizedResponseResult
AttributionStorageDelegateImpl::GetRandomizedResponse(
    SourceType source_type,
    const attribution_reporting::TriggerSpecs& trigger_specs,
    attribution_reporting::MaxEventLevelReports max_event_level_reports,
    attribution_reporting::EventLevelEpsilon epsilon,
    base::Time source_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RandomizedResponseData response =
      DoRandomizedResponse(trigger_specs, max_event_level_reports, epsilon);

  if (response.channel_capacity() > GetMaxChannelCapacity(source_type)) {
    return base::unexpected(ExceedsChannelCapacityLimit());
  }

  switch (noise_mode_) {
    case AttributionNoiseMode::kDefault:
      return response;
    case AttributionNoiseMode::kNone:
      return RandomizedResponseData(response.rate(),
                                    response.channel_capacity(), absl::nullopt);
  }
}

std::vector<AttributionStorageDelegate::NullAggregatableReport>
AttributionStorageDelegateImpl::GetNullAggregatableReports(
    const AttributionTrigger& trigger,
    base::Time trigger_time,
    absl::optional<base::Time> attributed_source_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

  bool has_trigger_context_id =
      trigger.registration()
          .aggregatable_trigger_config.trigger_context_id()
          .has_value();

  switch (trigger.registration()
              .aggregatable_trigger_config.source_registration_time_config()) {
    case attribution_reporting::mojom::SourceRegistrationTimeConfig::kInclude: {
      absl::optional<base::Time> rounded_attributed_source_time;
      if (attributed_source_time) {
        rounded_attributed_source_time =
            RoundDownToWholeDaySinceUnixEpoch(*attributed_source_time);
      }

      static_assert(attribution_reporting::kMaxSourceExpiry == base::Days(30),
                    "update null reports rate");

      CHECK(!has_trigger_context_id);

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
          has_trigger_context_id
              ? 1.
              : config_.aggregate_limit
                    .null_reports_rate_exclude_source_registration_time);
    }
  }
}

}  // namespace content
