// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/logging_timer.h"

#include <array>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/time/tick_clock.h"

namespace extensions {

namespace {

const base::TickClock* g_clock_for_testing = nullptr;

// A global record of all tracked times. It is trivially destructible so it
// can be a global without an exit-time destructor.
class TimeTracker {
 public:
  constexpr TimeTracker() = default;

  TimeTracker(const TimeTracker&) = delete;
  TimeTracker& operator=(const TimeTracker&) = delete;

  void IncrementTime(const char* key, base::TimeDelta elapsed) {
    Data* data = Find(key);
    if (!data) {
      CHECK_LT(num_keys_, kMaxKeys);
      data = &tracked_times_[num_keys_++];
      data->key = key;
    }
    data->total_time += elapsed;
    data->num_samples++;
  }

  base::TimeDelta GetTrackedTime(const char* key) {
    Data* data = Find(key);
    return data ? data->total_time : base::TimeDelta();
  }

  void Print() {
    for (const Data& data : Tracked()) {
      LOG(WARNING) << "\n"
                   << data.key << ":"
                   << "\n    total: " << data.total_time
                   << "\n    average: " << data.average_time();
    }
  }

 private:
  struct Data {
    const char* key = nullptr;
    base::TimeDelta total_time;
    size_t num_samples = 0u;

    base::TimeDelta average_time() const {
      return num_samples == 0u ? base::TimeDelta() : total_time / num_samples;
    }
  };

  // Far more keys than any test uses.
  static constexpr size_t kMaxKeys = 32u;

  base::span<Data> Tracked() {
    return base::span(tracked_times_).first(num_keys_);
  }

  // Keys are compared by address, as the map that used to hold them did.
  Data* Find(const char* key) {
    for (Data& data : Tracked()) {
      if (data.key == key) {
        return &data;
      }
    }
    return nullptr;
  }

  std::array<Data, kMaxKeys> tracked_times_ = {};
  size_t num_keys_ = 0u;
};

TimeTracker g_time_tracker;

base::TimeTicks GetNow() {
  return g_clock_for_testing ? g_clock_for_testing->NowTicks()
                             : base::TimeTicks::Now();
}

TimeTracker& GetTimeTracker() {
  return g_time_tracker;
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
