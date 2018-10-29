// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_JS_EVENT_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_JS_EVENT_HANDLER_H_

#include "third_party/blink/renderer/bindings/core/v8/js_based_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_handler_non_null.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"

namespace blink {

// |JSEventHandler| implements EventHandler in the HTML standard.
// https://html.spec.whatwg.org/C/webappapis.html#event-handler-attributes
class CORE_EXPORT JSEventHandler : public JSBasedEventListener {
 public:
  enum class HandlerType {
    kEventHandler,
    // For kOnErrorEventHandler
    // https://html.spec.whatwg.org/C/webappapis.html#onerroreventhandler
    kOnErrorEventHandler,
    // For OnBeforeUnloadEventHandler
    // https://html.spec.whatwg.org/C/webappapis.html#onbeforeunloadeventhandler
    kOnBeforeUnloadEventHandler,
  };

  static JSEventHandler* Create(ScriptState* script_state,
                                v8::Local<v8::Object> listener,
                                const V8PrivateProperty::Symbol& property,
                                HandlerType type) {
    return new JSEventHandler(script_state, listener, property, type);
  }

  // blink::CustomWrappable overrides:
  void Trace(blink::Visitor* visitor) override;

  // blink::EventListener overrides:
  bool operator==(const EventListener& other) const override {
    return this == &other;
  }
  bool IsEventHandler() const final { return true; }

  // blink::JSBasedEventListener overrides:
  // TODO(crbug.com/881688): remove empty check for this method. This method
  // should return v8::Object or v8::Null.
  v8::Local<v8::Value> GetListenerObject(EventTarget&) override {
    return event_handler_->CallbackObject();
  }
  v8::Local<v8::Value> GetEffectiveFunction(EventTarget&) override;

 protected:
  JSEventHandler(ScriptState* script_state,
                 v8::Local<v8::Object> listener,
                 const V8PrivateProperty::Symbol& property,
                 HandlerType type)
      : JSBasedEventListener(kJSEventHandlerType),
        event_handler_(V8EventHandlerNonNull::Create(listener)),
        type_(type) {
    Attach(script_state, listener, property, this);
  }

  explicit JSEventHandler(HandlerType type)
      : JSBasedEventListener(kJSEventHandlerType), type_(type) {}

  // blink::JSBasedEventListener override:
  v8::Isolate* GetIsolate() const override {
    return event_handler_->GetIsolate();
  }
  ScriptState* GetScriptState() const override {
    return event_handler_->CallbackRelevantScriptState();
  }
  DOMWrapperWorld& GetWorld() const override {
    return event_handler_->CallbackRelevantScriptState()->World();
  }

  // Initializes |event_handler_| with |listener|. This method must be used only
  // when content attribute gets lazily compiled.
  void SetCompiledHandler(ScriptState* script_state,
                          v8::Local<v8::Function> listener,
                          const V8PrivateProperty::Symbol& property);

  bool HasCompiledHandler() const { return event_handler_; }

  // For checking special types of EventHandler.
  bool IsOnErrorEventHandler() const {
    return type_ == HandlerType::kOnErrorEventHandler;
  }
  bool IsOnBeforeUnloadEventHandler() const {
    return type_ == HandlerType::kOnBeforeUnloadEventHandler;
  }

 private:
  // blink::JSBasedEventListener override:
  // Performs "The event handler processing algorithm"
  // https://html.spec.whatwg.org/C/webappapis.html#the-event-handler-processing-algorithm
  void CallListenerFunction(EventTarget&,
                            Event&,
                            v8::Local<v8::Value> js_event) override;

  TraceWrapperMember<V8EventHandlerNonNull> event_handler_;
  const HandlerType type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_JS_EVENT_HANDLER_H_
