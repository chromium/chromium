// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class ScriptPromiseTester::ThenFunction : public ScriptFunction::Callable {
 public:
  ThenFunction(base::WeakPtr<ScriptPromiseTester> owner,
               ScriptPromiseTester::State target_state)
      : owner_(std::move(owner)), target_state_(target_state) {}

  ScriptValue Call(ScriptState*, ScriptValue value) override {
    if (!owner_)
      return value;

    DCHECK_EQ(owner_->state_, State::kNotSettled);
    owner_->state_ = target_state_;

    DCHECK(owner_->value_object_->Value().IsEmpty());
    owner_->value_object_->Value() = value;
    return value;
  }

 private:
  GC_PLUGIN_IGNORE("Pointer to on-stack class is valid here.")
  base::WeakPtr<ScriptPromiseTester> owner_;
  State target_state_;
};

ScriptPromiseTester::ScriptPromiseTester(ScriptState* script_state,
                                         ScriptPromiseUntyped script_promise)
    : script_state_(script_state),
      value_object_(MakeGarbageCollected<ScriptValueObject>()) {
  DCHECK(script_state);
  script_promise.Then(
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<ThenFunction>(
                            weak_factory_.GetWeakPtr(), State::kFulfilled)),
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<ThenFunction>(
                            weak_factory_.GetWeakPtr(), State::kRejected)));
}

void ScriptPromiseTester::WaitUntilSettled() {
  auto* isolate = script_state_->GetIsolate();
  while (state_ == State::kNotSettled) {
    script_state_->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
        isolate);
    test::RunPendingTasks();
  }
}

ScriptValue ScriptPromiseTester::Value() const {
  return value_object_->Value();
}

}  // namespace blink
