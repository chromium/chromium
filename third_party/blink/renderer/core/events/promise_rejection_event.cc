// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/promise_rejection_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_promise_rejection_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

PromiseRejectionEvent::PromiseRejectionEvent(
    ScriptState* script_state,
    const AtomicString& type,
    const PromiseRejectionEventInit* initializer)
    : Event(type, initializer), world_(&script_state->World()) {
  DCHECK(initializer->hasPromise());
  promise_.Reset(script_state->GetIsolate(),
                 initializer->promise().V8Promise());
  if (initializer->hasReason()) {
    reason_.Reset(script_state->GetIsolate(), initializer->reason().V8Value());
  }
}

PromiseRejectionEvent::~PromiseRejectionEvent() = default;

ScriptPromise<IDLAny> PromiseRejectionEvent::promise(
    ScriptState* script_state) const {
  // Return null when the promise is accessed by a different world than the
  // world that created the promise.
  if (!CanBeDispatchedInWorld(script_state->World())) {
    return EmptyPromise();
  }
  return ScriptPromise<IDLAny>::FromV8Promise(
      script_state->GetIsolate(), promise_.Get(script_state->GetIsolate()));
}

ScriptValue PromiseRejectionEvent::reason(ScriptState* script_state) const {
  // Return undefined when the value is accessed by a different world than the
  // world that created the value.
  if (reason_.IsEmpty() || !CanBeDispatchedInWorld(script_state->World())) {
    return ScriptValue(script_state->GetIsolate(),
                       v8::Undefined(script_state->GetIsolate()));
  }
  return ScriptValue(script_state->GetIsolate(),
                     reason_.Get(script_state->GetIsolate()));
}

const AtomicString& PromiseRejectionEvent::InterfaceName() const {
  return event_interface_names::kPromiseRejectionEvent;
}

bool PromiseRejectionEvent::CanBeDispatchedInWorld(
    const DOMWrapperWorld& world) const {
  DCHECK(world_);
  return world_->GetWorldId() == world.GetWorldId();
}

void PromiseRejectionEvent::Trace(Visitor* visitor) const {
  visitor->Trace(promise_);
  visitor->Trace(reason_);
  visitor->Trace(world_);
  Event::Trace(visitor);
}

}  // namespace blink
