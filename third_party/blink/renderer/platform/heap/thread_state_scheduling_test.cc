// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

void RunLoop() {
  base::RunLoop rl;
  // Push quit task.
  ThreadScheduler::Current()->V8TaskRunner()->PostNonNestableTask(
      FROM_HERE, WTF::Bind(rl.QuitWhenIdleClosure()));
  rl.Run();
}

}  // namespace

class ThreadStateSchedulingTest : public TestSupportingGC {
 public:
  void SetUp() override {
    state_ = ThreadState::Current();
    ClearOutOldGarbage();
    initial_gc_age_ = state_->GcAge();
  }

  void TearDown() override {
    PreciselyCollectGarbage();
    EXPECT_EQ(ThreadState::kNoGCScheduled, state_->GetGCState());
    EXPECT_FALSE(state_->IsMarkingInProgress());
    EXPECT_FALSE(state_->IsSweepingInProgress());
  }

  BlinkGC::GCReason LastReason() const {
    return state_->reason_for_scheduled_gc_;
  }

  void StartLazySweepingForPreciseGC() {
    EXPECT_EQ(ThreadState::kNoGCScheduled, state_->GetGCState());
    state_->SchedulePreciseGC();
    EXPECT_EQ(ThreadState::kPreciseGCScheduled, state_->GetGCState());
    RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);
    EXPECT_TRUE(state_->IsSweepingInProgress());
    EXPECT_EQ(ThreadState::kNoGCScheduled, state_->GetGCState());
  }

  void RunScheduledGC(BlinkGC::StackState stack_state) {
    state_->RunScheduledGC(stack_state);
  }

  // Counter that is incremented when sweep finishes.
  int GCCount() { return state_->GcAge() - initial_gc_age_; }

  ThreadState* state() { return state_; }

 private:
  ThreadState* state_;
  int initial_gc_age_;
};

TEST_F(ThreadStateSchedulingTest, RunIncrementalGCForTesting) {
  ThreadStateSchedulingTest* test = this;

  EXPECT_EQ(ThreadState::kNoGCScheduled, test->state()->GetGCState());
  test->state()->StartIncrementalMarking(
      BlinkGC::GCReason::kForcedGCForTesting);
  EXPECT_EQ(ThreadState::kIncrementalMarkingStepScheduled,
            test->state()->GetGCState());

  RunLoop();
  EXPECT_EQ(ThreadState::kNoGCScheduled, test->state()->GetGCState());
}

TEST_F(ThreadStateSchedulingTest, SchedulePreciseGCWhileLazySweeping) {
  ThreadStateSchedulingTest* test = this;

  test->StartLazySweepingForPreciseGC();

  test->state()->SchedulePreciseGC();

  // Scheduling a precise GC should finish lazy sweeping.
  EXPECT_FALSE(test->state()->IsSweepingInProgress());
  EXPECT_EQ(ThreadState::kPreciseGCScheduled, test->state()->GetGCState());
}

TEST_F(ThreadStateSchedulingTest, SchedulePreciseGCWhileIncrementalMarking) {
  ThreadStateSchedulingTest* test = this;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kBlinkHeapIncrementalMarking);
  test->state()->StartIncrementalMarking(
      BlinkGC::GCReason::kForcedGCForTesting);
  test->state()->SchedulePreciseGC();
  // Scheduling a precise GC should cancel incremental marking tasks.
  EXPECT_EQ(ThreadState::kPreciseGCScheduled, test->state()->GetGCState());

  EXPECT_EQ(0, test->GCCount());
  test->RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);
  EXPECT_TRUE(test->state()->IsSweepingInProgress());
  EXPECT_EQ(ThreadState::kNoGCScheduled, test->state()->GetGCState());

  // Running the precise GC should simply finish the incremental marking idle GC
  // (not run another GC).
  EXPECT_EQ(0, test->GCCount());
  test->state()->CompleteSweep();
  EXPECT_EQ(1, test->GCCount());

  // Check that incremental GC hasn't been run.
  RunLoop();
  EXPECT_EQ(1, test->GCCount());
}

}  // namespace blink
