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

using AttributionLogic = ::content::StorableSource::AttributionLogic;
using AttributionMode = ::content::AttributionPolicy::AttributionMode;

uint64_t TriggerDataCardinality(StorableSource::SourceType source_type) {
  switch (source_type) {
    case StorableSource::SourceType::kNavigation:
      return 8;
    case StorableSource::SourceType::kEvent:
      return 2;
  }
}

}  // namespace

AttributionMode::AttributionMode(AttributionLogic logic,
                                 absl::optional<uint64_t> fake_trigger_data)
    : logic_(logic), fake_trigger_data_(fake_trigger_data) {
  DCHECK_EQ(logic_ == AttributionLogic::kFalsely,
            fake_trigger_data_.has_value());
}

AttributionMode::~AttributionMode() = default;

AttributionMode::AttributionMode(const AttributionMode&) = default;
AttributionMode::AttributionMode(AttributionMode&&) = default;

AttributionMode& AttributionMode::operator=(const AttributionMode&) = default;
AttributionMode& AttributionMode::operator=(AttributionMode&&) = default;

AttributionPolicy::AttributionPolicy(bool debug_mode)
    : debug_mode_(debug_mode) {}

AttributionPolicy::~AttributionPolicy() = default;

bool AttributionPolicy::ShouldNoiseTriggerData() const {
  return base::RandDouble() <= .05;
}

uint64_t AttributionPolicy::MakeNoisedTriggerData(uint64_t cardinality) const {
  return base::RandGenerator(cardinality);
}

uint64_t AttributionPolicy::SanitizeTriggerData(
    uint64_t trigger_data,
    StorableSource::SourceType source_type) const {
  const uint64_t cardinality = TriggerDataCardinality(source_type);

  // Add noise to the conversion when the value is first sanitized from a
  // conversion registration event. This noised data will be used for all
  // associated impressions that convert.
  if (!debug_mode_ && ShouldNoiseTriggerData()) {
    const uint64_t noised_data = MakeNoisedTriggerData(cardinality);
    DCHECK_LT(noised_data, cardinality);
    return noised_data;
  }

  return trigger_data % cardinality;
}

bool AttributionPolicy::IsTriggerDataInRange(
    uint64_t trigger_data,
    StorableSource::SourceType source_type) const {
  return trigger_data < TriggerDataCardinality(source_type);
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

absl::optional<AttributionPolicy::OfflineReportDelayConfig>
AttributionPolicy::GetOfflineReportDelayConfig() const {
  if (debug_mode_)
    return absl::nullopt;

  // Add uniform random noise in the range of [0, 5 minutes] to the report time.
  // TODO(https://crbug.com/1075600): This delay is very conservative. Consider
  // increasing this delay once we can be sure reports are still sent at
  // reasonable times, and not delayed for many browser sessions due to short
  // session up-times.
  return OfflineReportDelayConfig{
      .min = base::Minutes(0),
      .max = base::Minutes(5),
  };
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

AttributionPolicy::AttributionMode AttributionPolicy::GetAttributionMode(
    StorableSource::SourceType source_type) const {
  if (debug_mode_)
    return AttributionMode(AttributionLogic::kTruthfully);

  double randomized_response_probability;

  // TODO(apaseltiner): Pick non-zero probabilities.
  switch (source_type) {
    case StorableSource::SourceType::kNavigation:
      randomized_response_probability = 0;
      break;
    case StorableSource::SourceType::kEvent:
      randomized_response_probability = 0;
      break;
  }

  if (base::RandDouble() < randomized_response_probability) {
    // The 0 value is reserved for `kNever`, so we add 1 here and subtract it
    // later.
    uint64_t r = MakeNoisedTriggerData(1 + TriggerDataCardinality(source_type));
    if (r == 0)
      return AttributionMode(AttributionLogic::kNever);

    return AttributionMode(AttributionLogic::kFalsely, r - 1);
  }

  return AttributionMode(AttributionLogic::kTruthfully);
}

}  // namespace content
