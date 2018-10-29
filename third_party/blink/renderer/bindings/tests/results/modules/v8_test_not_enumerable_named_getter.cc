// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/modules/v8_test_not_enumerable_named_getter.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestNotEnumerableNamedGetter::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestNotEnumerableNamedGetter::domTemplate,
    nullptr,
    "TestNotEnumerableNamedGetter",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestNotEnumerableNamedGetter.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestNotEnumerableNamedGetter::wrapper_type_info_ = V8TestNotEnumerableNamedGetter::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestNotEnumerableNamedGetter>::value,
    "TestNotEnumerableNamedGetter inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestNotEnumerableNamedGetter::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestNotEnumerableNamedGetter is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_not_enumerable_named_getter_v8_internal {

static void namedPropertyGetter(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestNotEnumerableNamedGetter* impl = V8TestNotEnumerableNamedGetter::ToImpl(info.Holder());
  int32_t result = impl->AnonymousNamedGetter(name);
  if ()
    return;
  V8SetReturnValueInt(info, result);
}

static void namedPropertyDescriptor(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  // This function is called when an IDL interface supports named properties
  // but not indexed properties. When a numeric property is queried, V8 calls
  // indexedPropertyDescriptorCallback(), which calls this function.

  // This IDL interface defines a [NotEnumerable] getter. We need to do
  // something similar to indexedPropertyDescriptor(): call
  // indexedPropertyGetterCallback, let it end up calling the named property
  // getter and, if all goes well, we create a v8::PropertyDescriptor with the
  // right values.
  V8TestNotEnumerableNamedGetter::indexedPropertyGetterCallback(index, info);
  v8::Local<v8::Value> getterValue = info.GetReturnValue().Get();
  if (!getterValue->IsUndefined()) {
    v8::PropertyDescriptor desc(getterValue, false);
    desc.set_enumerable(false);
    desc.set_configurable(true);
    V8SetReturnValue(info, desc);
  }
}

}  // namespace test_not_enumerable_named_getter_v8_internal

void V8TestNotEnumerableNamedGetter::namedPropertyGetterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestNotEnumerableNamedGetter_NamedPropertyGetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_not_enumerable_named_getter_v8_internal::namedPropertyGetter(propertyName, info);
}

void V8TestNotEnumerableNamedGetter::indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestNotEnumerableNamedGetter_IndexedPropertyGetter");

  const AtomicString& propertyName = AtomicString::Number(index);

  test_not_enumerable_named_getter_v8_internal::namedPropertyGetter(propertyName, info);
}

void V8TestNotEnumerableNamedGetter::indexedPropertyDescriptorCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_not_enumerable_named_getter_v8_internal::namedPropertyDescriptor(index, info);
}

static void installV8TestNotEnumerableNamedGetterTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestNotEnumerableNamedGetter::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestNotEnumerableNamedGetter::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.

  // Indexed properties
  v8::IndexedPropertyHandlerConfiguration indexedPropertyHandlerConfig(
      V8TestNotEnumerableNamedGetter::indexedPropertyGetterCallback,
      nullptr,
      V8TestNotEnumerableNamedGetter::indexedPropertyDescriptorCallback,
      nullptr,
      nullptr,
      nullptr,
      v8::Local<v8::Value>(),
      v8::PropertyHandlerFlags::kNone);
  instanceTemplate->SetHandler(indexedPropertyHandlerConfig);
  // Named properties
  v8::NamedPropertyHandlerConfiguration namedPropertyHandlerConfig(V8TestNotEnumerableNamedGetter::namedPropertyGetterCallback, nullptr, nullptr, nullptr, nullptr, v8::Local<v8::Value>(), static_cast<v8::PropertyHandlerFlags>(int(v8::PropertyHandlerFlags::kOnlyInterceptStrings) | int(v8::PropertyHandlerFlags::kNonMasking)));
  instanceTemplate->SetHandler(namedPropertyHandlerConfig);

  // Custom signature

  V8TestNotEnumerableNamedGetter::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestNotEnumerableNamedGetter::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestNotEnumerableNamedGetter::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestNotEnumerableNamedGetterTemplate);
}

bool V8TestNotEnumerableNamedGetter::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestNotEnumerableNamedGetter::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestNotEnumerableNamedGetter* V8TestNotEnumerableNamedGetter::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestNotEnumerableNamedGetter* NativeValueTraits<TestNotEnumerableNamedGetter>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestNotEnumerableNamedGetter* nativeValue = V8TestNotEnumerableNamedGetter::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestNotEnumerableNamedGetter"));
  }
  return nativeValue;
}

}  // namespace blink
