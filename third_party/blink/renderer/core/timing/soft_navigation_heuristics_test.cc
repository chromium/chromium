// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

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

  bool IsDocumentTrackingSoftNavigations() {
    return LocalDOMWindow::From(GetScriptStateForTest())
        ->document()
        ->IsTrackingSoftNavigationHeuristics();
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

// TODO(crbug.com/1503284): This test validates that the renderer does not crash
// when presented with an unset timestamp. Figure out whether it is possible to
// void ever calling InteractionCallbackCalled in that situation instead.
TEST_F(SoftNavigationHeuristicsTest,
       EarlyReturnOnInvalidPendingInteractionTimestamp) {
  auto* test_heuristics = CreateSoftNavigationHeuristicsForTest();
  // A non-new interaction will try to use the pending timestamp, which will
  // never have been set in this case.
  SoftNavigationHeuristics::EventScope event_scope(
      test_heuristics->CreateEventScope(
          SoftNavigationHeuristics::EventScope::Type::kKeyboard,
          /*is_new_interaction=*/false, GetScriptStateForTest()));
  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetScriptStateForTest()->GetIsolate());
  ASSERT_TRUE(tracker);
  {
    // Simulate a top-level event dispatch with no context to propagate.
    std::optional<TaskScope> task_scope =
        tracker->MaybeCreateTaskScopeForCallback(GetScriptStateForTest(),
                                                 nullptr);
  }
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

  auto* script_state = GetScriptStateForTest();
  auto* tracker =
      scheduler::TaskAttributionTracker::From(script_state->GetIsolate());
  ASSERT_TRUE(tracker);

  Persistent<scheduler::TaskAttributionInfo> root_task = nullptr;
  // Simulate a click.
  {
    EXPECT_FALSE(IsDocumentTrackingSoftNavigations());
    SoftNavigationHeuristics::EventScope event_scope(
        heuristics->CreateEventScope(
            SoftNavigationHeuristics::EventScope::Type::kClick,
            /*is_new_interaction=*/true, script_state));

    // Simulate a top-level event dispatch with no context to propagate.
    std::optional<TaskScope> task_scope =
        tracker->MaybeCreateTaskScopeForCallback(script_state, nullptr);
    // This won't create a new task scope because there's already one on the
    // stack to propagate the soft navigation context, but it should notify
    // `heuristics`.
    EXPECT_FALSE(task_scope);
    root_task = tracker->RunningTask();
  }
  EXPECT_TRUE(root_task);
  EXPECT_TRUE(IsDocumentTrackingSoftNavigations());

  // Simulate a descendant task.
  Persistent<scheduler::TaskAttributionInfo> descendant_task = nullptr;
  {
    TaskScope task_scope = tracker->CreateTaskScope(script_state, root_task,
                                                    TaskScopeType::kCallback);
    descendant_task = tracker->RunningTask();
  }
  EXPECT_TRUE(descendant_task);

  EXPECT_TRUE(IsDocumentTrackingSoftNavigations());
  EXPECT_EQ(root_task.Get(), descendant_task.Get());

  root_task = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  // The heuristics still should not have been reset since there is a live
  // root task, which is being held onto by its descendant task.
  EXPECT_TRUE(IsDocumentTrackingSoftNavigations());

  // Finally, this should allow the click task to be GCed, which should cause
  // the heuristics to be reset.
  descendant_task = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(IsDocumentTrackingSoftNavigations());
}

