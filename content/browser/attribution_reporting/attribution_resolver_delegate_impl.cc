// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_resolver_delegate_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "base/uuid.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/event_level_epsilon.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/trigger_config.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/stored_source.h"

namespace content {

namespace {

using ::attribution_reporting::mojom::SourceType;

}  // namespace

// static
std::unique_ptr<AttributionResolverDelegate>
AttributionResolverDelegateImpl::CreateForTesting(
    AttributionNoiseMode noise_mode,
    AttributionDelayMode delay_mode,
    const AttributionConfig& config) {
  return base::WrapUnique(
      new AttributionResolverDelegateImpl(noise_mode, delay_mode, config));
}

AttributionResolverDelegateImpl::AttributionResolverDelegateImpl(
    AttributionNoiseMode noise_mode,
    AttributionDelayMode delay_mode)
    : AttributionResolverDelegateImpl(noise_mode,
                                     delay_mode,
                                     AttributionConfig()) {}

AttributionResolverDelegateImpl::AttributionResolverDelegateImpl(
    AttributionNoiseMode noise_mode,
    AttributionDelayMode delay_mode,
    const AttributionConfig& config)
    : AttributionResolverDelegate(config),
      noise_mode_(noise_mode),
      delay_mode_(delay_mode) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AttributionResolverDelegateImpl::~AttributionResolverDelegateImpl() = default;

base::TimeDelta
AttributionResolverDelegateImpl::GetDeleteExpiredSourcesFrequency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Minutes(5);
}

base::TimeDelta
AttributionResolverDelegateImpl::GetDeleteExpiredRateLimitsFrequency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Minutes(5);
}

base::Time AttributionResolverDelegateImpl::GetEventLevelReportTime(
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

base::Time AttributionResolverDelegateImpl::GetAggregatableReportTime(
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

base::Uuid AttributionResolverDelegateImpl::NewReportID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Uuid::GenerateRandomV4();
}

std::optional<AttributionResolverDelegate::OfflineReportDelayConfig>
AttributionResolverDelegateImpl::GetOfflineReportDelayConfig() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (noise_mode_ == AttributionNoiseMode::kDefault &&
      delay_mode_ == AttributionDelayMode::kDefault) {
    // Add uniform random noise in the range of [0, 1 minutes] to the report
    // time.
    // TODO(crbug.com/40687765): This delay is very conservative.
    // Consider increasing this delay once we can be sure reports are still
    // sent at reasonable times, and not delayed for many browser sessions due
    // to short session up-times.
    return OfflineReportDelayConfig{
        .min = base::Minutes(0),
        .max = base::Minutes(1),
    };
  }

  return std::nullopt;
}

void AttributionResolverDelegateImpl::ShuffleReports(
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

std::optional<double>
AttributionResolverDelegateImpl::GetRandomizedResponseRate(
    const attribution_reporting::TriggerSpecs& trigger_specs,
    attribution_reporting::EventLevelEpsilon epsilon) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto num_states = GetNumStates(trigger_specs);
  if (!num_states.has_value()) {
    return std::nullopt;
  }
  return attribution_reporting::GetRandomizedResponseRate(*num_states, epsilon);
}

AttributionResolverDelegate::GetRandomizedResponseResult
AttributionResolverDelegateImpl::GetRandomizedResponse(
    SourceType source_type,
    const attribution_reporting::TriggerSpecs& trigger_specs,
    attribution_reporting::EventLevelEpsilon epsilon,
    const std::optional<attribution_reporting::AttributionScopesData>&
        scopes_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ASSIGN_OR_RETURN(auto response,
                   attribution_reporting::DoRandomizedResponse(
                       trigger_specs, epsilon, source_type, scopes_data,
                       config_.privacy_math_config));

  switch (noise_mode_) {
    case AttributionNoiseMode::kDefault:
      break;
    case AttributionNoiseMode::kNone:
      // TODO(apaseltiner): When noise is disabled, we shouldn't even bother
      // generating the response in the first place.
      response.response() = std::nullopt;
      break;
  }

  return response;
}

bool AttributionResolverDelegateImpl::
    GenerateNullAggregatableReportForLookbackDay(
        int lookback_day,
        attribution_reporting::mojom::SourceRegistrationTimeConfig
            source_registration_time_config) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  switch (noise_mode_) {
    case AttributionNoiseMode::kDefault:
      break;
    case AttributionNoiseMode::kNone:
      return false;
  }

  double rate;
  switch (source_registration_time_config) {
    case attribution_reporting::mojom::SourceRegistrationTimeConfig::kInclude:
      rate = config_.aggregate_limit
                 .null_reports_rate_include_source_registration_time;
      break;
    case attribution_reporting::mojom::SourceRegistrationTimeConfig::kExclude:
      rate = config_.aggregate_limit
                 .null_reports_rate_exclude_source_registration_time;
      break;
  }

  return attribution_reporting::GenerateWithRate(rate);
}

}  // namespace content
