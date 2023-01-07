// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/radio_activity_tracker.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "net/base/features.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net::android {

namespace {

// The minimum interval for recording possible radio wake-ups. It's unlikely
// that radio state transitions happen in seconds.
constexpr base::TimeDelta kMinimumRecordIntervalForPossibleWakeupTrigger =
    base::Seconds(1);

}  // namespace

// static
RadioActivityTracker& RadioActivityTracker::GetInstance() {
  static base::NoDestructor<RadioActivityTracker> s_instance;
  return *s_instance;
}

RadioActivityTracker::RadioActivityTracker() = default;

bool RadioActivityTracker::ShouldRecordActivityForWakeupTrigger() {
  if (!base::FeatureList::IsEnabled(features::kRecordRadioWakeupTrigger))
    return false;

  if (!IsRadioUtilsSupported())
    return false;

  base::TimeTicks now = base::TimeTicks::Now();

  // Check recording interval first to reduce overheads of calling Android's
  // platform APIs.
  if (!last_check_time_.is_null() &&
      now - last_check_time_ < kMinimumRecordIntervalForPossibleWakeupTrigger)
    return false;

  last_check_time_ = now;

  bool should_record = ShouldRecordActivityForWakeupTriggerInternal();

  // TODO(crbug.com/1232623): Use "Net." prefix instead of "Network."
  base::UmaHistogramTimes(
      "Network.Radio.PossibleWakeupTrigger.RadioUtilsOverhead",
      base::TimeTicks::Now() - now);

  return should_record;
}

bool RadioActivityTracker::IsRadioUtilsSupported() {
  return base::android::RadioUtils::IsSupported() ||
         radio_activity_override_for_testing_.has_value() ||
         radio_type_override_for_testing_.has_value();
}

bool RadioActivityTracker::ShouldRecordActivityForWakeupTriggerInternal() {
  base::android::RadioConnectionType radio_type =
      radio_type_override_for_testing_.value_or(
          base::android::RadioUtils::GetConnectionType());
  if (radio_type != base::android::RadioConnectionType::kCell)
    return false;

  absl::optional<base::android::RadioDataActivity> radio_activity =
      radio_activity_override_for_testing_.has_value()
          ? radio_activity_override_for_testing_
          : base::android::RadioUtils::GetCellDataActivity();

  if (!radio_activity.has_value())
    return false;

  // When the last activity was dormant, don't treat this event as a wakeup
  // trigger since there could be state transition delay and startup latency.
  bool should_record =
      *radio_activity == base::android::RadioDataActivity::kDormant &&
      last_radio_data_activity_ != base::android::RadioDataActivity::kDormant;
  last_radio_data_activity_ = *radio_activity;
  return should_record;
}

void MaybeRecordTCPWriteForWakeupTrigger(
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  if (!RadioActivityTracker::GetInstance()
           .ShouldRecordActivityForWakeupTrigger()) {
    return;
  }

  base::UmaHistogramSparse(kUmaNamePossibleWakeupTriggerTCPWriteAnnotationId,
                           traffic_annotation.unique_id_hash_code);
}

void MaybeRecordUDPWriteForWakeupTrigger(
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  if (!RadioActivityTracker::GetInstance()
           .ShouldRecordActivityForWakeupTrigger()) {
    return;
  }

  base::UmaHistogramSparse(kUmaNamePossibleWakeupTriggerUDPWriteAnnotationId,
                           traffic_annotation.unique_id_hash_code);
}

}  // namespace net::android
