// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_custom_element_definition_builder.h"

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_custom_element_definition.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_adopted_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_attribute_changed_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_constructor.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_form_associated_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_form_disabled_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_custom_element_form_state_restore_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/platform/bindings/callback_method_retriever.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

ScriptCustomElementDefinitionBuilder::ScriptCustomElementDefinitionBuilder(
    ScriptState* script_state,
    CustomElementRegistry* registry,
    V8CustomElementConstructor* constructor,
    ExceptionState& exception_state)
    : exception_state_(exception_state) {
  data_.script_state_ = script_state;
  data_.registry_ = registry;
  data_.constructor_ = constructor;
}

bool ScriptCustomElementDefinitionBuilder::CheckConstructorIntrinsics() {
  DCHECK(GetScriptState()->World().IsMainWorld());

  if (!Constructor()->IsConstructor()) {
    exception_state_.ThrowTypeError(
        "constructor argument is not a constructor");
    return false;
  }
  return true;
}

bool ScriptCustomElementDefinitionBuilder::CheckConstructorNotRegistered() {
  if (!ScriptCustomElementDefinition::ForConstructor(
          GetScriptState(), data_.registry_, Constructor()->CallbackObject()))
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
  CallbackMethodRetriever retriever(Constructor());

  retriever.GetPrototypeObject(exception_state_);
  if (exception_state_.HadException())
    return false;

  v8_connected_callback_ =
      retriever.GetMethodOrUndefined("connectedCallback", exception_state_);
  if (exception_state_.HadException())
    return false;
  if (v8_connected_callback_->IsFunction()) {
    data_.connected_callback_ =
        V8VoidFunction::Create(v8_connected_callback_.As<v8::Function>());
  }
  v8_disconnected_callback_ =
      retriever.GetMethodOrUndefined("disconnectedCallback", exception_state_);
  if (exception_state_.HadException())
    return false;
  if (v8_disconnected_callback_->IsFunction()) {
    data_.disconnected_callback_ =
        V8VoidFunction::Create(v8_disconnected_callback_.As<v8::Function>());
  }
  v8_adopted_callback_ =
      retriever.GetMethodOrUndefined("adoptedCallback", exception_state_);
  if (exception_state_.HadException())
    return false;
  if (v8_adopted_callback_->IsFunction()) {
    data_.adopted_callback_ = V8CustomElementAdoptedCallback::Create(
        v8_adopted_callback_.As<v8::Function>());
  }
  v8_attribute_changed_callback_ = retriever.GetMethodOrUndefined(
      "attributeChangedCallback", exception_state_);
  if (exception_state_.HadException())
    return false;
  if (v8_attribute_changed_callback_->IsFunction()) {
    data_.attribute_changed_callback_ =
        V8CustomElementAttributeChangedCallback::Create(
            v8_attribute_changed_callback_.As<v8::Function>());
  }

  // step 10.6. If the value of the entry in lifecycleCallbacks with key
  //   "attributeChangedCallback" is not null, then:
  if (data_.attribute_changed_callback_) {
    v8::Isolate* isolate = Isolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Value> v8_observed_attributes;

    if (!Constructor()
             ->CallbackObject()
             ->Get(current_context,
                   V8AtomicString(isolate, "observedAttributes"))
             .ToLocal(&v8_observed_attributes)) {
      exception_state_.RethrowV8Exception(try_catch.Exception());
      return false;
    }

    if (!v8_observed_attributes->IsUndefined()) {
      const Vector<String>& observed_attrs =
          NativeValueTraits<IDLSequence<IDLString>>::NativeValue(
              isolate, v8_observed_attributes, exception_state_);
      if (exception_state_.HadException())
        return false;
      data_.observed_attributes_.ReserveCapacityForSize(observed_attrs.size());
      for (const auto& attribute : observed_attrs)
        data_.observed_attributes_.insert(AtomicString(attribute));
    }
  }

  {
    auto* isolate = Isolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Value> v8_disabled_features;

    if (!Constructor()
             ->CallbackObject()
             ->Get(current_context, V8AtomicString(isolate, "disabledFeatures"))
             .ToLocal(&v8_disabled_features)) {
      exception_state_.RethrowV8Exception(try_catch.Exception());
      return false;
    }

    if (!v8_disabled_features->IsUndefined()) {
      data_.disabled_features_ =
          NativeValueTraits<IDLSequence<IDLString>>::NativeValue(
              isolate, v8_disabled_features, exception_state_);
      if (exception_state_.HadException())
        return false;
    }
  }

  {
    auto* isolate = Isolate();
    v8::Local<v8::Context> current_context = isolate->GetCurrentContext();
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Value> v8_form_associated;

    if (!Constructor()
             ->CallbackObject()
             ->Get(current_context, V8AtomicString(isolate, "formAssociated"))
             .ToLocal(&v8_form_associated)) {
      exception_state_.RethrowV8Exception(try_catch.Exception());
      return false;
    }

    if (!v8_form_associated->IsUndefined()) {
      data_.is_form_associated_ = NativeValueTraits<IDLBoolean>::NativeValue(
          isolate, v8_form_associated, exception_state_);
      if (exception_state_.HadException())
        return false;
    }
  }
  if (data_.is_form_associated_) {
    v8_form_associated_callback_ = retriever.GetMethodOrUndefined(
        "formAssociatedCallback", exception_state_);
    if (exception_state_.HadException())
      return false;
    if (v8_form_associated_callback_->IsFunction()) {
      data_.form_associated_callback_ =
          V8CustomElementFormAssociatedCallback::Create(
              v8_form_associated_callback_.As<v8::Function>());
    }

    v8_form_reset_callback_ =
        retriever.GetMethodOrUndefined("formResetCallback", exception_state_);
    if (exception_state_.HadException())
      return false;
    if (v8_form_reset_callback_->IsFunction()) {
      data_.form_reset_callback_ =
          V8VoidFunction::Create(v8_form_reset_callback_.As<v8::Function>());
    }

    v8_form_disabled_callback_ = retriever.GetMethodOrUndefined(
        "formDisabledCallback", exception_state_);
    if (exception_state_.HadException())
      return false;
    if (v8_form_disabled_callback_->IsFunction()) {
      data_.form_disabled_callback_ =
          V8CustomElementFormDisabledCallback::Create(
              v8_form_disabled_callback_.As<v8::Function>());
    }

    v8_form_state_restore_callback_ = retriever.GetMethodOrUndefined(
        "formStateRestoreCallback", exception_state_);
    if (exception_state_.HadException())
      return false;
    if (v8_form_state_restore_callback_->IsFunction()) {
      data_.form_state_restore_callback_ =
          V8CustomElementFormStateRestoreCallback::Create(
              v8_form_state_restore_callback_.As<v8::Function>());
    }
  }

  return true;
}

CustomElementDefinition* ScriptCustomElementDefinitionBuilder::Build(
    const CustomElementDescriptor& descriptor,
    CustomElementDefinition::Id id) {
  return MakeGarbageCollected<ScriptCustomElementDefinition>(data_, descriptor,
                                                             id);
}

v8::Isolate* ScriptCustomElementDefinitionBuilder::Isolate() {
  return data_.script_state_->GetIsolate();
}

}  // namespace blink
