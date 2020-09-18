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
}  // namespace

namespace net {

std::unique_ptr<base::Value> BackoffEntrySerializer::SerializeToValue(
    const BackoffEntry& entry,
    base::Time time_now) {
  std::unique_ptr<base::ListValue> serialized(new base::ListValue());
  serialized->AppendInteger(kSerializationFormatVersion);

  serialized->AppendInteger(entry.failure_count());

  // Can't use entry.GetTimeUntilRelease as it doesn't allow negative deltas.
  base::TimeDelta backoff_duration =
      entry.GetReleaseTime() - entry.GetTimeTicksNow();
  // Redundantly stores both the remaining time delta and the absolute time.
  // The delta is used to work around some cases where wall clock time changes.
  serialized->AppendDouble(backoff_duration.InSecondsF());
  base::Time absolute_release_time = backoff_duration + time_now;
  serialized->AppendString(
      base::NumberToString(absolute_release_time.ToInternalValue()));

  return std::move(serialized);
}

std::unique_ptr<BackoffEntry> BackoffEntrySerializer::DeserializeFromValue(
    const base::Value& serialized,
    const BackoffEntry::Policy* policy,
    const base::TickClock* tick_clock,
    base::Time time_now) {
  const base::ListValue* serialized_list = nullptr;
  if (!serialized.GetAsList(&serialized_list))
    return nullptr;
  if (serialized_list->GetSize() != 4)
    return nullptr;
  int version_number;
  if (!serialized_list->GetInteger(0, &version_number) ||
      version_number != kSerializationFormatVersion) {
    return nullptr;
  }

  int failure_count;
  if (!serialized_list->GetInteger(1, &failure_count) || failure_count < 0) {
    return nullptr;
  }
  failure_count = std::min(failure_count, kMaxFailureCount);

  double original_backoff_duration_double;
  if (!serialized_list->GetDouble(2, &original_backoff_duration_double))
    return nullptr;
  std::string absolute_release_time_string;
  if (!serialized_list->GetString(3, &absolute_release_time_string))
    return nullptr;
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
  // Before computing |backoff_duration|, throw out +/- infinity values for
  // either operand. This way, we can use base::TimeDelta's saturated math.
  if (absolute_release_time.is_min() || absolute_release_time.is_max() ||
      time_now.is_min() || time_now.is_max()) {
    return nullptr;
  }
  base::TimeDelta backoff_duration =
      absolute_release_time.ToDeltaSinceWindowsEpoch() -
      time_now.ToDeltaSinceWindowsEpoch();
  // In cases where the system wall clock is rewound, use the redundant
  // original_backoff_duration to ensure the backoff duration isn't longer
  // than it was before serializing (note that it's not possible to protect
  // against the clock being wound forward).
  if (backoff_duration > original_backoff_duration)
    backoff_duration = original_backoff_duration;
  if (backoff_duration.is_min() || backoff_duration.is_max())
    return nullptr;
  entry->SetCustomReleaseTime(
      entry->BackoffDurationToReleaseTime(backoff_duration));

  return entry;
}

}  // namespace net
