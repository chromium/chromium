// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/underlying_source_base.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8.h"

namespace blink {

ScriptPromise<IDLUndefined> UnderlyingSourceBase::StartWrapper(
    ScriptState* script_state,
    ReadableStreamDefaultController* controller) {
  // Cannot call start twice (e.g., cannot use the same UnderlyingSourceBase to
  // construct multiple streams).
  DCHECK(!controller_);

  controller_ =
      MakeGarbageCollected<ReadableStreamDefaultControllerWithScriptScope>(
          script_state, controller);
  return Start(script_state);
}

ScriptPromise<IDLUndefined> UnderlyingSourceBase::Start(
    ScriptState* script_state) {
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> UnderlyingSourceBase::Pull(
    ScriptState* script_state,
    ExceptionState&) {
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> UnderlyingSourceBase::CancelWrapper(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  DCHECK(controller_);  // StartWrapper() must have been called
  controller_->Deactivate();
  return Cancel(script_state, reason, exception_state);
}

ScriptPromise<IDLUndefined> UnderlyingSourceBase::Cancel(
    ScriptState* script_state,
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

ScriptPromise<IDLUndefined> UnderlyingStartAlgorithm::Run(
    ScriptState* script_state) {
  return source_->StartWrapper(script_state, controller_.Get());
}

void UnderlyingStartAlgorithm::Trace(Visitor* visitor) const {
  StreamStartAlgorithm::Trace(visitor);
  visitor->Trace(source_);
  visitor->Trace(controller_);
}

ScriptPromise<IDLUndefined> UnderlyingPullAlgorithm::Run(
    ScriptState* script_state,
    base::span<v8::Local<v8::Value>> argv) {
  DCHECK_EQ(argv.size(), 0u);
  return source_->Pull(script_state,
                       PassThroughException(script_state->GetIsolate()));
}

void UnderlyingPullAlgorithm::Trace(Visitor* visitor) const {
  StreamAlgorithm::Trace(visitor);
  visitor->Trace(source_);
}

ScriptPromise<IDLUndefined> UnderlyingCancelAlgorithm::Run(
    ScriptState* script_state,
    base::span<v8::Local<v8::Value>> argv) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> reason =
      !argv.empty() ? argv[0] : v8::Undefined(isolate).As<v8::Value>();
  return source_->CancelWrapper(
      script_state, ScriptValue(isolate, reason),
      PassThroughException(script_state->GetIsolate()));
}

void UnderlyingCancelAlgorithm::Trace(Visitor* visitor) const {
  StreamAlgorithm::Trace(visitor);
  visitor->Trace(source_);
}

}  // namespace blink
