// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_constructor_4.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_constructor_4.h"
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
const WrapperTypeInfo V8TestInterfaceConstructor4::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceConstructor4::domTemplate,
    nullptr,
    "TestInterfaceConstructor4",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceConstructor4.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceConstructor4::wrapper_type_info_ = V8TestInterfaceConstructor4::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceConstructor4>::value,
    "TestInterfaceConstructor4 inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceConstructor4::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceConstructor4 is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_constructor_4_v8_internal {

static void constructor1(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor4_ConstructorCallback");

  TestInterfaceConstructor4* testInterface4Arg;
  testInterface4Arg = V8TestInterfaceConstructor4::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterface4Arg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToConstruct("TestInterfaceConstructor4", "parameter 1 is not of type 'TestInterfaceConstructor4'."));
    return;
  }

  TestInterfaceConstructor4* impl = TestInterfaceConstructor4::Create(testInterface4Arg);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructor4::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

static void constructor2(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor4_ConstructorCallback");

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor4");

  V8StringResource<> usvStringArg;
  usvStringArg = NativeValueTraits<IDLUSVString>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfaceConstructor4* impl = TestInterfaceConstructor4::Create(usvStringArg);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructor4::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

static void constructor(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor4");
  switch (std::min(1, info.Length())) {
    case 1:
      if (V8TestInterfaceConstructor4::hasInstance(info[0], info.GetIsolate())) {
        test_interface_constructor_4_v8_internal::constructor1(info);
        return;
      }
      if (true) {
        test_interface_constructor_4_v8_internal::constructor2(info);
        return;
      }
      break;
    default:
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
  }
  exceptionState.ThrowTypeError("No matching constructor signature.");
}

}  // namespace test_interface_constructor_4_v8_internal

void V8TestInterfaceConstructor4::constructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor4_Constructor");

  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::ConstructorNotCallableAsFunction("TestInterfaceConstructor4"));
    return;
  }

  if (ConstructorMode::Current(info.GetIsolate()) == ConstructorMode::kWrapExistingObject) {
    V8SetReturnValue(info, info.Holder());
    return;
  }

  test_interface_constructor_4_v8_internal::constructor(info);
}

static void installV8TestInterfaceConstructor4Template(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterfaceConstructor4::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceConstructor4::internalFieldCount);
  interfaceTemplate->SetCallHandler(V8TestInterfaceConstructor4::constructorCallback);
  interfaceTemplate->SetLength(1);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.

  // Custom signature

  V8TestInterfaceConstructor4::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterfaceConstructor4::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceConstructor4::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterfaceConstructor4Template);
}

bool V8TestInterfaceConstructor4::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceConstructor4::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceConstructor4* V8TestInterfaceConstructor4::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceConstructor4* NativeValueTraits<TestInterfaceConstructor4>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceConstructor4* nativeValue = V8TestInterfaceConstructor4::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceConstructor4"));
  }
  return nativeValue;
}

}  // namespace blink
