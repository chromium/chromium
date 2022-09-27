// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/abort_signal.h"

#include <utility>

#include "base/callback.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {

class OnceCallbackAlgorithm final : public AbortSignal::Algorithm {
 public:
  explicit OnceCallbackAlgorithm(base::OnceClosure callback)
      : callback_(std::move(callback)) {}
  ~OnceCallbackAlgorithm() override = default;

  void Run() override { std::move(callback_).Run(); }

 private:
  base::OnceClosure callback_;
};

class FollowAlgorithm final : public AbortSignal::Algorithm {
 public:
  FollowAlgorithm(ScriptState* script_state,
                  AbortSignal* parent,
                  AbortSignal* following)
      : script_state_(script_state), parent_(parent), following_(following) {}
  ~FollowAlgorithm() override = default;

  void Run() override {
    following_->SignalAbort(script_state_, parent_->reason(script_state_));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(parent_);
    visitor->Trace(following_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<ScriptState> script_state_;
  Member<AbortSignal> parent_;
  Member<AbortSignal> following_;
};

}  // namespace

AbortSignal::AbortSignal(ExecutionContext* execution_context)
    : execution_context_(execution_context) {}
AbortSignal::~AbortSignal() = default;

// static
AbortSignal* AbortSignal::abort(ScriptState* script_state) {
  v8::Local<v8::Value> dom_exception = V8ThrowDOMException::CreateOrEmpty(
      script_state->GetIsolate(), DOMExceptionCode::kAbortError,
      "signal is aborted without reason");
  CHECK(!dom_exception.IsEmpty());
  ScriptValue reason(script_state->GetIsolate(), dom_exception);
  return abort(script_state, reason);
}

// static
AbortSignal* AbortSignal::abort(ScriptState* script_state, ScriptValue reason) {
  DCHECK(!reason.IsEmpty());
  AbortSignal* signal =
      MakeGarbageCollected<AbortSignal>(ExecutionContext::From(script_state));
  signal->abort_reason_ = reason;
  return signal;
}

// static
AbortSignal* AbortSignal::timeout(ScriptState* script_state,
                                  uint64_t milliseconds) {
  ExecutionContext* context = ExecutionContext::From(script_state);
  AbortSignal* signal = MakeGarbageCollected<AbortSignal>(context);
  // The spec requires us to use the timer task source, but there are a few
  // timer task sources due to our throttling implementation. We match
  // setTimeout for immediate timeouts, but use the high-nesting task type for
  // all positive timeouts so they are eligible for throttling (i.e. no
  // nesting-level exception).
  TaskType task_type = milliseconds == 0
                           ? TaskType::kJavascriptTimerImmediate
                           : TaskType::kJavascriptTimerDelayedHighNesting;
  // `signal` needs to be held with a strong reference to keep it alive in case
  // there are or will be event handlers attached.
  context->GetTaskRunner(task_type)->PostDelayedTask(
      FROM_HERE,
      WTF::BindOnce(&AbortSignal::AbortTimeoutFired, WrapPersistent(signal),
                    WrapPersistent(script_state)),
      base::Milliseconds(milliseconds));
  return signal;
}

void AbortSignal::AbortTimeoutFired(ScriptState* script_state) {
  if (GetExecutionContext()->IsContextDestroyed() ||
      !script_state->ContextIsValid()) {
    return;
  }
  ScriptState::Scope scope(script_state);
  auto* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> reason = V8ThrowDOMException::CreateOrEmpty(
      isolate, DOMExceptionCode::kTimeoutError, "signal timed out");
  SignalAbort(script_state, ScriptValue(isolate, reason));
}

ScriptValue AbortSignal::reason(ScriptState* script_state) const {
  DCHECK(script_state->GetIsolate()->InContext());
  if (abort_reason_.IsEmpty()) {
    return ScriptValue(script_state->GetIsolate(),
                       v8::Undefined(script_state->GetIsolate()));
  }
  return abort_reason_;
}

void AbortSignal::throwIfAborted(ScriptState* script_state,
                                 ExceptionState& exception_state) const {
  if (!aborted())
    return;
  exception_state.RethrowV8Exception(reason(script_state).V8Value());
}

const AtomicString& AbortSignal::InterfaceName() const {
  return event_target_names::kAbortSignal;
}

ExecutionContext* AbortSignal::GetExecutionContext() const {
  return execution_context_.Get();
}

void AbortSignal::AddAlgorithm(Algorithm* algorithm) {
  if (aborted())
    return;

  abort_algorithms_.push_back(algorithm);
}

void AbortSignal::AddAlgorithm(base::OnceClosure algorithm) {
  if (aborted())
    return;
  abort_algorithms_.push_back(
      MakeGarbageCollected<OnceCallbackAlgorithm>(std::move(algorithm)));
}

void AbortSignal::SignalAbort(ScriptState* script_state) {
  v8::Local<v8::Value> dom_exception = V8ThrowDOMException::CreateOrEmpty(
      script_state->GetIsolate(), DOMExceptionCode::kAbortError,
      "signal is aborted without reason");
  CHECK(!dom_exception.IsEmpty());
  ScriptValue reason(script_state->GetIsolate(), dom_exception);
  SignalAbort(script_state, reason);
}

void AbortSignal::SignalAbort(ScriptState* script_state, ScriptValue reason) {
  DCHECK(!reason.IsEmpty());
  if (aborted())
    return;
  if (reason.IsUndefined()) {
    abort_reason_ = ScriptValue(
        script_state->GetIsolate(),
        V8ThrowDOMException::CreateOrEmpty(
            script_state->GetIsolate(), DOMExceptionCode::kAbortError,
            "signal is aborted with undefined reason"));
  } else {
    abort_reason_ = reason;
  }
  for (Algorithm* algorithm : abort_algorithms_) {
    algorithm->Run();
  }
  abort_algorithms_.clear();
  DispatchEvent(*Event::Create(event_type_names::kAbort));
}

void AbortSignal::Follow(ScriptState* script_state, AbortSignal* parent) {
  if (aborted())
    return;
  if (parent->aborted()) {
    SignalAbort(script_state, parent->reason(script_state));
    return;
  }

  parent->AddAlgorithm(
      MakeGarbageCollected<FollowAlgorithm>(script_state, parent, this));
}

void AbortSignal::Trace(Visitor* visitor) const {
  visitor->Trace(abort_reason_);
  visitor->Trace(execution_context_);
  visitor->Trace(abort_algorithms_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
