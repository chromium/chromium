/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/v0_custom_element_constructor_builder.h"

#include "third_party/blink/renderer/bindings/core/v8/string_or_element_creation_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_svg_element.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_registration_options.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_definition.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_descriptor.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_exception.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_processing_stack.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v0_custom_element_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

const V8PrivateProperty::SymbolKey kPrivatePropertyDocument;
const V8PrivateProperty::SymbolKey kPrivatePropertyIsInterfacePrototypeObject;
const V8PrivateProperty::SymbolKey kPrivatePropertyNamespaceURI;
const V8PrivateProperty::SymbolKey kPrivatePropertyTagName;
const V8PrivateProperty::SymbolKey kPrivatePropertyType;

static void ConstructCustomElement(const v8::FunctionCallbackInfo<v8::Value>&);

V0CustomElementConstructorBuilder::V0CustomElementConstructorBuilder(
    ScriptState* script_state,
    const ElementRegistrationOptions* options)
    : script_state_(script_state), options_(options) {
  DCHECK(script_state_->GetContext() ==
         script_state_->GetIsolate()->GetCurrentContext());
}

bool V0CustomElementConstructorBuilder::IsFeatureAllowed() const {
  return script_state_->World().IsMainWorld();
}

bool V0CustomElementConstructorBuilder::ValidateOptions(
    const AtomicString& type,
    QualifiedName& tag_name,
    ExceptionState& exception_state) {
  DCHECK(prototype_.IsEmpty());

  v8::TryCatch try_catch(script_state_->GetIsolate());

  if (!script_state_->PerContextData()) {
    // FIXME: This should generate an InvalidContext exception at a later point.
    V0CustomElementException::ThrowException(
        V0CustomElementException::kContextDestroyedCheckingPrototype, type,
        exception_state);
    try_catch.ReThrow();
    return false;
  }

  if (options_->hasPrototype()) {
    DCHECK(options_->prototype().IsObject());
    prototype_ = options_->prototype().V8Value().As<v8::Object>();
  } else {
    prototype_ = v8::Object::New(script_state_->GetIsolate());
    v8::Local<v8::Object> base_prototype =
        script_state_->PerContextData()->PrototypeForType(
            V8HTMLElement::GetWrapperTypeInfo());
    if (!base_prototype.IsEmpty()) {
      bool set_prototype;
      if (!prototype_->SetPrototype(script_state_->GetContext(), base_prototype)
               .To(&set_prototype) ||
          !set_prototype) {
        return false;
      }
    }
  }

  AtomicString namespace_uri = html_names::xhtmlNamespaceURI;
  if (HasValidPrototypeChainFor(V8SVGElement::GetWrapperTypeInfo()))
    namespace_uri = svg_names::kNamespaceURI;

  DCHECK(!try_catch.HasCaught());

  AtomicString local_name;

  if (options_->hasExtends()) {
    local_name = AtomicString(options_->extends().DeprecatedLower());

    if (!Document::IsValidName(local_name)) {
      V0CustomElementException::ThrowException(
          V0CustomElementException::kExtendsIsInvalidName, type,
          exception_state);
      try_catch.ReThrow();
      return false;
    }
    if (V0CustomElement::IsValidName(local_name)) {
      V0CustomElementException::ThrowException(
          V0CustomElementException::kExtendsIsCustomElementName, type,
          exception_state);
      try_catch.ReThrow();
      return false;
    }
  } else {
    if (namespace_uri == svg_names::kNamespaceURI) {
      V0CustomElementException::ThrowException(
          V0CustomElementException::kExtendsIsInvalidName, type,
          exception_state);
      try_catch.ReThrow();
      return false;
    }
    local_name = type;
  }

  DCHECK(!try_catch.HasCaught());
  tag_name = QualifiedName(g_null_atom, local_name, namespace_uri);
  return true;
}

V0CustomElementLifecycleCallbacks*
V0CustomElementConstructorBuilder::CreateCallbacks() {
  DCHECK(!prototype_.IsEmpty());

  v8::TryCatch exception_catcher(script_state_->GetIsolate());
  exception_catcher.SetVerbose(true);

  v8::MaybeLocal<v8::Function> created = RetrieveCallback("createdCallback");
  v8::MaybeLocal<v8::Function> attached = RetrieveCallback("attachedCallback");
  v8::MaybeLocal<v8::Function> detached = RetrieveCallback("detachedCallback");
  v8::MaybeLocal<v8::Function> attribute_changed =
      RetrieveCallback("attributeChangedCallback");

  callbacks_ = MakeGarbageCollected<V8V0CustomElementLifecycleCallbacks>(
      script_state_, prototype_, created, attached, detached,
      attribute_changed);
  return callbacks_.Get();
}

