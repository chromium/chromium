// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_named_constructor_2.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestInterfaceNamedConstructor2::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceNamedConstructor2::domTemplate,
    nullptr,
    "TestInterfaceNamedConstructor2",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceNamedConstructor2.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceNamedConstructor2::wrapper_type_info_ = V8TestInterfaceNamedConstructor2::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceNamedConstructor2>::value,
    "TestInterfaceNamedConstructor2 inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceNamedConstructor2::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceNamedConstructor2 is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_named_constructor_2_v8_internal {

}  // namespace test_interface_named_constructor_2_v8_internal

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestInterfaceNamedConstructor2Constructor::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceNamedConstructor2Constructor::domTemplate,
    nullptr,
    "TestInterfaceNamedConstructor2",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

static void V8TestInterfaceNamedConstructor2ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNamedConstructor2_ConstructorCallback");

  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::ConstructorNotCallableAsFunction("Audio"));
    return;
  }

  if (ConstructorMode::Current(info.GetIsolate()) == ConstructorMode::kWrapExistingObject) {
    V8SetReturnValue(info, info.Holder());
    return;
  }

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToConstruct("TestInterfaceNamedConstructor2", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  TestInterfaceNamedConstructor2* impl = TestInterfaceNamedConstructor2::CreateForJSConstructor(stringArg);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceNamedConstructor2Constructor::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

v8::Local<v8::FunctionTemplate> V8TestInterfaceNamedConstructor2Constructor::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  static int domTemplateKey; // This address is used for a key to look up the dom template.
  V8PerIsolateData* data = V8PerIsolateData::From(isolate);
  v8::Local<v8::FunctionTemplate> result = data->FindInterfaceTemplate(world, &domTemplateKey);
  if (!result.IsEmpty())
    return result;

  result = v8::FunctionTemplate::New(isolate, V8TestInterfaceNamedConstructor2ConstructorCallback);
  v8::Local<v8::ObjectTemplate> instanceTemplate = result->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(V8TestInterfaceNamedConstructor2::internalFieldCount);
  result->SetClassName(V8AtomicString(isolate, "Audio"));
  result->Inherit(V8TestInterfaceNamedConstructor2::domTemplate(isolate, world));
  data->SetInterfaceTemplate(world, &domTemplateKey, result);
  return result;
}

void V8TestInterfaceNamedConstructor2Constructor::NamedConstructorAttributeGetter(
    v8::Local<v8::Name> propertyName,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Context> creationContext = info.Holder()->CreationContext();
  V8PerContextData* perContextData = V8PerContextData::From(creationContext);
  if (!perContextData) {
    // TODO(yukishiino): Return a valid named constructor even after the context is detached
    return;
  }

  v8::Local<v8::Function> namedConstructor = perContextData->ConstructorForType(&V8TestInterfaceNamedConstructor2Constructor::wrapperTypeInfo);

  // Set the prototype of named constructors to the regular constructor.
  auto privateProperty = V8PrivateProperty::GetNamedConstructorInitialized(info.GetIsolate());
  v8::Local<v8::Context> currentContext = info.GetIsolate()->GetCurrentContext();
  v8::Local<v8::Value> privateValue;

  if (!privateProperty.GetOrUndefined(namedConstructor).ToLocal(&privateValue) || privateValue->IsUndefined()) {
    v8::Local<v8::Function> interface = perContextData->ConstructorForType(&V8TestInterfaceNamedConstructor2::wrapperTypeInfo);
    v8::Local<v8::Value> interfacePrototype = interface->Get(currentContext, V8AtomicString(info.GetIsolate(), "prototype")).ToLocalChecked();
    bool result = namedConstructor->Set(currentContext, V8AtomicString(info.GetIsolate(), "prototype"), interfacePrototype).ToChecked();
    if (!result)
      return;
    privateProperty.Set(namedConstructor, v8::True(info.GetIsolate()));
  }

  V8SetReturnValue(info, namedConstructor);
}

static void installV8TestInterfaceNamedConstructor2Template(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterfaceNamedConstructor2::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceNamedConstructor2::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.

  // Custom signature

  V8TestInterfaceNamedConstructor2::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterfaceNamedConstructor2::InstallRuntimeEnabledFeaturesOnTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.

  // Custom signature
}

v8::Local<v8::FunctionTemplate> V8TestInterfaceNamedConstructor2::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterfaceNamedConstructor2Template);
}

bool V8TestInterfaceNamedConstructor2::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceNamedConstructor2::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceNamedConstructor2* V8TestInterfaceNamedConstructor2::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceNamedConstructor2* NativeValueTraits<TestInterfaceNamedConstructor2>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceNamedConstructor2* nativeValue = V8TestInterfaceNamedConstructor2::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceNamedConstructor2"));
  }
  return nativeValue;
}

}  // namespace blink
