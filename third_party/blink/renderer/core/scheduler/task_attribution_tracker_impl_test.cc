// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/task_attribution_tracker_impl.h"

#include <optional>

#include "base/run_loop.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/scheduler/task_attribution_info_impl.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock-matchers.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace blink::scheduler {

class TaskAttributionTrackerImplTest : public PageTestBase {
 public:
  TaskAttributionTrackerImplTest() = default;

  void SetUp() override {
    PageTestBase::SetUp();
    NavigateTo(KURL("https://example.com/"));
    tracker_ = TaskAttributionTracker::From(
        GetDocument().GetExecutionContext()->GetIsolate());
    ASSERT_TRUE(tracker_);
  }

  void TearDown() override {
    tracker_ = nullptr;
    PageTestBase::TearDown();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return GetDocument().GetTaskRunner(TaskType::kInternalTest);
  }

  void PostNestedEventLoopTaskAndQuit(base::RunLoop& run_loop,
                                      const base::Location& from_here,
                                      const uint32_t current_iteration,
                                      const uint32_t num_iterations) {
    GetTaskRunner()->PostTask(
        from_here,
        BindOnce(&TaskAttributionTrackerImplTest::NestedEventLoopTask,
                 Unretained(this), current_iteration, num_iterations));
    GetTaskRunner()->PostTask(from_here, run_loop.QuitClosure());
  }

  void NestedEventLoopTask(const uint32_t current_iteration,
                           const uint32_t num_iterations) {
    // This should be null initially and when entering a nested event loop.
    EXPECT_EQ(tracker_->CurrentTaskState(), nullptr);

    if (current_iteration == num_iterations) {
      return;
    }

    // Next set up new task state. This state is expected to be restored after
    // the nested run loop.
    TaskAttributionInfoImpl* task_state =
        MakeGarbageCollected<TaskAttributionInfoImpl>(
            /*soft_navigation_context=*/nullptr,
            /*resource_timing_context=*/nullptr, current_iteration);
    std::optional<TaskAttributionTracker::TaskScope> scope(
        tracker_->SetCurrentTaskStateIfTopLevel(task_state,
                                                TaskScopeType::kMiscEvent));
    EXPECT_EQ(tracker_->CurrentTaskState(), task_state);
    {
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      PostNestedEventLoopTaskAndQuit(run_loop, FROM_HERE, current_iteration + 1,
                                     num_iterations);
      run_loop.Run();
    }
    EXPECT_EQ(tracker_->CurrentTaskState(), task_state);
  }

 protected:
  TaskAttributionTracker* tracker_ = nullptr;
};

TEST_F(TaskAttributionTrackerImplTest, TaskStateClearedOnNestedRunLoop) {
  // Start a run loop to simulate the main run loop. This is necessary because
  // the number of active run loops is actually 0 here, so the first one created
  // won't be considered nested.
  base::RunLoop run_loop;
  PostNestedEventLoopTaskAndQuit(run_loop, FROM_HERE, /*current_iteration=*/1,
                                 /*num_iterations=*/3);
  run_loop.Run();
}

}  // namespace blink::scheduler
