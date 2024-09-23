// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ScriptEvaluationResult::ScriptEvaluationResult(
    mojom::blink::ScriptType script_type,
    ResultType result_type,
    v8::Local<v8::Value> value)
    :
#if DCHECK_IS_ON()
      script_type_(script_type),
#endif
      result_type_(result_type),
      value_(value) {
}

// static
ScriptEvaluationResult ScriptEvaluationResult::FromClassicNotRun() {
  return ScriptEvaluationResult(mojom::blink::ScriptType::kClassic,
                                ResultType::kNotRun, {});
}

// static
ScriptEvaluationResult ScriptEvaluationResult::FromModuleNotRun() {
  return ScriptEvaluationResult(mojom::blink::ScriptType::kModule,
                                ResultType::kNotRun, {});
}

// static
ScriptEvaluationResult ScriptEvaluationResult::FromClassicSuccess(
    v8::Local<v8::Value> value) {
  DCHECK(!value.IsEmpty());
  return ScriptEvaluationResult(mojom::blink::ScriptType::kClassic,
                                ResultType::kSuccess, value);
}

// static
ScriptEvaluationResult ScriptEvaluationResult::FromModuleSuccess(
    v8::Local<v8::Value> value) {
  DCHECK(!value.IsEmpty());
  DCHECK(value->IsPromise());

  return ScriptEvaluationResult(mojom::blink::ScriptType::kModule,
                                ResultType::kSuccess, value);
}

// static
ScriptEvaluationResult ScriptEvaluationResult::FromClassicExceptionRethrown() {
  return ScriptEvaluationResult(mojom::blink::ScriptType::kClassic,
                                ResultType::kException, {});
}

// static
ScriptEvaluationResult ScriptEvaluationResult::FromClassicException(
    v8::Local<v8::Value> exception) {
  DCHECK(!exception.IsEmpty());
  return ScriptEvaluationResult(mojom::blink::ScriptType::kClassic,
                                ResultType::kException, exception);
}

// static
ScriptEvaluationResult ScriptEvaluationResult::FromModuleException(
    v8::Local<v8::Value> exception) {
  DCHECK(!exception.IsEmpty());
  return ScriptEvaluationResult(mojom::blink::ScriptType::kModule,
                                ResultType::kException, exception);
}

// static
ScriptEvaluationResult ScriptEvaluationResult::FromClassicAborted() {
  return ScriptEvaluationResult(mojom::blink::ScriptType::kClassic,
                                ResultType::kAborted, {});
}

// static
ScriptEvaluationResult ScriptEvaluationResult::FromModuleAborted() {
  return ScriptEvaluationResult(mojom::blink::ScriptType::kModule,
                                ResultType::kAborted, {});
}

v8::Local<v8::Value> ScriptEvaluationResult::GetSuccessValue() const {
  DCHECK_EQ(result_type_, ResultType::kSuccess);
  DCHECK(!value_.IsEmpty());
  return value_;
}

v8::Local<v8::Value> ScriptEvaluationResult::GetSuccessValueOrEmpty() const {
  if (GetResultType() == ResultType::kSuccess)
    return GetSuccessValue();
  return v8::Local<v8::Value>();
}

v8::Local<v8::Value> ScriptEvaluationResult::GetExceptionForModule() const {
#if DCHECK_IS_ON()
  DCHECK_EQ(script_type_, mojom::blink::ScriptType::kModule);
#endif
  DCHECK_EQ(result_type_, ResultType::kException);
  DCHECK(!value_.IsEmpty());

  return value_;
}

v8::Local<v8::Value> ScriptEvaluationResult::GetExceptionForWorklet() const {
#if DCHECK_IS_ON()
  DCHECK_EQ(script_type_, mojom::blink::ScriptType::kClassic);
#endif
  DCHECK_EQ(result_type_, ResultType::kException);
  DCHECK(!value_.IsEmpty());

  return value_;
}

v8::Local<v8::Value> ScriptEvaluationResult::GetExceptionForClassicForTesting()
    const {
  DCHECK_EQ(result_type_, ResultType::kException);
  DCHECK(!value_.IsEmpty());

  return value_;
}

ScriptPromise<IDLAny> ScriptEvaluationResult::GetPromise(
    ScriptState* script_state) const {
#if DCHECK_IS_ON()
  DCHECK_EQ(script_type_, mojom::blink::ScriptType::kModule);
#endif

  switch (result_type_) {
    case ResultType::kSuccess:
      return ScriptPromise<IDLAny>::FromV8Promise(
          script_state->GetIsolate(), GetSuccessValue().As<v8::Promise>());

    case ResultType::kException:
      return ScriptPromise<IDLAny>::Reject(script_state,
                                           GetExceptionForModule());

    case ResultType::kNotRun:
    case ResultType::kAborted:
      NOTREACHED();
  }
}

}  // namespace blink
