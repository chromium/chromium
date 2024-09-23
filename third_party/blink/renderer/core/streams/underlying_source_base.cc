// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/underlying_source_base.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8.h"

namespace blink {

ScriptPromiseUntyped UnderlyingSourceBase::StartWrapper(
    ScriptState* script_state,
    ReadableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  // Cannot call start twice (e.g., cannot use the same UnderlyingSourceBase to
  // construct multiple streams).
  DCHECK(!controller_);

  controller_ =
      MakeGarbageCollected<ReadableStreamDefaultControllerWithScriptScope>(
          script_state, controller);
  return Start(script_state, exception_state);
}

ScriptPromiseUntyped UnderlyingSourceBase::Start(ScriptState* script_state,
                                                 ExceptionState&) {
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromiseUntyped UnderlyingSourceBase::Pull(ScriptState* script_state,
                                                ExceptionState&) {
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromiseUntyped UnderlyingSourceBase::CancelWrapper(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  DCHECK(controller_);  // StartWrapper() must have been called
  controller_->Deactivate();
  return Cancel(script_state, reason, exception_state);
}

ScriptPromiseUntyped UnderlyingSourceBase::Cancel(ScriptState* script_state,
                                                  ScriptValue reason,
                                                  ExceptionState&) {
  return ToResolvedUndefinedPromise(script_state);
}

void UnderlyingSourceBase::ContextDestroyed() {
  // `controller_` can be unset in two cases:
  // 1. The UnderlyingSourceBase is never used to create a ReadableStream. For
  //    example, BodyStreamBuffer inherits from UnderlyingSourceBase but if an
  //    existing stream is passed to the constructor it won't create a new one.
  // 2. ContextDestroyed() is called re-entrantly during construction. This can
  //    happen when a worker is terminated.
  if (controller_)
    controller_->Deactivate();
}

void UnderlyingSourceBase::Trace(Visitor* visitor) const {
  visitor->Trace(controller_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

v8::MaybeLocal<v8::Promise> UnderlyingStartAlgorithm::Run(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return source_->StartWrapper(script_state, controller_.Get(), exception_state)
      .V8Promise();
}

void UnderlyingStartAlgorithm::Trace(Visitor* visitor) const {
  StreamStartAlgorithm::Trace(visitor);
  visitor->Trace(source_);
  visitor->Trace(controller_);
}

v8::Local<v8::Promise> UnderlyingPullAlgorithm::Run(
    ScriptState* script_state,
    int argc,
    v8::Local<v8::Value> argv[]) {
  DCHECK_EQ(argc, 0);
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kUnknown, "", "");
  return source_->Pull(script_state, exception_state).V8Promise();
}

void UnderlyingPullAlgorithm::Trace(Visitor* visitor) const {
  StreamAlgorithm::Trace(visitor);
  visitor->Trace(source_);
}

v8::Local<v8::Promise> UnderlyingCancelAlgorithm::Run(
    ScriptState* script_state,
    int argc,
    v8::Local<v8::Value> argv[]) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> reason =
      argc > 0 ? argv[0] : v8::Undefined(isolate).As<v8::Value>();
  ExceptionState exception_state(script_state->GetIsolate(),
                                 v8::ExceptionContext::kUnknown, "", "");
  return source_
      ->CancelWrapper(script_state, ScriptValue(isolate, reason),
                      exception_state)
      .V8Promise();
}

void UnderlyingCancelAlgorithm::Trace(Visitor* visitor) const {
  StreamAlgorithm::Trace(visitor);
  visitor->Trace(source_);
}

}  // namespace blink
