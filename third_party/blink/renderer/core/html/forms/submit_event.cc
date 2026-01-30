// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/submit_event.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_submit_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

SubmitEvent::SubmitEvent(const AtomicString& type,
                         const SubmitEventInit* event_init)
    : Event(type, event_init),
      submitter_(event_init ? event_init->submitter() : nullptr),
      agent_invoked_(event_init && event_init->agentInvoked()) {}

SubmitEvent* SubmitEvent::Create(const AtomicString& type,
                                 const SubmitEventInit* event_init) {
  return MakeGarbageCollected<SubmitEvent>(type, event_init);
}

void SubmitEvent::Trace(Visitor* visitor) const {
  visitor->Trace(submitter_);
  visitor->Trace(respond_with_promise_);
  visitor->Trace(respond_with_script_state_);
  Event::Trace(visitor);
}

const AtomicString& SubmitEvent::InterfaceName() const {
  return event_interface_names::kSubmitEvent;
}

void SubmitEvent::respondWith(ScriptState* script_state,
                              ScriptPromise<IDLAny> script_promise,
                              ExceptionState& exception_state) {
  if (!agent_invoked_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "You can only call respondWith on an submit event that has "
        "agentInvoked == true.");
    return;
  }
  if (!defaultPrevented()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "To call respondWith, you must first call preventDefault.");
    return;
  }
  if (!IsBeingDispatched()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "To call respondWith, the event must be currently being "
        "dispatched.");
    return;
  }
  respond_with_promise_ = std::move(script_promise);
  respond_with_script_state_ = script_state;
}

std::optional<SubmitEvent::PromiseResult>
SubmitEvent::TakeRespondWithPromise() {
  if (respond_with_promise_.IsEmpty() || !respond_with_script_state_ ||
      !respond_with_script_state_->ContextIsValid()) {
    return std::nullopt;
  }
  ScriptState::Scope unwrap_scope(respond_with_script_state_);
  PromiseResult result(std::move(respond_with_script_state_),
                       respond_with_promise_.Unwrap());
  respond_with_promise_.Clear();
  respond_with_script_state_ = nullptr;
  return result;
}

}  // namespace blink
