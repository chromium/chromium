// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/change_rate_monitor.h"

#include "base/rand_util.h"
#include "third_party/blink/renderer/modules/compute_pressure/pressure_source_index.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// |observation_window_time| boundaries in seconds.
constexpr uint64_t kMinObservationWindowInSeconds = 300;
constexpr uint64_t kMaxObservationWindowInSeconds = 600;

// |change_count_threshold| boundaries in changes.
constexpr uint64_t kMinChangesThreshold = 50;
constexpr uint64_t kMaxChangesThreshold = 100;

// |penalty_duration| boundaries in seconds.
constexpr uint64_t kMinPenaltyDurationInSeconds = 5;
constexpr uint64_t kMaxPenaltyDurationInSeconds = 10;

ChangeRateMonitor::ChangeRateMonitor() {
  Reset();
}

ChangeRateMonitor::~ChangeRateMonitor() = default;

void ChangeRateMonitor::Reset() {
  observation_window_time_ = base::Seconds(base::RandInt(
      kMinObservationWindowInSeconds, kMaxObservationWindowInSeconds));
  change_count_threshold_ =
      base::RandInt(kMinChangesThreshold, kMaxChangesThreshold);
  penalty_duration_ = base::Seconds(base::RandInt(
      kMinPenaltyDurationInSeconds, kMaxPenaltyDurationInSeconds));
  start_time_ = base::TimeTicks::Now();
  change_count_.fill(0);
}

void ChangeRateMonitor::ResetIfNeeded() {
  const base::TimeDelta time_diff = base::TimeTicks::Now() - start_time_;
  CHECK(time_diff.is_positive());
  if (time_diff > observation_window_time_) {
    Reset();
  }
}

void ChangeRateMonitor::ResetChangeCount(V8PressureSource::Enum source) {
  change_count_[ToSourceIndex(source)] = 0;
}

void ChangeRateMonitor::IncreaseChangeCount(V8PressureSource::Enum source) {
  change_count_[ToSourceIndex(source)]++;
}

bool ChangeRateMonitor::ChangeCountExceedsLimit(
    V8PressureSource::Enum source) const {
  return change_count_[ToSourceIndex(source)] >= change_count_threshold_;
}

}  // namespace blink
