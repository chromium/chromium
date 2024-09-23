// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_error.h"
#include "base/memory/raw_ref.h"
#include "services/device/public/mojom/smart_card.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

class PromiseRejectedFunction : public ScriptFunction::Callable {
 public:
  explicit PromiseRejectedFunction(bool& result) : result_(result) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    *result_ = true;
    return ScriptValue();
  }

 private:
  const raw_ref<bool> result_;
};

TEST(SmartCardError, RejectWithoutScriptStateScope) {
  test::TaskEnvironment task_environment;

  std::unique_ptr<DummyPageHolder> page_holder =
      DummyPageHolder::CreateAndCommitNavigation(KURL());

  DummyExceptionStateForTesting exception_state;

  ScriptState* script_state =
      ToScriptStateForMainWorld(page_holder->GetDocument().GetFrame());

  ScriptPromiseResolver<IDLUndefined>* resolver = nullptr;
  bool rejected = false;
  {
    ScriptState::Scope script_state_scope(script_state);

    resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
        script_state, exception_state.GetContext());

    auto promise = resolver->Promise();
    promise.Then(nullptr,
                 MakeGarbageCollected<ScriptFunction>(
                     script_state,
                     MakeGarbageCollected<PromiseRejectedFunction>(rejected)));
  }

  // Call it without a current v8 context.
  // Should still just work.
  SmartCardError::MaybeReject(
      resolver, device::mojom::blink::SmartCardError::kInvalidHandle);

  // Run the pending "Then" function of the rejected promise, if any.
  {
    ScriptState::Scope script_state_scope(script_state);
    script_state->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
        script_state->GetContext()->GetIsolate());
  }

  EXPECT_TRUE(rejected);
}

}  // namespace

}  // namespace blink
