// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_history.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TestAction : public InspectorHistory::Action {
 public:
  TestAction() : InspectorHistory::Action("TestAction") {}

  bool Perform(ExceptionState&) override {
    performed++;
    return true;
  }
  bool Undo(ExceptionState&) override {
    undone++;
    return true;
  }
  bool Redo(ExceptionState&) override {
    redone++;
    return true;
  }

  int performed = 0;
  int undone = 0;
  int redone = 0;
};

class PerformFailsAction final : public TestAction {
 public:
  bool Perform(ExceptionState&) override { return false; }
};

class MergeableAction : public TestAction {
 public:
  explicit MergeableAction(String token) { this->token = token; }
  String MergeId() override {
    return "mergeMe!";  // Everything can merge.
  }
  void Merge(Action* other) override {
    this->token = static_cast<MergeableAction*>(other)->token;
  }

  String token;
};

// Becomes a no-op after merge.
class NoOpMergeableAction : public MergeableAction {
 public:
  explicit NoOpMergeableAction(String token) : MergeableAction(token) {}

  void Merge(Action* other) override {
    merged = true;
    this->token = static_cast<MergeableAction*>(other)->token;
  }
  bool IsNoop() override { return merged; }

  bool merged = false;
};

TEST(InspectorHistoryTest, UndoEmptyHistorySucceeds) {
  InspectorHistory* history = MakeGarbageCollected<InspectorHistory>();

  DummyExceptionStateForTesting exception_state;
  ASSERT_TRUE(history->Undo(exception_state));
}

TEST(InspectorHistoryTest, UndoUndoableMarkerSucceeds) {
  InspectorHistory* history = MakeGarbageCollected<InspectorHistory>();
  history->MarkUndoableState();

  DummyExceptionStateForTesting exception_state;
  ASSERT_TRUE(history->Undo(exception_state));
}

TEST(InspectorHistoryTest, PerformedActionIsPerformed) {
  InspectorHistory* history = MakeGarbageCollected<InspectorHistory>();

  TestAction* action = MakeGarbageCollected<TestAction>();
  DummyExceptionStateForTesting exception_state;
  ASSERT_TRUE(history->Perform(action, exception_state));
  ASSERT_EQ(action->performed, 1);
  ASSERT_EQ(action->undone, 0);
  ASSERT_EQ(action->redone, 0);
}

TEST(InspectorHistoryTest, UndoPerformedAction) {
  InspectorHistory* history = MakeGarbageCollected<InspectorHistory>();

  TestAction* action = MakeGarbageCollected<TestAction>();
  DummyExceptionStateForTesting exception_state;
  history->Perform(action, exception_state);
  history->Undo(exception_state);
  ASSERT_EQ(action->performed, 1);
  ASSERT_EQ(action->undone, 1);
  ASSERT_EQ(action->redone, 0);
}

TEST(InspectorHistoryTest, RedoUndoneAction) {
  InspectorHistory* history = MakeGarbageCollected<InspectorHistory>();

  TestAction* action = MakeGarbageCollected<TestAction>();
  DummyExceptionStateForTesting exception_state;
  history->Perform(action, exception_state);
  history->Undo(exception_state);
  history->Redo(exception_state);
  ASSERT_EQ(action->performed, 1);
  ASSERT_EQ(action->undone, 1);
  ASSERT_EQ(action->redone, 1);
}

TEST(InspectorHistoryTest, TwoActionsBothAreUndone) {
  InspectorHistory* history = MakeGarbageCollected<InspectorHistory>();

  TestAction* action = MakeGarbageCollected<TestAction>();
  TestAction* action2 = MakeGarbageCollected<TestAction>();
  DummyExceptionStateForTesting exception_state;
  history->MarkUndoableState();
  history->Perform(action, exception_state);
  history->Perform(action2, exception_state);
  history->Undo(exception_state);

  ASSERT_EQ(action->performed, 1);
  ASSERT_EQ(action->undone, 1);
  ASSERT_EQ(action->redone, 0);
  ASSERT_EQ(action2->performed, 1);
  ASSERT_EQ(action2->undone, 1);
  ASSERT_EQ(action2->redone, 0);
}

