// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/events/before_unload_event.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
JSEventHandler* JSEventHandler::CreateOrNull(v8::Local<v8::Value> value,
                                             HandlerType type) {
  if (!value->IsObject())
    return nullptr;

  return MakeGarbageCollected<JSEventHandler>(
      V8EventHandlerNonNull::Create(value.As<v8::Object>()), type);
}

v8::Local<v8::Value> JSEventHandler::GetEffectiveFunction(EventTarget& target) {
  v8::Local<v8::Value> v8_listener = GetListenerObject(target);
  if (!v8_listener.IsEmpty() && v8_listener->IsFunction())
    return GetBoundFunction(v8_listener.As<v8::Function>());
  return v8::Undefined(GetIsolate());
}

void JSEventHandler::SetCompiledHandler(ScriptState* incumbent_script_state,
                                        v8::Local<v8::Function> listener) {
  DCHECK(!HasCompiledHandler());

  // https://html.spec.whatwg.org/C/#getting-the-current-value-of-the-event-handler
  // Step 12: Set eventHandler's value to the result of creating a Web IDL
  // EventHandler callback function object whose object reference is function
  // and whose callback context is settings object.
  //
  // Push |script_state|'s context onto the backup incumbent settings object
  // stack because appropriate incumbent realm does not always exist when
  // content attribute gets lazily compiled. This context is the same one of the
  // relevant realm of |listener| and its event target.
  v8::Context::BackupIncumbentScope backup_incumbent_scope(
      incumbent_script_state->GetContext());
  event_handler_ = V8EventHandlerNonNull::Create(listener);
}

