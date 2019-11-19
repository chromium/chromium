// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/modules/v8_test_inherited_legacy_unenumerable_named_properties.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo v8_test_inherited_legacy_unenumerable_named_properties_wrapper_type_info = {
    gin::kEmbedderBlink,
    V8TestInheritedLegacyUnenumerableNamedProperties::DomTemplate,
    nullptr,
    "TestInheritedLegacyUnenumerableNamedProperties",
    V8TestSpecialOperationsNotEnumerable::GetWrapperTypeInfo(),
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInheritedLegacyUnenumerableNamedProperties.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInheritedLegacyUnenumerableNamedProperties::wrapper_type_info_ = v8_test_inherited_legacy_unenumerable_named_properties_wrapper_type_info;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInheritedLegacyUnenumerableNamedProperties>::value,
    "TestInheritedLegacyUnenumerableNamedProperties inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInheritedLegacyUnenumerableNamedProperties::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInheritedLegacyUnenumerableNamedProperties is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_inherited_legacy_unenumerable_named_properties_v8_internal {

static void LongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInheritedLegacyUnenumerableNamedProperties* impl = V8TestInheritedLegacyUnenumerableNamedProperties::ToImpl(holder);

  V8SetReturnValueInt(info, impl->longAttribute());
}

static void NamedPropertyGetter(const AtomicString& name,
                                const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInheritedLegacyUnenumerableNamedProperties* impl = V8TestInheritedLegacyUnenumerableNamedProperties::ToImpl(info.Holder());
  int32_t result = impl->AnonymousNamedGetter(name);
  if ()
    return;
  V8SetReturnValueInt(info, result);
}

template <typename T>
static void NamedPropertyQuery(
    const AtomicString& name, const v8::PropertyCallbackInfo<T>& info) {
  const std::string& name_in_utf8 = name.Utf8();
  ExceptionState exception_state(
      info.GetIsolate(),
      ExceptionState::kGetterContext,
      "TestInheritedLegacyUnenumerableNamedProperties",
      name_in_utf8.c_str());

  TestInheritedLegacyUnenumerableNamedProperties* impl = V8TestInheritedLegacyUnenumerableNamedProperties::ToImpl(info.Holder());

  bool result = impl->NamedPropertyQuery(name, exception_state);
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

static void NamedPropertyDescriptor(
    uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
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
  const AtomicString& index_as_name = AtomicString::Number(index);
  test_inherited_legacy_unenumerable_named_properties_v8_internal::NamedPropertyQuery(index_as_name, info);
  v8::Local<v8::Value> getter_value = info.GetReturnValue().Get();
  if (!getter_value->IsUndefined()) {
    DCHECK(getter_value->IsInt32());
    const int32_t props =
        getter_value->ToInt32(info.GetIsolate()->GetCurrentContext())
            .ToLocalChecked()
            ->Value();
    v8::PropertyDescriptor desc(V8String(info.GetIsolate(), index_as_name),
                                !(props & v8::ReadOnly));
    desc.set_enumerable(!(props & v8::DontEnum));
    desc.set_configurable(!(props & v8::DontDelete));
    V8SetReturnValue(info, desc);
  }
}

static void NamedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  ExceptionState exception_state(
      info.GetIsolate(),
      ExceptionState::kEnumerationContext,
      "TestInheritedLegacyUnenumerableNamedProperties");

  TestInheritedLegacyUnenumerableNamedProperties* impl = V8TestInheritedLegacyUnenumerableNamedProperties::ToImpl(info.Holder());

  Vector<String> names;
  impl->NamedPropertyEnumerator(names, exception_state);
  if (exception_state.HadException())
    return;
  V8SetReturnValue(info, ToV8(names, info.Holder(), info.GetIsolate()).As<v8::Array>());
}

}  // namespace test_inherited_legacy_unenumerable_named_properties_v8_internal

void V8TestInheritedLegacyUnenumerableNamedProperties::LongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInheritedLegacyUnenumerableNamedProperties_longAttribute_Getter");

  test_inherited_legacy_unenumerable_named_properties_v8_internal::LongAttributeAttributeGetter(info);
}

void V8TestInheritedLegacyUnenumerableNamedProperties::NamedPropertyGetterCallback(
    v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInheritedLegacyUnenumerableNamedProperties_NamedPropertyGetter");

  if (!name->IsString())
    return;
  const AtomicString& property_name = ToCoreAtomicString(name.As<v8::String>());

  test_inherited_legacy_unenumerable_named_properties_v8_internal::NamedPropertyGetter(property_name, info);
}

