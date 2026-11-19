// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/window_idle_tasks_manager.h"

#include <limits>
#include <memory>
#include <optional>

#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_idle_request_options.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/scheduler/scripted_idle_task_controller.h"
#include "third_party/blink/renderer/core/scheduler/window_idle_tasks.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

class MockIdleTask : public IdleTask {
 public:
  MOCK_METHOD1(invoke, void(IdleDeadline*));
};

void IncrementCallCount(const v8::FunctionCallbackInfo<v8::Value>& info) {
  auto* call_count = static_cast<int*>(info.Data().As<v8::External>()->Value(
      v8::kExternalPointerTypeTagDefault));
  ++(*call_count);
}

}  // namespace

class WindowIdleTasksManagerTest : public PageTestBase {
 public:
  WindowIdleTasksManagerTest()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    EnablePlatform();
    PageTestBase::SetUp();
    GetFrame().GetSettings()->SetScriptEnabled(true);
  }

  WindowIdleTasksManager& Manager() {
    return WindowIdleTasksManager::From(*GetFrame().DomWindow());
  }

  base::PassKey<WindowIdleTasksManagerTest> PassKey() {
    return base::PassKey<WindowIdleTasksManagerTest>();
  }

  V8IdleRequestCallback* CreateIdleCallback() {
    ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
    ScriptState::Scope scope(script_state);
    v8::Local<v8::Function> fn =
        v8::Function::New(script_state->GetContext(), nullptr).ToLocalChecked();
    return V8IdleRequestCallback::Create(fn);
  }

  V8IdleRequestCallback* CreateCountingIdleCallback(int* call_count) {
    ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
    ScriptState::Scope scope(script_state);
    v8::Local<v8::External> data =
        v8::External::New(script_state->GetIsolate(), call_count,
                          v8::kExternalPointerTypeTagDefault);
    v8::Local<v8::Function> fn =
        v8::Function::New(script_state->GetContext(), IncrementCallCount, data)
            .ToLocalChecked();
    return V8IdleRequestCallback::Create(fn);
  }

  void SetIdleCallbackIdentifier(uint32_t id) {
    Manager().idle_callback_identifier_ = id;
  }

  bool HasWebExposedCallback(uint32_t id) {
    return Manager().web_exposed_ids_to_callback_ids_.Contains(id);
  }

  size_t WebExposedCallbackCount() {
    return WebExposedCallbackCount(Manager());
  }

  size_t WebExposedCallbackCount(WindowIdleTasksManager& manager) {
    return manager.web_exposed_ids_to_callback_ids_.size();
  }

  std::optional<ScriptedIdleTaskController::CallbackId> GetInternalCallbackId(
      uint32_t id) {
    auto it = Manager().web_exposed_ids_to_callback_ids_.find(id);
    if (it == Manager().web_exposed_ids_to_callback_ids_.end()) {
      return std::nullopt;
    }
    return it->value;
  }
};

TEST_F(WindowIdleTasksManagerTest, FromReturnsSameInstanceAndController) {
  LocalDOMWindow* window = GetFrame().DomWindow();
  WindowIdleTasksManager& manager1 = WindowIdleTasksManager::From(*window);
  WindowIdleTasksManager& manager2 = WindowIdleTasksManager::From(*window);
  EXPECT_EQ(&manager1, &manager2);

  ScriptedIdleTaskController& controller =
      ScriptedIdleTaskController::From(*window);
  EXPECT_EQ(&manager1.GetScriptedIdleTaskController(), &controller);
}

