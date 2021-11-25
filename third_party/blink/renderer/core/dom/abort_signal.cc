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

void AbortSignal::AddAlgorithm(base::OnceClosure algorithm) {
  if (aborted())
    return;
  abort_algorithms_.push_back(std::move(algorithm));
}

void AbortSignal::AddSignalAbortAlgorithm(ScriptState* script_state,
                                          AbortSignal* dependent_signal) {
  if (aborted())
    return;

  // The signal should be kept alive as long as parentSignal is allow chained
  // requests like the following:
  // controller -owns-> signal1 -owns-> signal2 -owns-> signal3 <-owns- request
  //
  // Due to lack to traced closures we pass a weak persistent but also add
  // |dependent_signal| as a dependency that is traced. We do not use
  // WrapPersistent here as this would create a root for Oilpan and unified heap
  // that leaks the |execution_context_| as there is no explicit event removing
  // the root anymore.
  abort_algorithms_.emplace_back(WTF::Bind(
      &AbortSignal::SignalAbortWithParent, WrapWeakPersistent(dependent_signal),
      WrapPersistent(script_state), WrapWeakPersistent(this)));
  dependent_signals_.push_back(dependent_signal);
}

void AbortSignal::SignalAbortWithParent(ScriptState* script_state,
                                        AbortSignal* parent_signal) {
  SignalAbort(script_state, parent_signal->reason(script_state));
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
  abort_reason_ = reason;
  for (base::OnceClosure& closure : abort_algorithms_) {
    std::move(closure).Run();
  }
  abort_algorithms_.clear();
  dependent_signals_.clear();
  DispatchEvent(*Event::Create(event_type_names::kAbort));
}

void AbortSignal::Follow(ScriptState* script_state, AbortSignal* parentSignal) {
  if (aborted())
    return;
  if (parentSignal->aborted())
    SignalAbort(script_state, parentSignal->reason(script_state));

  parentSignal->AddSignalAbortAlgorithm(script_state, this);
}

void AbortSignal::Trace(Visitor* visitor) const {
  visitor->Trace(abort_reason_);
  visitor->Trace(execution_context_);
  visitor->Trace(dependent_signals_);
  EventTargetWithInlineData::Trace(visitor);
}

}  // namespace blink