v8::MaybeLocal<v8::Function>
V0CustomElementConstructorBuilder::RetrieveCallback(const char* name) {
  v8::Local<v8::Value> value;
  if (!prototype_
           ->Get(script_state_->GetContext(),
                 V8AtomicString(script_state_->GetIsolate(), name))
           .ToLocal(&value) ||
      !value->IsFunction())
    return v8::MaybeLocal<v8::Function>();
  return v8::MaybeLocal<v8::Function>(value.As<v8::Function>());
}

bool V0CustomElementConstructorBuilder::CreateConstructor(
    Document* document,
    V0CustomElementDefinition* definition,
    ExceptionState& exception_state) {
  DCHECK(!prototype_.IsEmpty());
  DCHECK(constructor_.IsEmpty());
  DCHECK(document);

  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Context> context = script_state_->GetContext();

  if (!PrototypeIsValid(definition->Descriptor().GetType(), exception_state))
    return false;

  const V0CustomElementDescriptor& descriptor = definition->Descriptor();

  v8::Local<v8::String> v8_tag_name = V8String(isolate, descriptor.LocalName());
  v8::Local<v8::Value> v8_type;
  if (descriptor.IsTypeExtension())
    v8_type = V8String(isolate, descriptor.GetType());
  else
    v8_type = v8::Null(isolate);

  v8::Local<v8::Object> data = v8::Object::New(isolate);
  V8PrivateProperty::GetSymbol(isolate, kPrivatePropertyDocument)
      .Set(data, ToV8(document, context->Global(), isolate));
  V8PrivateProperty::GetSymbol(isolate, kPrivatePropertyNamespaceURI)
      .Set(data, V8String(isolate, descriptor.NamespaceURI()));
  V8PrivateProperty::GetSymbol(isolate, kPrivatePropertyTagName)
      .Set(data, v8_tag_name);
  V8PrivateProperty::GetSymbol(isolate, kPrivatePropertyType)
      .Set(data, v8_type);

  v8::Local<v8::FunctionTemplate> constructor_template =
      v8::FunctionTemplate::New(isolate);
  constructor_template->SetCallHandler(ConstructCustomElement, data);
  if (!constructor_template->GetFunction(context).ToLocal(&constructor_)) {
    V0CustomElementException::ThrowException(
        V0CustomElementException::kContextDestroyedRegisteringDefinition,
        definition->Descriptor().GetType(), exception_state);
    return false;
  }

  constructor_->SetName(v8_type->IsNull() ? v8_tag_name
                                          : v8_type.As<v8::String>());

  v8::Local<v8::String> prototype_key = V8AtomicString(isolate, "prototype");
  bool has_own_property;
  if (!constructor_->HasOwnProperty(context, prototype_key)
           .To(&has_own_property) ||
      !has_own_property) {
    return false;
  }

  // This sets the property *value*; calling Set is safe because
  // "prototype" is a non-configurable data property so there can be
  // no side effects.
  bool set_prototype_key;
  if (!constructor_->Set(context, prototype_key, prototype_)
           .To(&set_prototype_key) ||
      !set_prototype_key) {
    return false;
  }

  // This *configures* the property. DefineOwnProperty of a function's
  // "prototype" does not affect the value, but can reconfigure the
  // property.
  bool configured_prototype;
  if (!constructor_
           ->DefineOwnProperty(
               context, prototype_key, prototype_,
               v8::PropertyAttribute(v8::ReadOnly | v8::DontEnum |
                                     v8::DontDelete))
           .To(&configured_prototype) ||
      !configured_prototype) {
    return false;
  }

  v8::Local<v8::String> constructor_key =
      V8AtomicString(isolate, "constructor");
  v8::Local<v8::Value> constructor_prototype;
  if (!prototype_->Get(context, constructor_key)
           .ToLocal(&constructor_prototype)) {
    return false;
  }

  bool set_prototype;
  if (!constructor_->SetPrototype(context, constructor_prototype)
           .To(&set_prototype) ||
      !set_prototype) {
    return false;
  }

  V8PrivateProperty::GetSymbol(isolate,
                               kPrivatePropertyIsInterfacePrototypeObject)
      .Set(prototype_, v8::True(isolate));

  bool configured_constructor;
  if (!prototype_
           ->DefineOwnProperty(context, constructor_key, constructor_,
                               v8::DontEnum)
           .To(&configured_constructor) ||
      !configured_constructor) {
    return false;
  }

  return true;
}

