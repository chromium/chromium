// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_3.h"

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
const WrapperTypeInfo V8TestInterface3::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterface3::domTemplate,
    nullptr,
    "TestInterface3",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterface3.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterface3::wrapper_type_info_ = V8TestInterface3::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterface3>::value,
    "TestInterface3 inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterface3::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterface3 is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_3_v8_internal {

static void lengthAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterface3* impl = V8TestInterface3::ToImpl(holder);

  V8SetReturnValueUnsigned(info, impl->length());
}

static void readonlyStringifierAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterface3* impl = V8TestInterface3::ToImpl(holder);

  V8SetReturnValueString(info, impl->readonlyStringifierAttribute(), info.GetIsolate());
}

static void voidMethodDocumentMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface3* impl = V8TestInterface3::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocument", "TestInterface3", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  Document* document;
  document = V8Document::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!document) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodDocument", "TestInterface3", "parameter 1 is not of type 'Document'."));
    return;
  }

  impl->voidMethodDocument(document);
}

static void toStringMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface3* impl = V8TestInterface3::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->readonlyStringifierAttribute(), info.GetIsolate());
}

static void indexedPropertyDescriptor(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // Steps 1.1 to 1.2.4 are covered here: we rely on indexedPropertyGetter() to
  // call the getter function and check that |index| is a valid property index,
  // in which case it will have set info.GetReturnValue() to something other
  // than undefined.
  V8TestInterface3::indexedPropertyGetterCallback(index, info);
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

}  // namespace test_interface_3_v8_internal

void V8TestInterface3::lengthAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface3_length_Getter");

  test_interface_3_v8_internal::lengthAttributeGetter(info);
}

void V8TestInterface3::readonlyStringifierAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface3_readonlyStringifierAttribute_Getter");

  test_interface_3_v8_internal::readonlyStringifierAttributeAttributeGetter(info);
}

void V8TestInterface3::voidMethodDocumentMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface3_voidMethodDocument");

  test_interface_3_v8_internal::voidMethodDocumentMethod(info);
}

void V8TestInterface3::toStringMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface3_toString");

  test_interface_3_v8_internal::toStringMethod(info);
}

void V8TestInterface3::namedPropertyGetterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface3_NamedPropertyGetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  V8TestInterface3::namedPropertyGetterCustom(propertyName, info);
}

void V8TestInterface3::namedPropertySetterCallback(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface3_NamedPropertySetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  V8TestInterface3::namedPropertySetterCustom(propertyName, v8Value, info);
}

void V8TestInterface3::namedPropertyDeleterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  V8TestInterface3::namedPropertyDeleterCustom(propertyName, info);
}

void V8TestInterface3::namedPropertyQueryCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface3_NamedPropertyQuery");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  V8TestInterface3::namedPropertyQueryCustom(propertyName, info);
}

void V8TestInterface3::namedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info) {
  V8TestInterface3::namedPropertyEnumeratorCustom(info);
}

void V8TestInterface3::indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface3_IndexedPropertyGetter");

  V8TestInterface3::indexedPropertyGetterCustom(index, info);
}

void V8TestInterface3::indexedPropertyDescriptorCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_interface_3_v8_internal::indexedPropertyDescriptor(index, info);
}

void V8TestInterface3::indexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  V8TestInterface3::indexedPropertySetterCustom(index, v8Value, info);
}

void V8TestInterface3::indexedPropertyDeleterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  V8TestInterface3::indexedPropertyDeleterCustom(index, info);
}

void V8TestInterface3::indexedPropertyDefinerCallback(
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
                                    "TestInterface3");
      exceptionState.ThrowTypeError("Accessor properties are not allowed.");
    }
    return;
  }

  // Return nothing and fall back to indexedPropertySetterCallback.
}

static const V8DOMConfiguration::AccessorConfiguration V8TestInterface3Accessors[] = {
    { "length", V8TestInterface3::lengthAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "readonlyStringifierAttribute", V8TestInterface3::readonlyStringifierAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
};

static const V8DOMConfiguration::MethodConfiguration V8TestInterface3Methods[] = {
    {"voidMethodDocument", V8TestInterface3::voidMethodDocumentMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"toString", V8TestInterface3::toStringMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

static void installV8TestInterface3Template(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterface3::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterface3::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterface3Accessors, base::size(V8TestInterface3Accessors));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterface3Methods, base::size(V8TestInterface3Methods));

  // Indexed properties
  v8::IndexedPropertyHandlerConfiguration indexedPropertyHandlerConfig(
      V8TestInterface3::indexedPropertyGetterCallback,
      V8TestInterface3::indexedPropertySetterCallback,
      V8TestInterface3::indexedPropertyDescriptorCallback,
      V8TestInterface3::indexedPropertyDeleterCallback,
      IndexedPropertyEnumerator<TestInterface3>,
      V8TestInterface3::indexedPropertyDefinerCallback,
      v8::Local<v8::Value>(),
      v8::PropertyHandlerFlags::kNone);
  instanceTemplate->SetHandler(indexedPropertyHandlerConfig);
  // Named properties
  v8::NamedPropertyHandlerConfiguration namedPropertyHandlerConfig(V8TestInterface3::namedPropertyGetterCallback, V8TestInterface3::namedPropertySetterCallback, V8TestInterface3::namedPropertyQueryCallback, V8TestInterface3::namedPropertyDeleterCallback, V8TestInterface3::namedPropertyEnumeratorCallback, v8::Local<v8::Value>(), static_cast<v8::PropertyHandlerFlags>(int(v8::PropertyHandlerFlags::kOnlyInterceptStrings) | int(v8::PropertyHandlerFlags::kNonMasking)));
  instanceTemplate->SetHandler(namedPropertyHandlerConfig);

  // Array iterator (@@iterator)
  prototypeTemplate->SetIntrinsicDataProperty(v8::Symbol::GetIterator(isolate), v8::kArrayProto_values, v8::DontEnum);
  // For value iterators, the properties below must originally be set to the corresponding ones in %ArrayPrototype%.
  // See https://heycam.github.io/webidl/#es-iterators.
  prototypeTemplate->SetIntrinsicDataProperty(V8AtomicString(isolate, "entries"), v8::kArrayProto_entries);
  prototypeTemplate->SetIntrinsicDataProperty(V8AtomicString(isolate, "forEach"), v8::kArrayProto_forEach);
  prototypeTemplate->SetIntrinsicDataProperty(V8AtomicString(isolate, "keys"), v8::kArrayProto_keys);
  prototypeTemplate->SetIntrinsicDataProperty(V8AtomicString(isolate, "values"), v8::kArrayProto_values);

  // Custom signature

  V8TestInterface3::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterface3::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestInterface3::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterface3Template);
}

bool V8TestInterface3::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterface3::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterface3* V8TestInterface3::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterface3* NativeValueTraits<TestInterface3>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterface3* nativeValue = V8TestInterface3::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterface3"));
  }
  return nativeValue;
}

}  // namespace blink
