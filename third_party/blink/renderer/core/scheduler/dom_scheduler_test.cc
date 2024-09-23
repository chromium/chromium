// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/dom_scheduler.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class DOMSchedulerTest : public PageTestBase {
 public:
  DOMSchedulerTest()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  void SetUp() override {
    EnablePlatform();
    PageTestBase::SetUp();
    GetFrame().GetSettings()->SetScriptEnabled(true);

    ExecutionContext* context = GetFrame().DomWindow();
    scheduler_ = WrapPersistent(DOMScheduler::scheduler(*context));
  }

  void ExecuteScript(const char* script) {
    ClassicScript::CreateUnspecifiedScript(script)->RunScript(
        GetFrame().DomWindow());
  }

  wtf_size_t GetDynamicPriorityTaskQueueCount() const {
    return scheduler_->signal_to_task_queue_map_.size();
  }

  DOMScheduler* GetScheduler() { return scheduler_.Get(); }

 private:
  Persistent<DOMScheduler> scheduler_;
};

TEST_F(DOMSchedulerTest, FixedPriorityTasksDontCreateTaskQueues) {
  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 0u);

  const char* kScript =
      "scheduler.postTask(() => {}, {priority: 'user-blocking'});"
      "scheduler.postTask(() => {}, {priority: 'user-blocking'});"
      "scheduler.postTask(() => {}, {priority: 'user-visible'});"
      "scheduler.postTask(() => {}, {priority: 'user-visible'});"
      "scheduler.postTask(() => {}, {priority: 'background'});"
      "scheduler.postTask(() => {}, {priority: 'background'});";
  ExecuteScript(kScript);

  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 0u);
}

TEST_F(DOMSchedulerTest,
       FixedPriorityTasksWithAbortSignalDontCreateTaskQueues) {
  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 0u);

  const char* kScript1 =
      "const controller = new AbortController();"
      "const signal = controller.signal;"
      "scheduler.postTask(() => {}, {signal});";
  ExecuteScript(kScript1);

  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 0u);

  const char* kScript2 = "scheduler.postTask(() => {}, {signal});";
  ExecuteScript(kScript2);

  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 0u);
}

TEST_F(DOMSchedulerTest, DynamicPriorityTaskQueueCreation) {
  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 0u);

  const char* kScript1 =
      "const controller1 = new TaskController();"
      "const signal1 = controller1.signal;"
      "scheduler.postTask(() => {}, {signal: signal1});";
  ExecuteScript(kScript1);

  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 1u);

  const char* kScript2 =
      "const controller2 = new TaskController();"
      "const signal2 = controller2.signal;"
      "scheduler.postTask(() => {}, {signal: signal2});";
  ExecuteScript(kScript2);

  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 2u);
}

TEST_F(DOMSchedulerTest, DynamicPriorityTaskQueueCreationReuseSignal) {
  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 0u);

  const char* kScript =
      "const controller = new TaskController();"
      "const signal = controller.signal;"
      "scheduler.postTask(() => {}, {signal});"
      "scheduler.postTask(() => {}, {signal});"
      "scheduler.postTask(() => {}, {signal});";
  ExecuteScript(kScript);

  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 1u);
}

TEST_F(DOMSchedulerTest, DynamicPriorityTaskQueueGarbageCollection) {
  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 0u);

  // Schedule a task but let the associated signal go out of scope. The dynamic
  // priority task queue should stay alive until after the task runs.
  const char* kScript =
      "function test() {"
      "  const controller = new TaskController();"
      "  const signal = controller.signal;"
      "  scheduler.postTask(() => {}, {signal});"
      "}"
      "test();";
  ExecuteScript(kScript);

  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 1u);

  // The signal and controller are out of scope in JS, but the task queue
  // should remain alive and tracked since the task hasn't run yet.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 1u);

  // Running the scheduled task and running garbage collection should now cause
  // the siganl to be untracked and the task queue to be destroyed.
  platform()->RunUntilIdle();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_EQ(GetDynamicPriorityTaskQueueCount(), 0u);
}

}  // namespace blink
