// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_error.h"

#include "base/memory/raw_ref.h"
#include "services/device/public/mojom/smart_card.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

class PromiseRejectedFunction
    : public ThenCallable<IDLAny, PromiseRejectedFunction> {
 public:
  explicit PromiseRejectedFunction(bool& result) : result_(result) {}
  void React(ScriptState*, ScriptValue value) { *result_ = true; }

 private:
  const raw_ref<bool> result_;
};

class PromiseRejectedDOMExceptionFunction
    : public ThenCallable<IDLAny, PromiseRejectedDOMExceptionFunction> {
 public:
  explicit PromiseRejectedDOMExceptionFunction(DOMExceptionCode& result_code)
      : result_code_(result_code) {}

  void React(ScriptState* script_state, ScriptValue value) {
    if (auto* dom_exception = V8DOMException::ToWrappable(
            script_state->GetIsolate(), value.V8Value())) {
      *result_code_ = static_cast<DOMExceptionCode>(dom_exception->code());
    }
  }

 private:
  const raw_ref<DOMExceptionCode> result_code_;
};

class SmartCardErrorTest : public testing::Test {
 protected:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<DummyPageHolder> page_holder_ =
      DummyPageHolder::CreateAndCommitNavigation(KURL());

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(page_holder_->GetDocument().GetFrame());
  }

  void RunPendingMicrotasks() {
    ScriptState* script_state = GetScriptState();
    ScriptState::Scope script_state_scope(script_state);
    script_state->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
        script_state->GetIsolate());
  }

  DOMExceptionCode RejectAndGetExceptionCode(
      device::mojom::blink::SmartCardError mojom_error) {
    DummyExceptionStateForTesting exception_state;
    ScriptState* script_state = GetScriptState();
    DOMExceptionCode rejected_code = DOMExceptionCode::kNoError;

    {
      ScriptState::Scope script_state_scope(script_state);

      auto* resolver =
          MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
              script_state, exception_state.GetContext());

      resolver->Promise().Catch(
          script_state,
          MakeGarbageCollected<PromiseRejectedDOMExceptionFunction>(
              rejected_code));

      SmartCardError::MaybeReject(resolver, mojom_error);
    }

    RunPendingMicrotasks();

    return rejected_code;
  }
};

TEST_F(SmartCardErrorTest, RejectWithoutScriptStateScope) {
  DummyExceptionStateForTesting exception_state;
  ScriptState* script_state = GetScriptState();

  ScriptPromiseResolver<IDLUndefined>* resolver = nullptr;
  bool rejected = false;
  {
    ScriptState::Scope script_state_scope(script_state);

    resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
        script_state, exception_state.GetContext());

    resolver->Promise().Catch(
        script_state, MakeGarbageCollected<PromiseRejectedFunction>(rejected));
  }

  // Call it without a current v8 context.
  // Should still just work.
  SmartCardError::MaybeReject(
      resolver, device::mojom::blink::SmartCardError::kInvalidHandle);

  RunPendingMicrotasks();

  EXPECT_TRUE(rejected);
}

TEST_F(SmartCardErrorTest, TimeoutError) {
  EXPECT_EQ(
      RejectAndGetExceptionCode(device::mojom::blink::SmartCardError::kTimeout),
      DOMExceptionCode::kTimeoutError);
}

TEST_F(SmartCardErrorTest, InvalidHandleError) {
  EXPECT_EQ(RejectAndGetExceptionCode(
                device::mojom::blink::SmartCardError::kInvalidHandle),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(SmartCardErrorTest, ServiceStoppedError) {
  EXPECT_EQ(RejectAndGetExceptionCode(
                device::mojom::blink::SmartCardError::kServiceStopped),
            DOMExceptionCode::kInvalidStateError);
}

TEST_F(SmartCardErrorTest, ShutdownError) {
  EXPECT_EQ(RejectAndGetExceptionCode(
                device::mojom::blink::SmartCardError::kShutdown),
            DOMExceptionCode::kAbortError);
}

}  // namespace

}  // namespace blink
