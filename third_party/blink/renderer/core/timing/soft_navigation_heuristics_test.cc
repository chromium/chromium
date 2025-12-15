// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

#include <memory>

#include "base/notreached.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics_test_util.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

using TaskScope = scheduler::TaskAttributionTracker::TaskScope;

class SoftNavigationHeuristicsTest : public testing::Test {
 protected:
  void SetUp() override {
    page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  }

  SoftNavigationHeuristics* CreateSoftNavigationHeuristicsForTest() {
    return page_holder_->GetDocument()
        .domWindow()
        ->GetSoftNavigationHeuristics();
  }

  Document& GetDocument() { return page_holder_->GetDocument(); }

  Node* CreateNodeForTest() {
    return GetDocument().CreateRawElement(html_names::kDivTag);
  }

  v8::Isolate* GetIsolate() {
    return GetDocument().GetExecutionContext()->GetIsolate();
  }

  ScriptState* GetScriptStateForTest() {
    return ToScriptStateForMainWorld(GetDocument().GetFrame());
  }

  Event* CreateEvent(SoftNavigationHeuristics::EventScope::Type type) {
    // Event scopes are only created for keyboard events that target <body>. Use
    // this for all types since this has no effect for other types.
    return CreateEventForEventScopeType(type, GetScriptStateForTest(),
                                        GetDocument().body());
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
  auto* event =
      CreateEvent(SoftNavigationHeuristics::EventScope::Type::kKeypress);
  std::optional<SoftNavigationHeuristics::EventScope> event_scope(
      test_heuristics->MaybeCreateEventScopeForInputEvent(*event));
}

TEST_F(SoftNavigationHeuristicsTest, ResetHeuristicOnSetBecameEmpty) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  ASSERT_TRUE(heuristics);

  auto* tracker = scheduler::TaskAttributionTracker::From(GetIsolate());
  ASSERT_TRUE(tracker);

  Persistent<scheduler::TaskAttributionInfo> root_task_state = nullptr;
  // Simulate a click.
  {
    EXPECT_FALSE(heuristics->IsTrackingSoftNavigationsForTest());
    auto* event =
        CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
    std::optional<SoftNavigationHeuristics::EventScope> event_scope(
        heuristics->MaybeCreateEventScopeForInputEvent(*event));
    root_task_state = tracker->CurrentTaskState();
  }
  EXPECT_TRUE(root_task_state);
  EXPECT_TRUE(heuristics->IsTrackingSoftNavigationsForTest());

  // Simulate a descendant task.
  Persistent<scheduler::TaskAttributionInfo> descendant_task_state = nullptr;
  {
    std::optional<TaskScope> task_scope =
        tracker->SetCurrentTaskStateIfTopLevel(root_task_state,
                                               TaskScopeType::kCallback);
    descendant_task_state = tracker->CurrentTaskState();
  }
  EXPECT_TRUE(descendant_task_state);

  EXPECT_TRUE(heuristics->IsTrackingSoftNavigationsForTest());
  EXPECT_EQ(root_task_state.Get(), descendant_task_state.Get());

  root_task_state = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  // The heuristics still should not have been reset since there is a live
  // root task, which is being held onto by its descendant task.
  EXPECT_TRUE(heuristics->IsTrackingSoftNavigationsForTest());

  // Finally, this should allow the click task to be GCed, which should cause
  // the heuristics to be reset.
  descendant_task_state = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(heuristics->IsTrackingSoftNavigationsForTest());
}

TEST_F(SoftNavigationHeuristicsTest, NestedEventScopesAreMerged) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  auto* event = CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
  std::optional<SoftNavigationHeuristics::EventScope> outer_event_scope(
      heuristics->MaybeCreateEventScopeForInputEvent(*event));
  auto* tracker = scheduler::TaskAttributionTracker::From(GetIsolate());
  ASSERT_TRUE(tracker);

  SoftNavigationContext* context1 =
      tracker->CurrentTaskState()->GetSoftNavigationContext();
  EXPECT_TRUE(context1);

  auto* inner_event =
      CreateEvent(SoftNavigationHeuristics::EventScope::Type::kNavigate);
  std::optional<SoftNavigationHeuristics::EventScope> inner_event_scope(
      heuristics->MaybeCreateEventScopeForInputEvent(*inner_event));

  SoftNavigationContext* context2 =
      tracker->CurrentTaskState()->GetSoftNavigationContext();
  EXPECT_TRUE(context2);

  EXPECT_EQ(context1, context2);
}

