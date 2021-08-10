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

// Maximum number of allowed conversion metadata values. Higher entropy
// conversion metadata is stripped to these lower bits.
const int kMaxAllowedConversionValues = 8;

// Maximum number of allowed event source trigger data values. Higher entropy
// event source trigger data is stripped to these lower bits.
const int kMaxAllowedEventSourceTriggerDataValues = 2;

}  // namespace

uint64_t ConversionPolicy::NoiseProvider::GetNoisedConversionData(
    uint64_t conversion_data) const {
  // Return |conversion_data| without any noise 95% of the time.
  if (base::RandDouble() > .05)
    return conversion_data;

  // 5% of the time return a random number in the allowed range. Note that the
  // value is noised 5% of the time, but only wrong 5 *
  // (kMaxAllowedConversionValues - 1) / kMaxAllowedConversionValues percent of
  // the time.
  return static_cast<uint64_t>(base::RandInt(0, kMaxAllowedConversionValues));
}

// static
uint64_t ConversionPolicy::NoiseProvider::GetNoisedEventSourceTriggerDataImpl(
    uint64_t event_source_trigger_data) {
  // Return |event_source_trigger_data| without any noise 95% of the time.
  if (base::RandDouble() > .05)
    return event_source_trigger_data;

  // 5% of the time return a random number in the allowed range. Note that the
  // value is noised 5% of the time, but only wrong 5 *
  // (kMaxAllowedEventSourceTriggerDataValues - 1) /
  // kMaxAllowedEventSourceTriggerDataValues percent of the time.
  return static_cast<uint64_t>(
      base::RandInt(0, kMaxAllowedEventSourceTriggerDataValues));
}

uint64_t ConversionPolicy::NoiseProvider::GetNoisedEventSourceTriggerData(
    uint64_t event_source_trigger_data) const {
  return GetNoisedEventSourceTriggerDataImpl(event_source_trigger_data);
}

// static
std::unique_ptr<ConversionPolicy> ConversionPolicy::CreateForTesting(
    std::unique_ptr<NoiseProvider> noise_provider) {
  return base::WrapUnique<ConversionPolicy>(
      new ConversionPolicy(std::move(noise_provider)));
}

ConversionPolicy::ConversionPolicy(bool debug_mode)
    : debug_mode_(debug_mode),
      noise_provider_(debug_mode ? nullptr
                                 : std::make_unique<NoiseProvider>()) {}

ConversionPolicy::ConversionPolicy(
    std::unique_ptr<ConversionPolicy::NoiseProvider> noise_provider)
    : debug_mode_(false), noise_provider_(std::move(noise_provider)) {}

ConversionPolicy::~ConversionPolicy() = default;

uint64_t ConversionPolicy::GetSanitizedConversionData(
    uint64_t conversion_data) const {
  // Add noise to the conversion when the value is first sanitized from a
  // conversion registration event. This noised data will be used for all
  // associated impressions that convert.
  if (noise_provider_)
    conversion_data = noise_provider_->GetNoisedConversionData(conversion_data);

  // Allow at most 3 bits of entropy in conversion data.
  return conversion_data % kMaxAllowedConversionValues;
}

uint64_t ConversionPolicy::GetSanitizedEventSourceTriggerData(
    uint64_t event_source_trigger_data) const {
  // Add noise to the conversion when the value is first sanitized from a
  // conversion registration event. This noised data will be used for all
  // associated impressions that convert.
  if (noise_provider_) {
    event_source_trigger_data =
        noise_provider_->GetNoisedEventSourceTriggerData(
            event_source_trigger_data);
  }

  // Allow at most 1 bit of entropy in event source trigger data.
  return event_source_trigger_data % kMaxAllowedEventSourceTriggerDataValues;
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
