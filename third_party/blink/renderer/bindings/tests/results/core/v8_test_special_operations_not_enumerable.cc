// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_special_operations_not_enumerable.h"

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
const WrapperTypeInfo V8TestSpecialOperationsNotEnumerable::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestSpecialOperationsNotEnumerable::domTemplate,
    nullptr,
    "TestSpecialOperationsNotEnumerable",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestSpecialOperationsNotEnumerable.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestSpecialOperationsNotEnumerable::wrapper_type_info_ = V8TestSpecialOperationsNotEnumerable::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestSpecialOperationsNotEnumerable>::value,
    "TestSpecialOperationsNotEnumerable inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestSpecialOperationsNotEnumerable::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestSpecialOperationsNotEnumerable is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_special_operations_not_enumerable_v8_internal {

static void namedPropertyGetter(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestSpecialOperationsNotEnumerable* impl = V8TestSpecialOperationsNotEnumerable::ToImpl(info.Holder());
  String result = impl->AnonymousNamedGetter(name);
  if (result.IsNull())
    return;
  V8SetReturnValueString(info, result, info.GetIsolate());
}

static void namedPropertyQuery(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  const CString& nameInUtf8 = name.Utf8();
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestSpecialOperationsNotEnumerable", nameInUtf8.data());

  TestSpecialOperationsNotEnumerable* impl = V8TestSpecialOperationsNotEnumerable::ToImpl(info.Holder());

  bool result = impl->NamedPropertyQuery(name, exceptionState);
  if (!result)
    return;
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // 2.7. If |O| implements an interface with a named property setter, then set
  //      desc.[[Writable]] to true, otherwise set it to false.
  // 2.8. If |O| implements an interface with the
  //      [LegacyUnenumerableNamedProperties] extended attribute, then set
  //      desc.[[Enumerable]] to false, otherwise set it to true.
  V8SetReturnValueInt(info, v8::DontEnum | v8::ReadOnly);
}

static void namedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kEnumerationContext, "TestSpecialOperationsNotEnumerable");

  TestSpecialOperationsNotEnumerable* impl = V8TestSpecialOperationsNotEnumerable::ToImpl(info.Holder());

  Vector<String> names;
  impl->NamedPropertyEnumerator(names, exceptionState);
  if (exceptionState.HadException())
    return;
  V8SetReturnValue(info, ToV8(names, info.Holder(), info.GetIsolate()).As<v8::Array>());
}

static void indexedPropertyGetter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestSpecialOperationsNotEnumerable* impl = V8TestSpecialOperationsNotEnumerable::ToImpl(info.Holder());

  // We assume that all the implementations support length() method, although
  // the spec doesn't require that length() must exist.  It's okay that
  // the interface does not have length attribute as long as the
  // implementation supports length() member function.
  if (index >= impl->length())
    return;  // Returns undefined due to out-of-range.

  String result = impl->AnonymousIndexedGetter(index);
  V8SetReturnValueString(info, result, info.GetIsolate());
}

static void indexedPropertyDescriptor(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // Steps 1.1 to 1.2.4 are covered here: we rely on indexedPropertyGetter() to
  // call the getter function and check that |index| is a valid property index,
  // in which case it will have set info.GetReturnValue() to something other
  // than undefined.
  V8TestSpecialOperationsNotEnumerable::indexedPropertyGetterCallback(index, info);
  v8::Local<v8::Value> getterValue = info.GetReturnValue().Get();
  if (!getterValue->IsUndefined()) {
    // 1.2.5. Let |desc| be a newly created Property Descriptor with no fields.
    // 1.2.6. Set desc.[[Value]] to the result of converting value to an
    //        ECMAScript value.
    // 1.2.7. If O implements an interface with an indexed property setter,
    //        then set desc.[[Writable]] to true, otherwise set it to false.
    v8::PropertyDescriptor desc(getterValue, false);
    // 1.2.8. Set desc.[[Enumerable]] and desc.[[Configurable]] to true.
    desc.set_enumerable(true);
    desc.set_configurable(true);
    // 1.2.9. Return |desc|.
    V8SetReturnValue(info, desc);
  }
}

}  // namespace test_special_operations_not_enumerable_v8_internal

void V8TestSpecialOperationsNotEnumerable::namedPropertyGetterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestSpecialOperationsNotEnumerable_NamedPropertyGetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_special_operations_not_enumerable_v8_internal::namedPropertyGetter(propertyName, info);
}

void V8TestSpecialOperationsNotEnumerable::namedPropertyQueryCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestSpecialOperationsNotEnumerable_NamedPropertyQuery");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_special_operations_not_enumerable_v8_internal::namedPropertyQuery(propertyName, info);
}

