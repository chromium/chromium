// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_named_constructor.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
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
const WrapperTypeInfo V8TestInterfaceNamedConstructor::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceNamedConstructor::domTemplate,
    nullptr,
    "TestInterfaceNamedConstructor",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceNamedConstructor.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceNamedConstructor::wrapper_type_info_ = V8TestInterfaceNamedConstructor::wrapperTypeInfo;

// [ActiveScriptWrappable]
static_assert(
    std::is_base_of<ActiveScriptWrappableBase, TestInterfaceNamedConstructor>::value,
    "TestInterfaceNamedConstructor does not inherit from ActiveScriptWrappable<>, but specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    !std::is_same<decltype(&TestInterfaceNamedConstructor::HasPendingActivity),
                  decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceNamedConstructor is not overriding hasPendingActivity(), but is specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_named_constructor_v8_internal {

}  // namespace test_interface_named_constructor_v8_internal

void V8TestInterfaceNamedConstructor::testNamedConstructorConstructorAttributeConstructorGetterCallback(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNamedConstructor_testNamedConstructorConstructorAttribute_ConstructorGetterCallback");

  V8TestNamedConstructor::NamedConstructorAttributeGetter(property, info);
}

// Suppress warning: global constructors, because AttributeConfiguration is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
static const V8DOMConfiguration::AttributeConfiguration V8TestInterfaceNamedConstructorAttributes[] = {
    { "testNamedConstructorConstructorAttribute", V8TestInterfaceNamedConstructor::testNamedConstructorConstructorAttributeConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kReplaceWithDataProperty, V8DOMConfiguration::kAllWorlds },
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestInterfaceNamedConstructorConstructor::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceNamedConstructorConstructor::domTemplate,
    nullptr,
    "TestInterfaceNamedConstructor",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

static void V8TestInterfaceNamedConstructorConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceNamedConstructor_ConstructorCallback");

  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::ConstructorNotCallableAsFunction("Audio"));
    return;
  }

  if (ConstructorMode::Current(info.GetIsolate()) == ConstructorMode::kWrapExistingObject) {
    V8SetReturnValue(info, info.Holder());
    return;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceNamedConstructor");

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> stringArg;
  bool defaultUndefinedOptionalBooleanArg;
  int32_t defaultUndefinedOptionalLongArg;
  V8StringResource<> defaultUndefinedOptionalStringArg;
  V8StringResource<> defaultNullStringOptionalstringArg;
  V8StringResource<> optionalStringArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  defaultUndefinedOptionalBooleanArg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  defaultUndefinedOptionalLongArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[2], exceptionState);
  if (exceptionState.HadException())
    return;

  defaultUndefinedOptionalStringArg = info[3];
  if (!defaultUndefinedOptionalStringArg.Prepare())
    return;

  if (!info[4]->IsUndefined()) {
    defaultNullStringOptionalstringArg = info[4];
    if (!defaultNullStringOptionalstringArg.Prepare())
      return;
  } else {
    defaultNullStringOptionalstringArg = nullptr;
  }
  if (UNLIKELY(numArgsPassed <= 5)) {
    Document& document = *To<Document>(ToExecutionContext(
        info.NewTarget().As<v8::Object>()->CreationContext()));
    TestInterfaceNamedConstructor* impl = TestInterfaceNamedConstructor::CreateForJSConstructor(document, stringArg, defaultUndefinedOptionalBooleanArg, defaultUndefinedOptionalLongArg, defaultUndefinedOptionalStringArg, defaultNullStringOptionalstringArg, exceptionState);
    if (exceptionState.HadException()) {
      return;
    }
    v8::Local<v8::Object> wrapper = info.Holder();
    wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceNamedConstructorConstructor::wrapperTypeInfo, wrapper);
    V8SetReturnValue(info, wrapper);
    return;
  }
  optionalStringArg = info[5];
  if (!optionalStringArg.Prepare())
    return;

  Document& document = *To<Document>(ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext()));
  TestInterfaceNamedConstructor* impl = TestInterfaceNamedConstructor::CreateForJSConstructor(document, stringArg, defaultUndefinedOptionalBooleanArg, defaultUndefinedOptionalLongArg, defaultUndefinedOptionalStringArg, defaultNullStringOptionalstringArg, optionalStringArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceNamedConstructorConstructor::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

v8::Local<v8::FunctionTemplate> V8TestInterfaceNamedConstructorConstructor::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  static int domTemplateKey; // This address is used for a key to look up the dom template.
  V8PerIsolateData* data = V8PerIsolateData::From(isolate);
  v8::Local<v8::FunctionTemplate> result = data->FindInterfaceTemplate(world, &domTemplateKey);
  if (!result.IsEmpty())
    return result;

  result = v8::FunctionTemplate::New(isolate, V8TestInterfaceNamedConstructorConstructorCallback);
  v8::Local<v8::ObjectTemplate> instanceTemplate = result->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(V8TestInterfaceNamedConstructor::internalFieldCount);
  result->SetClassName(V8AtomicString(isolate, "Audio"));
  result->Inherit(V8TestInterfaceNamedConstructor::domTemplate(isolate, world));
  data->SetInterfaceTemplate(world, &domTemplateKey, result);
  return result;
}

