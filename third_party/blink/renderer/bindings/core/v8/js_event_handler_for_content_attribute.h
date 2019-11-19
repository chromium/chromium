// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_JS_EVENT_HANDLER_FOR_CONTENT_ATTRIBUTE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_JS_EVENT_HANDLER_FOR_CONTENT_ATTRIBUTE_H_

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

namespace blink {

// |JSEventHandlerForContentAttribute| supports lazy compilation for content
// attribute. This performs in the same way as |JSEventHandler| after it gets
// compiled.
class JSEventHandlerForContentAttribute final : public JSEventHandler {
 public:
  JSEventHandlerForContentAttribute(
      v8::Isolate* isolate,
      DOMWrapperWorld& world,
      const AtomicString& function_name,
      const String& script_body,
      const String& source_url,
      const TextPosition& position,
      HandlerType type = HandlerType::kEventHandler)
      : JSEventHandler(type),
        did_compile_(false),
        function_name_(function_name),
        script_body_(script_body),
        source_url_(source_url),
        position_(position),
        isolate_(isolate),
        world_(&world) {}

  // blink::EventListener overrides:
  bool IsEventHandlerForContentAttribute() const override { return true; }

  // blink::JSBasedEventListener overrides:
  v8::Local<v8::Value> GetListenerObject(EventTarget&) override;
  std::unique_ptr<SourceLocation> GetSourceLocation(EventTarget&) override;

  const String& ScriptBody() const override { return script_body_; }

 protected:
  // blink::JSBasedEventListener override:
  v8::Isolate* GetIsolate() const override { return isolate_; }
  ScriptState* GetScriptState() const override {
    DCHECK(HasCompiledHandler());
    return JSEventHandler::GetScriptState();
  }
  DOMWrapperWorld& GetWorld() const override { return *world_; }

 private:
  // Implements Step 3. of "get the current value of the event handler".
  // The compiled v8::Function is returned and |JSEventHandler::event_handler_|
  // gets initialized with it if lazy compilation succeeds.
  // Otherwise, v8::Null is returned.
  // https://html.spec.whatwg.org/C/#getting-the-current-value-of-the-event-handler
  v8::Local<v8::Value> GetCompiledHandler(EventTarget&);

  // Lazy compilation for content attribute should be tried only once, but we
  // cannot see whether it had never tried to compile or it has already failed
  // when |HasCompiledHandler()| returns false. |did_compile_| is used for
  // checking that.
  bool did_compile_;
  const AtomicString function_name_;
  String script_body_;
  String source_url_;
  TextPosition position_;
  v8::Isolate* isolate_;
  scoped_refptr<DOMWrapperWorld> world_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_JS_EVENT_HANDLER_FOR_CONTENT_ATTRIBUTE_H_