void V8TestInheritedLegacyUnenumerableNamedProperties::NamedPropertyQueryCallback(
    v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInheritedLegacyUnenumerableNamedProperties_NamedPropertyQuery");

  if (!name->IsString())
    return;
  const AtomicString& property_name = ToCoreAtomicString(name.As<v8::String>());

  test_inherited_legacy_unenumerable_named_properties_v8_internal::NamedPropertyQuery(property_name, info);
}

void V8TestInheritedLegacyUnenumerableNamedProperties::NamedPropertyEnumeratorCallback(
    const v8::PropertyCallbackInfo<v8::Array>& info) {
  test_inherited_legacy_unenumerable_named_properties_v8_internal::NamedPropertyEnumerator(info);
}

void V8TestInheritedLegacyUnenumerableNamedProperties::IndexedPropertyGetterCallback(
    uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInheritedLegacyUnenumerableNamedProperties_IndexedPropertyGetter");

  const AtomicString& property_name = AtomicString::Number(index);

  test_inherited_legacy_unenumerable_named_properties_v8_internal::NamedPropertyGetter(property_name, info);
}

void V8TestInheritedLegacyUnenumerableNamedProperties::IndexedPropertyDescriptorCallback(
    uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_inherited_legacy_unenumerable_named_properties_v8_internal::NamedPropertyDescriptor(index, info);
}

static void InstallV8TestInheritedLegacyUnenumerableNamedPropertiesTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interface_template, V8TestInheritedLegacyUnenumerableNamedProperties::GetWrapperTypeInfo()->interface_name, V8TestSpecialOperationsNotEnumerable::DomTemplate(isolate, world), V8TestInheritedLegacyUnenumerableNamedProperties::kInternalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.
  static constexpr V8DOMConfiguration::AccessorConfiguration
  kAccessorConfigurations[] = {
      { "longAttribute", V8TestInheritedLegacyUnenumerableNamedProperties::LongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
  };
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instance_template, prototype_template, interface_template,
      signature, kAccessorConfigurations,
      base::size(kAccessorConfigurations));

  // Indexed properties
  v8::IndexedPropertyHandlerConfiguration indexedPropertyHandlerConfig(
      V8TestInheritedLegacyUnenumerableNamedProperties::IndexedPropertyGetterCallback,
      nullptr,
      V8TestInheritedLegacyUnenumerableNamedProperties::IndexedPropertyDescriptorCallback,
      nullptr,
      nullptr,
      nullptr,
      v8::Local<v8::Value>(),
      v8::PropertyHandlerFlags::kNone);
  instance_template->SetHandler(indexedPropertyHandlerConfig);
  // Named properties
  v8::NamedPropertyHandlerConfiguration namedPropertyHandlerConfig(V8TestInheritedLegacyUnenumerableNamedProperties::NamedPropertyGetterCallback, nullptr, V8TestInheritedLegacyUnenumerableNamedProperties::NamedPropertyQueryCallback, nullptr, V8TestInheritedLegacyUnenumerableNamedProperties::NamedPropertyEnumeratorCallback, v8::Local<v8::Value>(), static_cast<v8::PropertyHandlerFlags>(int(v8::PropertyHandlerFlags::kOnlyInterceptStrings) | int(v8::PropertyHandlerFlags::kNonMasking)));
  instance_template->SetHandler(namedPropertyHandlerConfig);

  // Custom signature

  V8TestInheritedLegacyUnenumerableNamedProperties::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interface_template);
}

void V8TestInheritedLegacyUnenumerableNamedProperties::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestInheritedLegacyUnenumerableNamedProperties::DomTemplate(
    v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world, const_cast<WrapperTypeInfo*>(V8TestInheritedLegacyUnenumerableNamedProperties::GetWrapperTypeInfo()),
      InstallV8TestInheritedLegacyUnenumerableNamedPropertiesTemplate);
}

bool V8TestInheritedLegacyUnenumerableNamedProperties::HasInstance(v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(V8TestInheritedLegacyUnenumerableNamedProperties::GetWrapperTypeInfo(), v8_value);
}

v8::Local<v8::Object> V8TestInheritedLegacyUnenumerableNamedProperties::FindInstanceInPrototypeChain(
    v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(
      V8TestInheritedLegacyUnenumerableNamedProperties::GetWrapperTypeInfo(), v8_value);
}

TestInheritedLegacyUnenumerableNamedProperties* V8TestInheritedLegacyUnenumerableNamedProperties::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInheritedLegacyUnenumerableNamedProperties* NativeValueTraits<TestInheritedLegacyUnenumerableNamedProperties>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestInheritedLegacyUnenumerableNamedProperties* native_value = V8TestInheritedLegacyUnenumerableNamedProperties::ToImplWithTypeCheck(isolate, value);
  if (!native_value) {
    exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInheritedLegacyUnenumerableNamedProperties"));
  }
  return native_value;
}

}  // namespace blink
