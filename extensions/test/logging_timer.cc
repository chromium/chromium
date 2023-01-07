// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/logging_timer.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/time/tick_clock.h"

namespace extensions {

namespace {

const base::TickClock* g_clock_for_testing = nullptr;

// A global record of all tracked times.
class TimeTracker {
 public:
  TimeTracker() = default;

  TimeTracker(const TimeTracker&) = delete;
  TimeTracker& operator=(const TimeTracker&) = delete;

  ~TimeTracker() = default;

  void IncrementTime(const char* key, base::TimeDelta elapsed) {
    auto& data = tracked_times_[key];
    data.total_time += elapsed;
    data.num_samples++;
  }

  base::TimeDelta GetTrackedTime(const char* key) {
    auto iter = tracked_times_.find(key);
    return iter != tracked_times_.end() ? iter->second.total_time
                                        : base::TimeDelta();
  }

  void Print() {
    for (const auto& key_value : tracked_times_) {
      LOG(WARNING) << "\n"
                   << key_value.first << ":"
                   << "\n    total: " << key_value.second.total_time
                   << "\n    average: " << key_value.second.average_time();
    }
  }

 private:
  struct Data {
    base::TimeDelta total_time;
    size_t num_samples = 0u;

    base::TimeDelta average_time() const {
      return num_samples == 0u ? base::TimeDelta() : total_time / num_samples;
    }
  };

  // NOTE(devlin): If we find that these map lookups are too expensive, we
  // could instead use a c-style array similar to RuntimeCallStats.
  std::map<const char*, Data> tracked_times_;
};

base::TimeTicks GetNow() {
  return g_clock_for_testing ? g_clock_for_testing->NowTicks()
                             : base::TimeTicks::Now();
}

TimeTracker& GetTimeTracker() {
  static TimeTracker time_tracker;
  return time_tracker;
}

}  // namespace

LoggingTimer::LoggingTimer(const char* key) : start_(GetNow()), key_(key) {}

LoggingTimer::~LoggingTimer() {
  GetTimeTracker().IncrementTime(key_, GetNow() - start_);
}

// static
base::TimeDelta LoggingTimer::GetTrackedTime(const char* key) {
  return GetTimeTracker().GetTrackedTime(key);
}

// static
void LoggingTimer::Print() {
  GetTimeTracker().Print();
}

// static
void LoggingTimer::set_clock_for_testing(const base::TickClock* clock) {
  g_clock_for_testing = clock;
}

}  // namespace extensions
