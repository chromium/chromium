// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_integer_indexed_global.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestIntegerIndexedGlobal::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestIntegerIndexedGlobal::domTemplate,
    nullptr,
    "TestIntegerIndexedGlobal",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestIntegerIndexedGlobal.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestIntegerIndexedGlobal::wrapper_type_info_ = V8TestIntegerIndexedGlobal::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestIntegerIndexedGlobal>::value,
    "TestIntegerIndexedGlobal inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestIntegerIndexedGlobal::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestIntegerIndexedGlobal is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_integer_indexed_global_v8_internal {

static void lengthAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestIntegerIndexedGlobal* impl = V8TestIntegerIndexedGlobal::ToImpl(holder);

  V8SetReturnValue(info, static_cast<double>(impl->length()));
}

static void lengthAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestIntegerIndexedGlobal* impl = V8TestIntegerIndexedGlobal::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestIntegerIndexedGlobal", "length");

  // Prepare the value to be set.
  uint64_t cppValue = NativeValueTraits<IDLUnsignedLongLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setLength(cppValue);
}

static void voidMethodDocumentMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestIntegerIndexedGlobal* impl = V8TestIntegerIndexedGlobal::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocument", "TestIntegerIndexedGlobal", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Document* document;
  document = V8Document::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!document) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocument", "TestIntegerIndexedGlobal", "parameter 1 is not of type 'Document'."));
    return;
  }

  impl->voidMethodDocument(document);
}

static void indexedPropertyDescriptor(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // Steps 1.1 to 1.2.4 are covered here: we rely on indexedPropertyGetter() to
  // call the getter function and check that |index| is a valid property index,
  // in which case it will have set info.GetReturnValue() to something other
  // than undefined.
  V8TestIntegerIndexedGlobal::indexedPropertyGetterCallback(index, info);
  v8::Local<v8::Value> getterValue = info.GetReturnValue().Get();
  if (!getterValue->IsUndefined()) {
    // 1.2.5. Let |desc| be a newly created Property Descriptor with no fields.
    // 1.2.6. Set desc.[[Value]] to the result of converting value to an
    //        ECMAScript value.
    // 1.2.7. If O implements an interface with an indexed property setter,
    //        then set desc.[[Writable]] to true, otherwise set it to false.
    v8::PropertyDescriptor desc(getterValue, true);
    // 1.2.8. Set desc.[[Enumerable]] and desc.[[Configurable]] to true.
    desc.set_enumerable(true);
    desc.set_configurable(true);
    // 1.2.9. Return |desc|.
    V8SetReturnValue(info, desc);
  }
}

}  // namespace test_integer_indexed_global_v8_internal

void V8TestIntegerIndexedGlobal::lengthAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestIntegerIndexedGlobal_length_Getter");

  test_integer_indexed_global_v8_internal::lengthAttributeGetter(info);
}

void V8TestIntegerIndexedGlobal::lengthAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestIntegerIndexedGlobal_length_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_integer_indexed_global_v8_internal::lengthAttributeSetter(v8Value, info);
}

void V8TestIntegerIndexedGlobal::voidMethodDocumentMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestIntegerIndexedGlobal_voidMethodDocument");

  test_integer_indexed_global_v8_internal::voidMethodDocumentMethod(info);
}

void V8TestIntegerIndexedGlobal::namedPropertyGetterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestIntegerIndexedGlobal_NamedPropertyGetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  V8TestIntegerIndexedGlobal::namedPropertyGetterCustom(propertyName, info);
}

void V8TestIntegerIndexedGlobal::namedPropertySetterCallback(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestIntegerIndexedGlobal_NamedPropertySetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  V8TestIntegerIndexedGlobal::namedPropertySetterCustom(propertyName, v8Value, info);
}

void V8TestIntegerIndexedGlobal::namedPropertyDeleterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  V8TestIntegerIndexedGlobal::namedPropertyDeleterCustom(propertyName, info);
}

void V8TestIntegerIndexedGlobal::namedPropertyQueryCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestIntegerIndexedGlobal_NamedPropertyQuery");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  V8TestIntegerIndexedGlobal::namedPropertyQueryCustom(propertyName, info);
}

void V8TestIntegerIndexedGlobal::namedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info) {
  V8TestIntegerIndexedGlobal::namedPropertyEnumeratorCustom(info);
}

void V8TestIntegerIndexedGlobal::indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestIntegerIndexedGlobal_IndexedPropertyGetter");

  V8TestIntegerIndexedGlobal::indexedPropertyGetterCustom(index, info);
}

void V8TestIntegerIndexedGlobal::indexedPropertyDescriptorCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_integer_indexed_global_v8_internal::indexedPropertyDescriptor(index, info);
}

void V8TestIntegerIndexedGlobal::indexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  V8TestIntegerIndexedGlobal::indexedPropertySetterCustom(index, v8Value, info);
}

void V8TestIntegerIndexedGlobal::indexedPropertyDeleterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  V8TestIntegerIndexedGlobal::indexedPropertyDeleterCustom(index, info);
}

