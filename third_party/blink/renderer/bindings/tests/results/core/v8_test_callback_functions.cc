// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_callback_functions.h"

#include <algorithm>

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
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo v8_test_callback_functions_wrapper_type_info = {
    gin::kEmbedderBlink,
    V8TestCallbackFunctions::DomTemplate,
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
const WrapperTypeInfo& TestCallbackFunctions::wrapper_type_info_ = v8_test_callback_functions_wrapper_type_info;

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

static void CustomElementsCallbacksReadonlyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(holder);

  V8SetReturnValueInt(info, impl->customElementsCallbacksReadonlyAttribute());
}

static void ReturnCallbackFunctionMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->returnCallbackFunctionMethod());
}

static void ReturnCallbackFunctionMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->returnCallbackFunctionMethod2());
}

static void VoidMethodCallbackFunctionInArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodCallbackFunctionInArg", "TestCallbackFunctions", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8VoidCallbackFunction* void_callback_function_arg;
  if (info[0]->IsFunction()) {
    void_callback_function_arg = V8VoidCallbackFunction::Create(info[0].As<v8::Function>());
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodCallbackFunctionInArg", "TestCallbackFunctions", "The callback provided as parameter 1 is not a function."));
    return;
  }

  impl->voidMethodCallbackFunctionInArg(void_callback_function_arg);
}

static void VoidMethodCallbackFunctionInArg2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodCallbackFunctionInArg2", "TestCallbackFunctions", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8AnyCallbackFunctionOptionalAnyArg* any_callback_function_optional_any_arg_arg;
  if (info[0]->IsFunction()) {
    any_callback_function_optional_any_arg_arg = V8AnyCallbackFunctionOptionalAnyArg::Create(info[0].As<v8::Function>());
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodCallbackFunctionInArg2", "TestCallbackFunctions", "The callback provided as parameter 1 is not a function."));
    return;
  }

  impl->voidMethodCallbackFunctionInArg2(any_callback_function_optional_any_arg_arg);
}

static void VoidMethodCallbackFunctionWithReturnValueInArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodCallbackFunctionWithReturnValueInArg", "TestCallbackFunctions", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8LongCallbackFunction* long_callback_function_arg;
  if (info[0]->IsFunction()) {
    long_callback_function_arg = V8LongCallbackFunction::Create(info[0].As<v8::Function>());
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodCallbackFunctionWithReturnValueInArg", "TestCallbackFunctions", "The callback provided as parameter 1 is not a function."));
    return;
  }

  impl->voidMethodCallbackFunctionWithReturnValueInArg(long_callback_function_arg);
}

static void VoidMethodOptionalCallbackFunctionInArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  V8VoidCallbackFunction* void_callback_function_arg;
  int num_args_passed = info.Length();
  while (num_args_passed > 0) {
    if (!info[num_args_passed - 1]->IsUndefined())
      break;
    --num_args_passed;
  }
  if (UNLIKELY(num_args_passed <= 0)) {
    impl->voidMethodOptionalCallbackFunctionInArg();
    return;
  }
  if (info[0]->IsFunction()) {
    void_callback_function_arg = V8VoidCallbackFunction::Create(info[0].As<v8::Function>());
  } else if (info[0]->IsUndefined()) {
    void_callback_function_arg = nullptr;
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodOptionalCallbackFunctionInArg", "TestCallbackFunctions", "The callback provided as parameter 1 is not a function."));
    return;
  }

  impl->voidMethodOptionalCallbackFunctionInArg(void_callback_function_arg);
}

static void VoidMethodNullableCallbackFunctionInArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNullableCallbackFunctionInArg", "TestCallbackFunctions", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8VoidCallbackFunction* void_callback_function_arg;
  if (info[0]->IsFunction()) {
    void_callback_function_arg = V8VoidCallbackFunction::Create(info[0].As<v8::Function>());
  } else if (info[0]->IsNullOrUndefined()) {
    void_callback_function_arg = nullptr;
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNullableCallbackFunctionInArg", "TestCallbackFunctions", "The callback provided as parameter 1 is not a function."));
    return;
  }

  impl->voidMethodNullableCallbackFunctionInArg(void_callback_function_arg);
}

static void CustomElementCallbacksMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestCallbackFunctions* impl = V8TestCallbackFunctions::ToImpl(info.Holder());

  V0CustomElementProcessingStack::CallbackDeliveryScope delivery_scope;

  impl->customElementCallbacksMethod();
}

}  // namespace test_callback_functions_v8_internal

void V8TestCallbackFunctions::CustomElementsCallbacksReadonlyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_customElementsCallbacksReadonlyAttribute_Getter");

  test_callback_functions_v8_internal::CustomElementsCallbacksReadonlyAttributeAttributeGetter(info);
}

