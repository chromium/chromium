/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener_helper.h"

#include "third_party/blink/renderer/bindings/core/v8/custom_wrappable_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/js_event_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/js_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"

namespace blink {
namespace {

template <typename ListenerType, typename ListenerFactory>
ListenerType* GetEventListenerInternal(
    ScriptState* script_state,
    v8::Local<v8::Object> object,
    const V8PrivateProperty::Symbol& listener_property,
    ListenerLookupType lookup,
    const ListenerFactory& listener_factory) {
  DCHECK(script_state->GetIsolate()->InContext());
  ListenerType* listener =
      CustomWrappableAdapter::Lookup<ListenerType>(object, listener_property);
  if (listener || lookup == kListenerFindOnly)
    return listener;

  return listener_factory();
}

}  // namespace

// static
EventListener* V8EventListenerHelper::GetEventListener(
    ScriptState* script_state,
    v8::Local<v8::Value> value,
    ListenerLookupType lookup) {
  RUNTIME_CALL_TIMER_SCOPE(script_state->GetIsolate(),
                           RuntimeCallStats::CounterId::kGetEventListener);

  if (!value->IsObject())
    return nullptr;
  v8::Local<v8::Object> object = value.As<v8::Object>();
  v8::Isolate* isolate = script_state->GetIsolate();
  V8PrivateProperty::Symbol listener_property =
      V8PrivateProperty::GetCustomWrappableEventListener(isolate);

  return GetEventListenerInternal<EventListener>(
      script_state, object, listener_property, lookup,
      [object, script_state, listener_property]() {
        return static_cast<EventListener*>(
            JSEventListener::Create(script_state, object, listener_property));
      });
}

// static
EventListener* V8EventListenerHelper::GetEventHandler(
    ScriptState* script_state,
    v8::Local<v8::Value> value,
    JSEventHandler::HandlerType handler_type,
    ListenerLookupType lookup) {
  RUNTIME_CALL_TIMER_SCOPE(script_state->GetIsolate(),
                           RuntimeCallStats::CounterId::kGetEventListener);

  if (!value->IsObject())
    return nullptr;
  v8::Local<v8::Object> object = value.As<v8::Object>();
  v8::Isolate* isolate = script_state->GetIsolate();
  V8PrivateProperty::Symbol listener_property =
      V8PrivateProperty::GetCustomWrappableEventHandler(isolate);

  return GetEventListenerInternal<EventListener>(
      script_state, object, listener_property, lookup,
      [object, script_state, listener_property, handler_type]() {
        return static_cast<EventListener*>(JSEventHandler::Create(
            script_state, object, listener_property, handler_type));
      });
}

}  // namespace blink
