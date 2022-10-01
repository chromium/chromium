// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/task_attribution_tracker_impl.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink::scheduler {

class TaskAttributionTrackerTest : public PageTestBase {
 protected:
  WTF::Vector<std::unique_ptr<TaskAttributionTracker::TaskScope>> task_stack_;

 public:
  class MockV8Adapter : public TaskAttributionTrackerImpl::V8Adapter {
   public:
    absl::optional<TaskAttributionId> GetValue(ScriptState*) override {
      return value_;
    }
    void SetValue(ScriptState*, absl::optional<TaskAttributionId> id) override {
      value_ = id;
    }

   private:
    absl::optional<TaskAttributionId> value_;
  };

  void PostTasks(TaskAttributionTrackerImpl& tracker,
                 unsigned task_number,
                 unsigned number_to_assert,
                 TaskAttributionIdType parent_to_assert,
                 bool complete,
                 TaskAttributionId* id = nullptr) {
    TaskAttributionId previous_id = TaskAttributionId(parent_to_assert);
    ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
    for (unsigned i = 0; i < task_number; ++i) {
      task_stack_.push_back(tracker.CreateTaskScope(
          script_state, previous_id,
          TaskAttributionTracker::TaskScopeType::kScriptExecution));
      absl::optional<TaskAttributionId> running_id =
          tracker.RunningTaskAttributionId(script_state);
      if (i < number_to_assert) {
        // Make sure that the parent task is an ancestor.
        TaskAttributionId parent_task(parent_to_assert);
        TaskAttributionTracker::AncestorStatus is_ancestor =
            tracker.IsAncestor(script_state, parent_task);
        ASSERT_TRUE(is_ancestor ==
                    TaskAttributionTracker::AncestorStatus::kAncestor);
        if (!complete) {
          ASSERT_TRUE(tracker.IsAncestor(script_state, previous_id) ==
                      TaskAttributionTracker::AncestorStatus::kAncestor);
        }
      }
      if (id) {
        *id = running_id.value();
      }
      previous_id = running_id.value();
      if (complete) {
        task_stack_.pop_back();
      }
    }
  }

  void TestAttributionQueue(unsigned overflow_length,
                            unsigned asserts_length,
                            bool nested_tasks_complete,
                            bool assert_last_task) {
    TaskAttributionTrackerImpl tracker;
    MockV8ForTracker(tracker);
    // Post tasks for half the queue.
    unsigned half_queue = TaskAttributionTrackerImpl::kVectorSize / 2;
    TaskAttributionId id(0);
    ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
    PostTasks(tracker, half_queue, /*number_to_assert=*/0,
              /*parent_to_assert=*/0,
              /*complete=*/true, &id);
    // Verify that the ID of the last task that ran is what we expect it to be.
    ASSERT_EQ(half_queue, id.value());
    // Start the parent task, but don't complete it.
    task_stack_.push_back(tracker.CreateTaskScope(
        script_state, tracker.RunningTaskAttributionId(script_state),
        TaskAttributionTracker::TaskScopeType::kScriptExecution));
    // Get its ID.
    TaskAttributionId parent_id =
        tracker.RunningTaskAttributionId(script_state).value();
    // Post |overflow_length| tasks.
    PostTasks(tracker, overflow_length, asserts_length, parent_id.value(),
              nested_tasks_complete);
    if (assert_last_task && ((overflow_length + half_queue) >
                             TaskAttributionTrackerImpl::kVectorSize)) {
      // Post another task.
      task_stack_.push_back(tracker.CreateTaskScope(
          script_state, tracker.RunningTaskAttributionId(script_state),
          TaskAttributionTracker::TaskScopeType::kScriptExecution));

      // Since it goes beyond the queue length and the parent task was
      // overwritten, we cannot track ancestry.
      ASSERT_TRUE(tracker.IsAncestor(script_state, parent_id) !=
                  TaskAttributionTracker::AncestorStatus::kAncestor);
      if (nested_tasks_complete) {
        task_stack_.pop_back();
      }
    }
    // Complete all the tasks
    while (!task_stack_.empty()) {
      task_stack_.pop_back();
    }
  }

  void MockV8ForTracker(TaskAttributionTrackerImpl& tracker) {
    tracker.SetV8AdapterForTesting(std::make_unique<MockV8Adapter>());
  }
};

TEST_F(TaskAttributionTrackerTest, TrackTaskLargerThanQueue) {
  TestAttributionQueue(
      /*overflow_length=*/TaskAttributionTrackerImpl::kVectorSize - 1,
      /*asserts_length=*/TaskAttributionTrackerImpl::kVectorSize - 1,
      /*nested_tasks_complete=*/true,
      /*assert_last_task=*/true);
}

TEST_F(TaskAttributionTrackerTest, TrackTwoTasksAcrossTheQueue) {
  TestAttributionQueue(
      /*overflow_length=*/TaskAttributionTrackerImpl::kVectorSize + 100,
      /*asserts_length=*/TaskAttributionTrackerImpl::kVectorSize - 1,
      /*nested_tasks_complete=*/true,
      /*assert_last_task=*/true);
}

TEST_F(TaskAttributionTrackerTest, CausalityChain) {
  TestAttributionQueue(/*overflow_length=*/100, /*asserts_length=*/100,
                       /*nested_tasks_complete=*/false,
                       /*assert_last_task=*/false);
}

TEST_F(TaskAttributionTrackerTest, CausalityChainOverflow) {
  TestAttributionQueue(
      /*overflow_length=*/TaskAttributionTrackerImpl::kVectorSize - 1,
      /*asserts_length=*/TaskAttributionTrackerImpl::kVectorSize - 1,
      /*nested_tasks_complete=*/false,
      /*assert_last_task=*/true);
}

TEST_F(TaskAttributionTrackerTest, NotAncestor) {
  TaskAttributionTrackerImpl tracker;
  MockV8ForTracker(tracker);
  TaskAttributionId first_id(0);
  ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());

  // Start a task, get its ID and complete it.
  {
    auto scope = tracker.CreateTaskScope(
        script_state, absl::nullopt,
        TaskAttributionTracker::TaskScopeType::kScriptExecution);
    first_id = tracker.RunningTaskAttributionId(script_state).value();
  }

  // Start an incomplete task.
  auto scope = tracker.CreateTaskScope(
      script_state, absl::nullopt,
      TaskAttributionTracker::TaskScopeType::kScriptExecution);
  // Make sure that the first task is not an ancestor.
  ASSERT_TRUE(tracker.IsAncestor(script_state, first_id) ==
              TaskAttributionTracker::AncestorStatus::kNotAncestor);
}

}  // namespace blink::scheduler