TEST_F(SoftNavigationHeuristicsTest, EventAfterSoftNavDetection) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  auto* outer_event =
      CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
  std::optional<SoftNavigationHeuristics::EventScope> outer_event_scope(
      heuristics->MaybeCreateEventScopeForInputEvent(*outer_event));
  auto* tracker = scheduler::TaskAttributionTracker::From(GetIsolate());
  ASSERT_TRUE(tracker);

  auto* context = tracker->CurrentTaskState()->GetSoftNavigationContext();
  ASSERT_TRUE(context);
  heuristics->ModifiedDOM(CreateNodeForTest());

  // Simulate default action link navigation after the click event.
  heuristics->SameDocumentNavigationCommitted(
      "foo",
      /*same_document_metrics_token=*/base::UnguessableToken::Create(),
      context);
  {
    auto* inner_event =
        CreateEvent(SoftNavigationHeuristics::EventScope::Type::kNavigate);
    std::optional<SoftNavigationHeuristics::EventScope> inner_event_scope(
        heuristics->MaybeCreateEventScopeForInputEvent(*inner_event));
  }
}

TEST_F(SoftNavigationHeuristicsTest,
       HeuristicNotResetDuringGCWithActiveContext) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  auto* tracker = scheduler::TaskAttributionTracker::From(GetIsolate());
  ASSERT_TRUE(tracker);

  {
    auto* event =
        CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
    std::optional<SoftNavigationHeuristics::EventScope> event_scope(
        heuristics->MaybeCreateEventScopeForInputEvent(*event));
  }
  // At this point there is a single `SoftNavigationContext` being tracked, but
  // it wasn't propagated anywhere, so it is eligible for GC.
  EXPECT_TRUE(heuristics->IsTrackingSoftNavigationsForTest());

  auto* event = CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
  std::optional<SoftNavigationHeuristics::EventScope> event_scope(
      heuristics->MaybeCreateEventScopeForInputEvent(*event));

  // If GC occurs here, e.g. during a blink allocation, the heuristic should not
  // be reset, otherwise the `SoftNavigationContext` created above will be
  // cleared.
  ThreadState::Current()->CollectAllGarbageForTesting(
      cppgc::EmbedderStackState::kMayContainHeapPointers);

  EXPECT_TRUE(heuristics->IsTrackingSoftNavigationsForTest());
}

TEST_F(SoftNavigationHeuristicsTest, SoftNavigationEmittedOnlyOnce) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  EXPECT_EQ(heuristics->SoftNavigationCount(), 0u);

  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetScriptStateForTest()->GetIsolate());
  ASSERT_TRUE(tracker);

  scheduler::TaskAttributionInfo* task_state = nullptr;
  SoftNavigationContext* context = nullptr;

  Node* node1 = CreateNodeForTest();
  Node* node2 = CreateNodeForTest();

  // Simulate an event listener that starts a soft-nav
  {
    auto* event =
        CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
    std::optional<SoftNavigationHeuristics::EventScope> event_scope(
        heuristics->MaybeCreateEventScopeForInputEvent(*event));
    task_state = tracker->CurrentTaskState();
    ASSERT_TRUE(task_state);
    context = task_state->GetSoftNavigationContext();
    ASSERT_TRUE(context);

    EXPECT_FALSE(context->SatisfiesSoftNavNonPaintCriteria());
    heuristics->SameDocumentNavigationCommitted(
        "foo.html",
        /*same_document_metrics_token=*/base::UnguessableToken::Create(),
        context);
    heuristics->ModifiedDOM(node1);
    EXPECT_FALSE(context->SatisfiesSoftNavNonPaintCriteria());
  }
  EXPECT_TRUE(context->SatisfiesSoftNavNonPaintCriteria());
  EXPECT_FALSE(context->SatisfiesSoftNavPaintCriteria(1));

  // Simulate a paint in a separate task.
  {
    TextRecord* record = CreateTextRecordForTest(node1, 1000, 1000, context);
    record->SetPaintTime(/*paint_time=*/base::TimeTicks::Now(),
                         /*info=*/DOMPaintTimingInfo());
    context->AddPaintedArea(record);
    heuristics->OnPaintFinished();
    EXPECT_TRUE(context->SatisfiesSoftNavPaintCriteria(1));
    EXPECT_TRUE(context->HasFirstContentfulPaint());
    EXPECT_EQ(heuristics->SoftNavigationCount(), 1u);
  }

  // Simulate another task for the same context, which does a second soft-nav
  {
    std::optional<TaskScope> task_scope =
        tracker->SetCurrentTaskStateIfTopLevel(task_state,
                                               TaskScopeType::kCallback);
    EXPECT_EQ(tracker->CurrentTaskState()->GetSoftNavigationContext(), context);
    heuristics->SameDocumentNavigationCommitted(
        "bar.html",
        /*same_document_metrics_token=*/base::UnguessableToken::Create(),
        context);
    heuristics->ModifiedDOM(node2);
  }

  // And another paint
  {
    TextRecord* record = CreateTextRecordForTest(node2, 1000, 1000, context);
    record->SetPaintTime(/*paint_time=*/base::TimeTicks::Now(),
                         /*info=*/DOMPaintTimingInfo());
    context->AddPaintedArea(record);
    heuristics->OnPaintFinished();
    EXPECT_TRUE(context->SatisfiesSoftNavPaintCriteria(1));
    // Should still just have one single soft-nav because a single context
    // with a single Interaction should only emit once, even if it e.g.
    // navigates twice (i.e. client-side redirects).
    EXPECT_EQ(heuristics->SoftNavigationCount(), 1u);
  }
}