void V8TestInterfaceNamedConstructorConstructor::NamedConstructorAttributeGetter(
    v8::Local<v8::Name> propertyName,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Context> creationContext = info.Holder()->CreationContext();
  V8PerContextData* perContextData = V8PerContextData::From(creationContext);
  if (!perContextData) {
    // TODO(yukishiino): Return a valid named constructor even after the context is detached
    return;
  }

  v8::Local<v8::Function> namedConstructor = perContextData->ConstructorForType(&V8TestInterfaceNamedConstructorConstructor::wrapperTypeInfo);

  // Set the prototype of named constructors to the regular constructor.
  auto privateProperty = V8PrivateProperty::GetNamedConstructorInitialized(info.GetIsolate());
  v8::Local<v8::Context> currentContext = info.GetIsolate()->GetCurrentContext();
  v8::Local<v8::Value> privateValue;

  if (!privateProperty.GetOrUndefined(namedConstructor).ToLocal(&privateValue) || privateValue->IsUndefined()) {
    v8::Local<v8::Function> interface = perContextData->ConstructorForType(&V8TestInterfaceNamedConstructor::wrapperTypeInfo);
    v8::Local<v8::Value> interfacePrototype = interface->Get(currentContext, V8AtomicString(info.GetIsolate(), "prototype")).ToLocalChecked();
    bool result = namedConstructor->Set(currentContext, V8AtomicString(info.GetIsolate(), "prototype"), interfacePrototype).ToChecked();
    if (!result)
      return;
    privateProperty.Set(namedConstructor, v8::True(info.GetIsolate()));
  }

  V8SetReturnValue(info, namedConstructor);
}

static void installV8TestInterfaceNamedConstructorTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterfaceNamedConstructor::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceNamedConstructor::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  V8DOMConfiguration::InstallAttributes(
      isolate, world, instanceTemplate, prototypeTemplate,
      V8TestInterfaceNamedConstructorAttributes, base::size(V8TestInterfaceNamedConstructorAttributes));

  // Custom signature

  V8TestInterfaceNamedConstructor::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterfaceNamedConstructor::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceNamedConstructor::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterfaceNamedConstructorTemplate);
}

bool V8TestInterfaceNamedConstructor::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceNamedConstructor::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceNamedConstructor* V8TestInterfaceNamedConstructor::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceNamedConstructor* NativeValueTraits<TestInterfaceNamedConstructor>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceNamedConstructor* nativeValue = V8TestInterfaceNamedConstructor::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceNamedConstructor"));
  }
  return nativeValue;
}

}  // namespace blink