bool V0CustomElementConstructorBuilder::PrototypeIsValid(
    const AtomicString& type,
    ExceptionState& exception_state) const {
  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Context> context = script_state_->GetContext();

  if (prototype_->InternalFieldCount() ||
      V8PrivateProperty::GetSymbol(isolate,
                                   kPrivatePropertyIsInterfacePrototypeObject)
          .HasValue(prototype_)) {
    V0CustomElementException::ThrowException(
        V0CustomElementException::kPrototypeInUse, type, exception_state);
    return false;
  }

  v8::PropertyAttribute property_attribute;
  if (!prototype_
           ->GetPropertyAttributes(context,
                                   V8AtomicString(isolate, "constructor"))
           .To(&property_attribute) ||
      (property_attribute & v8::DontDelete)) {
    V0CustomElementException::ThrowException(
        V0CustomElementException::kConstructorPropertyNotConfigurable, type,
        exception_state);
    return false;
  }

  return true;
}

bool V0CustomElementConstructorBuilder::DidRegisterDefinition() const {
  DCHECK(!constructor_.IsEmpty());

  return callbacks_->SetBinding(std::make_unique<V0CustomElementBinding>(
      script_state_->GetIsolate(), prototype_));
}

ScriptValue V0CustomElementConstructorBuilder::BindingsReturnValue() const {
  return ScriptValue(script_state_->GetIsolate(), constructor_);
}

bool V0CustomElementConstructorBuilder::HasValidPrototypeChainFor(
    const WrapperTypeInfo* type) const {
  v8::Local<v8::Object> element_prototype =
      script_state_->PerContextData()->PrototypeForType(type);
  if (element_prototype.IsEmpty())
    return false;

  v8::Local<v8::Value> chain = prototype_;
  while (!chain.IsEmpty() && chain->IsObject()) {
    if (chain == element_prototype)
      return true;
    chain = chain.As<v8::Object>()->GetPrototype();
  }

  return false;
}

static void ConstructCustomElement(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();

  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(
        isolate, "DOM object constructor cannot be called as a function.");
    return;
  }

  if (info.Length() > 0) {
    V8ThrowException::ThrowTypeError(
        isolate, "This constructor should be called without arguments.");
    return;
  }

  v8::Local<v8::Object> data = v8::Local<v8::Object>::Cast(info.Data());
  v8::Local<v8::Value> document_value;
  if (!V8PrivateProperty::GetSymbol(isolate, kPrivatePropertyDocument)
           .GetOrUndefined(data)
           .ToLocal(&document_value)) {
    return;
  }
  Document* document = V8Document::ToImpl(document_value.As<v8::Object>());
  v8::Local<v8::Value> namespace_uri_value;
  if (!V8PrivateProperty::GetSymbol(isolate, kPrivatePropertyNamespaceURI)
           .GetOrUndefined(data)
           .ToLocal(&namespace_uri_value) ||
      namespace_uri_value->IsUndefined()) {
    return;
  }
  TOSTRING_VOID(V8StringResource<>, namespace_uri, namespace_uri_value);
  v8::Local<v8::Value> tag_name_value;
  if (!V8PrivateProperty::GetSymbol(isolate, kPrivatePropertyTagName)
           .GetOrUndefined(data)
           .ToLocal(&tag_name_value) ||
      tag_name_value->IsUndefined()) {
    return;
  }
  TOSTRING_VOID(V8StringResource<>, tag_name, tag_name_value);
  v8::Local<v8::Value> maybe_type;
  if (!V8PrivateProperty::GetSymbol(isolate, kPrivatePropertyType)
           .GetOrUndefined(data)
           .ToLocal(&maybe_type) ||
      maybe_type->IsUndefined()) {
    return;
  }
  TOSTRING_VOID(V8StringResource<kTreatNullAsNullString>, type, maybe_type);

  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 "CustomElement");
  V0CustomElementProcessingStack::CallbackDeliveryScope delivery_scope;
  Element* element = document->createElementNS(
      namespace_uri, tag_name, StringOrElementCreationOptions::FromString(type),
      exception_state);
  if (element) {
    UseCounter::Count(document, WebFeature::kV0CustomElementsConstruct);
  }
  V8SetReturnValueFast(info, element, document);
}

}  // namespace blink