void V8TestSpecialOperationsNotEnumerable::namedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info) {
  test_special_operations_not_enumerable_v8_internal::namedPropertyEnumerator(info);
}

void V8TestSpecialOperationsNotEnumerable::indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestSpecialOperationsNotEnumerable_IndexedPropertyGetter");

  test_special_operations_not_enumerable_v8_internal::indexedPropertyGetter(index, info);
}

void V8TestSpecialOperationsNotEnumerable::indexedPropertyDescriptorCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_special_operations_not_enumerable_v8_internal::indexedPropertyDescriptor(index, info);
}

void V8TestSpecialOperationsNotEnumerable::indexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  // No indexed property setter defined.  Do not fall back to the default
  // setter.
  V8SetReturnValue(info, v8::Null(info.GetIsolate()));
  if (info.ShouldThrowOnError()) {
    ExceptionState exceptionState(info.GetIsolate(),
                                  ExceptionState::kIndexedSetterContext,
                                  "TestSpecialOperationsNotEnumerable");
    exceptionState.ThrowTypeError("Index property setter is not supported.");
  }
}

void V8TestSpecialOperationsNotEnumerable::indexedPropertyDefinerCallback(
    uint32_t index,
    const v8::PropertyDescriptor& desc,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  // https://heycam.github.io/webidl/#legacy-platform-object-defineownproperty
  // 3.9.3. [[DefineOwnProperty]]
  // step 1.2. If O does not implement an interface with an indexed property
  //   setter, then return false.
  //
  // https://html.spec.whatwg.org/C/window-object.html#windowproxy-defineownproperty
  // 7.4.6 [[DefineOwnProperty]] (P, Desc)
  // step 2.1. If P is an array index property name, return false.
  V8SetReturnValue(info, v8::Null(info.GetIsolate()));
  if (info.ShouldThrowOnError()) {
    ExceptionState exceptionState(info.GetIsolate(),
                                  ExceptionState::kIndexedSetterContext,
                                  "TestSpecialOperationsNotEnumerable");
    exceptionState.ThrowTypeError("Index property setter is not supported.");
  }
}

static void installV8TestSpecialOperationsNotEnumerableTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestSpecialOperationsNotEnumerable::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestSpecialOperationsNotEnumerable::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.

  // Indexed properties
  v8::IndexedPropertyHandlerConfiguration indexedPropertyHandlerConfig(
      V8TestSpecialOperationsNotEnumerable::indexedPropertyGetterCallback,
      V8TestSpecialOperationsNotEnumerable::indexedPropertySetterCallback,
      V8TestSpecialOperationsNotEnumerable::indexedPropertyDescriptorCallback,
      nullptr,
      IndexedPropertyEnumerator<TestSpecialOperationsNotEnumerable>,
      V8TestSpecialOperationsNotEnumerable::indexedPropertyDefinerCallback,
      v8::Local<v8::Value>(),
      v8::PropertyHandlerFlags::kNone);
  instanceTemplate->SetHandler(indexedPropertyHandlerConfig);
  // Named properties
  v8::NamedPropertyHandlerConfiguration namedPropertyHandlerConfig(V8TestSpecialOperationsNotEnumerable::namedPropertyGetterCallback, nullptr, V8TestSpecialOperationsNotEnumerable::namedPropertyQueryCallback, nullptr, V8TestSpecialOperationsNotEnumerable::namedPropertyEnumeratorCallback, v8::Local<v8::Value>(), static_cast<v8::PropertyHandlerFlags>(int(v8::PropertyHandlerFlags::kOnlyInterceptStrings) | int(v8::PropertyHandlerFlags::kNonMasking)));
  instanceTemplate->SetHandler(namedPropertyHandlerConfig);

  // Custom signature

  V8TestSpecialOperationsNotEnumerable::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestSpecialOperationsNotEnumerable::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestSpecialOperationsNotEnumerable::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestSpecialOperationsNotEnumerableTemplate);
}

bool V8TestSpecialOperationsNotEnumerable::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestSpecialOperationsNotEnumerable::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestSpecialOperationsNotEnumerable* V8TestSpecialOperationsNotEnumerable::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestSpecialOperationsNotEnumerable* NativeValueTraits<TestSpecialOperationsNotEnumerable>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestSpecialOperationsNotEnumerable* nativeValue = V8TestSpecialOperationsNotEnumerable::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestSpecialOperationsNotEnumerable"));
  }
  return nativeValue;
}

}  // namespace blink