TEST_F(WindowIdleTasksManagerTest, RequestIdleCallbackAllocatesSequentialIds) {
  uint32_t id1 = Manager().RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());
  uint32_t id2 = Manager().RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());
  uint32_t id3 = Manager().RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());

  EXPECT_EQ(id1, 1u);
  EXPECT_EQ(id2, 2u);
  EXPECT_EQ(id3, 3u);

  EXPECT_EQ(WebExposedCallbackCount(), 3u);
  EXPECT_TRUE(HasWebExposedCallback(id1));
  EXPECT_TRUE(HasWebExposedCallback(id2));
  EXPECT_TRUE(HasWebExposedCallback(id3));

  auto internal_id1 = GetInternalCallbackId(id1);
  auto internal_id2 = GetInternalCallbackId(id2);
  auto internal_id3 = GetInternalCallbackId(id3);
  ASSERT_TRUE(internal_id1.has_value());
  ASSERT_TRUE(internal_id2.has_value());
  ASSERT_TRUE(internal_id3.has_value());

  EXPECT_TRUE(
      Manager().GetScriptedIdleTaskController().HasCallback(*internal_id1));
  EXPECT_TRUE(
      Manager().GetScriptedIdleTaskController().HasCallback(*internal_id2));
  EXPECT_TRUE(
      Manager().GetScriptedIdleTaskController().HasCallback(*internal_id3));
}

TEST_F(WindowIdleTasksManagerTest, CancelIdleCallback) {
  uint32_t id1 = Manager().RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());
  uint32_t id2 = Manager().RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());
  uint32_t id3 = Manager().RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());

  auto internal_id2 = GetInternalCallbackId(id2);
  ASSERT_TRUE(internal_id2.has_value());

  Manager().CancelIdleCallback(id2, PassKey());

  EXPECT_EQ(WebExposedCallbackCount(), 2u);
  EXPECT_TRUE(HasWebExposedCallback(id1));
  EXPECT_FALSE(HasWebExposedCallback(id2));
  EXPECT_TRUE(HasWebExposedCallback(id3));

  // The underlying task should also be cancelled in ScriptedIdleTaskController.
  EXPECT_FALSE(
      Manager().GetScriptedIdleTaskController().HasCallback(*internal_id2));

  // Cancelling a nonexistent ID is a safe no-op.
  Manager().CancelIdleCallback(0, PassKey());
  Manager().CancelIdleCallback(9999, PassKey());
  EXPECT_EQ(WebExposedCallbackCount(), 2u);
}

TEST_F(WindowIdleTasksManagerTest, CancelViaWindowIdleTasks) {
  LocalDOMWindow* window = GetFrame().DomWindow();
  uint32_t id = WindowIdleTasks::requestIdleCallback(
      *window, CreateIdleCallback(), IdleRequestOptions::Create());
  EXPECT_EQ(id, 1u);
  EXPECT_TRUE(HasWebExposedCallback(id));

  WindowIdleTasks::cancelIdleCallback(*window, id);
  EXPECT_FALSE(HasWebExposedCallback(id));
  EXPECT_EQ(WebExposedCallbackCount(), 0u);
}

