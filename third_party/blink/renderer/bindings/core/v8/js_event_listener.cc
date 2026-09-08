// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/js_event_listener.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"

namespace blink {

v8::Local<v8::Value> JSEventListener::GetEffectiveFunction(
    EventTarget& target) {
  v8::Isolate* isolate = GetIsolate();
  v8::Local<v8::Value> v8_listener = GetListenerObject(target);
  if (v8_listener.IsEmpty())
    return v8::Undefined(isolate);

  if (v8_listener->IsFunction())
    return GetBoundFunction(v8_listener.As<v8::Function>());

  if (v8_listener->IsObject()) {
    // Do not propagate any exceptions.
    v8::TryCatch try_catch(isolate);

    v8::Local<v8::Value> property;

    // Try the "handleEvent" method (EventListener interface).
    // v8::Object::Get() may throw if "handleEvent" is an accessor and its
    // getter throws.
    if (v8_listener.As<v8::Object>()
            ->Get(isolate->GetCurrentContext(),
                  V8AtomicString(isolate, "handleEvent"))
            .ToLocal(&property) &&
        property->IsFunction()) {
      return GetBoundFunction(property.As<v8::Function>());
    }
  }

  return v8::Undefined(isolate);
}

// https://dom.spec.whatwg.org/#concept-event-listener-inner-invoke
void JSEventListener::InvokeInternal(EventTarget& current_target,
                                     Event& event,
                                     v8::Local<v8::Value> js_event) {
  // Step 10: Call a listener with event's currentTarget as receiver and event
  // and handle errors if thrown.
  if (!event_listener_->IsRunnableOrThrowException(
          event.ShouldDispatchEvenWhenExecutionContextIsPaused()
              ? V8EventListener::IgnorePause::kIgnore
              : V8EventListener::IgnorePause::kDontIgnore)) {
    return;
  }

  // Fast path for callable function listeners (the common case, e.g.
  // addEventListener('click', (e) => { ... })).
  // This bypasses the generic CallbackInvokeHelper machinery in
  // InvokeWithoutRunnabilityCheck(), avoids looking up the "handleEvent"
  // property on the callback object, and directly reuses the existing
  // |js_event| wrapper instead of re-wrapping the event a second time.
  if (event_listener_->IsCallbackObjectCallable()) {
    ScriptState* script_state = event_listener_->CallbackRelevantScriptState();
    v8::Context::BackupIncumbentScope backup_incumbent_scope(
        event_listener_->IncumbentScriptState()->GetContext());
    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    v8::Local<v8::Function> function =
        event_listener_->CallbackObject().As<v8::Function>();
    v8::Local<v8::Value> receiver = current_target.ToV8(script_state);
    if (receiver.IsEmpty()) {
      receiver = v8::Undefined(GetIsolate());
    }
    v8::Local<v8::Value> argv[1] = {js_event};
    probe::InvokeCallback probe_scope(*script_state, "EventListener",
                                      /*callback=*/nullptr, function);
    std::ignore = V8ScriptRunner::CallFunction(function, execution_context,
                                               receiver, 1, argv, GetIsolate());
    return;
  }

  [[maybe_unused]] v8::Maybe<void> maybe_result =
      event_listener_->InvokeWithoutRunnabilityCheck(&current_target, &event);
}

void JSEventListener::Trace(Visitor* visitor) const {
  visitor->Trace(event_listener_);
  JSBasedEventListener::Trace(visitor);
}

}  // namespace blink