void V8TestCallbackFunctions::ReturnCallbackFunctionMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_returnCallbackFunctionMethod");

  test_callback_functions_v8_internal::ReturnCallbackFunctionMethodMethod(info);
}

void V8TestCallbackFunctions::ReturnCallbackFunctionMethod2MethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_returnCallbackFunctionMethod2");

  test_callback_functions_v8_internal::ReturnCallbackFunctionMethod2Method(info);
}

void V8TestCallbackFunctions::VoidMethodCallbackFunctionInArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_voidMethodCallbackFunctionInArg");

  test_callback_functions_v8_internal::VoidMethodCallbackFunctionInArgMethod(info);
}

void V8TestCallbackFunctions::VoidMethodCallbackFunctionInArg2MethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_voidMethodCallbackFunctionInArg2");

  test_callback_functions_v8_internal::VoidMethodCallbackFunctionInArg2Method(info);
}

void V8TestCallbackFunctions::VoidMethodCallbackFunctionWithReturnValueInArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_voidMethodCallbackFunctionWithReturnValueInArg");

  test_callback_functions_v8_internal::VoidMethodCallbackFunctionWithReturnValueInArgMethod(info);
}

void V8TestCallbackFunctions::VoidMethodOptionalCallbackFunctionInArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_voidMethodOptionalCallbackFunctionInArg");

  test_callback_functions_v8_internal::VoidMethodOptionalCallbackFunctionInArgMethod(info);
}

void V8TestCallbackFunctions::VoidMethodNullableCallbackFunctionInArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_voidMethodNullableCallbackFunctionInArg");

  test_callback_functions_v8_internal::VoidMethodNullableCallbackFunctionInArgMethod(info);
}

void V8TestCallbackFunctions::CustomElementCallbacksMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestCallbackFunctions_customElementCallbacksMethod");

  test_callback_functions_v8_internal::CustomElementCallbacksMethodMethod(info);
}

static constexpr V8DOMConfiguration::MethodConfiguration kV8TestCallbackFunctionsMethods[] = {
    {"returnCallbackFunctionMethod", V8TestCallbackFunctions::ReturnCallbackFunctionMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"returnCallbackFunctionMethod2", V8TestCallbackFunctions::ReturnCallbackFunctionMethod2MethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodCallbackFunctionInArg", V8TestCallbackFunctions::VoidMethodCallbackFunctionInArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodCallbackFunctionInArg2", V8TestCallbackFunctions::VoidMethodCallbackFunctionInArg2MethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodCallbackFunctionWithReturnValueInArg", V8TestCallbackFunctions::VoidMethodCallbackFunctionWithReturnValueInArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodOptionalCallbackFunctionInArg", V8TestCallbackFunctions::VoidMethodOptionalCallbackFunctionInArgMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodNullableCallbackFunctionInArg", V8TestCallbackFunctions::VoidMethodNullableCallbackFunctionInArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"customElementCallbacksMethod", V8TestCallbackFunctions::CustomElementCallbacksMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

static void InstallV8TestCallbackFunctionsTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interface_template, V8TestCallbackFunctions::GetWrapperTypeInfo()->interface_name, v8::Local<v8::FunctionTemplate>(), V8TestCallbackFunctions::kInternalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.
  static constexpr V8DOMConfiguration::AccessorConfiguration
  kAccessorConfigurations[] = {
      { "customElementsCallbacksReadonlyAttribute", V8TestCallbackFunctions::CustomElementsCallbacksReadonlyAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
  };
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instance_template, prototype_template, interface_template,
      signature, kAccessorConfigurations,
      base::size(kAccessorConfigurations));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instance_template, prototype_template, interface_template,
      signature, kV8TestCallbackFunctionsMethods, base::size(kV8TestCallbackFunctionsMethods));

  // Custom signature

  V8TestCallbackFunctions::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interface_template);
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

v8::Local<v8::FunctionTemplate> V8TestCallbackFunctions::DomTemplate(
    v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world, const_cast<WrapperTypeInfo*>(V8TestCallbackFunctions::GetWrapperTypeInfo()),
      InstallV8TestCallbackFunctionsTemplate);
}

bool V8TestCallbackFunctions::HasInstance(v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(V8TestCallbackFunctions::GetWrapperTypeInfo(), v8_value);
}

v8::Local<v8::Object> V8TestCallbackFunctions::FindInstanceInPrototypeChain(
    v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(
      V8TestCallbackFunctions::GetWrapperTypeInfo(), v8_value);
}

TestCallbackFunctions* V8TestCallbackFunctions::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestCallbackFunctions* NativeValueTraits<TestCallbackFunctions>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestCallbackFunctions* native_value = V8TestCallbackFunctions::ToImplWithTypeCheck(isolate, value);
  if (!native_value) {
    exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestCallbackFunctions"));
  }
  return native_value;
}

}  // namespace blink
