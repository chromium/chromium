// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/js_event_listener.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

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
void JSEventListener::InvokeInternal(EventTarget&,
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
  [[maybe_unused]] v8::Maybe<void> maybe_result =
      event_listener_->InvokeWithoutRunnabilityCheck(event.currentTarget(),
                                                     &event);
}

void JSEventListener::Trace(Visitor* visitor) const {
  visitor->Trace(event_listener_);
  JSBasedEventListener::Trace(visitor);
}

}  // namespace blink