TEST_F(SoftNavigationHeuristicsTest, NestedEventScopesAreMerged) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  auto* script_state = GetScriptStateForTest();

  SoftNavigationHeuristics::EventScope outer_event_scope(
      heuristics->CreateEventScope(
          SoftNavigationHeuristics::EventScope::Type::kClick,
          /*is_new_interaction=*/true, script_state));
  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetScriptStateForTest()->GetIsolate());
  ASSERT_TRUE(tracker);

  SoftNavigationContext* context1 = nullptr;
  {
    std::optional<TaskScope> task_scope =
        tracker->MaybeCreateTaskScopeForCallback(script_state, nullptr);
    context1 = tracker->RunningTask()->GetSoftNavigationContext();
  }
  EXPECT_TRUE(context1);

  SoftNavigationHeuristics::EventScope inner_event_scope(
      heuristics->CreateEventScope(
          SoftNavigationHeuristics::EventScope::Type::kNavigate,
          /*is_new_interaction=*/true, script_state));

  SoftNavigationContext* context2 = nullptr;
  {
    std::optional<TaskScope> task_scope =
        tracker->MaybeCreateTaskScopeForCallback(script_state, nullptr);
    context2 = tracker->RunningTask()->GetSoftNavigationContext();
  }
  EXPECT_TRUE(context2);

  EXPECT_EQ(context1, context2);
}

TEST_F(SoftNavigationHeuristicsTest, EventAfterSoftNavDetection) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  auto* script_state = GetScriptStateForTest();

  SoftNavigationHeuristics::EventScope outer_event_scope(
      heuristics->CreateEventScope(
          SoftNavigationHeuristics::EventScope::Type::kClick,
          /*is_new_interaction=*/true, script_state));
  auto* tracker =
      scheduler::TaskAttributionTracker::From(script_state->GetIsolate());
  ASSERT_TRUE(tracker);

  {
    std::optional<TaskScope> task_scope =
        tracker->MaybeCreateTaskScopeForCallback(script_state, nullptr);
    heuristics->ModifiedDOM();
  }

  // Simulate default action link navigation after the click event.
  heuristics->SameDocumentNavigationStarted();
  heuristics->SameDocumentNavigationCommitted("foo");
  {
    SoftNavigationHeuristics::EventScope inner_event_scope(
        heuristics->CreateEventScope(
            SoftNavigationHeuristics::EventScope::Type::kNavigate,
            /*is_new_interaction=*/true, script_state));
  }

  // crbug.com/335945346: Some events, e.g. blur, can fire after all of the soft
  // navigation criteria have been met and all of the input event handlers have
  // run, while there's still an EventScope on the stack. Since
  // SoftNavigationHeuristics::OnCreateTaskScope relies on the active context
  // being non-null, emitting a soft navigation entry and resetting the
  // heuristic prematurely would clear the context while it still may be needed.
  // An event firing here, after the criteria have been met, should not cause a
  // crash.
  {
    std::optional<TaskScope> task_scope =
        tracker->MaybeCreateTaskScopeForCallback(script_state, nullptr);
  }
}

TEST_F(SoftNavigationHeuristicsTest,
       HeuristicNotResetDuringGCWithActiveContext) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  auto* script_state = GetScriptStateForTest();
  auto* tracker =
      scheduler::TaskAttributionTracker::From(script_state->GetIsolate());
  ASSERT_TRUE(tracker);

  {
    SoftNavigationHeuristics::EventScope event_scope(
        heuristics->CreateEventScope(
            SoftNavigationHeuristics::EventScope::Type::kClick,
            /*is_new_interaction=*/true, script_state));
    {
      std::optional<TaskScope> task_scope =
          tracker->MaybeCreateTaskScopeForCallback(script_state, nullptr);
    }
  }
  // At this point there is a single `SoftNavigationContext` being tracked, but
  // it wasn't propagated anywhere, so it is eligible for GC.
  EXPECT_TRUE(IsDocumentTrackingSoftNavigations());

  SoftNavigationHeuristics::EventScope event_scope(heuristics->CreateEventScope(
      SoftNavigationHeuristics::EventScope::Type::kClick,
      /*is_new_interaction=*/true, script_state));

  // If GC occurs here, e.g. during a blink allocation, the heuristic should not
  // be reset, otherwise the `SoftNavigationContext` created above will be
  // cleared.
  ThreadState::Current()->CollectAllGarbageForTesting(
      cppgc::EmbedderStackState::kMayContainHeapPointers);

  std::optional<TaskScope> task_scope =
      tracker->MaybeCreateTaskScopeForCallback(script_state, nullptr);
  EXPECT_TRUE(IsDocumentTrackingSoftNavigations());
}

}  // namespace blink
