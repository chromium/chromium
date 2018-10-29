// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_callback_functions.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_any_callback_function_optional_any_arg.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_long_callback_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_callback_function.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_processing_stack.h"
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
const WrapperTypeInfo V8TestCallbackFunctions::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestCallbackFunctions::domTemplate,
    nullptr,
    "TestCallbackFunctions",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestCallbackFunctions.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestCallbackFunctions::wrapper_type_info_ = V8TestCallbackFunctions::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestCallbackFunctions>::value,
    "TestCallbackFunctions inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestCallbackFunctions::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestCallbackFunctions is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_callback_functions_v8_internal {

static void customElementsCallbacksReadonlyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(holder);

  V8SetReturnValueInt(info, impl->customElementsCallbacksReadonlyAttribute());
}

static void returnCallbackFunctionMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->returnCallbackFunctionMethod());
}

static void returnCallbackFunctionMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->returnCallbackFunctionMethod2());
}

static void voidMethodCallbackFunctionInArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodCallbackFunctionInArg", "TestCallbackFunctions", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8VoidCallbackFunction* voidCallbackFunctionArg;
  if (info[0]->IsFunction()) {
    voidCallbackFunctionArg = V8VoidCallbackFunction::Create(info[0].As<v8::Function>());
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodCallbackFunctionInArg", "TestCallbackFunctions", "The callback provided as parameter 1 is not a function."));
    return;
  }

  impl->voidMethodCallbackFunctionInArg(voidCallbackFunctionArg);
}

static void voidMethodCallbackFunctionInArg2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodCallbackFunctionInArg2", "TestCallbackFunctions", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8AnyCallbackFunctionOptionalAnyArg* anyCallbackFunctionOptionalAnyArgArg;
  if (info[0]->IsFunction()) {
    anyCallbackFunctionOptionalAnyArgArg = V8AnyCallbackFunctionOptionalAnyArg::Create(info[0].As<v8::Function>());
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodCallbackFunctionInArg2", "TestCallbackFunctions", "The callback provided as parameter 1 is not a function."));
    return;
  }

  impl->voidMethodCallbackFunctionInArg2(anyCallbackFunctionOptionalAnyArgArg);
}

static void voidMethodCallbackFunctionWithReturnValueInArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodCallbackFunctionWithReturnValueInArg", "TestCallbackFunctions", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8LongCallbackFunction* longCallbackFunctionArg;
  if (info[0]->IsFunction()) {
    longCallbackFunctionArg = V8LongCallbackFunction::Create(info[0].As<v8::Function>());
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodCallbackFunctionWithReturnValueInArg", "TestCallbackFunctions", "The callback provided as parameter 1 is not a function."));
    return;
  }

  impl->voidMethodCallbackFunctionWithReturnValueInArg(longCallbackFunctionArg);
}

static void voidMethodOptionalCallbackFunctionInArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  V8VoidCallbackFunction* voidCallbackFunctionArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (UNLIKELY(numArgsPassed <= 0)) {
    impl->voidMethodOptionalCallbackFunctionInArg();
    return;
  }
  if (info[0]->IsFunction()) {
    voidCallbackFunctionArg = V8VoidCallbackFunction::Create(info[0].As<v8::Function>());
  } else if (info[0]->IsUndefined()) {
    voidCallbackFunctionArg = nullptr;
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodOptionalCallbackFunctionInArg", "TestCallbackFunctions", "The callback provided as parameter 1 is not a function."));
    return;
  }

  impl->voidMethodOptionalCallbackFunctionInArg(voidCallbackFunctionArg);
}

static void voidMethodNullableCallbackFunctionInArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNullableCallbackFunctionInArg", "TestCallbackFunctions", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8VoidCallbackFunction* voidCallbackFunctionArg;
  if (info[0]->IsFunction()) {
    voidCallbackFunctionArg = V8VoidCallbackFunction::Create(info[0].As<v8::Function>());
  } else if (info[0]->IsNullOrUndefined()) {
    voidCallbackFunctionArg = nullptr;
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNullableCallbackFunctionInArg", "TestCallbackFunctions", "The callback provided as parameter 1 is not a function."));
    return;
  }

  impl->voidMethodNullableCallbackFunctionInArg(voidCallbackFunctionArg);
}

