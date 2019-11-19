/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/v8_pop_state_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_history.h"
#include "third_party/blink/renderer/core/events/pop_state_event.h"
#include "third_party/blink/renderer/core/frame/history.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"

namespace blink {

const V8PrivateProperty::SymbolKey kPrivatePropertyState;

// Save the state value to a hidden attribute in the V8PopStateEvent, and return
// it, for convenience.
static v8::Local<v8::Value> CacheState(ScriptState* script_state,
                                       v8::Local<v8::Object> pop_state_event,
                                       v8::Local<v8::Value> state) {
  V8PrivateProperty::GetSymbol(script_state->GetIsolate(),
                               kPrivatePropertyState)
      .Set(pop_state_event, state);
  return state;
}

void V8PopStateEvent::StateAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ScriptState* script_state = ScriptState::Current(isolate);
  V8PrivateProperty::Symbol property_symbol =
      V8PrivateProperty::GetSymbol(isolate, kPrivatePropertyState);
  v8::Local<v8::Value> result;

  if (property_symbol.GetOrUndefined(info.Holder()).ToLocal(&result) &&
      !result->IsUndefined()) {
    V8SetReturnValue(info, result);
    return;
  }

  PopStateEvent* event = V8PopStateEvent::ToImpl(info.Holder());
  History* history = event->GetHistory();
  if (!history || !event->SerializedState()) {
    // If the event doesn't have serializedState(), it means that the
    // event was initialized with PopStateEventInit. In such case, we need
    // to get a v8 value for the current world from state().
    if (event->SerializedState())
      result = event->SerializedState()->Deserialize(isolate);
    else
      result = event->state(script_state).V8Value();
    if (result.IsEmpty())
      result = v8::Null(isolate);
    V8SetReturnValue(info, CacheState(script_state, info.Holder(), result));
    return;
  }

  // There's no cached value from a previous invocation, nor a state value was
  // provided by the event, but there is a history object, so first we need to
  // see if the state object has been deserialized through the history object
  // already.
  // The current history state object might've changed in the meantime, so we
  // need to take care of using the correct one, and always share the same
  // deserialization with history.state.

  bool is_same_state = history->IsSameAsCurrentState(event->SerializedState());
  if (is_same_state) {
    V8PrivateProperty::Symbol history_state =
        V8PrivateProperty::GetHistoryStateSymbol(info.GetIsolate());
    v8::Local<v8::Value> v8_history_value =
        ToV8(history, info.Holder(), isolate);
    if (v8_history_value.IsEmpty())
      return;
    v8::Local<v8::Object> v8_history = v8_history_value.As<v8::Object>();
    if (!history->stateChanged() && history_state.HasValue(v8_history)) {
      v8::Local<v8::Value> value;
      if (!history_state.GetOrUndefined(v8_history).ToLocal(&value))
        return;
      V8SetReturnValue(info, CacheState(script_state, info.Holder(), value));
      return;
    }
    result = event->SerializedState()->Deserialize(isolate);
    history_state.Set(v8_history, result);
  } else {
    result = event->SerializedState()->Deserialize(isolate);
  }

  V8SetReturnValue(info, CacheState(script_state, info.Holder(), result));
}

}  // namespace blink
