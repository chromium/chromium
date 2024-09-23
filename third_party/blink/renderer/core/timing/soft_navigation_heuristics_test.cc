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
#include "third_party/blink/renderer/bindings/core/v8/v8_keyboard_event_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mouse_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
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

  Document& GetDocument() { return page_holder_->GetDocument(); }

  bool IsDocumentTrackingSoftNavigations() {
    return LocalDOMWindow::From(GetScriptStateForTest())
        ->document()
        ->IsTrackingSoftNavigationHeuristics();
  }

  static WTF::AtomicString KeyboardEventScopeTypeToEventName(
      SoftNavigationHeuristics::EventScope::Type type) {
    switch (type) {
      case SoftNavigationHeuristics::EventScope::Type::kKeydown:
        return event_type_names::kKeydown;
      case SoftNavigationHeuristics::EventScope::Type::kKeypress:
        return event_type_names::kKeypress;
      case SoftNavigationHeuristics::EventScope::Type::kKeyup:
        return event_type_names::kKeyup;
      default:
        NOTREACHED();
    }
  }

  Event* CreateEvent(SoftNavigationHeuristics::EventScope::Type type) {
    Event* event = nullptr;
    switch (type) {
      case SoftNavigationHeuristics::EventScope::Type::kKeydown:
      case SoftNavigationHeuristics::EventScope::Type::kKeypress:
      case SoftNavigationHeuristics::EventScope::Type::kKeyup:
        event = KeyboardEvent::Create(GetScriptStateForTest(),
                                      KeyboardEventScopeTypeToEventName(type),
                                      KeyboardEventInit::Create());
        event->SetTarget(MakeGarbageCollected<HTMLBodyElement>(GetDocument()));
        break;
      case SoftNavigationHeuristics::EventScope::Type::kClick:
        event = MouseEvent::Create(GetScriptStateForTest(),
                                   event_type_names::kClick,
                                   MouseEventInit::Create());
        break;
      case SoftNavigationHeuristics::EventScope::Type::kNavigate:
        event = Event::Create(event_type_names::kNavigate);
        break;
    }
    event->SetTrusted(true);
    return event;
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
      test_heuristics->MaybeCreateEventScopeForEvent(*event));
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
    auto* event =
        CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
    std::optional<SoftNavigationHeuristics::EventScope> event_scope(
        heuristics->MaybeCreateEventScopeForEvent(*event));

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

  auto* event = CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
  std::optional<SoftNavigationHeuristics::EventScope> outer_event_scope(
      heuristics->MaybeCreateEventScopeForEvent(*event));
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

  auto* inner_event =
      CreateEvent(SoftNavigationHeuristics::EventScope::Type::kNavigate);
  std::optional<SoftNavigationHeuristics::EventScope> inner_event_scope(
      heuristics->MaybeCreateEventScopeForEvent(*inner_event));

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

  auto* outer_event =
      CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
  std::optional<SoftNavigationHeuristics::EventScope> outer_event_scope(
      heuristics->MaybeCreateEventScopeForEvent(*outer_event));
  auto* tracker =
      scheduler::TaskAttributionTracker::From(script_state->GetIsolate());
  ASSERT_TRUE(tracker);

  auto* context = tracker->RunningTask()->GetSoftNavigationContext();
  ASSERT_TRUE(context);

  {
    std::optional<TaskScope> task_scope =
        tracker->MaybeCreateTaskScopeForCallback(script_state, nullptr);
    heuristics->ModifiedDOM();
  }

  // Simulate default action link navigation after the click event.
  heuristics->SameDocumentNavigationCommitted("foo", context);
  {
    auto* inner_event =
        CreateEvent(SoftNavigationHeuristics::EventScope::Type::kNavigate);
    std::optional<SoftNavigationHeuristics::EventScope> inner_event_scope(
        heuristics->MaybeCreateEventScopeForEvent(*inner_event));
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
    auto* event =
        CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
    std::optional<SoftNavigationHeuristics::EventScope> event_scope(
        heuristics->MaybeCreateEventScopeForEvent(*event));
    {
      std::optional<TaskScope> task_scope =
          tracker->MaybeCreateTaskScopeForCallback(script_state, nullptr);
    }
  }
  // At this point there is a single `SoftNavigationContext` being tracked, but
  // it wasn't propagated anywhere, so it is eligible for GC.
  EXPECT_TRUE(IsDocumentTrackingSoftNavigations());

  auto* event = CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
  std::optional<SoftNavigationHeuristics::EventScope> event_scope(
      heuristics->MaybeCreateEventScopeForEvent(*event));

  // If GC occurs here, e.g. during a blink allocation, the heuristic should not
  // be reset, otherwise the `SoftNavigationContext` created above will be
  // cleared.
  ThreadState::Current()->CollectAllGarbageForTesting(
      cppgc::EmbedderStackState::kMayContainHeapPointers);

  std::optional<TaskScope> task_scope =
      tracker->MaybeCreateTaskScopeForCallback(script_state, nullptr);
  EXPECT_TRUE(IsDocumentTrackingSoftNavigations());
}

