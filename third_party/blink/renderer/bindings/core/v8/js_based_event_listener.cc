// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/js_based_event_listener.h"

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"

namespace blink {

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
      IsA<LocalDOMWindow>(execution_context)) {
    Document* document = To<LocalDOMWindow>(execution_context)->document();
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

  probe::InvokeEventHandler probe_scope(script_state_of_listener, event, this);
  ScriptState::Scope listener_script_state_scope(script_state_of_listener);

  // https://dom.spec.whatwg.org/#firing-events
  // Step 2. of firing events: Let event be the result of creating an event
  // given eventConstructor, in the relevant Realm of target.
  //
  // |js_event|, a V8 wrapper object for |event|, must be created in the
  // relevant realm of the event target. The world must match the event
  // listener's world.
  ScriptState* script_state_of_event_target =
      ToScriptState(execution_context_of_event_target, GetWorld());
  if (!script_state_of_event_target) {
    return;
  }
  DCHECK_EQ(script_state_of_event_target->World().GetWorldId(),
            GetWorld().GetWorldId());

  // Step 6: Let |global| be listener callback’s associated Realm’s global
  // object.
  LocalDOMWindow* window = ToLocalDOMWindow(script_state_of_listener);

  // Check if the current context, which is set to the listener's relevant
  // context by creating |listener_script_state_scope|, has access to the
  // event target's relevant context before creating |js_event|. SecurityError
  // is thrown if it doesn't have access.
  if (!BindingSecurity::ShouldAllowAccessToV8Context(
          script_state_of_listener, script_state_of_event_target)) {
    LocalDOMWindow* target_window =
        DynamicTo<LocalDOMWindow>(execution_context_of_event_target);
    if (window && target_window) {
      window->PrintErrorMessage(target_window->CrossDomainAccessErrorMessage(
          window, DOMWindow::CrossDocumentAccessPolicy::kDisallowed));
    }
    return;
  }

  v8::Local<v8::Value> js_event =
      ToV8Traits<Event>::ToV8(script_state_of_event_target, event);

  // Step 7: Let |current_event| be undefined.
  Event* current_event = nullptr;

  // Step 8: If |global| is a Window object, then:
  if (window) {
    // Step 8-1: Set |current_event| to |global|’s current event.
    current_event = window->CurrentEvent();

    // Step 8-2: If |struct|’s invocation-target-in-shadow-tree is false (i.e.,
    // event's target is in a shadow tree), then set |global|’s current
    // event to event.
    Node* target_node = event->target()->ToNode();
    if (!(target_node && target_node->IsInShadowTree()))
      window->SetCurrentEvent(event);
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
  }

  // Step 12: If |global| is a Window object, then set |global|’s current event
  // to |current_event|.
  if (window)
    window->SetCurrentEvent(current_event);
}

std::unique_ptr<SourceLocation> JSBasedEventListener::GetSourceLocation(
    EventTarget& target) {
  v8::HandleScope handle_scope(GetIsolate());
  v8::Local<v8::Value> effective_function = GetEffectiveFunction(target);
  if (effective_function->IsFunction()) {
    return CaptureSourceLocation(GetIsolate(),
                                 effective_function.As<v8::Function>());
  }
  return nullptr;
}

}  // namespace blink