// Regression test for crbug.com/540068924: Web-exposed idle callbacks must have
// a separate ID namespace from internal Blink idle tasks so that web pages
// cannot detect or cancel internal tasks (e.g. spell check).
TEST_F(WindowIdleTasksManagerTest, InternalCallbackIsolation) {
  LocalDOMWindow* window = GetFrame().DomWindow();

  // 1. Web-exposed callback gets ID 1.
  uint32_t web_id1 = WindowIdleTasks::requestIdleCallback(
      *window, CreateIdleCallback(), IdleRequestOptions::Create());
  EXPECT_EQ(web_id1, 1u);

  // 2. An internal idle task is registered directly on
  // ScriptedIdleTaskController.
  auto* internal_task1 = MakeGarbageCollected<MockIdleTask>();
  ScriptedIdleTaskController::CallbackId internal_id1 =
      ScriptedIdleTaskController::From(*window).RegisterCallback(
          internal_task1, IdleRequestOptions::Create());
  EXPECT_TRUE(
      ScriptedIdleTaskController::From(*window).HasCallback(internal_id1));

  // 3. Web-exposed callback gets ID 2 (no gap caused by internal callback).
  uint32_t web_id2 = WindowIdleTasks::requestIdleCallback(
      *window, CreateIdleCallback(), IdleRequestOptions::Create());
  EXPECT_EQ(web_id2, 2u);

  // 4. Register a second internal idle task. Its internal CallbackId is 4
  // (internal IDs are 1, 2, 3, 4), which does not match any web-exposed ID.
  auto* internal_task2 = MakeGarbageCollected<MockIdleTask>();
  ScriptedIdleTaskController::CallbackId internal_id2 =
      ScriptedIdleTaskController::From(*window).RegisterCallback(
          internal_task2, IdleRequestOptions::Create());
  EXPECT_TRUE(
      ScriptedIdleTaskController::From(*window).HasCallback(internal_id2));
  EXPECT_NE(static_cast<uint32_t>(internal_id2), web_id1);
  EXPECT_NE(static_cast<uint32_t>(internal_id2), web_id2);

  // Calling WindowIdleTasks::cancelIdleCallback with internal_id2 must NOT
  // cancel the internal task.
  WindowIdleTasks::cancelIdleCallback(*window,
                                      static_cast<uint32_t>(internal_id2));
  EXPECT_TRUE(
      ScriptedIdleTaskController::From(*window).HasCallback(internal_id2));

  // Web callbacks must remain intact.
  EXPECT_TRUE(HasWebExposedCallback(web_id1));
  EXPECT_TRUE(HasWebExposedCallback(web_id2));

  // 5. Internal tasks can still be cancelled directly via
  // ScriptedIdleTaskController.
  ScriptedIdleTaskController::From(*window).CancelCallback(internal_id1);
  ScriptedIdleTaskController::From(*window).CancelCallback(internal_id2);
  EXPECT_FALSE(
      ScriptedIdleTaskController::From(*window).HasCallback(internal_id1));
  EXPECT_FALSE(
      ScriptedIdleTaskController::From(*window).HasCallback(internal_id2));

  // Web callbacks are still unaffected.
  EXPECT_TRUE(HasWebExposedCallback(web_id1));
  EXPECT_TRUE(HasWebExposedCallback(web_id2));
}

TEST_F(WindowIdleTasksManagerTest,
       CancelWebCallbackDoesNotCancelInternalCallbackWithSameId) {
  LocalDOMWindow* window = GetFrame().DomWindow();

  // Register internal task first so internal_id is 1.
  auto* internal_task = MakeGarbageCollected<MockIdleTask>();
  ScriptedIdleTaskController::CallbackId internal_id =
      ScriptedIdleTaskController::From(*window).RegisterCallback(
          internal_task, IdleRequestOptions::Create());
  EXPECT_EQ(internal_id, 1);

  // Register web-exposed callback; it also gets web ID 1, which maps to
  // internal_id 2 in ScriptedIdleTaskController.
  uint32_t web_id = WindowIdleTasks::requestIdleCallback(
      *window, CreateIdleCallback(), IdleRequestOptions::Create());
  EXPECT_EQ(web_id, 1u);

  // Cancelling web_id 1 cancels internal task 2, but NOT internal task 1.
  WindowIdleTasks::cancelIdleCallback(*window, web_id);

  EXPECT_TRUE(
      ScriptedIdleTaskController::From(*window).HasCallback(internal_id));
  EXPECT_FALSE(HasWebExposedCallback(web_id));
}

TEST_F(WindowIdleTasksManagerTest, CallbackExecutionAndAutomaticCleanup) {
  int call_count = 0;

  IdleRequestOptions* options = IdleRequestOptions::Create();
  options->setTimeout(50);

  uint32_t id = WindowIdleTasks::requestIdleCallback(
      *GetFrame().DomWindow(), CreateCountingIdleCallback(&call_count),
      options);
  EXPECT_TRUE(HasWebExposedCallback(id));

  FastForwardBy(base::Milliseconds(100));

  EXPECT_EQ(call_count, 1);
  EXPECT_FALSE(HasWebExposedCallback(id));
  EXPECT_EQ(WebExposedCallbackCount(), 0u);

  // Redundant cancellation after completion should be a safe no-op.
  WindowIdleTasks::cancelIdleCallback(*GetFrame().DomWindow(), id);
  EXPECT_EQ(WebExposedCallbackCount(), 0u);
}

