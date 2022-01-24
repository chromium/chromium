// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_policy.h"

#include <math.h>

#include "base/check.h"
#include "base/cxx17_backports.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/time/time.h"

namespace content {

namespace {

WARN_UNUSED_RESULT
uint64_t MaxAllowedValueForSourceType(StorableSource::SourceType source_type) {
  switch (source_type) {
    case StorableSource::SourceType::kNavigation:
      return 8;
    case StorableSource::SourceType::kEvent:
      return 2;
  }
}

}  // namespace

AttributionPolicy::AttributionPolicy(bool debug_mode)
    : debug_mode_(debug_mode) {}

AttributionPolicy::~AttributionPolicy() = default;

bool AttributionPolicy::ShouldNoiseTriggerData() const {
  return base::RandDouble() <= .05;
}

uint64_t AttributionPolicy::MakeNoisedTriggerData(uint64_t max) const {
  return base::RandGenerator(max);
}

uint64_t AttributionPolicy::SanitizeTriggerData(
    uint64_t trigger_data,
    StorableSource::SourceType source_type) const {
  const uint64_t max_allowed_values = MaxAllowedValueForSourceType(source_type);

  // Add noise to the conversion when the value is first sanitized from a
  // conversion registration event. This noised data will be used for all
  // associated impressions that convert.
  if (!debug_mode_ && ShouldNoiseTriggerData()) {
    const uint64_t noised_data = MakeNoisedTriggerData(max_allowed_values);
    DCHECK_LT(noised_data, max_allowed_values);
    return noised_data;
  }

  return trigger_data % max_allowed_values;
}

bool AttributionPolicy::IsTriggerDataInRange(
    uint64_t trigger_data,
    StorableSource::SourceType source_type) const {
  return trigger_data < MaxAllowedValueForSourceType(source_type);
}

uint64_t AttributionPolicy::SanitizeSourceEventId(
    uint64_t source_event_id) const {
  // Impression data is allowed the full 64 bits.
  return source_event_id;
}

base::Time AttributionPolicy::GetExpiryTimeForImpression(
    const absl::optional<base::TimeDelta>& declared_expiry,
    base::Time impression_time,
    StorableSource::SourceType source_type) const {
  constexpr base::TimeDelta kMinImpressionExpiry = base::Days(1);
  constexpr base::TimeDelta kDefaultImpressionExpiry = base::Days(30);

  // Default to the maximum expiry time.
  base::TimeDelta expiry = declared_expiry.value_or(kDefaultImpressionExpiry);

  // Expiry time for event sources must be a whole number of days.
  if (source_type == StorableSource::SourceType::kEvent)
    expiry = expiry.RoundToMultiple(base::Days(1));

  // If the impression specified its own expiry, clamp it to the minimum and
  // maximum.
  return impression_time +
         base::clamp(expiry, kMinImpressionExpiry, kDefaultImpressionExpiry);
}

base::Time AttributionPolicy::GetReportTimeForReportPastSendTime(
    base::Time now) const {
  // Do not use any delay in debug mode.
  if (debug_mode_)
    return now;

  // Add uniform random noise in the range of [0, 5 minutes] to the report time.
  // TODO(https://crbug.com/1075600): This delay is very conservative. Consider
  // increasing this delay once we can be sure reports are still sent at
  // reasonable times, and not delayed for many browser sessions due to short
  // session up-times.
  return now + base::Milliseconds(base::RandInt(0, 5 * 60 * 1000));
}

absl::optional<base::TimeDelta> AttributionPolicy::GetFailedReportDelay(
    int failed_send_attempts) const {
  DCHECK_GT(failed_send_attempts, 0);

  const int kMaxFailedSendAttempts = 2;
  const base::TimeDelta kInitialReportDelay = base::Minutes(5);
  const int kDelayFactor = 3;

  if (failed_send_attempts > kMaxFailedSendAttempts)
    return absl::nullopt;

  return kInitialReportDelay * pow(kDelayFactor, failed_send_attempts - 1);
}

StorableSource::AttributionLogic
AttributionPolicy::GetAttributionLogicForImpression(
    StorableSource::SourceType source_type) const {
  if (debug_mode_)
    return StorableSource::AttributionLogic::kTruthfully;

  switch (source_type) {
    case StorableSource::SourceType::kNavigation:
      return StorableSource::AttributionLogic::kTruthfully;
    case StorableSource::SourceType::kEvent: {
      // TODO(apaseltiner): Finalize a value for this so that noise is actually
      // triggered.
      const double kNoise = 0;
      if (base::RandDouble() < (1 - kNoise))
        return StorableSource::AttributionLogic::kTruthfully;
      if (base::RandInt(0, 1) == 0)
        return StorableSource::AttributionLogic::kNever;
      return StorableSource::AttributionLogic::kFalsely;
    }
  }
}

}  // namespace content
