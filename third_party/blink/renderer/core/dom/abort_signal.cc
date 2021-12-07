// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/abort_signal.h"

#include <utility>

#include "base/callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
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
  ScriptValue reason(
      script_state->GetIsolate(),
      V8ThrowDOMException::CreateOrEmpty(script_state->GetIsolate(),
                                         DOMExceptionCode::kAbortError,
                                         "signal is aborted without reason"));
  return abort(script_state, reason);
}

AbortSignal* AbortSignal::abort(ScriptState* script_state, ScriptValue reason) {
  DCHECK(!reason.IsEmpty());
  AbortSignal* signal =
      MakeGarbageCollected<AbortSignal>(ExecutionContext::From(script_state));
  signal->abort_reason_ = reason;
  return signal;
}

ScriptValue AbortSignal::reason(ScriptState* script_state) {
  DCHECK(script_state->GetIsolate()->InContext());
  if (abort_reason_.IsEmpty()) {
    return ScriptValue(script_state->GetIsolate(),
                       v8::Undefined(script_state->GetIsolate()));
  }
  return abort_reason_;
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
  ScriptValue reason(
      script_state->GetIsolate(),
      V8ThrowDOMException::CreateOrEmpty(script_state->GetIsolate(),
                                         DOMExceptionCode::kAbortError,
                                         "signal is aborted without reason"));
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
