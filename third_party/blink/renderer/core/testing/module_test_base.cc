// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"

namespace blink {

void ParametrizedModuleTestBase::SetUp(bool use_top_level_await) {
  if (use_top_level_await) {
    feature_list_.InitAndEnableFeature(features::kTopLevelAwait);
  } else {
    feature_list_.InitAndDisableFeature(features::kTopLevelAwait);
  }
  SetV8Flags(use_top_level_await);
}

void ParametrizedModuleTestBase::SetV8Flags(bool use_top_level_await) {
  if (use_top_level_await) {
    v8::V8::SetFlagsFromString("--harmony-top-level-await");
  } else {
    v8::V8::SetFlagsFromString("--no-harmony-top-level-await");
  }
}

void ParametrizedModuleTest::SetUp() {
  ParametrizedModuleTestBase::SetUp(UseTopLevelAwait());
}

class SaveResultFunction final : public ScriptFunction {
 public:
  explicit SaveResultFunction(ScriptState* script_state)
      : ScriptFunction(script_state) {}

  v8::Local<v8::Function> Bind() { return BindToV8Function(); }

  v8::Local<v8::Value> GetResult() {
    EXPECT_TRUE(result_);
    EXPECT_FALSE(result_->IsEmpty());
    return result_->V8Value();
  }

 private:
  ScriptValue Call(ScriptValue value) override {
    *result_ = value;
    return value;
  }

  ScriptValue* result_ = nullptr;
};

class ExpectNotReached final : public ScriptFunction {
 public:
  static v8::Local<v8::Function> Create(ScriptState* script_state) {
    auto* self = MakeGarbageCollected<ExpectNotReached>(script_state);
    return self->BindToV8Function();
  }
  explicit ExpectNotReached(ScriptState* script_state)
      : ScriptFunction(script_state) {}

 private:
  ScriptValue Call(ScriptValue value) override {
    ADD_FAILURE() << "ExpectNotReached was reached";
    return value;
  }
};

v8::Local<v8::Value> ParametrizedModuleTestBase::GetResult(
    ScriptState* script_state,
    ScriptEvaluationResult result) {
  CHECK_EQ(result.GetResultType(),
           ScriptEvaluationResult::ResultType::kSuccess);
  if (!base::FeatureList::IsEnabled(features::kTopLevelAwait)) {
    return result.GetSuccessValue();
  }

  ScriptPromise script_promise = result.GetPromise(script_state);
  v8::Local<v8::Promise> promise = script_promise.V8Value().As<v8::Promise>();
  if (promise->State() == v8::Promise::kFulfilled) {
    return promise->Result();
  }

  auto* resolve_function =
      MakeGarbageCollected<SaveResultFunction>(script_state);
  result.GetPromise(script_state)
      .Then(resolve_function->Bind(), ExpectNotReached::Create(script_state));

  v8::MicrotasksScope::PerformCheckpoint(script_state->GetIsolate());

  return resolve_function->GetResult();
}

v8::Local<v8::Value> ParametrizedModuleTestBase::GetException(
    ScriptState* script_state,
    ScriptEvaluationResult result) {
  if (!base::FeatureList::IsEnabled(features::kTopLevelAwait)) {
    CHECK_EQ(result.GetResultType(),
             ScriptEvaluationResult::ResultType::kException);
    return result.GetExceptionForModule();
  }

  CHECK_EQ(result.GetResultType(),
           ScriptEvaluationResult::ResultType::kSuccess);

  ScriptPromise script_promise = result.GetPromise(script_state);
  v8::Local<v8::Promise> promise = script_promise.V8Value().As<v8::Promise>();
  if (promise->State() == v8::Promise::kRejected) {
    return promise->Result();
  }

  auto* reject_function =
      MakeGarbageCollected<SaveResultFunction>(script_state);
  script_promise.Then(ExpectNotReached::Create(script_state),
                      reject_function->Bind());

  v8::MicrotasksScope::PerformCheckpoint(script_state->GetIsolate());

  return reject_function->GetResult();
}

}  // namespace blink
