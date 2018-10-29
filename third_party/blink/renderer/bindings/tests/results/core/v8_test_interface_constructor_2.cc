// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_constructor_2.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_empty.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
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
const WrapperTypeInfo V8TestInterfaceConstructor2::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceConstructor2::domTemplate,
    nullptr,
    "TestInterfaceConstructor2",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceConstructor2.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceConstructor2::wrapper_type_info_ = V8TestInterfaceConstructor2::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceConstructor2>::value,
    "TestInterfaceConstructor2 inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceConstructor2::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceConstructor2 is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_constructor_2_v8_internal {

static void constructor1(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor2_ConstructorCallback");

  V8StringResource<> stringArg;
  stringArg = info[0];
  if (!stringArg.Prepare())
    return;

  TestInterfaceConstructor2* impl = TestInterfaceConstructor2::Create(stringArg);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructor2::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

static void constructor2(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor2_ConstructorCallback");

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor2");

  Dictionary dictionaryArg;
  if (!info[0]->IsNullOrUndefined() && !info[0]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 1 ('dictionaryArg') is not an object.");
    return;
  }
  dictionaryArg = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfaceConstructor2* impl = TestInterfaceConstructor2::Create(dictionaryArg);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructor2::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

static void constructor3(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor2_ConstructorCallback");

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor2");

  Vector<Vector<String>> stringSequenceSequenceArg;
  stringSequenceSequenceArg = NativeValueTraits<IDLSequence<IDLSequence<IDLString>>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfaceConstructor2* impl = TestInterfaceConstructor2::Create(stringSequenceSequenceArg);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructor2::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

static void constructor4(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor2_ConstructorCallback");

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor2");

  TestInterfaceEmpty* testInterfaceEmptyArg;
  int32_t longArg;
  V8StringResource<> defaultUndefinedOptionalStringArg;
  V8StringResource<> defaultNullStringOptionalStringArg;
  Dictionary defaultUndefinedOptionalDictionaryArg;
  V8StringResource<> optionalStringArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  defaultUndefinedOptionalStringArg = info[2];
  if (!defaultUndefinedOptionalStringArg.Prepare())
    return;

  if (!info[3]->IsUndefined()) {
    defaultNullStringOptionalStringArg = info[3];
    if (!defaultNullStringOptionalStringArg.Prepare())
      return;
  } else {
    defaultNullStringOptionalStringArg = nullptr;
  }
  if (!info[4]->IsNullOrUndefined() && !info[4]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 5 ('defaultUndefinedOptionalDictionaryArg') is not an object.");
    return;
  }
  defaultUndefinedOptionalDictionaryArg = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[4], exceptionState);
  if (exceptionState.HadException())
    return;

  if (UNLIKELY(numArgsPassed <= 5)) {
    TestInterfaceConstructor2* impl = TestInterfaceConstructor2::Create(testInterfaceEmptyArg, longArg, defaultUndefinedOptionalStringArg, defaultNullStringOptionalStringArg, defaultUndefinedOptionalDictionaryArg);
    v8::Local<v8::Object> wrapper = info.Holder();
    wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructor2::wrapperTypeInfo, wrapper);
    V8SetReturnValue(info, wrapper);
    return;
  }
  optionalStringArg = info[5];
  if (!optionalStringArg.Prepare())
    return;

  TestInterfaceConstructor2* impl = TestInterfaceConstructor2::Create(testInterfaceEmptyArg, longArg, defaultUndefinedOptionalStringArg, defaultNullStringOptionalStringArg, defaultUndefinedOptionalDictionaryArg, optionalStringArg);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructor2::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

static void constructor(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor2");
  switch (std::min(6, info.Length())) {
    case 1:
      if (info[0]->IsArray()) {
        test_interface_constructor_2_v8_internal::constructor3(info);
        return;
      }
      if (HasCallableIteratorSymbol(info.GetIsolate(), info[0], exceptionState)) {
        test_interface_constructor_2_v8_internal::constructor3(info);
        return;
      }
      if (exceptionState.HadException()) {
        exceptionState.RethrowV8Exception(exceptionState.GetException());
        return;
      }
      if (info[0]->IsObject()) {
        test_interface_constructor_2_v8_internal::constructor2(info);
        return;
      }
      if (true) {
        test_interface_constructor_2_v8_internal::constructor1(info);
        return;
      }
      break;
    case 2:
      if (true) {
        test_interface_constructor_2_v8_internal::constructor4(info);
        return;
      }
      break;
    case 3:
      if (true) {
        test_interface_constructor_2_v8_internal::constructor4(info);
        return;
      }
      break;
    case 4:
      if (true) {
        test_interface_constructor_2_v8_internal::constructor4(info);
        return;
      }
      break;
    case 5:
      if (true) {
        test_interface_constructor_2_v8_internal::constructor4(info);
        return;
      }
      break;
    case 6:
      if (true) {
        test_interface_constructor_2_v8_internal::constructor4(info);
        return;
      }
      break;
    default:
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
  }
  exceptionState.ThrowTypeError("No matching constructor signature.");
}

}  // namespace test_interface_constructor_2_v8_internal

void V8TestInterfaceConstructor2::constructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor2_Constructor");

  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::ConstructorNotCallableAsFunction("TestInterfaceConstructor2"));
    return;
  }

  if (ConstructorMode::Current(info.GetIsolate()) == ConstructorMode::kWrapExistingObject) {
    V8SetReturnValue(info, info.Holder());
    return;
  }

  test_interface_constructor_2_v8_internal::constructor(info);
}

static void installV8TestInterfaceConstructor2Template(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterfaceConstructor2::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceConstructor2::internalFieldCount);
  interfaceTemplate->SetCallHandler(V8TestInterfaceConstructor2::constructorCallback);
  interfaceTemplate->SetLength(1);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.

  // Custom signature

  V8TestInterfaceConstructor2::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterfaceConstructor2::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceConstructor2::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterfaceConstructor2Template);
}

bool V8TestInterfaceConstructor2::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceConstructor2::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceConstructor2* V8TestInterfaceConstructor2::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceConstructor2* NativeValueTraits<TestInterfaceConstructor2>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceConstructor2* nativeValue = V8TestInterfaceConstructor2::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceConstructor2"));
  }
  return nativeValue;
}

}  // namespace blink