TEST_F(SoftNavigationHeuristicsTest, SoftNavigationEmittedOnlyOnce) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  EXPECT_EQ(heuristics->SoftNavigationCount(), 0u);

  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetScriptStateForTest()->GetIsolate());
  ASSERT_TRUE(tracker);

  auto* script_state = GetScriptStateForTest();
  scheduler::TaskAttributionInfo* task_state = nullptr;
  SoftNavigationContext* context = nullptr;

  {
    auto* event =
        CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
    std::optional<SoftNavigationHeuristics::EventScope> event_scope(
        heuristics->MaybeCreateEventScopeForEvent(*event));
    {
      std::optional<TaskScope> task_scope =
          tracker->MaybeCreateTaskScopeForCallback(script_state, nullptr);
      task_state = tracker->RunningTask();
      ASSERT_TRUE(task_state);
      context = task_state->GetSoftNavigationContext();
      ASSERT_TRUE(context);

      heuristics->SameDocumentNavigationCommitted("foo.html", context);
      heuristics->ModifiedDOM();
    }
  }
  EXPECT_EQ(heuristics->SoftNavigationCount(), 1u);

  {
    std::optional<TaskScope> task_scope =
        tracker->MaybeCreateTaskScopeForCallback(script_state, task_state);
    heuristics->SameDocumentNavigationCommitted("bar.html", context);
    heuristics->ModifiedDOM();
  }
  EXPECT_EQ(heuristics->SoftNavigationCount(), 1u);
}

TEST_F(SoftNavigationHeuristicsTest, AsyncSameDocumentNavigation) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  EXPECT_EQ(heuristics->SoftNavigationCount(), 0u);

  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetScriptStateForTest()->GetIsolate());
  ASSERT_TRUE(tracker);

  auto* script_state = GetScriptStateForTest();
  scheduler::TaskAttributionInfo* task_state = nullptr;
  SoftNavigationContext* context = nullptr;

  {
    auto* event =
        CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
    std::optional<SoftNavigationHeuristics::EventScope> event_scope(
        heuristics->MaybeCreateEventScopeForEvent(*event));
    task_state = tracker->RunningTask();
    ASSERT_TRUE(task_state);
    context = task_state->GetSoftNavigationContext();
    ASSERT_TRUE(context);
  }

  // Simulate starting a same-document navigation in a JavaScript task
  // associated with `context`.
  std::optional<scheduler::TaskAttributionId> navigation_task_id;
  {
    std::optional<TaskScope> task_scope =
        tracker->MaybeCreateTaskScopeForCallback(script_state, task_state);
    navigation_task_id = heuristics->AsyncSameDocumentNavigationStarted();
  }
  ASSERT_TRUE(navigation_task_id);

  // Simulate committing the same-document navigation asynchronously.
  task_state = tracker->CommitSameDocumentNavigation(*navigation_task_id);
  ASSERT_TRUE(task_state);
  EXPECT_EQ(task_state->GetSoftNavigationContext(), context);

  EXPECT_TRUE(context->Url().empty());
  heuristics->SameDocumentNavigationCommitted("foo.html", context);
  EXPECT_FALSE(context->Url().empty());
}

TEST_F(SoftNavigationHeuristicsTest, AsyncSameDocumentNavigationNoContext) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();
  EXPECT_EQ(heuristics->SoftNavigationCount(), 0u);

  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetScriptStateForTest()->GetIsolate());
  ASSERT_TRUE(tracker);

  // Simulate starting a same-document navigation in a JavaScript task that
  // isn't associated with a `SoftNavigationContext`
  std::optional<scheduler::TaskAttributionId> navigation_task_id;
  {
    std::optional<TaskScope> task_scope =
        tracker->MaybeCreateTaskScopeForCallback(GetScriptStateForTest(),
                                                 /*task_state=*/nullptr);
    navigation_task_id = heuristics->AsyncSameDocumentNavigationStarted();
  }
  EXPECT_FALSE(navigation_task_id);

  // Simulate committing the same-document navigation asynchronously without a
  // `SoftNavigationContext`. This shouldn't crash.
  heuristics->SameDocumentNavigationCommitted("foo.html", /*context=*/nullptr);
}

TEST_F(SoftNavigationHeuristicsTest, MaybeCreateEventScopeForEvent) {
  auto* heuristics = CreateSoftNavigationHeuristicsForTest();

  for (unsigned type = 0;
       type <=
       static_cast<unsigned>(SoftNavigationHeuristics::EventScope::Type::kLast);
       type++) {
    auto* event = CreateEvent(
        static_cast<SoftNavigationHeuristics::EventScope::Type>(type));
    auto event_scope = heuristics->MaybeCreateEventScopeForEvent(*event);
    EXPECT_TRUE(event_scope);
  }

  // Untrusted events should be ignored.
  Event* event =
      CreateEvent(SoftNavigationHeuristics::EventScope::Type::kClick);
  event->SetTrusted(false);
  std::optional<SoftNavigationHeuristics::EventScope> event_scope =
      heuristics->MaybeCreateEventScopeForEvent(*event);
  EXPECT_FALSE(event_scope);

  // Unrelated events should be ignored.
  event = Event::Create(event_type_names::kDrag);
  event_scope = heuristics->MaybeCreateEventScopeForEvent(*event);
  EXPECT_FALSE(event_scope);

  // Keyboard events without a target or that target a non-body element should
  // be ignored.
  event = Event::Create(event_type_names::kKeydown);
  event_scope = heuristics->MaybeCreateEventScopeForEvent(*event);
  EXPECT_FALSE(event_scope);
  event->SetTarget(MakeGarbageCollected<HTMLDivElement>(GetDocument()));
  event_scope = heuristics->MaybeCreateEventScopeForEvent(*event);
  EXPECT_FALSE(event_scope);
}

}  // namespace blink
