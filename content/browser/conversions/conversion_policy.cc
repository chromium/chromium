// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_policy.h"

#include "base/cxx17_backports.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/time/time.h"

namespace content {

namespace {

WARN_UNUSED_RESULT
uint64_t MaxAllowedValueForSourceType(
    StorableImpression::SourceType source_type) {
  switch (source_type) {
    case StorableImpression::SourceType::kNavigation:
      return 8;
    case StorableImpression::SourceType::kEvent:
      return 2;
  }
}

}  // namespace

ConversionPolicy::ConversionPolicy(bool debug_mode) : debug_mode_(debug_mode) {}

ConversionPolicy::~ConversionPolicy() = default;

bool ConversionPolicy::ShouldNoiseConversionData() const {
  return base::RandDouble() <= .05;
}

uint64_t ConversionPolicy::MakeNoisedConversionData(uint64_t max) const {
  return base::RandGenerator(max);
}

uint64_t ConversionPolicy::GetSanitizedConversionData(
    uint64_t conversion_data,
    StorableImpression::SourceType source_type) const {
  const uint64_t max_allowed_values = MaxAllowedValueForSourceType(source_type);

  // Add noise to the conversion when the value is first sanitized from a
  // conversion registration event. This noised data will be used for all
  // associated impressions that convert.
  if (!debug_mode_ && ShouldNoiseConversionData()) {
    const uint64_t noised_data = MakeNoisedConversionData(max_allowed_values);
    DCHECK_LT(noised_data, max_allowed_values);
    return noised_data;
  }

  return conversion_data % max_allowed_values;
}

bool ConversionPolicy::IsConversionDataInRange(
    uint64_t conversion_data,
    StorableImpression::SourceType source_type) const {
  return conversion_data < MaxAllowedValueForSourceType(source_type);
}

uint64_t ConversionPolicy::GetSanitizedImpressionData(
    uint64_t impression_data) const {
  // Impression data is allowed the full 64 bits.
  return impression_data;
}

base::Time ConversionPolicy::GetExpiryTimeForImpression(
    const absl::optional<base::TimeDelta>& declared_expiry,
    base::Time impression_time,
    StorableImpression::SourceType source_type) const {
  constexpr base::TimeDelta kMinImpressionExpiry = base::TimeDelta::FromDays(1);
  constexpr base::TimeDelta kDefaultImpressionExpiry =
      base::TimeDelta::FromDays(30);

  // Default to the maximum expiry time.
  base::TimeDelta expiry = declared_expiry.value_or(kDefaultImpressionExpiry);

  // Expiry time for event sources must be a whole number of days.
  if (source_type == StorableImpression::SourceType::kEvent)
    expiry = expiry.RoundToMultiple(base::TimeDelta::FromDays(1));

  // If the impression specified its own expiry, clamp it to the minimum and
  // maximum.
  return impression_time +
         base::clamp(expiry, kMinImpressionExpiry, kDefaultImpressionExpiry);
}

base::Time ConversionPolicy::GetReportTimeForReportPastSendTime(
    base::Time now) const {
  // Do not use any delay in debug mode.
  if (debug_mode_)
    return now;

  // Add uniform random noise in the range of [0, 5 minutes] to the report time.
  // TODO(https://crbug.com/1075600): This delay is very conservative. Consider
  // increasing this delay once we can be sure reports are still sent at
  // reasonable times, and not delayed for many browser sessions due to short
  // session up-times.
  return now +
         base::TimeDelta::FromMilliseconds(base::RandInt(0, 5 * 60 * 1000));
}

base::TimeDelta ConversionPolicy::GetMaxReportAge() const {
  // Chosen from looking at "Conversions.ExtraReportDelay" histogram.
  return base::TimeDelta::FromDays(14);
}

}  // namespace content
