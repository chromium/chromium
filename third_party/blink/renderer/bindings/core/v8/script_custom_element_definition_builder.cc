// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_custom_element_definition_builder.h"

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_custom_element_definition.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_constructor.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_function.h"
#include "third_party/blink/renderer/platform/bindings/callback_method_retriever.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"

namespace blink {

ScriptCustomElementDefinitionBuilder::ScriptCustomElementDefinitionBuilder(
    ScriptState* script_state,
    CustomElementRegistry* registry,
    V8CustomElementConstructor* constructor,
    ExceptionState& exception_state)
    : script_state_(script_state),
      exception_state_(exception_state),
      registry_(registry),
      constructor_(constructor) {}

bool ScriptCustomElementDefinitionBuilder::CheckConstructorIntrinsics() {
  DCHECK(script_state_->World().IsMainWorld());

  if (!constructor_->IsConstructor()) {
    exception_state_.ThrowTypeError(
        "constructor argument is not a constructor");
    return false;
  }
  return true;
}

bool ScriptCustomElementDefinitionBuilder::CheckConstructorNotRegistered() {
  if (!ScriptCustomElementDefinition::ForConstructor(
          script_state_, registry_, constructor_->CallbackObject()))
    return true;

  // Constructor is already registered.
  exception_state_.ThrowDOMException(
      DOMExceptionCode::kNotSupportedError,
      "this constructor has already been used with this registry");
  return false;
}

bool ScriptCustomElementDefinitionBuilder::RememberOriginalProperties() {
  // https://html.spec.whatwg.org/C/custom-elements.html#element-definition
  // step 10. Run the following substeps while catching any exceptions:
  CallbackMethodRetriever retriever(constructor_);

  retriever.GetPrototypeObject(exception_state_);
  if (exception_state_.HadException())
    return false;

  v8_connected_callback_ =
      retriever.GetMethodOrUndefined("connectedCallback", exception_state_);
  if (exception_state_.HadException())
    return false;
  if (v8_connected_callback_->IsFunction()) {
    connected_callback_ =
        V8Function::Create(v8_connected_callback_.As<v8::Function>());
  }
  v8_disconnected_callback_ =
      retriever.GetMethodOrUndefined("disconnectedCallback", exception_state_);
  if (exception_state_.HadException())
    return false;
  if (v8_disconnected_callback_->IsFunction()) {
    disconnected_callback_ =
        V8Function::Create(v8_disconnected_callback_.As<v8::Function>());
  }
  v8_adopted_callback_ =
      retriever.GetMethodOrUndefined("adoptedCallback", exception_state_);
  if (exception_state_.HadException())
    return false;
  if (v8_adopted_callback_->IsFunction()) {
    adopted_callback_ =
        V8Function::Create(v8_adopted_callback_.As<v8::Function>());
  }
  v8_attribute_changed_callback_ = retriever.GetMethodOrUndefined(
      "attributeChangedCallback", exception_state_);
  if (exception_state_.HadException())
    return false;
  if (v8_attribute_changed_callback_->IsFunction()) {
    attribute_changed_callback_ =
        V8Function::Create(v8_attribute_changed_callback_.As<v8::Function>());
  }

  // step 10.6. If the value of the entry in lifecycleCallbacks with key
  //   "attributeChangedCallback" is not null, then:
  if (attribute_changed_callback_) {
    v8::Isolate* isolate = script_state_->GetIsolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    v8::TryCatch try_catch(script_state_->GetIsolate());
    v8::Local<v8::Value> v8_observed_attributes;

    if (!constructor_->CallbackObject()
             ->Get(current_context,
                   V8AtomicString(isolate, "observedAttributes"))
             .ToLocal(&v8_observed_attributes)) {
      exception_state_.RethrowV8Exception(try_catch.Exception());
      return false;
    }

    if (v8_observed_attributes->IsUndefined())
      return true;

    const Vector<String>& observed_attrs =
        NativeValueTraits<IDLSequence<IDLString>>::NativeValue(
            isolate, v8_observed_attributes, exception_state_);
    if (exception_state_.HadException())
      return false;
    observed_attributes_.ReserveCapacityForSize(observed_attrs.size());
    for (const auto& attribute : observed_attrs)
      observed_attributes_.insert(AtomicString(attribute));
  }

  return true;
}

CustomElementDefinition* ScriptCustomElementDefinitionBuilder::Build(
    const CustomElementDescriptor& descriptor,
    CustomElementDefinition::Id id) {
  return ScriptCustomElementDefinition::Create(
      script_state_, registry_, descriptor, id, constructor_,
      connected_callback_, disconnected_callback_, adopted_callback_,
      attribute_changed_callback_, std::move(observed_attributes_));
}

}  // namespace blink