TEST(InspectorHistoryTest, TwoActionsBothAreRedone) {
  InspectorHistory* history = MakeGarbageCollected<InspectorHistory>();

  TestAction* action = MakeGarbageCollected<TestAction>();
  TestAction* action2 = MakeGarbageCollected<TestAction>();
  DummyExceptionStateForTesting exception_state;
  history->MarkUndoableState();
  history->Perform(action, exception_state);
  history->Perform(action2, exception_state);
  history->Undo(exception_state);
  history->Redo(exception_state);

  ASSERT_EQ(action->performed, 1);
  ASSERT_EQ(action->undone, 1);
  ASSERT_EQ(action->redone, 1);
  ASSERT_EQ(action2->performed, 1);
  ASSERT_EQ(action2->undone, 1);
  ASSERT_EQ(action2->redone, 1);
}

TEST(InspectorHistoryTest, PerformFails) {
  InspectorHistory* history = MakeGarbageCollected<InspectorHistory>();

  PerformFailsAction* action = MakeGarbageCollected<PerformFailsAction>();
  DummyExceptionStateForTesting exception_state;
  ASSERT_FALSE(history->Perform(action, exception_state));

  ASSERT_TRUE(history->Undo(exception_state));
  ASSERT_TRUE(history->Redo(exception_state));
  ASSERT_EQ(action->undone, 0);
  ASSERT_EQ(action->redone, 0);
}

TEST(InspectorHistoryTest, ResetClearsPerformedAction) {
  InspectorHistory* history = MakeGarbageCollected<InspectorHistory>();

  TestAction* action = MakeGarbageCollected<TestAction>();
  DummyExceptionStateForTesting exception_state;
  ASSERT_TRUE(history->Perform(action, exception_state));
  history->Reset();

  ASSERT_TRUE(history->Undo(exception_state));
  ASSERT_EQ(action->performed, 1);
  ASSERT_EQ(action->undone, 0);
  ASSERT_EQ(action->redone, 0);
}

TEST(InspectorHistoryTest, MergeableActionIsNotStored) {
  InspectorHistory* history = MakeGarbageCollected<InspectorHistory>();

  MergeableAction* action = MakeGarbageCollected<MergeableAction>("A");
  MergeableAction* action2 = MakeGarbageCollected<MergeableAction>("B");
  DummyExceptionStateForTesting exception_state;
  ASSERT_TRUE(history->Perform(action, exception_state));
  ASSERT_TRUE(history->Perform(action2, exception_state));

  ASSERT_EQ(action->token, "B");  // Merge happened successfully.

  ASSERT_TRUE(history->Undo(exception_state));
  ASSERT_EQ(action->performed, 1);
  ASSERT_EQ(action->undone, 1);
  ASSERT_EQ(action2->performed, 1);
  // The second action was never stored after the merge.
  ASSERT_EQ(action2->undone, 0);
}

TEST(InspectorHistoryTest, NoOpMergeableActionIsCleared) {
  InspectorHistory* history = MakeGarbageCollected<InspectorHistory>();

  NoOpMergeableAction* action = MakeGarbageCollected<NoOpMergeableAction>("A");
  NoOpMergeableAction* action2 = MakeGarbageCollected<NoOpMergeableAction>("B");
  DummyExceptionStateForTesting exception_state;
  ASSERT_TRUE(history->Perform(action, exception_state));
  // This will cause action to become a no-op.
  ASSERT_TRUE(history->Perform(action2, exception_state));

  ASSERT_TRUE(history->Undo(exception_state));
  ASSERT_EQ(action->performed, 1);
  // The first action was cleared after merge because it became a no-op.
  ASSERT_EQ(action->undone, 0);
  ASSERT_EQ(action2->performed, 1);
  ASSERT_EQ(action2->undone, 0);
}

TEST(InspectorHistoryTest, RedoEmptyHistorySucceeds) {
  InspectorHistory* history = MakeGarbageCollected<InspectorHistory>();

  DummyExceptionStateForTesting exception_state;
  ASSERT_TRUE(history->Redo(exception_state));
}

}  // namespace blink
