// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/js_based_event_listener.h"

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"

namespace blink {

// extern
const V8PrivateProperty::SymbolKey kPrivatePropertyGlobalEvent;

JSBasedEventListener::JSBasedEventListener() {
  if (IsMainThread()) {
    InstanceCounters::IncrementCounter(
        InstanceCounters::kJSEventListenerCounter);
  }
}

JSBasedEventListener::~JSBasedEventListener() {
  if (IsMainThread()) {
    InstanceCounters::DecrementCounter(
        InstanceCounters::kJSEventListenerCounter);
  }
}

bool JSBasedEventListener::BelongsToTheCurrentWorld(
    ExecutionContext* execution_context) const {
  v8::Isolate* isolate = GetIsolate();
  if (!isolate->GetCurrentContext().IsEmpty() &&
      &GetWorld() == &DOMWrapperWorld::Current(isolate))
    return true;
  // If currently parsing, the parser could be accessing this listener
  // outside of any v8 context; check if it belongs to the main world.
  if (!isolate->InContext() && execution_context &&
      execution_context->IsDocument()) {
    Document* document = To<Document>(execution_context);
    if (document->Parser() && document->Parser()->IsParsing())
      return GetWorld().IsMainWorld();
  }
  return false;
}

// Implements step 2. of "inner invoke".
// https://dom.spec.whatwg.org/#concept-event-listener-inner-invoke
void JSBasedEventListener::Invoke(
    ExecutionContext* execution_context_of_event_target,
    Event* event) {
  DCHECK(execution_context_of_event_target);
  DCHECK(event);
  DCHECK(event->target());
  DCHECK(event->currentTarget());

  v8::Isolate* isolate = GetIsolate();

  // Don't reenter V8 if execution was terminated in this instance of V8.
  // For example, worker can be terminated in event listener, and also window
  // can be terminated from inspector by the TerminateExecution method.
  if (isolate->IsExecutionTerminating())
    return;

  if (!event->CanBeDispatchedInWorld(GetWorld()))
    return;

  {
    v8::HandleScope handle_scope(isolate);

    // Calling |GetListenerObject()| here may cause compilation of the
    // uncompiled script body in eventHandler's value earlier than standard's
    // order, which says it should be done in step 10. There is no behavioral
    // difference but the advantage that we can use listener's |ScriptState|
    // after it get compiled.
    // https://html.spec.whatwg.org/C/#event-handler-value
    v8::Local<v8::Value> listener = GetListenerObject(*event->currentTarget());

    if (listener.IsEmpty() || !listener->IsObject())
      return;
  }

  ScriptState* script_state_of_listener = GetScriptStateOrReportError("invoke");
  if (!script_state_of_listener)
    return;  // The error is already reported.
  if (!script_state_of_listener->ContextIsValid())
    return;  // Silently fail.

  ScriptState::Scope listener_script_state_scope(script_state_of_listener);

  // https://dom.spec.whatwg.org/#firing-events
  // Step 2. of firing events: Let event be the result of creating an event
  // given eventConstructor, in the relevant Realm of target.
  //
  // |js_event|, a V8 wrapper object for |event|, must be created in the
  // relevant realm of the event target. The world must match the event
  // listener's world.
  v8::Local<v8::Context> v8_context_of_event_target =
      ToV8Context(execution_context_of_event_target, GetWorld());
  if (v8_context_of_event_target.IsEmpty())
    return;

  // Check if the current context, which is set to the listener's relevant
  // context by creating |listener_script_state_scope|, has access to the
  // event target's relevant context before creating |js_event|. SecurityError
  // is thrown if it doesn't have access.
  if (!BindingSecurity::ShouldAllowAccessToV8Context(
          script_state_of_listener->GetContext(), v8_context_of_event_target,
          BindingSecurity::ErrorReportOption::kReport)) {
    return;
  }

  v8::Local<v8::Value> js_event =
      ToV8(event, v8_context_of_event_target->Global(), isolate);
  if (js_event.IsEmpty())
    return;

  // Step 6: Let |global| be listener callback’s associated Realm’s global
  // object.
  v8::Local<v8::Object> global =
      script_state_of_listener->GetContext()->Global();

  // Step 8: If global is a Window object, then:
  // Set currentEvent to global’s current event.
  // If tuple’s item-in-shadow-tree is false, then set global’s current event to
  // event.
  V8PrivateProperty::Symbol event_symbol =
      V8PrivateProperty::GetSymbol(isolate, kPrivatePropertyGlobalEvent);
  ExecutionContext* execution_context_of_listener =
      ExecutionContext::From(script_state_of_listener);
  v8::Local<v8::Value> current_event;
  if (execution_context_of_listener->IsDocument()) {
    current_event = event_symbol.GetOrUndefined(global).ToLocalChecked();
    // Expose the event object as |window.event|, except when the event's target
    // is in a V1 shadow tree.
    Node* target_node = event->target()->ToNode();
    if (!(target_node && target_node->IsInV1ShadowTree()))
      event_symbol.Set(global, js_event);
  }

  {
    // Catch exceptions thrown in the event listener if any and report them to
    // DevTools console.
    v8::TryCatch try_catch(isolate);
    try_catch.SetVerbose(true);

    // Step 10: Call a listener with event's currentTarget as receiver and event
    // and handle errors if thrown.
    InvokeInternal(*event->currentTarget(), *event, js_event);

    if (try_catch.HasCaught()) {
      // Step 10-2: Set legacyOutputDidListenersThrowFlag if given.
      event->LegacySetDidListenersThrowFlag();
    }

    // |event_symbol.Set(global, current_event)| cannot and does not have to be
    // performed when the isolate is terminating.
    if (isolate->IsExecutionTerminating())
      return;
  }

  // Step 12: If |global| is a Window object, then set |global|’s current event
  // to |current_event|.
  if (execution_context_of_listener->IsDocument())
    event_symbol.Set(global, current_event);
}

std::unique_ptr<SourceLocation> JSBasedEventListener::GetSourceLocation(
    EventTarget& target) {
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Value> effective_function = GetEffectiveFunction(target);
  if (effective_function->IsFunction())
    return SourceLocation::FromFunction(effective_function.As<v8::Function>());
  return nullptr;
}

}  // namespace blink
