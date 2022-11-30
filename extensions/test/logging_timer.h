// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_LOGGING_TIMER_H_
#define EXTENSIONS_TEST_LOGGING_TIMER_H_

#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"

namespace base {
class TickClock;
}

namespace extensions {

// A class to keep track of how long certain actions take, and print the
// results.
// !!IMPORTANT!!
// WHAT IS THIS NOT:
// - A substitute for UMA. This is a very simple implementation, designed to be
//   used locally, and is not an alternative for real data from real users.
// - A fool-proof measurement. Again, very simple implementation. This only uses
//   wall-clock time, meaning that it is highly susceptible to any number of
//   local factors. And, given simple enough methods, the cost of measurement
//   may have significant impacts on the results.
// - A tool for production code. Use UMA instead. This is deliberately in
//   a test-only directory.
// WHAT THIS IS:
// A tool for quick, local testing of various methods in order to get a good
// idea of the time taken in different operations. Say you have a method,
// DoFoo(), that calls different submethods (DoSubFoo1(), DoSubFoo2(), ...
// DoSubFooN()). You know from a reliable source (like UMA) that DoFoo() is
// taking a long time, and want to optimize it, but you don't know where to
// start. Each of the DoSubFooN() methods has multiple other method calls, and
// it's hard to reason about which might be most expensive. You can use this
// class to log how long is spent in each of the different DoSubFooN() calls.
// Note again that this *will not be foolproof*, but given a significant enough
// difference, should be representative. Now, you can dive into optimizing the
// expensive DoSubFooN() methods, and verify the results using a reliable
// measurement (like UMA).
// Note: Have you looked at existing UMA, Tracing, and Profiling? We have a lot
// of tools that may have the information you're looking for. But if not,
// experiment away!
//
// Usage:
// Instantiate a LoggingTimer in a given method with a unique key, e.g.
// void DoFoo() {
//   LoggingTimer timer("DoFoo");
// }
// Each time the method is executed, the timer will record the amount of time
// it takes (from timer instantiation to destruction), and keep track of the
// number of calls, total time, and average time spent in the method. Timers
// with the same key will be aggregated (i.e., we track all "DoFoo" timers in
// a single record).
// Note: The keys are const char*, and are compared by pointer equality for
// efficiency. If you want to aggregate timers from separate call sites,
// extract the key to a variable.
class LoggingTimer {
 public:
  explicit LoggingTimer(const char* key);

  LoggingTimer(const LoggingTimer&) = delete;
  LoggingTimer& operator=(const LoggingTimer&) = delete;

  ~LoggingTimer();

  // Returns the tracked time for the given |key|.
  static base::TimeDelta GetTrackedTime(const char* key);

  // Prints the result for all LoggingTimers.
  static void Print();

  // Allows for setting a test clock. Otherwise, defaults to using
  // base::TimeTicks::Now().
  static void set_clock_for_testing(const base::TickClock* clock);

 private:
  // When the timer started. We don't use base::ElapsedTimer so that we can
  // curry in a test clock for unittests.
  base::TimeTicks start_;

  // The key for this timer.
  const char* const key_;
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_LOGGING_TIMER_H_
