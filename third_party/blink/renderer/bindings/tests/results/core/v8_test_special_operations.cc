// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_special_operations.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node_list.h"
#include "third_party/blink/renderer/core/dom/name_node_list.h"
#include "third_party/blink/renderer/core/dom/node_list.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"
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
const WrapperTypeInfo V8TestSpecialOperations::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestSpecialOperations::domTemplate,
    nullptr,
    "TestSpecialOperations",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestSpecialOperations.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestSpecialOperations::wrapper_type_info_ = V8TestSpecialOperations::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestSpecialOperations>::value,
    "TestSpecialOperations inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestSpecialOperations::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestSpecialOperations is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_special_operations_v8_internal {

static void namedItemMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestSpecialOperations* impl = V8TestSpecialOperations::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("namedItem", "TestSpecialOperations", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8StringResource<> name;
  name = info[0];
  if (!name.Prepare())
    return;

  NodeOrNodeList result;
  impl->getItem(name, result);
  V8SetReturnValue(info, result);
}

static void namedPropertyGetter(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestSpecialOperations* impl = V8TestSpecialOperations::ToImpl(info.Holder());
  NodeOrNodeList result;
  impl->getItem(name, result);
  if (result.IsNull())
    return;
  V8SetReturnValue(info, result);
}

static void namedPropertySetter(const AtomicString& name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestSpecialOperations* impl = V8TestSpecialOperations::ToImpl(info.Holder());
  Node* propertyValue = V8Node::ToImplWithTypeCheck(info.GetIsolate(), v8Value);
  if (!propertyValue && !IsUndefinedOrNull(v8Value)) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Node'.");
    return;
  }

  bool result = impl->AnonymousNamedSetter(name, propertyValue);
  if (!result)
    return;
  V8SetReturnValue(info, v8Value);
}

template <typename T>
static void namedPropertyQuery(const AtomicString& name, const v8::PropertyCallbackInfo<T>& info) {
  const CString& nameInUtf8 = name.Utf8();
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestSpecialOperations", nameInUtf8.data());

  TestSpecialOperations* impl = V8TestSpecialOperations::ToImpl(info.Holder());

  bool result = impl->NamedPropertyQuery(name, exceptionState);
  if (!result)
    return;
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // 2.7. If |O| implements an interface with a named property setter, then set
  //      desc.[[Writable]] to true, otherwise set it to false.
  // 2.8. If |O| implements an interface with the
  //      [LegacyUnenumerableNamedProperties] extended attribute, then set
  //      desc.[[Enumerable]] to false, otherwise set it to true.
  V8SetReturnValueInt(info, v8::None);
}

static void namedPropertyDescriptor(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  // This function is called when an IDL interface supports named properties
  // but not indexed properties. When a numeric property is queried, V8 calls
  // indexedPropertyDescriptorCallback(), which calls this function.

  // Since we initialize our indexed and named property handlers differently
  // (the former use descriptors and definers, the latter uses a query
  // callback), we need to inefficiently call the query callback and convert
  // the v8::PropertyAttribute integer it returns into a v8::PropertyDescriptor
  // expected by a descriptor callback.
  // TODO(rakuco): remove this hack once we switch named property handlers to
  // using descriptor and definer callbacks (bug 764633).
  const AtomicString& indexAsName = AtomicString::Number(index);
  test_special_operations_v8_internal::namedPropertyQuery(indexAsName, info);
  v8::Local<v8::Value> getterValue = info.GetReturnValue().Get();
  if (!getterValue->IsUndefined()) {
    DCHECK(getterValue->IsInt32());
    const int32_t props =
        getterValue->ToInt32(info.GetIsolate()->GetCurrentContext())
            .ToLocalChecked()
            ->Value();
    v8::PropertyDescriptor desc(V8String(info.GetIsolate(), indexAsName),
                                !(props & v8::ReadOnly));
    desc.set_enumerable(!(props & v8::DontEnum));
    desc.set_configurable(!(props & v8::DontDelete));
    V8SetReturnValue(info, desc);
  }
}