TEST_F(SoftNavigationHeuristicsTest, AsyncSameDocumentNavigation) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  EXPECT_EQ(heuristics->SoftNavigationCount(), 0u);

  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetScriptStateForTest()->GetIsolate());
  ASSERT_TRUE(tracker);

  scheduler::TaskAttributionInfo* task_state = nullptr;
  SoftNavigationContext* context = nullptr;

  {
    auto* event =
        CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
    std::optional<SoftNavigationHeuristics::EventScope> event_scope(
        heuristics->MaybeCreateEventScopeForInputEvent(*event));
    task_state = tracker->CurrentTaskState();
    ASSERT_TRUE(task_state);
    context = task_state->GetSoftNavigationContext();
    ASSERT_TRUE(context);
  }

  // Simulate starting a same-document navigation in a JavaScript task
  // associated with `context`.
  std::optional<scheduler::TaskAttributionId> navigation_task_id;
  {
    std::optional<TaskScope> task_scope =
        tracker->SetCurrentTaskStateIfTopLevel(task_state,
                                               TaskScopeType::kCallback);
    navigation_task_id = tracker->AsyncSameDocumentNavigationStarted();
  }
  ASSERT_TRUE(navigation_task_id);

  // Simulate committing the same-document navigation asynchronously.
  task_state = tracker->CommitSameDocumentNavigation(*navigation_task_id);
  ASSERT_TRUE(task_state);
  EXPECT_EQ(task_state->GetSoftNavigationContext(), context);

  EXPECT_FALSE(context->HasUrl());
  heuristics->SameDocumentNavigationCommitted(
      "foo.html",
      /*same_document_metrics_token=*/base::UnguessableToken::Create(),
      context);
  EXPECT_TRUE(context->HasUrl());
}

TEST_F(SoftNavigationHeuristicsTest, AsyncSameDocumentNavigationNoContext) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  EXPECT_EQ(heuristics->SoftNavigationCount(), 0u);

  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetScriptStateForTest()->GetIsolate());
  ASSERT_TRUE(tracker);

  // Simulate starting a same-document navigation in a JavaScript task that
  // isn't associated with a `SoftNavigationContext`
  std::optional<scheduler::TaskAttributionId> navigation_task_id =
      tracker->AsyncSameDocumentNavigationStarted();
  EXPECT_FALSE(navigation_task_id);

  // Simulate committing the same-document navigation asynchronously without a
  // `SoftNavigationContext`. This shouldn't crash.
  heuristics->SameDocumentNavigationCommitted(
      "foo.html",
      /*same_document_metrics_token=*/base::UnguessableToken::Create(),
      /*context=*/nullptr);
}

TEST_F(SoftNavigationHeuristicsTest, MaybeCreateEventScopeForInputEvent) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();

  for (unsigned type = 0;
       type <=
       static_cast<unsigned>(SoftNavigationHeuristics::EventScope::Type::kLast);
       type++) {
    auto* event = CreateEvent(
        static_cast<SoftNavigationHeuristics::EventScope::Type>(type));
    auto event_scope = heuristics->MaybeCreateEventScopeForInputEvent(*event);
    bool is_navigate =
        type == static_cast<unsigned>(
                    SoftNavigationHeuristics::EventScope::Type::kNavigate);
    EXPECT_EQ(!event_scope, is_navigate);
  }

  // Untrusted events should be ignored.
  Event* event =
      CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
  event->SetTrusted(false);
  std::optional<SoftNavigationHeuristics::EventScope> event_scope =
      heuristics->MaybeCreateEventScopeForInputEvent(*event);
  EXPECT_FALSE(event_scope);

  // Unrelated events should be ignored.
  event = Event::Create(event_type_names::kDrag);
  event_scope = heuristics->MaybeCreateEventScopeForInputEvent(*event);
  EXPECT_FALSE(event_scope);

  // Keyboard events without a target or that target a non-body element should
  // be ignored.
  event = Event::Create(event_type_names::kKeydown);
  event_scope = heuristics->MaybeCreateEventScopeForInputEvent(*event);
  EXPECT_FALSE(event_scope);
  event->SetTarget(MakeGarbageCollected<HTMLDivElement>(GetDocument()));
  event_scope = heuristics->MaybeCreateEventScopeForInputEvent(*event);
  EXPECT_FALSE(event_scope);
}

}  // namespace blink
