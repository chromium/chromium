// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/backoff_entry_serializer.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "net/base/backoff_entry.h"

namespace {
// This max defines how many times we are willing to call
// |BackoffEntry::InformOfRequest| in |DeserializeFromList|.
//
// This value is meant to large enough that the computed backoff duration can
// still be saturated. Given that the duration is an int64 and assuming 1.01 as
// a conservative lower bound for BackoffEntry::Policy::multiply_factor,
// ceil(log(2**63-1, 1.01)) = 4389.
const int kMaxFailureCount = 4389;

// This function returns true iff |duration| is finite and can be serialized and
// deserialized without becoming infinite. This function is aligned with the
// latest version.
bool BackoffDurationSafeToSerialize(const base::TimeDelta& duration) {
  return !duration.is_inf() &&
         !base::Microseconds(duration.InMicroseconds()).is_inf();
}
}  // namespace

namespace net {

base::Value::List BackoffEntrySerializer::SerializeToList(
    const BackoffEntry& entry,
    base::Time time_now) {
  base::Value::List serialized;
  serialized.Append(SerializationFormatVersion::kVersion2);

  serialized.Append(entry.failure_count());

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
  serialized.Append(base::NumberToString(backoff_duration.InMicroseconds()));
  serialized.Append(
      base::NumberToString(absolute_release_time.ToInternalValue()));

  return serialized;
}

std::unique_ptr<BackoffEntry> BackoffEntrySerializer::DeserializeFromList(
    const base::Value::List& serialized,
    const BackoffEntry::Policy* policy,
    const base::TickClock* tick_clock,
    base::Time time_now) {
  if (serialized.size() != 4)
    return nullptr;

  if (!serialized[0].is_int())
    return nullptr;
  int version_number = serialized[0].GetInt();
  if (version_number != kVersion1 && version_number != kVersion2)
    return nullptr;

  if (!serialized[1].is_int())
    return nullptr;
  int failure_count = serialized[1].GetInt();
  if (failure_count < 0) {
    return nullptr;
  }
  failure_count = std::min(failure_count, kMaxFailureCount);

  base::TimeDelta original_backoff_duration;
  switch (version_number) {
    case kVersion1: {
      if (!serialized[2].is_double())
        return nullptr;
      double original_backoff_duration_double = serialized[2].GetDouble();
      original_backoff_duration =
          base::Seconds(original_backoff_duration_double);
      break;
    }
    case kVersion2: {
      if (!serialized[2].is_string())
        return nullptr;
      std::string original_backoff_duration_string = serialized[2].GetString();
      int64_t original_backoff_duration_us;
      if (!base::StringToInt64(original_backoff_duration_string,
                               &original_backoff_duration_us)) {
        return nullptr;
      }
      original_backoff_duration =
          base::Microseconds(original_backoff_duration_us);
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unexpected version_number: " << version_number;
  }

  if (!serialized[3].is_string())
    return nullptr;
  std::string absolute_release_time_string = serialized[3].GetString();

  int64_t absolute_release_time_us;
  if (!base::StringToInt64(absolute_release_time_string,
                           &absolute_release_time_us)) {
    return nullptr;
  }

  auto entry = std::make_unique<BackoffEntry>(policy, tick_clock);

  for (int n = 0; n < failure_count; n++)
    entry->InformOfRequest(false);

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

  const base::TimeTicks release_time =
      entry->BackoffDurationToReleaseTime(backoff_duration);
  if (release_time.is_inf())
    return nullptr;
  entry->SetCustomReleaseTime(release_time);

  return entry;
}

}  // namespace net
