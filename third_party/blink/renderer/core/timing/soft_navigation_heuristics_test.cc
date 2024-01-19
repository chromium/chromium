// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

using TaskScope = scheduler::TaskAttributionTracker::TaskScope;
using TaskScopeType = scheduler::TaskAttributionTracker::TaskScopeType;

class SoftNavigationHeuristicsTest : public testing::Test {
 protected:
  void SetUp() override {
    page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }

  SoftNavigationHeuristics* CreateSoftNavigationHeuristicsForTest() {
    ScriptState* script_state = GetScriptStateForTest();

    LocalDOMWindow* window = LocalDOMWindow::From(script_state);

    SoftNavigationHeuristics* heuristics =
        SoftNavigationHeuristics::From(*window);

    return heuristics;
  }

  ScriptState* GetScriptStateForTest() {
    return ToScriptStateForMainWorld(page_holder_->GetDocument().GetFrame());
  }

 private:
  std::unique_ptr<DummyPageHolder> page_holder_;
};

// TODO(crbug.com/1503284): This test validates that the renderer does not crash
// when presented with an unset timestamp. Figure out whether it is possible to
// void ever calling InteractionCallbackCalled in that situation instead.
TEST_F(SoftNavigationHeuristicsTest,
       EarlyReturnOnInvalidPendingInteractionTimestamp) {
  auto* test_heuristics = CreateSoftNavigationHeuristicsForTest();
  // NextId() required so that the first task ID is non-zero (because we hash on
  // key).
  Persistent<scheduler::TaskAttributionInfo> task =
      MakeGarbageCollected<scheduler::TaskAttributionInfo>(
          scheduler::TaskAttributionId().NextId(), nullptr);

  test_heuristics->InteractionCallbackCalled(
      *task, SoftNavigationHeuristics::EventScopeType::kClick, true);
  ASSERT_TRUE(test_heuristics->GetInitialInteractionEncounteredForTest());
}

TEST_F(SoftNavigationHeuristicsTest, UmaHistogramRecording) {
  base::HistogramTester histogram_tester;

  // Test case where user interaction timestamp and reference monotonic
  // timestamp are both null.
  base::TimeTicks user_interaction_ts;
  base::TimeTicks reference_ts;
  internal::
      RecordUmaForPageLoadInternalSoftNavigationFromReferenceInvalidTiming(
          user_interaction_ts, reference_ts);

  histogram_tester.ExpectBucketCount(
      internal::kPageLoadInternalSoftNavigationFromReferenceInvalidTiming,
      internal::SoftNavigationFromReferenceInvalidTimingReasons::
          kUserInteractionTsAndReferenceTsBothNull,
      1);

  // Test case where both user interaction timestamp is not null and reference
  // monotonic timestamp is null.
  user_interaction_ts = base::TimeTicks() + base::Milliseconds(1);

  internal::
      RecordUmaForPageLoadInternalSoftNavigationFromReferenceInvalidTiming(
          user_interaction_ts, reference_ts);

  histogram_tester.ExpectBucketCount(
      internal::kPageLoadInternalSoftNavigationFromReferenceInvalidTiming,
      internal::SoftNavigationFromReferenceInvalidTimingReasons::
          kNullReferenceTsAndNotNullUserInteractionTs,
      1);

  // Test case where user interaction timestamp is null and reference
  // monotonic timestamp is not null.
  user_interaction_ts = base::TimeTicks();
  reference_ts = base::TimeTicks() + base::Milliseconds(1);

  internal::
      RecordUmaForPageLoadInternalSoftNavigationFromReferenceInvalidTiming(
          user_interaction_ts, reference_ts);

  histogram_tester.ExpectBucketCount(
      internal::kPageLoadInternalSoftNavigationFromReferenceInvalidTiming,
      internal::SoftNavigationFromReferenceInvalidTimingReasons::
          kNullUserInteractionTsAndNotNullReferenceTs,
      1);

  // Test case where user interaction timestamp and reference monotonic
  // timestamp are both not null.
  user_interaction_ts = base::TimeTicks() + base::Milliseconds(1);
  reference_ts = base::TimeTicks() + base::Milliseconds(2);

  internal::
      RecordUmaForPageLoadInternalSoftNavigationFromReferenceInvalidTiming(
          user_interaction_ts, reference_ts);

  histogram_tester.ExpectBucketCount(
      internal::kPageLoadInternalSoftNavigationFromReferenceInvalidTiming,
      internal::SoftNavigationFromReferenceInvalidTimingReasons::
          kUserInteractionTsAndReferenceTsBothNotNull,
      1);
}

TEST_F(SoftNavigationHeuristicsTest, ResetHeuristicOnSetBecameEmpty) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  ASSERT_TRUE(heuristics);
  auto* tracker = ThreadScheduler::Current()->GetTaskAttributionTracker();
  ASSERT_TRUE(tracker);

  auto* script_state = GetScriptStateForTest();
  Persistent<scheduler::TaskAttributionInfo> root_task = nullptr;
  // Simulate a click.
  {
    SoftNavigationEventScope event_scope(
        heuristics, SoftNavigationHeuristics::EventScopeType::kClick,
        /*is_new_interaction=*/true);
    std::unique_ptr<TaskScope> task_scope = tracker->CreateTaskScope(
        script_state, /*parent_task=*/nullptr, TaskScopeType::kCallback);
    root_task = tracker->RunningTask(script_state);
  }
  EXPECT_TRUE(root_task);
  EXPECT_GT(heuristics->GetLastInteractionTaskIdForTest(), 0u);

  // Simulate a descendant task.
  Persistent<scheduler::TaskAttributionInfo> descendant_task = nullptr;
  {
    std::unique_ptr<TaskScope> task_scope = tracker->CreateTaskScope(
        script_state, root_task, TaskScopeType::kCallback);
    descendant_task = tracker->RunningTask(script_state);
  }
  EXPECT_TRUE(descendant_task);

  root_task = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  // The heuristics still should not have been reset since there is a live
  // root task, which is being held onto by its descendant task.
  EXPECT_GT(heuristics->GetLastInteractionTaskIdForTest(), 0u);

  // Finally, this should allow the click task to be GCed, which should cause
  // the heuristics to be reset.
  descendant_task = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(heuristics->GetLastInteractionTaskIdForTest(), 0u);
}

}  // namespace blink
