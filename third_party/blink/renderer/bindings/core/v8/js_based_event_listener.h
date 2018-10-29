// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_JS_BASED_EVENT_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_JS_BASED_EVENT_LISTENER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "v8/include/v8.h"

namespace blink {

class DOMWrapperWorld;
class Event;
class EventTarget;
class SourceLocation;

// |JSBasedEventListener| is the base class for JS-based event listeners,
// i.e. EventListener and EventHandler in the standards.
// This provides the essential APIs of JS-based event listeners and also
// implements the common features.
class CORE_EXPORT JSBasedEventListener : public EventListener {
 public:
  static const JSBasedEventListener* Cast(const EventListener* listener) {
    return listener && listener->IsJSBased()
               ? static_cast<const JSBasedEventListener*>(listener)
               : nullptr;
  }

  static JSBasedEventListener* Cast(EventListener* listener) {
    return const_cast<JSBasedEventListener*>(
        Cast(const_cast<const EventListener*>(listener)));
  }

  // TODO(bindings): consider to remove this (and use GetListenerObject()
  // instead) because this method is used in mostly only generated classes.
  static v8::Local<v8::Value> GetListenerOrNull(v8::Isolate* isolate,
                                                EventTarget* event_target,
                                                EventListener* listener) {
    if (auto* v8_listener = Cast(listener))
      return v8_listener->GetListenerObject(*event_target);
    return v8::Null(isolate);
  }

  ~JSBasedEventListener() override;

  // blink::EventListener overrides:
  bool BelongsToTheCurrentWorld(ExecutionContext*) const final;
  // Implements step 2. of "inner invoke".
  // See: https://dom.spec.whatwg.org/#concept-event-listener-inner-invoke
  void handleEvent(ExecutionContext*, Event*) final;

  // |GetListenerObject()| and |GetEffectiveFunction()| may cause JS in the
  // content attribute to get compiled, potentially unsuccessfully.
  //
  // Implements "get the current value of the event handler".
  // https://html.spec.whatwg.org/multipage/webappapis.html#getting-the-current-value-of-the-event-handler
  // Returns v8::Null with firing error event instead of throwing an exception
  // on failing to compile the uncompiled script body in eventHandler's value.
  // Also, this can return empty because of crbug.com/881688 .
  virtual v8::Local<v8::Value> GetListenerObject(EventTarget&) = 0;

  // Returns v8::Function that handles invoked event or v8::Undefined without
  // throwing any exception.
  virtual v8::Local<v8::Value> GetEffectiveFunction(EventTarget&) = 0;

  // Only DevTools is allowed to use this method.
  DOMWrapperWorld& GetWorldForInspector() const { return GetWorld(); }

  virtual std::unique_ptr<SourceLocation> GetSourceLocation(EventTarget&);

 protected:
  explicit JSBasedEventListener(ListenerType);
  virtual v8::Isolate* GetIsolate() const = 0;
  virtual ScriptState* GetScriptState() const = 0;
  virtual DOMWrapperWorld& GetWorld() const = 0;

 private:
  // Performs "call a user object's operation", required in "inner-invoke".
  // "The event handler processing algorithm" corresponds to this in the case of
  // EventHandler.
  // This may throw an exception on invoking the listener.
  // See step 2-10:
  // https://dom.spec.whatwg.org/#concept-event-listener-inner-invoke
  virtual void CallListenerFunction(EventTarget&,
                                    Event&,
                                    v8::Local<v8::Value> js_event) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_JS_BASED_EVENT_LISTENER_H_
