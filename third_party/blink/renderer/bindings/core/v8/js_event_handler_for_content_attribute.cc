// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"

namespace blink {

JSEventHandlerForContentAttribute* JSEventHandlerForContentAttribute::Create(
    ExecutionContext* context,
    const QualifiedName& name,
    const AtomicString& value,
    HandlerType type) {
  if (!context || !context->CanExecuteScripts(kAboutToExecuteScript))
    return nullptr;
  if (value.IsNull())
    return nullptr;
  DCHECK(IsA<LocalDOMWindow>(context));
  return MakeGarbageCollected<JSEventHandlerForContentAttribute>(context, name,
                                                                 value, type);
}

JSEventHandlerForContentAttribute::JSEventHandlerForContentAttribute(
    ExecutionContext* context,
    const QualifiedName& name,
    const AtomicString& value,
    HandlerType type)
    : JSEventHandler(type),
      did_compile_(false),
      function_name_(name.LocalName()),
      script_body_(value),
      source_url_(context->Url().GetString()),
      position_(To<LocalDOMWindow>(context)
                    ->GetScriptController()
                    .EventHandlerPosition()),
      isolate_(context->GetIsolate()) {}

v8::Local<v8::Value> JSEventHandlerForContentAttribute::GetListenerObject(
    EventTarget& event_target) {
  // Step 3. of get the current value of the event handler should be executed
  // only if EventHandler's value is an internal raw uncompiled handler and it
  // has never tried to get compiled.
  if (HasCompiledHandler())
    return JSEventHandler::GetListenerObject(event_target);
  if (did_compile_)
    return v8::Null(GetIsolate());

  return GetCompiledHandler(event_target);
}

// Implements Step 3. of "get the current value of the event handler"
// https://html.spec.whatwg.org/C/#getting-the-current-value-of-the-event-handler
v8::Local<v8::Value> JSEventHandlerForContentAttribute::GetCompiledHandler(
    EventTarget& event_target) {
  // Do not compile the same code twice.
  DCHECK(!did_compile_);
  did_compile_ = true;

  ExecutionContext* execution_context_of_event_target =
      event_target.GetExecutionContext();
  if (!execution_context_of_event_target)
    return v8::Null(GetIsolate());

  v8::Local<v8::Context> v8_context_of_event_target =
      ToV8Context(execution_context_of_event_target, GetWorld());
  if (v8_context_of_event_target.IsEmpty())
    return v8::Null(GetIsolate());

  ScriptState* script_state_of_event_target =
      ScriptState::From(GetIsolate(), v8_context_of_event_target);
  if (!script_state_of_event_target->ContextIsValid())
    return v8::Null(GetIsolate());

  // Step 1. If eventTarget is an element, then let element be eventTarget, and
  // document be element's node document. Otherwise, eventTarget is a Window
  // object, let element be null, and document be eventTarget's associated
  // Document.
  Element* element = nullptr;
  const LocalDOMWindow* window = nullptr;
  Document* document = nullptr;
  if (Node* node = event_target.ToNode()) {
    if (node->IsDocumentNode()) {
      // Some of content attributes for |HTMLBodyElement| are treated as ones
      // for |Document| unlike the definition in HTML standard.  Those content
      // attributes are not listed in the Window-reflecting body element event
      // handler set.
      // https://html.spec.whatwg.org/C/#window-reflecting-body-element-event-handler-set
      document = &node->GetDocument();
    } else {
      element = To<Element>(node);
      document = &node->GetDocument();
    }
    // EventTarget::GetExecutionContext() sometimes returns the document which
    // created the EventTarget, and sometimes returns the document to which
    // the EventTarget is currently attached.  The former might be different
    // from |document|.
  } else {
    window = event_target.ToLocalDOMWindow();
    DCHECK(window);
    DCHECK_EQ(window, execution_context_of_event_target);
    document = window->document();
  }
  DCHECK(document);

  // Step 6. Let settings object be the relevant settings object of document.
  // Step 9. Push settings object's realm execution context onto the JavaScript
  // execution context stack; it is now the running JavaScript execution
  // context.
  //
  // |document->AllowInlineEventHandler()| checks the world of current context,
  // so this scope needs to be defined before calling it.
  v8::Context::Scope event_target_context_scope(v8_context_of_event_target);

  // Step 2. If scripting is disabled for document, then return null.
  if (!document->AllowInlineEventHandler(element, this, source_url_,
                                         position_.line_))
    return v8::Null(GetIsolate());

  // Step 5. If element is not null and element has a form owner, let form owner
  // be that form owner. Otherwise, let form owner be null.
  HTMLFormElement* form_owner = nullptr;
  if (auto* html_element = DynamicTo<HTMLElement>(element)) {
    form_owner = html_element->formOwner();

    // https://html.spec.whatwg.org/C/#window-reflecting-body-element-event-handler-set
    // The Event handlers on HTMLBodyElement and HTMLFrameSetElement which are
    // listed in the Window-reflecting body element event handler set should be
    // treated as if they are the corresponding event handlers of the window
    // object.
    if (html_element->IsHTMLBodyElement() ||
        html_element->IsHTMLFrameSetElement()) {
      window = To<LocalDOMWindow>(execution_context_of_event_target);
    }
  }

  // Step 10. Let function be the result of calling FunctionCreate, with
  // arguments:
  //   kind
  //     Normal
  //   ParameterList
  //     If eventHandler is an onerror event handler of a Window object
  //       Let the function have five arguments, named event, source, lineno,
  //       colno, and error.
  //     Otherwise
  //       Let the function have a single argument called event.
  //   Body
  //     The result of parsing body above.
  //   Scope
  //     1. If eventHandler is an element's event handler, then let Scope be
  //        NewObjectEnvironment(document, the global environment). Otherwise,
  //        eventHandler is a Window object's event handler: let Scope be the
  //        global environment.
  //     2. If form owner is not null, let Scope be NewObjectEnvironment(form
  //        owner, Scope).
  //     3. If element is not null, let Scope be NewObjectEnvironment(element,
  //        Scope).
  //   Strict
  //     The value of strict.
  //
  // Note: Strict is set by V8.
  v8::Isolate* isolate = script_state_of_event_target->GetIsolate();
  v8::Local<v8::String> parameter_list[5];
  size_t parameter_list_size = 0;
  if (IsOnErrorEventHandler() && window) {
    // SVG requires to introduce evt as an alias to event in event handlers.
    // See ANNOTATION 3: https://www.w3.org/TR/SVG/interact.html#SVGEvents
    parameter_list[parameter_list_size++] =
        V8String(isolate, element && element->IsSVGElement() ? "evt" : "event");
    parameter_list[parameter_list_size++] = V8String(isolate, "source");
    parameter_list[parameter_list_size++] = V8String(isolate, "lineno");
    parameter_list[parameter_list_size++] = V8String(isolate, "colno");
    parameter_list[parameter_list_size++] = V8String(isolate, "error");
  } else {
    // SVG requires to introduce evt as an alias to event in event handlers.
    // See ANNOTATION 3: https://www.w3.org/TR/SVG/interact.html#SVGEvents
    parameter_list[parameter_list_size++] =
        V8String(isolate, element && element->IsSVGElement() ? "evt" : "event");
  }
  DCHECK_LE(parameter_list_size, std::size(parameter_list));

  v8::Local<v8::Object> scopes[3];
  size_t scopes_size = 0;
  if (element) {
    scopes[scopes_size++] =
        ToV8Traits<Document>::ToV8(script_state_of_event_target, document)
            .As<v8::Object>();
  }
  if (form_owner) {
    scopes[scopes_size++] = ToV8Traits<HTMLFormElement>::ToV8(
                                script_state_of_event_target, form_owner)
                                .As<v8::Object>();
  }
  if (element) {
    scopes[scopes_size++] =
        ToV8Traits<Element>::ToV8(script_state_of_event_target, element)
            .As<v8::Object>();
  }
  DCHECK_LE(scopes_size, std::size(scopes));

  v8::ScriptOrigin origin(
      V8String(isolate, source_url_), position_.line_.ZeroBasedInt(),
      position_.column_.ZeroBasedInt(),
      true);  // true as |SanitizeScriptErrors::kDoNotSanitize|
  v8::ScriptCompiler::Source source(V8String(isolate, script_body_), origin);

  v8::Local<v8::Function> compiled_function;
  {
    v8::TryCatch block(isolate);
    block.SetVerbose(true);
    v8::MaybeLocal<v8::Function> maybe_result =
        v8::ScriptCompiler::CompileFunction(v8_context_of_event_target, &source,
                                            parameter_list_size, parameter_list,
                                            scopes_size, scopes);

    // Step 7. If body is not parsable as FunctionBody or if parsing detects an
    // early error, then follow these substeps:
    //   1. Set eventHandler's value to null.
    //   2. Report the error for the appropriate script and with the appropriate
    //      position (line number and column number) given by location, using
    //      settings object's global object. If the error is still not handled
    //      after this, then the error may be reported to a developer console.
    //   3. Return null.
    if (!maybe_result.ToLocal(&compiled_function))
      return v8::Null(isolate);
  }

  // Step 12. Set eventHandler's value to the result of creating a Web IDL
  // EventHandler callback function object whose object reference is function
  // and whose callback context is settings object.
  compiled_function->SetName(V8String(isolate, function_name_));
  SetCompiledHandler(script_state_of_event_target, compiled_function);

  return JSEventHandler::GetListenerObject(event_target);
}

std::unique_ptr<SourceLocation>
JSEventHandlerForContentAttribute::GetSourceLocation(EventTarget& target) {
  auto source_location = JSEventHandler::GetSourceLocation(target);
  if (source_location)
    return source_location;
  // Fallback to uncompiled source info.
  return std::make_unique<SourceLocation>(
      source_url_, String(), position_.line_.ZeroBasedInt(),
      position_.column_.ZeroBasedInt(), nullptr);
}

}  // namespace blink