TEST_F(WindowIdleTasksManagerTest, IdWrapAround) {
  SetIdleCallbackIdentifier(std::numeric_limits<int>::max() - 1);

  uint32_t id1 = Manager().RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());
  EXPECT_EQ(id1, static_cast<uint32_t>(std::numeric_limits<int>::max()));

  uint32_t id2 = Manager().RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());
  EXPECT_EQ(id2, 1u);

  EXPECT_TRUE(HasWebExposedCallback(id1));
  EXPECT_TRUE(HasWebExposedCallback(id2));
  EXPECT_EQ(WebExposedCallbackCount(), 2u);
}

TEST_F(WindowIdleTasksManagerTest, IdWrapAroundSkipsOccupiedIds) {
  // Allocate ID 1.
  uint32_t id1 = Manager().RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());
  EXPECT_EQ(id1, 1u);

  // Set counter to max so next allocation will wrap around to 1.
  SetIdleCallbackIdentifier(std::numeric_limits<int>::max());

  // Since ID 1 is occupied, NextIdleCallbackId() should skip 1 and allocate 2.
  uint32_t id2 = Manager().RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());
  EXPECT_EQ(id2, 2u);

  EXPECT_TRUE(HasWebExposedCallback(1));
  EXPECT_TRUE(HasWebExposedCallback(2));
  EXPECT_EQ(WebExposedCallbackCount(), 2u);
}

TEST_F(WindowIdleTasksManagerTest, RequestIdleCallbackWithDestroyedContext) {
  auto page_holder = std::make_unique<DummyPageHolder>();
  LocalDOMWindow* window = page_holder->GetFrame().DomWindow();
  WindowIdleTasksManager& manager = WindowIdleTasksManager::From(*window);

  window->FrameDestroyed();
  EXPECT_TRUE(window->IsContextDestroyed());

  uint32_t id = manager.RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());
  EXPECT_EQ(id, 0u);
}

TEST_F(WindowIdleTasksManagerTest, ContextDestroyedClearsCallbacks) {
  auto page_holder = std::make_unique<DummyPageHolder>();
  LocalDOMWindow* window = page_holder->GetFrame().DomWindow();
  WindowIdleTasksManager& manager = WindowIdleTasksManager::From(*window);

  uint32_t id = manager.RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());
  EXPECT_EQ(id, 1u);
  EXPECT_EQ(WebExposedCallbackCount(manager), 1u);

  window->FrameDestroyed();
  EXPECT_TRUE(window->IsContextDestroyed());
  EXPECT_EQ(WebExposedCallbackCount(manager), 0u);
}

TEST_F(WindowIdleTasksManagerTest, CancelInvalidIdsDoesNotCorruptMap) {
  uint32_t id1 = Manager().RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());
  EXPECT_EQ(id1, 1u);
  Manager().CancelIdleCallback(id1, PassKey());
  EXPECT_EQ(WebExposedCallbackCount(), 0u);

  Manager().CancelIdleCallback(0, PassKey());
  Manager().CancelIdleCallback(std::numeric_limits<uint32_t>::max(), PassKey());
  EXPECT_EQ(WebExposedCallbackCount(), 0u);

  uint32_t id2 = Manager().RequestIdleCallback(
      CreateIdleCallback(), IdleRequestOptions::Create(), PassKey());
  EXPECT_EQ(id2, 2u);
  EXPECT_EQ(WebExposedCallbackCount(), 1u);
}

}  // namespace blink