static void namedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kEnumerationContext, "TestSpecialOperations");

  TestSpecialOperations* impl = V8TestSpecialOperations::ToImpl(info.Holder());

  Vector<String> names;
  impl->NamedPropertyEnumerator(names, exceptionState);
  if (exceptionState.HadException())
    return;
  V8SetReturnValue(info, ToV8(names, info.Holder(), info.GetIsolate()).As<v8::Array>());
}

}  // namespace test_special_operations_v8_internal

void V8TestSpecialOperations::namedItemMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestSpecialOperations_namedItem");

  test_special_operations_v8_internal::namedItemMethod(info);
}

void V8TestSpecialOperations::namedPropertyGetterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestSpecialOperations_NamedPropertyGetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_special_operations_v8_internal::namedPropertyGetter(propertyName, info);
}

void V8TestSpecialOperations::namedPropertySetterCallback(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestSpecialOperations_NamedPropertySetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_special_operations_v8_internal::namedPropertySetter(propertyName, v8Value, info);
}

void V8TestSpecialOperations::namedPropertyQueryCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestSpecialOperations_NamedPropertyQuery");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_special_operations_v8_internal::namedPropertyQuery(propertyName, info);
}

void V8TestSpecialOperations::namedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info) {
  test_special_operations_v8_internal::namedPropertyEnumerator(info);
}

void V8TestSpecialOperations::indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestSpecialOperations_IndexedPropertyGetter");

  const AtomicString& propertyName = AtomicString::Number(index);

  test_special_operations_v8_internal::namedPropertyGetter(propertyName, info);
}

void V8TestSpecialOperations::indexedPropertyDescriptorCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_special_operations_v8_internal::namedPropertyDescriptor(index, info);
}

void V8TestSpecialOperations::indexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  const AtomicString& propertyName = AtomicString::Number(index);

  test_special_operations_v8_internal::namedPropertySetter(propertyName, v8Value, info);
}

static const V8DOMConfiguration::MethodConfiguration V8TestSpecialOperationsMethods[] = {
    {"namedItem", V8TestSpecialOperations::namedItemMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

static void installV8TestSpecialOperationsTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestSpecialOperations::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestSpecialOperations::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestSpecialOperationsMethods, base::size(V8TestSpecialOperationsMethods));

  // Indexed properties
  v8::IndexedPropertyHandlerConfiguration indexedPropertyHandlerConfig(
      V8TestSpecialOperations::indexedPropertyGetterCallback,
      V8TestSpecialOperations::indexedPropertySetterCallback,
      V8TestSpecialOperations::indexedPropertyDescriptorCallback,
      nullptr,
      nullptr,
      nullptr,
      v8::Local<v8::Value>(),
      v8::PropertyHandlerFlags::kNone);
  instanceTemplate->SetHandler(indexedPropertyHandlerConfig);
  // Named properties
  v8::NamedPropertyHandlerConfiguration namedPropertyHandlerConfig(V8TestSpecialOperations::namedPropertyGetterCallback, V8TestSpecialOperations::namedPropertySetterCallback, V8TestSpecialOperations::namedPropertyQueryCallback, nullptr, V8TestSpecialOperations::namedPropertyEnumeratorCallback, v8::Local<v8::Value>(), static_cast<v8::PropertyHandlerFlags>(int(v8::PropertyHandlerFlags::kOnlyInterceptStrings)));
  instanceTemplate->SetHandler(namedPropertyHandlerConfig);

  // Custom signature

  V8TestSpecialOperations::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestSpecialOperations::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestSpecialOperations::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestSpecialOperationsTemplate);
}

bool V8TestSpecialOperations::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestSpecialOperations::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestSpecialOperations* V8TestSpecialOperations::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestSpecialOperations* NativeValueTraits<TestSpecialOperations>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestSpecialOperations* nativeValue = V8TestSpecialOperations::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestSpecialOperations"));
  }
  return nativeValue;
}

}  // namespace blink
