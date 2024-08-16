// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_html_constructor.h"

#include "third_party/blink/renderer/bindings/core/v8/script_custom_element_definition.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_construction_stack.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_registry.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

// https://html.spec.whatwg.org/C/#html-element-constructors
void V8HTMLConstructor::HtmlConstructor(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const WrapperTypeInfo& wrapper_type_info,
    const HTMLElementType element_interface_name) {
  TRACE_EVENT0("blink", "HTMLConstructor");
  DCHECK(info.IsConstructCall());

  v8::Isolate* isolate = info.GetIsolate();
  ScriptState* script_state = ScriptState::ForCurrentRealm(isolate);
  v8::Local<v8::Value> new_target = info.NewTarget();

  if (!script_state->ContextIsValid()) {
    V8ThrowException::ThrowError(isolate, "The context has been destroyed");
    return;
  }

  if (!script_state->World().IsMainWorld()) {
    V8ThrowException::ThrowTypeError(isolate, "Illegal constructor");
    return;
  }

  // 2. If NewTarget is equal to the active function object, then
  // throw a TypeError and abort these steps.
  v8::Local<v8::Function> active_function_object =
      script_state->PerContextData()->ConstructorForType(&wrapper_type_info);
  if (new_target == active_function_object) {
    V8ThrowException::ThrowTypeError(isolate, "Illegal constructor");
    return;
  }

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);

  // 3. Let definition be the entry in registry with constructor equal to
  // NewTarget.
  // If there is no such definition, then throw a TypeError and abort these
  // steps.
  v8::Local<v8::Object> constructor = new_target.As<v8::Object>();
  CustomElementDefinition* definition = nullptr;
  if (RuntimeEnabledFeatures::ScopedCustomElementRegistryEnabled()) {
    // For scoped registries, we first check the construction stack for
    // definition in a scoped registry.
    CustomElementConstructionStack* construction_stack =
        GetCustomElementConstructionStack(window, constructor);
    if (construction_stack && construction_stack->size()) {
      definition = construction_stack->back().definition;
    }
  }
  if (!definition) {
    definition =
        window->customElements()->DefinitionForConstructor(constructor);
  }
  if (!definition) {
    V8ThrowException::ThrowTypeError(isolate, "Illegal constructor");
    return;
  }

  const AtomicString& local_name = definition->Descriptor().LocalName();
  const AtomicString& name = definition->Descriptor().GetName();

  if (local_name == name) {
    // Autonomous custom element
    // 4.1. If the active function object is not HTMLElement, then throw a
    // TypeError
    if (!V8HTMLElement::GetWrapperTypeInfo()->Equals(&wrapper_type_info)) {
      V8ThrowException::ThrowTypeError(isolate,
                                       "Illegal constructor: autonomous custom "
                                       "elements must extend HTMLElement");
      return;
    }
  } else {
    // Customized built-in element
    // 5. If local name is not valid for interface, throw TypeError
    if (HtmlElementTypeForTag(local_name, window->document()) !=
        element_interface_name) {
      V8ThrowException::ThrowTypeError(isolate,
                                       "Illegal constructor: localName does "
                                       "not match the HTML element interface");
      return;
    }
  }

  ExceptionState exception_state(isolate, v8::ExceptionContext::kConstructor,
                                 "HTMLElement");
  // 6. Let prototype be Get(NewTarget, "prototype"). Rethrow any exceptions.
  v8::Local<v8::Value> prototype;
  v8::Local<v8::String> prototype_string = V8AtomicString(isolate, "prototype");
  if (!new_target.As<v8::Object>()
           ->Get(script_state->GetContext(), prototype_string)
           .ToLocal(&prototype)) {
    return;
  }

  // 7. If Type(prototype) is not Object, then: ...
  if (!prototype->IsObject()) {
    ScriptState* new_target_script_state =
        ScriptState::ForRelevantRealm(isolate, new_target.As<v8::Object>());
    if (V8PerContextData* per_context_data =
            new_target_script_state->PerContextData()) {
      prototype = per_context_data->PrototypeForType(&wrapper_type_info);
    } else {
      V8ThrowException::ThrowError(isolate, "The context has been destroyed");
      return;
    }
  }

  // 8. If definition's construction stack is empty...
  Element* element;
  CustomElementConstructionStack* construction_stack =
      GetCustomElementConstructionStack(window, constructor);
  if (!construction_stack || construction_stack->empty()) {
    // This is an element being created with 'new' from script
    element = definition->CreateElementForConstructor(*window->document());
  } else {
    element = construction_stack->back().element;
    if (element) {
      // This is an element being upgraded that has called super
      construction_stack->back() = CustomElementConstructionStackEntry();
    } else {
      // During upgrade an element has invoked the same constructor
      // before calling 'super' and that invocation has poached the
      // element.
      exception_state.ThrowTypeError("This instance is already constructed");
      return;
    }
  }
  const WrapperTypeInfo* wrapper_type = element->GetWrapperTypeInfo();
  v8::Local<v8::Object> wrapper = V8DOMWrapper::AssociateObjectWithWrapper(
      isolate, element, wrapper_type, info.This());
  // If the element had a wrapper, we now update and return that
  // instead.
  bindings::V8SetReturnValue(info, wrapper);

  // 11. Perform element.[[SetPrototypeOf]](prototype). Rethrow any exceptions.
  // Note that SetPrototype doesn't actually return the exceptions, it just
  // returns false or Nothing on exception. See crbug.com/1197894 for an
  // example.
  v8::Maybe<bool> maybe_result = wrapper->SetPrototype(
      script_state->GetContext(), prototype.As<v8::Object>());
  bool success;
  if (!maybe_result.To(&success)) {
    // Exception has already been thrown in this case.
    return;
  }
  if (!success) {
    // Likely, Reflect.preventExtensions() has been called on the element.
    exception_state.ThrowTypeError(
        "Unable to call SetPrototype on this element");
    return;
  }
}
}  // namespace blink
