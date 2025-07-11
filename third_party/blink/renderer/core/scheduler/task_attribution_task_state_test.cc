// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/task_attribution_task_state.h"

#include <tuple>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

namespace {

class TestTaskAttributionTaskState final : public TaskAttributionTaskState {
 public:
  void Trace(Visitor* visitor) const override {
    TaskAttributionTaskState::Trace(visitor);
  }

  scheduler::TaskAttributionInfo* GetTaskAttributionInfo() override {
    return nullptr;
  }

  SchedulerTaskContext* GetSchedulerTaskContext() override { return nullptr; }
};

}  // namespace

class TaskAttributionTaskStateTest
    : public PageTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  TaskAttributionTaskStateTest() = default;

  void SetUp() override {
    PageTestBase::SetUp();
    NavigateTo(KURL("https://example.com/"));
  }

  v8::Isolate* GetIsolate() {
    return GetDocument().GetExecutionContext()->GetIsolate();
  }
};

TEST_F(TaskAttributionTaskStateTest, GetAndSet) {
  v8::Isolate* isolate = GetIsolate();
  EXPECT_EQ(TaskAttributionTaskState::GetCurrent(isolate), nullptr);
  WeakPersistent<TaskAttributionTaskState> task_state(
      MakeGarbageCollected<TestTaskAttributionTaskState>());
  TaskAttributionTaskState::SetCurrent(isolate, task_state);
  EXPECT_EQ(TaskAttributionTaskState::GetCurrent(isolate), task_state.Get());

  // `task_state` should not be GCed because it's still stored in CPED.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(task_state);
  EXPECT_EQ(TaskAttributionTaskState::GetCurrent(isolate), task_state.Get());

  TaskAttributionTaskState::SetCurrent(isolate, nullptr);
  EXPECT_EQ(TaskAttributionTaskState::GetCurrent(isolate), nullptr);
  // `task_state` be GCed because since v8 no longer holds a reference.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(task_state);
}

}  // namespace blink