void V8TestIntegerIndexedGlobal::indexedPropertyDefinerCallback(
    uint32_t index,
    const v8::PropertyDescriptor& desc,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  // https://heycam.github.io/webidl/#legacy-platform-object-defineownproperty
  // 3.9.3. [[DefineOwnProperty]]
  // step 1.1. If the result of calling IsDataDescriptor(Desc) is false, then
  //   return false.
  if (desc.has_get() || desc.has_set()) {
    V8SetReturnValue(info, v8::Null(info.GetIsolate()));
    if (info.ShouldThrowOnError()) {
      ExceptionState exceptionState(info.GetIsolate(),
                                    ExceptionState::kIndexedSetterContext,
                                    "TestIntegerIndexedGlobal");
      exceptionState.ThrowTypeError("Accessor properties are not allowed.");
    }
    return;
  }

  // Return nothing and fall back to indexedPropertySetterCallback.
}

static const V8DOMConfiguration::AccessorConfiguration V8TestIntegerIndexedGlobalAccessors[] = {
    { "length", V8TestIntegerIndexedGlobal::lengthAttributeGetterCallback, V8TestIntegerIndexedGlobal::lengthAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
};

static const V8DOMConfiguration::MethodConfiguration V8TestIntegerIndexedGlobalMethods[] = {
    {"voidMethodDocument", V8TestIntegerIndexedGlobal::voidMethodDocumentMethodCallback, 1, v8::None, V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

static void installV8TestIntegerIndexedGlobalTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestIntegerIndexedGlobal::wrapperTypeInfo.interface_name, V8TestIntegerIndexedGlobal::domTemplateForNamedPropertiesObject(isolate, world), V8TestIntegerIndexedGlobal::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Global object prototype chain consists of Immutable Prototype Exotic Objects
  prototypeTemplate->SetImmutableProto();

  // Global objects are Immutable Prototype Exotic Objects
  instanceTemplate->SetImmutableProto();

  // Register IDL constants, attributes and operations.
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestIntegerIndexedGlobalAccessors, base::size(V8TestIntegerIndexedGlobalAccessors));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestIntegerIndexedGlobalMethods, base::size(V8TestIntegerIndexedGlobalMethods));

  // Indexed properties
  v8::IndexedPropertyHandlerConfiguration indexedPropertyHandlerConfig(
      V8TestIntegerIndexedGlobal::indexedPropertyGetterCallback,
      V8TestIntegerIndexedGlobal::indexedPropertySetterCallback,
      V8TestIntegerIndexedGlobal::indexedPropertyDescriptorCallback,
      V8TestIntegerIndexedGlobal::indexedPropertyDeleterCallback,
      IndexedPropertyEnumerator<TestIntegerIndexedGlobal>,
      V8TestIntegerIndexedGlobal::indexedPropertyDefinerCallback,
      v8::Local<v8::Value>(),
      v8::PropertyHandlerFlags::kNone);
  instanceTemplate->SetHandler(indexedPropertyHandlerConfig);

  // Array iterator (@@iterator)
  instanceTemplate->SetIntrinsicDataProperty(v8::Symbol::GetIterator(isolate), v8::kArrayProto_values, v8::DontEnum);

  // Custom signature

  V8TestIntegerIndexedGlobal::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestIntegerIndexedGlobal::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestIntegerIndexedGlobal::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestIntegerIndexedGlobalTemplate);
}

v8::Local<v8::FunctionTemplate> V8TestIntegerIndexedGlobal::domTemplateForNamedPropertiesObject(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  v8::Local<v8::FunctionTemplate> parentTemplate = V8None::domTemplate(isolate, world);

  v8::Local<v8::FunctionTemplate> namedPropertiesObjectFunctionTemplate = v8::FunctionTemplate::New(isolate, V8ObjectConstructor::IsValidConstructorMode);
  namedPropertiesObjectFunctionTemplate->SetClassName(V8AtomicString(isolate, "TestIntegerIndexedGlobalProperties"));
  namedPropertiesObjectFunctionTemplate->Inherit(parentTemplate);

  v8::Local<v8::ObjectTemplate> namedPropertiesObjectTemplate = namedPropertiesObjectFunctionTemplate->PrototypeTemplate();
  namedPropertiesObjectTemplate->SetInternalFieldCount(V8TestIntegerIndexedGlobal::internalFieldCount);
  // Named Properties object has SetPrototype method of Immutable Prototype Exotic Objects
  namedPropertiesObjectTemplate->SetImmutableProto();
  V8DOMConfiguration::SetClassString(isolate, namedPropertiesObjectTemplate, "TestIntegerIndexedGlobalProperties");
  v8::NamedPropertyHandlerConfiguration namedPropertyHandlerConfig(V8TestIntegerIndexedGlobal::namedPropertyGetterCallback, V8TestIntegerIndexedGlobal::namedPropertySetterCallback, V8TestIntegerIndexedGlobal::namedPropertyQueryCallback, V8TestIntegerIndexedGlobal::namedPropertyDeleterCallback, V8TestIntegerIndexedGlobal::namedPropertyEnumeratorCallback, v8::Local<v8::Value>(), static_cast<v8::PropertyHandlerFlags>(int(v8::PropertyHandlerFlags::kOnlyInterceptStrings) | int(v8::PropertyHandlerFlags::kNonMasking)));
  namedPropertiesObjectTemplate->SetHandler(namedPropertyHandlerConfig);

  return namedPropertiesObjectFunctionTemplate;
}

bool V8TestIntegerIndexedGlobal::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestIntegerIndexedGlobal::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestIntegerIndexedGlobal* V8TestIntegerIndexedGlobal::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestIntegerIndexedGlobal* NativeValueTraits<TestIntegerIndexedGlobal>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestIntegerIndexedGlobal* nativeValue = V8TestIntegerIndexedGlobal::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestIntegerIndexedGlobal"));
  }
  return nativeValue;
}

}  // namespace blink