static void customElementCallbacksMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  V0CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;

  impl->customElementCallbacksMethod();
}

}  // namespace test_callback_functions_v8_internal

void V8TestCallbackFunctions::customElementsCallbacksReadonlyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_customElementsCallbacksReadonlyAttribute_Getter");

  test_callback_functions_v8_internal::customElementsCallbacksReadonlyAttributeAttributeGetter(info);
}

void V8TestCallbackFunctions::returnCallbackFunctionMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_returnCallbackFunctionMethod");

  test_callback_functions_v8_internal::returnCallbackFunctionMethodMethod(info);
}

void V8TestCallbackFunctions::returnCallbackFunctionMethod2MethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_returnCallbackFunctionMethod2");

  test_callback_functions_v8_internal::returnCallbackFunctionMethod2Method(info);
}

void V8TestCallbackFunctions::voidMethodCallbackFunctionInArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_voidMethodCallbackFunctionInArg");

  test_callback_functions_v8_internal::voidMethodCallbackFunctionInArgMethod(info);
}

void V8TestCallbackFunctions::voidMethodCallbackFunctionInArg2MethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_voidMethodCallbackFunctionInArg2");

  test_callback_functions_v8_internal::voidMethodCallbackFunctionInArg2Method(info);
}

void V8TestCallbackFunctions::voidMethodCallbackFunctionWithReturnValueInArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_voidMethodCallbackFunctionWithReturnValueInArg");

  test_callback_functions_v8_internal::voidMethodCallbackFunctionWithReturnValueInArgMethod(info);
}

void V8TestCallbackFunctions::voidMethodOptionalCallbackFunctionInArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_voidMethodOptionalCallbackFunctionInArg");

  test_callback_functions_v8_internal::voidMethodOptionalCallbackFunctionInArgMethod(info);
}

void V8TestCallbackFunctions::voidMethodNullableCallbackFunctionInArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_voidMethodNullableCallbackFunctionInArg");

  test_callback_functions_v8_internal::voidMethodNullableCallbackFunctionInArgMethod(info);
}

void V8TestCallbackFunctions::customElementCallbacksMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_customElementCallbacksMethod");

  test_callback_functions_v8_internal::customElementCallbacksMethodMethod(info);
}

static const V8DOMConfiguration::AccessorConfiguration V8TestCallbackFunctionsAccessors[] = {
    { "customElementsCallbacksReadonlyAttribute", V8TestCallbackFunctions::customElementsCallbacksReadonlyAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
};

static const V8DOMConfiguration::MethodConfiguration V8TestCallbackFunctionsMethods[] = {
    {"returnCallbackFunctionMethod", V8TestCallbackFunctions::returnCallbackFunctionMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"returnCallbackFunctionMethod2", V8TestCallbackFunctions::returnCallbackFunctionMethod2MethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodCallbackFunctionInArg", V8TestCallbackFunctions::voidMethodCallbackFunctionInArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodCallbackFunctionInArg2", V8TestCallbackFunctions::voidMethodCallbackFunctionInArg2MethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodCallbackFunctionWithReturnValueInArg", V8TestCallbackFunctions::voidMethodCallbackFunctionWithReturnValueInArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOptionalCallbackFunctionInArg", V8TestCallbackFunctions::voidMethodOptionalCallbackFunctionInArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodNullableCallbackFunctionInArg", V8TestCallbackFunctions::voidMethodNullableCallbackFunctionInArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"customElementCallbacksMethod", V8TestCallbackFunctions::customElementCallbacksMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

static void installV8TestCallbackFunctionsTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestCallbackFunctions::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestCallbackFunctions::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestCallbackFunctionsAccessors, base::size(V8TestCallbackFunctionsAccessors));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestCallbackFunctionsMethods, base::size(V8TestCallbackFunctionsMethods));

  // Custom signature

  V8TestCallbackFunctions::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestCallbackFunctions::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestCallbackFunctions::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestCallbackFunctionsTemplate);
}

bool V8TestCallbackFunctions::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestCallbackFunctions::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestCallbackFunctions* V8TestCallbackFunctions::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestCallbackFunctions* NativeValueTraits<TestCallbackFunctions>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestCallbackFunctions* nativeValue = V8TestCallbackFunctions::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestCallbackFunctions"));
  }
  return nativeValue;
}

}  // namespace blink
