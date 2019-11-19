// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class WriteBarrierPerfTest : public TestSupportingGC {};

namespace {

class PerfDummyObject : public GarbageCollected<PerfDummyObject> {
 public:
  PerfDummyObject() = default;
  virtual void Trace(Visitor*) {}
};

base::TimeDelta TimedRun(base::RepeatingCallback<void()> callback) {
  const base::TimeTicks start = base::TimeTicks::Now();
  callback.Run();
  return base::TimeTicks::Now() - start;
}

}  // namespace

TEST_F(WriteBarrierPerfTest, MemberWritePerformance) {
  // Setup.
  constexpr wtf_size_t kNumElements = 100000;
  Persistent<HeapVector<Member<PerfDummyObject>>> holder(
      MakeGarbageCollected<HeapVector<Member<PerfDummyObject>>>());
  for (wtf_size_t i = 0; i < kNumElements; ++i) {
    holder->push_back(MakeGarbageCollected<PerfDummyObject>());
  }
  PreciselyCollectGarbage();
  // Benchmark.
  base::RepeatingCallback<void()> benchmark = base::BindRepeating(
      [](const Persistent<HeapVector<Member<PerfDummyObject>>>& holder) {
        for (wtf_size_t i = 0; i < kNumElements / 2; ++i) {
          (*holder)[i].Swap((*holder)[kNumElements / 2 + i]);
        }
      },
      holder);

  // During GC.
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.Start();
  base::TimeDelta during_gc_duration = TimedRun(benchmark);
  driver.FinishSteps();
  PreciselyCollectGarbage();

  // Outside GC.
  base::TimeDelta outside_gc_duration = TimedRun(benchmark);

  // Cleanup.
  holder.Clear();
  PreciselyCollectGarbage();

  // Reporting.
  perf_test::PrintResult(
      "WriteBarrierPerfTest", " writes during GC", "",
      static_cast<double>(kNumElements) / during_gc_duration.InMillisecondsF(),
      "writes/ms", true);
  perf_test::PrintResult(
      "WriteBarrierPerfTest", " writes outside GC", "",
      static_cast<double>(kNumElements) / outside_gc_duration.InMillisecondsF(),
      "writes/ms", true);
  perf_test::PrintResult("WriteBarrierPerfTest", " relative speed difference",
                         "",
                         during_gc_duration.InMillisecondsF() /
                             outside_gc_duration.InMillisecondsF(),
                         "times", true);
}

}  // namespace blink
