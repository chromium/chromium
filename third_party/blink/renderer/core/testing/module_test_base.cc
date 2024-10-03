// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/module_test_base.h"
#include "third_party/blink/renderer/bindings/core/v8/module_record.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_fetch_options.h"

namespace blink {

v8::Local<v8::Module> ModuleTestBase::CompileModule(ScriptState* script_state,
                                                    const char* source,
                                                    const KURL& url) {
  return CompileModule(script_state, String(source), url);
}

v8::Local<v8::Module> ModuleTestBase::CompileModule(ScriptState* script_state,
                                                    String source,
                                                    const KURL& url) {
  ModuleScriptCreationParams params(
      /*source_url=*/url, /*base_url=*/url,
      ScriptSourceLocationType::kExternalFile, ModuleType::kJavaScript,
      ParkableString(source.Impl()), nullptr,
      network::mojom::ReferrerPolicy::kDefault);
  return ModuleRecord::Compile(script_state, params, ScriptFetchOptions(),
                               TextPosition::MinimumPosition());
}

class SaveResultFunction final : public ScriptFunction::Callable {
 public:
  SaveResultFunction() = default;

  v8::Local<v8::Value> GetResult() {
    EXPECT_TRUE(result_);
    EXPECT_FALSE(result_->IsEmpty());
    return result_->V8Value();
  }

  ScriptValue Call(ScriptState*, ScriptValue value) override {
    *result_ = value;
    return value;
  }

 private:
  ScriptValue* result_ = nullptr;
};

class ExpectNotReached final : public ScriptFunction::Callable {
 public:
  ExpectNotReached() = default;

  ScriptValue Call(ScriptState*, ScriptValue value) override {
    ADD_FAILURE() << "ExpectNotReached was reached";
    return value;
  }
};

v8::Local<v8::Value> ModuleTestBase::GetResult(ScriptState* script_state,
                                               ScriptEvaluationResult result) {
  CHECK_EQ(result.GetResultType(),
           ScriptEvaluationResult::ResultType::kSuccess);

  ScriptPromise<IDLAny> script_promise = result.GetPromise(script_state);
  v8::Local<v8::Promise> promise = script_promise.V8Promise();
  if (promise->State() == v8::Promise::kFulfilled) {
    return promise->Result();
  }

  auto* resolve_function = MakeGarbageCollected<SaveResultFunction>();
  result.GetPromise(script_state)
      .Then(
          MakeGarbageCollected<ScriptFunction>(script_state, resolve_function),
          MakeGarbageCollected<ScriptFunction>(
              script_state, MakeGarbageCollected<ExpectNotReached>()));

  script_state->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
      script_state->GetIsolate());

  return resolve_function->GetResult();
}

v8::Local<v8::Value> ModuleTestBase::GetException(
    ScriptState* script_state,
    ScriptEvaluationResult result) {
  CHECK_EQ(result.GetResultType(),
           ScriptEvaluationResult::ResultType::kSuccess);

  ScriptPromise<IDLAny> script_promise = result.GetPromise(script_state);
  v8::Local<v8::Promise> promise = script_promise.V8Promise();
  if (promise->State() == v8::Promise::kRejected) {
    return promise->Result();
  }

  auto* reject_function = MakeGarbageCollected<SaveResultFunction>();
  script_promise.Then(
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<ExpectNotReached>()),
      MakeGarbageCollected<ScriptFunction>(script_state, reject_function));

  script_state->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
      script_state->GetIsolate());

  return reject_function->GetResult();
}

}  // namespace blink
