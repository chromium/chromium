// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/backoff_entry_serializer.h"

#include <algorithm>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "net/base/backoff_entry.h"

namespace {
// Increment this number when changing the serialization format, to avoid old
// serialized values loaded from disk etc being misinterpreted.
const int kSerializationFormatVersion = 1;

// This max defines how many times we are willing to call
// |BackoffEntry::InformOfRequest| in |DeserializeFromValue|.
//
// This value is meant to large enough that the computed backoff duration can
// still be saturated. Given that the duration is an int64 and assuming 1.01 as
// a conservative lower bound for BackoffEntry::Policy::multiply_factor,
// ceil(log(2**63-1, 1.01)) = 4389.
const int kMaxFailureCount = 4389;

// This function returns true iff |duration| is finite and can be converted to
// double and back without becoming infinite.
bool BackoffDurationSafeToSerialize(const base::TimeDelta& duration) {
  return !duration.is_inf() &&
         !base::TimeDelta::FromSecondsD(duration.InSecondsF()).is_inf();
}
}  // namespace

namespace net {

base::Value BackoffEntrySerializer::SerializeToValue(const BackoffEntry& entry,
                                                     base::Time time_now) {
  std::vector<base::Value> serialized;
  serialized.emplace_back(kSerializationFormatVersion);

  serialized.emplace_back(entry.failure_count());

  // Convert both |base::TimeTicks| values into |base::TimeDelta| values by
  // subtracting |kZeroTicks. This way, the top-level subtraction uses
  // |base::TimeDelta::operator-|, which has clamping semantics.
  const base::TimeTicks kZeroTicks;
  const base::TimeDelta kReleaseTime = entry.GetReleaseTime() - kZeroTicks;
  const base::TimeDelta kTimeTicksNow = entry.GetTimeTicksNow() - kZeroTicks;
  base::TimeDelta backoff_duration;
  if (!kReleaseTime.is_inf() && !kTimeTicksNow.is_inf()) {
    backoff_duration = kReleaseTime - kTimeTicksNow;
  }
  if (!BackoffDurationSafeToSerialize(backoff_duration)) {
    backoff_duration = base::TimeDelta();
  }

  base::Time absolute_release_time = backoff_duration + time_now;
  // If the computed release time is infinite, default to zero. The deserializer
  // should pick up on this.
  if (absolute_release_time.is_inf()) {
    absolute_release_time = base::Time();
  }

  // Redundantly stores both the remaining time delta and the absolute time.
  // The delta is used to work around some cases where wall clock time changes.
  serialized.emplace_back(backoff_duration.InSecondsF());
  serialized.emplace_back(
      base::NumberToString(absolute_release_time.ToInternalValue()));

  return base::Value(std::move(serialized));
}

std::unique_ptr<BackoffEntry> BackoffEntrySerializer::DeserializeFromValue(
    const base::Value& serialized,
    const BackoffEntry::Policy* policy,
    const base::TickClock* tick_clock,
    base::Time time_now) {
  if (!serialized.is_list())
    return nullptr;
  const base::Value::ConstListView& list_view = serialized.GetList();

  if (list_view.size() != 4)
    return nullptr;

  if (!list_view[0].is_int())
    return nullptr;
  int version_number = list_view[0].GetInt();
  if (version_number != kSerializationFormatVersion)
    return nullptr;

  if (!list_view[1].is_int())
    return nullptr;
  int failure_count = list_view[1].GetInt();
  if (failure_count < 0) {
    return nullptr;
  }
  failure_count = std::min(failure_count, kMaxFailureCount);

  if (!list_view[2].is_double())
    return nullptr;
  double original_backoff_duration_double = list_view[2].GetDouble();

  if (!list_view[3].is_string())
    return nullptr;
  std::string absolute_release_time_string = list_view[3].GetString();

  int64_t absolute_release_time_us;
  if (!base::StringToInt64(absolute_release_time_string,
                           &absolute_release_time_us)) {
    return nullptr;
  }

  std::unique_ptr<BackoffEntry> entry(new BackoffEntry(policy, tick_clock));

  for (int n = 0; n < failure_count; n++)
    entry->InformOfRequest(false);

  base::TimeDelta original_backoff_duration =
      base::TimeDelta::FromSecondsD(original_backoff_duration_double);
  base::Time absolute_release_time =
      base::Time::FromInternalValue(absolute_release_time_us);

  base::TimeDelta backoff_duration;
  if (absolute_release_time == base::Time()) {
    // When the serializer cannot compute a finite release time, it uses zero.
    // When we see this, fall back to the redundant original_backoff_duration.
    backoff_duration = original_backoff_duration;
  } else {
    // Before computing |backoff_duration|, throw out +/- infinity values for
    // either operand. This way, we can use base::TimeDelta's saturated math.
    if (absolute_release_time.is_inf() || time_now.is_inf())
      return nullptr;

    backoff_duration = absolute_release_time.ToDeltaSinceWindowsEpoch() -
                       time_now.ToDeltaSinceWindowsEpoch();

    // In cases where the system wall clock is rewound, use the redundant
    // original_backoff_duration to ensure the backoff duration isn't longer
    // than it was before serializing (note that it's not possible to protect
    // against the clock being wound forward).
    if (backoff_duration > original_backoff_duration)
      backoff_duration = original_backoff_duration;
  }
  if (!BackoffDurationSafeToSerialize(backoff_duration))
    return nullptr;
  entry->SetCustomReleaseTime(
      entry->BackoffDurationToReleaseTime(backoff_duration));

  return entry;
}

}  // namespace net
