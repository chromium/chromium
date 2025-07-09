// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_target_names.h"
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
      IsA<ErrorEvent>(event) && event.type() == event_type_names::kError &&
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
  v8::Isolate* isolate = script_state_of_listener->GetIsolate();

  if (special_error_event_handling) {
    auto* error_event = To<ErrorEvent>(&event);

    // The error argument should be initialized to null for dedicated workers.
    // https://html.spec.whatwg.org/C/#runtime-script-errors-2
    ScriptValue error_attribute = error_event->error(script_state_of_listener);
    if (error_attribute.IsEmpty() ||
        error_event->target()->InterfaceName() == event_target_names::kWorker) {
      error_attribute = ScriptValue::CreateNull(isolate);
    }
    arguments = {
        ScriptValue(isolate,
                    ToV8Traits<IDLString>::ToV8(script_state_of_listener,
                                                error_event->message())),
        ScriptValue(isolate,
                    ToV8Traits<IDLString>::ToV8(script_state_of_listener,
                                                error_event->filename())),
        ScriptValue(isolate,
                    ToV8Traits<IDLUnsignedLong>::ToV8(script_state_of_listener,
                                                      error_event->lineno())),
        ScriptValue(isolate,
                    ToV8Traits<IDLUnsignedLong>::ToV8(script_state_of_listener,
                                                      error_event->colno())),
        error_attribute};
  } else {
    arguments.push_back(ScriptValue(isolate, js_event));
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
      isolate->IsExecutionTerminating())
    return;
  v8::Local<v8::Value> v8_return_value = result.V8Value();

  // There is nothing to do if |v8_return_value| is null or undefined.
  // See Step 5. for more information.
  if (v8_return_value->IsNullOrUndefined())
    return;

  // https://webidl.spec.whatwg.org/#invoke-a-callback-function
  // step 13: Set completion to the result of converting callResult.[[Value]] to
  //          an IDL value of the same type as the operation's return type.
  //
  // OnBeforeUnloadEventHandler returns DOMString? while OnErrorEventHandler and
  // EventHandler return any, so converting |v8_return_value| to return type is
  // necessary only for OnBeforeUnloadEventHandler.
  String result_for_beforeunload;
  if (IsOnBeforeUnloadEventHandler()) {
    event_handler_->EvaluateAsPartOfCallback(WTF::BindOnce(
        [](v8::Local<v8::Value>& v8_return_value,
           String& result_for_beforeunload, ScriptState* script_state) {
          v8::Isolate* isolate = script_state->GetIsolate();
          v8::TryCatch try_catch(isolate);
          String result =
              NativeValueTraits<IDLNullable<IDLString>>::NativeValue(
                  isolate, v8_return_value, PassThroughException(isolate));
          if (try_catch.HasCaught()) [[unlikely]] {
            // TODO(crbug.com/1480485): Understand why we need to explicitly
            // report the exception. The TryCatch handler that is on the call
            // stack has setVerbose(true) but doesn't end up dispatching an
            // ErrorEvent.
            V8ScriptRunner::ReportException(isolate, try_catch.Exception());
            return;
          }
          result_for_beforeunload = result;
        },
        std::ref(v8_return_value), std::ref(result_for_beforeunload)));
    if (!result_for_beforeunload) {
      return;
    }
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
  auto* before_unload_event = DynamicTo<BeforeUnloadEvent>(&event);
  const bool is_beforeunload_event =
      before_unload_event && event.type() == event_type_names::kBeforeunload;
  if (is_beforeunload_event) {
    if (result_for_beforeunload) {
      event.preventDefault();
      if (before_unload_event->returnValue().empty())
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

void JSEventHandler::Trace(Visitor* visitor) const {
  visitor->Trace(event_handler_);
  JSBasedEventListener::Trace(visitor);
}

}  // namespace blink
