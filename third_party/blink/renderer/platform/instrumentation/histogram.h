// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_HISTOGRAM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_HISTOGRAM_H_

#include <stdint.h>
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace base {
class HistogramBase;
}

namespace blink {

class PLATFORM_EXPORT CustomCountHistogram {
  USING_FAST_MALLOC(CustomCountHistogram);

 public:
  // Min values should be >=1 as emitted 0s still go into the underflow bucket.
  CustomCountHistogram(const char* name,
                       base::HistogramBase::Sample min,
                       base::HistogramBase::Sample max,
                       int32_t bucket_count);
  void Count(base::HistogramBase::Sample);
  void CountMany(base::HistogramBase::Sample, int count);
  void CountMicroseconds(base::TimeDelta);
  void CountMilliseconds(base::TimeDelta);

 protected:
  explicit CustomCountHistogram(base::HistogramBase*);

  base::HistogramBase* histogram_;
};

class PLATFORM_EXPORT BooleanHistogram : public CustomCountHistogram {
 public:
  BooleanHistogram(const char* name);
};

class PLATFORM_EXPORT EnumerationHistogram : public CustomCountHistogram {
 public:
  // |boundaryValue| must be strictly greater than samples passed to |count|.
  EnumerationHistogram(const char* name,
                       base::HistogramBase::Sample boundary_value);
};

class PLATFORM_EXPORT SparseHistogram {
  USING_FAST_MALLOC(SparseHistogram);

 public:
  explicit SparseHistogram(const char* name);

  void Sample(base::HistogramBase::Sample);

 private:
  base::HistogramBase* histogram_;
};

class PLATFORM_EXPORT LinearHistogram : public CustomCountHistogram {
 public:
  explicit LinearHistogram(const char* name,
                           base::HistogramBase::Sample min,
                           base::HistogramBase::Sample max,
                           int32_t bucket_count);
};

template <typename Derived>
class ScopedUsHistogramTimerBase {
  USING_FAST_MALLOC(ScopedUsHistogramTimerBase);

 public:
  explicit ScopedUsHistogramTimerBase(CustomCountHistogram& counter)
      : ScopedUsHistogramTimerBase(counter,
                                   base::DefaultTickClock::GetInstance()) {}

  ScopedUsHistogramTimerBase(CustomCountHistogram& counter,
                             const base::TickClock* clock)
      : clock_(*clock), start_time_(clock_.NowTicks()), counter_(counter) {}

  ~ScopedUsHistogramTimerBase() {
    if (Derived::ShouldRecord())
      counter_.CountMicroseconds(clock_.NowTicks() - start_time_);
  }

 private:
  const base::TickClock& clock_;
  base::TimeTicks start_time_;
  CustomCountHistogram& counter_;
};

class ScopedUsHistogramTimer
    : public ScopedUsHistogramTimerBase<ScopedUsHistogramTimer> {
 public:
  using ScopedUsHistogramTimerBase::ScopedUsHistogramTimerBase;
  static bool ShouldRecord() { return true; }
};

class ScopedHighResUsHistogramTimer
    : public ScopedUsHistogramTimerBase<ScopedHighResUsHistogramTimer> {
 public:
  using ScopedUsHistogramTimerBase::ScopedUsHistogramTimerBase;
  static bool ShouldRecord() { return base::TimeTicks::IsHighResolution(); }
};

#define SCOPED_BLINK_UMA_HISTOGRAM_TIMER_IMPL(name, allow_cross_thread)  \
  DEFINE_STATIC_LOCAL_IMPL(CustomCountHistogram, scoped_us_counter,      \
                           (name, 0, 10000000, 50), allow_cross_thread); \
  ScopedUsHistogramTimer timer(scoped_us_counter);

#define SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES_IMPL(name,               \
                                                      allow_cross_thread) \
  DEFINE_STATIC_LOCAL_IMPL(CustomCountHistogram, scoped_us_counter,       \
                           (name, 0, 10000000, 50), allow_cross_thread);  \
  ScopedHighResUsHistogramTimer timer(scoped_us_counter);

// Use code like this to record time, in microseconds, to execute a block of
// code:
//
// {
//     SCOPED_BLINK_UMA_HISTOGRAM_TIMER(myUmaStatName)
//     RunMyCode();
// }
// This macro records all times between 0us and 10 seconds.
// Do not change this macro without renaming all metrics that use it!
#define SCOPED_BLINK_UMA_HISTOGRAM_TIMER(name) \
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER_IMPL(name, false)

// Only record samples when we have a high resolution timer
#define SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES(name) \
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER_HIGHRES_IMPL(name, false)

// Thread-safe variant of SCOPED_BLINK_UMA_HISTOGRAM_TIMER.
// Use if the histogram can be accessed by multiple threads.
#define SCOPED_BLINK_UMA_HISTOGRAM_TIMER_THREAD_SAFE(name) \
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER_IMPL(name, true)

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_INSTRUMENTATION_HISTOGRAM_H_