// https://html.spec.whatwg.org/C/#the-event-handler-processing-algorithm
void JSEventHandler::InvokeInternal(EventTarget& event_target,
                                    Event& event,
                                    v8::Local<v8::Value> js_event) {
  DCHECK(!js_event.IsEmpty());

  // Step 1. Let callback be the result of getting the current value of the
  //         event handler given eventTarget and name.
  // Step 2. If callback is null, then return.
  v8::Local<v8::Value> listener_value =
      GetListenerObject(*event.currentTarget());
  if (listener_value.IsEmpty() || listener_value->IsNull())
    return;
  DCHECK(HasCompiledHandler());

  // Step 3. Let special error event handling be true if event is an ErrorEvent
  // object, event's type is error, and event's currentTarget implements the
  // WindowOrWorkerGlobalScope mixin. Otherwise, let special error event
  // handling be false.
  const bool special_error_event_handling =
      event.IsErrorEvent() && event.type() == event_type_names::kError &&
      event.currentTarget()->IsWindowOrWorkerGlobalScope();

  // Step 4. Process the Event object event as follows:
  //   If special error event handling is true
  //     Invoke callback with five arguments, the first one having the value of
  //     event's message attribute, the second having the value of event's
  //     filename attribute, the third having the value of event's lineno
  //     attribute, the fourth having the value of event's colno attribute, the
  //     fifth having the value of event's error attribute, and with the
  //     callback this value set to event's currentTarget. Let return value be
  //     the callback's return value.
  //   Otherwise
  //     Invoke callback with one argument, the value of which is the Event
  //     object event, with the callback this value set to event's
  //     currentTarget. Let return value be the callback's return value.
  //   If an exception gets thrown by the callback, end these steps and allow
  //   the exception to propagate. (It will propagate to the DOM event dispatch
  //   logic, which will then report the exception.)
  HeapVector<ScriptValue> arguments;
  ScriptState* script_state_of_listener =
      event_handler_->CallbackRelevantScriptState();

  if (special_error_event_handling) {
    ErrorEvent* error_event = ToErrorEvent(&event);

    // The error argument should be initialized to null for dedicated workers.
    // https://html.spec.whatwg.org/C/#runtime-script-errors-2
    ScriptValue error_attribute = error_event->error(script_state_of_listener);
    if (error_attribute.IsEmpty() ||
        error_event->target()->InterfaceName() == event_target_names::kWorker) {
      error_attribute =
          ScriptValue::CreateNull(script_state_of_listener->GetIsolate());
    }
    arguments = {
        ScriptValue::From(script_state_of_listener, error_event->message()),
        ScriptValue::From(script_state_of_listener, error_event->filename()),
        ScriptValue::From(script_state_of_listener, error_event->lineno()),
        ScriptValue::From(script_state_of_listener, error_event->colno()),
        error_attribute};
  } else {
    arguments = {ScriptValue::From(script_state_of_listener, js_event)};
  }

  if (!event_handler_->IsRunnableOrThrowException(
          event.ShouldDispatchEvenWhenExecutionContextIsPaused()
              ? V8EventHandlerNonNull::IgnorePause::kIgnore
              : V8EventHandlerNonNull::IgnorePause::kDontIgnore)) {
    return;
  }
  ScriptValue result;
  if (!event_handler_
           ->InvokeWithoutRunnabilityCheck(event.currentTarget(), arguments)
           .To(&result) ||
      GetIsolate()->IsExecutionTerminating())
    return;
  v8::Local<v8::Value> v8_return_value = result.V8Value();

  // There is nothing to do if |v8_return_value| is null or undefined.
  // See Step 5. for more information.
  if (v8_return_value->IsNullOrUndefined())
    return;

  // https://heycam.github.io/webidl/#invoke-a-callback-function
  // step 13: Set completion to the result of converting callResult.[[Value]] to
  //          an IDL value of the same type as the operation's return type.
  //
  // OnBeforeUnloadEventHandler returns DOMString? while OnErrorEventHandler and
  // EventHandler return any, so converting |v8_return_value| to return type is
  // necessary only for OnBeforeUnloadEventHandler.
  String result_for_beforeunload;
  if (IsOnBeforeUnloadEventHandler()) {
    event_handler_->EvaluateAsPartOfCallback(Bind(
        [](v8::Local<v8::Value>& v8_return_value,
           String& result_for_beforeunload) {
          // TODO(yukiy): use |NativeValueTraits|.
          V8StringResource<kTreatNullAsNullString> native_result(
              v8_return_value);

          // |native_result.Prepare()| throws exception if it fails to convert
          // |native_result| to String.
          if (!native_result.Prepare())
            return;
          result_for_beforeunload = native_result;
        },
        std::ref(v8_return_value), std::ref(result_for_beforeunload)));
    if (!result_for_beforeunload)
      return;
  }

  // Step 5. Process return value as follows:
  //   If event is a BeforeUnloadEvent object and event's type is beforeunload
  //     If return value is not null, then:
  //       1. Set event's canceled flag.
  //       2. If event's returnValue attribute's value is the empty string, then
  //          set event's returnValue attribute's value to return value.
  //   If special error event handling is true
  //     If return value is true, then set event's canceled flag.
  //   Otherwise
  //     If return value is false, then set event's canceled flag.
  //       Note: If we've gotten to this "Otherwise" clause because event's type
  //             is beforeunload but event is not a BeforeUnloadEvent object,
  //             then return value will never be false, since in such cases
  //             return value will have been coerced into either null or a
  //             DOMString.
  const bool is_beforeunload_event =
      event.IsBeforeUnloadEvent() &&
      event.type() == event_type_names::kBeforeunload;
  if (is_beforeunload_event) {
    if (result_for_beforeunload) {
      event.preventDefault();
      BeforeUnloadEvent* before_unload_event = ToBeforeUnloadEvent(&event);
      if (before_unload_event->returnValue().IsEmpty())
        before_unload_event->setReturnValue(result_for_beforeunload);
    }
  } else if (!IsOnBeforeUnloadEventHandler()) {
    if (special_error_event_handling && v8_return_value->IsBoolean() &&
        v8_return_value.As<v8::Boolean>()->Value())
      event.preventDefault();
    else if (!special_error_event_handling && v8_return_value->IsBoolean() &&
             !v8_return_value.As<v8::Boolean>()->Value())
      event.preventDefault();
  }
}

void JSEventHandler::Trace(blink::Visitor* visitor) {
  visitor->Trace(event_handler_);
  JSBasedEventListener::Trace(visitor);
}

}  // namespace blink
