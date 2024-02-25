// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/abort_signal.h"

#include <tuple>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/dom/abort_signal_registry.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class TestEventListener : public NativeEventListener {
 public:
  TestEventListener() = default;

  void Invoke(ExecutionContext*, Event*) override {}
};

}  // namespace

class AbortSignalTest : public PageTestBase {
 public:
  AbortSignalTest() = default;

  void SetUp() override {
    PageTestBase::SetUp();

    ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
    controller_ = AbortController::Create(script_state);
    signal_ = controller_->signal();
  }

  void SignalAbort() {
    ScriptState* script_state = ToScriptStateForMainWorld(&GetFrame());
    ScriptState::Scope script_scope(script_state);
    controller_->abort(script_state);
  }

  AbortSignalRegistry* GetRegistry() const {
    return AbortSignalRegistry::From(*GetFrame().DomWindow());
  }

 protected:
  Persistent<AbortController> controller_;
  Persistent<AbortSignal> signal_;
  Persistent<AbortSignal::AlgorithmHandle> abort_handle_;
};

TEST_F(AbortSignalTest, AbortAlgorithmRuns) {
  int count = 0;
  abort_handle_ = signal_->AddAlgorithm(
      WTF::BindOnce([](int* count) { ++(*count); }, WTF::Unretained(&count)));

  // GC should not affect whether or not the algorithm runs.
  ThreadState::Current()->CollectAllGarbageForTesting();

  SignalAbort();
  EXPECT_EQ(count, 1);

  // Subsequent aborts are no-ops.
  SignalAbort();
  EXPECT_EQ(count, 1);
}

TEST_F(AbortSignalTest, AbortAlgorithmHandleRemoved) {
  int count = 0;
  abort_handle_ = signal_->AddAlgorithm(
      WTF::BindOnce([](int* count) { ++(*count); }, WTF::Unretained(&count)));

  signal_->RemoveAlgorithm(abort_handle_.Get());

  SignalAbort();
  EXPECT_EQ(count, 0);
}

TEST_F(AbortSignalTest, AbortAlgorithmHandleGCed) {
  int count = 0;
  abort_handle_ = signal_->AddAlgorithm(
      WTF::BindOnce([](int* count) { ++(*count); }, WTF::Unretained(&count)));

  abort_handle_.Clear();
  ThreadState::Current()->CollectAllGarbageForTesting();

  SignalAbort();
  EXPECT_EQ(count, 0);
}

TEST_F(AbortSignalTest, RegisteredSignalAlgorithmRuns) {
  int count = 0;
  Persistent<TestEventListener> listener =
      MakeGarbageCollected<TestEventListener>();
  {
    auto* handle = signal_->AddAlgorithm(
        WTF::BindOnce([](int* count) { ++(*count); }, WTF::Unretained(&count)));
    GetRegistry()->RegisterAbortAlgorithm(listener.Get(), handle);
  }

  // GC should not affect whether or not the algorithm runs.
  ThreadState::Current()->CollectAllGarbageForTesting();

  SignalAbort();
  EXPECT_EQ(count, 1);
}

TEST_F(AbortSignalTest, RegisteredSignalAlgorithmListenerGCed) {
  int count = 0;
  Persistent<TestEventListener> listener =
      MakeGarbageCollected<TestEventListener>();
  {
    auto* handle = signal_->AddAlgorithm(
        WTF::BindOnce([](int* count) { ++(*count); }, WTF::Unretained(&count)));
    GetRegistry()->RegisterAbortAlgorithm(listener.Get(), handle);
  }

  listener.Clear();
  ThreadState::Current()->CollectAllGarbageForTesting();

  SignalAbort();
  EXPECT_EQ(count, 0);
}

TEST_F(AbortSignalTest, CanAbort) {
  EXPECT_TRUE(signal_->CanAbort());
  SignalAbort();
  EXPECT_FALSE(signal_->CanAbort());
}

TEST_F(AbortSignalTest, CanAbortAfterGC) {
  controller_.Clear();
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(signal_->CanAbort());
}

}  // namespace blink
