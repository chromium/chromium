// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/script_wrappable_task_state.h"

#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

namespace {

class TestWrappableTaskState final : public WrappableTaskState {
 public:
  void Trace(Visitor* visitor) const override {
    WrappableTaskState::Trace(visitor);
  }

  scheduler::TaskAttributionInfo* GetTaskAttributionInfo() override {
    return nullptr;
  }

  SchedulerTaskContext* GetSchedulerTaskContext() override { return nullptr; }
};

}  // namespace

class ScriptWrappableTaskStateTest
    : public PageTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  ScriptWrappableTaskStateTest() = default;

  void SetUp() override {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(kTaskAttributionUsesV8CppHeapExternal);
    } else {
      feature_list_.InitAndDisableFeature(
          kTaskAttributionUsesV8CppHeapExternal);
    }
    PageTestBase::SetUp();
    NavigateTo(KURL("https://example.com/"));
  }

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(&GetFrame());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(ScriptWrappableTaskStateTest, GetAndSet) {
  ScriptState* script_state = GetScriptState();
  EXPECT_EQ(ScriptWrappableTaskState::GetCurrent(script_state->GetIsolate()),
            nullptr);
  WeakPersistent<WrappableTaskState> task_state(
      MakeGarbageCollected<TestWrappableTaskState>());
  ScriptWrappableTaskState::SetCurrent(script_state, task_state);
  EXPECT_EQ(ScriptWrappableTaskState::GetCurrent(script_state->GetIsolate())
                ->WrappedState(),
            task_state.Get());

  // `task_state` should not be GCed because it's still stored in CPED.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_TRUE(task_state);
  EXPECT_EQ(ScriptWrappableTaskState::GetCurrent(script_state->GetIsolate())
                ->WrappedState(),
            task_state.Get());

  ScriptWrappableTaskState::SetCurrent(script_state, nullptr);
  EXPECT_EQ(ScriptWrappableTaskState::GetCurrent(script_state->GetIsolate()),
            nullptr);
  // `task_state` be GCed because since v8 no longer holds a reference.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(task_state);
}

INSTANTIATE_TEST_SUITE_P(All, ScriptWrappableTaskStateTest, testing::Bool());

}  // namespace blink
