/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/events/custom_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_custom_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

CustomEvent::CustomEvent() = default;

CustomEvent::CustomEvent(ScriptState* script_state,
                         const AtomicString& type,
                         const CustomEventInit* initializer)
    : Event(type, initializer) {
  // TODO(crbug.com/1070964): Remove this existence check.  There is a bug that
  // the current code generator does not initialize a ScriptValue with the
  // v8::Null value despite that the dictionary member has the default value of
  // IDL null.  |hasDetail| guard is necessary here.
  if (initializer->hasDetail()) {
    v8::Local<v8::Value> detail = initializer->detail().V8Value();
    // TODO(crbug.com/1070871): Remove the following IsNullOrUndefined() check.
    // This null/undefined check fills the gap between the new and old bindings
    // code.  The new behavior is preferred in a long term, and we'll switch to
    // the new behavior once the migration to the new bindings gets settled.
    if (!detail->IsNullOrUndefined()) {
      detail_.SetAcrossWorld(script_state->GetIsolate(), detail);
    }
  }
}

CustomEvent::~CustomEvent() = default;

void CustomEvent::initCustomEvent(ScriptState* script_state,
                                  const AtomicString& type,
                                  bool bubbles,
                                  bool cancelable,
                                  const ScriptValue& script_value) {
  initEvent(type, bubbles, cancelable);
  if (!IsBeingDispatched() && !script_value.IsEmpty())
    detail_.SetAcrossWorld(script_state->GetIsolate(), script_value.V8Value());
}

ScriptValue CustomEvent::detail(ScriptState* script_state) const {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (detail_.IsEmpty())
    return ScriptValue(isolate, v8::Null(isolate));
  return ScriptValue(isolate, detail_.GetAcrossWorld(script_state));
}

const AtomicString& CustomEvent::InterfaceName() const {
  return event_interface_names::kCustomEvent;
}

void CustomEvent::Trace(Visitor* visitor) const {
  visitor->Trace(detail_);
  Event::Trace(visitor);
}

}  // namespace blink
