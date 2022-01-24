// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"

namespace blink {

namespace {

class AllocationPerfTest : public TestSupportingGC {};

class TinyObject final : public GarbageCollected<TinyObject> {
 public:
  void Trace(Visitor*) const {}
};

class LargeObject final : public GarbageCollected<LargeObject> {
  static constexpr size_t kLargeObjectSizeThreshold =
      cppgc::internal::api_constants::kLargeObjectSizeThreshold;
 public:
  void Trace(Visitor*) const {}
  char padding[kLargeObjectSizeThreshold + 1];
};

template <typename Callback>
base::TimeDelta TimedRun(Callback callback) {
  const auto start = base::TimeTicks::Now();
  callback();
  return base::TimeTicks::Now() - start;
}

constexpr char kMetricPrefix[] = "Allocation.";
constexpr char kMetricThroughput[] = "throughput";

perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
  perf_test::PerfResultReporter reporter(kMetricPrefix, story_name);
  reporter.RegisterImportantMetric(kMetricThroughput, "Mbytes/s");
  return reporter;
}

}  // namespace

template <>
struct ThreadingTrait<TinyObject> {
  STATIC_ONLY(ThreadingTrait);
  static const ThreadAffinity kAffinity = ThreadAffinity::kMainThreadOnly;
};

template <>
struct ThreadingTrait<LargeObject> {
  STATIC_ONLY(ThreadingTrait);
  static const ThreadAffinity kAffinity = ThreadAffinity::kMainThreadOnly;
};

TEST_F(AllocationPerfTest, Allocate10MTiny) {
  constexpr size_t kTargetMemoryBytes = 10 * 1024 * 1024;
  constexpr size_t kObjectBytes = sizeof(TinyObject);
  constexpr size_t kNumObjects = kTargetMemoryBytes / kObjectBytes;

  ThreadState* thread_state = ThreadState::Current();
  ThreadState::GCForbiddenScope no_gc(thread_state);

  auto delta = TimedRun([]() {
    for (size_t i = 0; i < kNumObjects; ++i) {
      MakeGarbageCollected<TinyObject>();
    }
  });
  auto reporter = SetUpReporter("Allocate10MTiny");
  reporter.AddResult(kMetricThroughput,
                     static_cast<double>(kNumObjects * kObjectBytes) /
                         (1024 * 1024) / delta.InSecondsF());
}

TEST_F(AllocationPerfTest, Allocate10MLarge) {
  constexpr size_t kTargetMemoryBytes = 10 * 1024 * 1024;
  constexpr size_t kObjectBytes = sizeof(LargeObject);
  constexpr size_t kNumObjects = kTargetMemoryBytes / kObjectBytes + 1;

  ThreadState* thread_state = ThreadState::Current();
  ThreadState::GCForbiddenScope no_gc(thread_state);

  auto delta = TimedRun([]() {
    for (size_t i = 0; i < kNumObjects; ++i) {
      MakeGarbageCollected<LargeObject>();
    }
  });
  auto reporter = SetUpReporter("Allocate10MLarge");
  reporter.AddResult(kMetricThroughput,
                     static_cast<double>(kNumObjects * kObjectBytes) /
                         (1024 * 1024) / delta.InSecondsF());
}

}  // namespace blink
