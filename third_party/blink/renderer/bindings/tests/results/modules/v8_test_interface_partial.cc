// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/partial_interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/modules/v8_test_interface_partial.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_document.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_callback_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface.h"
#include "third_party/blink/renderer/bindings/tests/idls/modules/test_interface_partial_3_implementation.h"
#include "third_party/blink/renderer/bindings/tests/idls/modules/test_interface_partial_4.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

namespace test_interface_implementation_partial_v8_internal {

static void partial4LongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, TestInterfacePartial4::partial4LongAttribute(*impl));
}

static void partial4LongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partial4LongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartial4::setPartial4LongAttribute(*impl, cppValue);
}

static void partial4StaticLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueInt(info, TestInterfacePartial4::partial4StaticLongAttribute());
}

static void partial4StaticLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partial4StaticLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartial4::setPartial4StaticLongAttribute(cppValue);
}

static void voidMethodPartialOverload3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  V8StringResource<> value;
  value = info[0];
  if (!value.Prepare())
    return;

  TestInterfacePartial3Implementation::voidMethodPartialOverload(*impl, value);
}

static void voidMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (true) {
        voidMethodPartialOverload3Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "voidMethodPartialOverload");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void staticVoidMethodPartialOverload2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8StringResource<> value;
  value = info[0];
  if (!value.Prepare())
    return;

  TestInterfacePartial3Implementation::staticVoidMethodPartialOverload(value);
}

static void staticVoidMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (true) {
        staticVoidMethodPartialOverload2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "staticVoidMethodPartialOverload");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void promiseMethodPartialOverload3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "promiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestInterface::hasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  Document* document;
  document = V8Document::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!document) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Document'.");
    return;
  }

  V8SetReturnValue(info, TestInterfacePartial3Implementation::promiseMethodPartialOverload(*impl, document).V8Value());
}

static void promiseMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (V8Document::hasInstance(info[0], info.GetIsolate())) {
        promiseMethodPartialOverload3Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "promiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void staticPromiseMethodPartialOverload2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "staticPromiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  V8StringResource<> value;
  value = info[0];
  if (!value.Prepare(exceptionState))
    return;

  V8SetReturnValue(info, TestInterfacePartial3Implementation::staticPromiseMethodPartialOverload(value).V8Value());
}

static void staticPromiseMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (true) {
        staticPromiseMethodPartialOverload2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "staticPromiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void partial2VoidMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  V8StringResource<> value;
  value = info[0];
  if (!value.Prepare())
    return;

  TestInterfacePartial3Implementation::partial2VoidMethod(*impl, value);
}

static void partial2VoidMethod3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  Node* node;
  node = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!node) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("partial2VoidMethod", "TestInterface", "parameter 1 is not of type 'Node'."));
    return;
  }

  TestInterfacePartial3Implementation::partial2VoidMethod(*impl, node);
}

static void partial2VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (V8Node::hasInstance(info[0], info.GetIsolate())) {
        partial2VoidMethod3Method(info);
        return;
      }
      if (true) {
        partial2VoidMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "partial2VoidMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void partialVoidTestEnumModulesArgMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "partialVoidTestEnumModulesArgMethod");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> arg;
  arg = info[0];
  if (!arg.Prepare())
    return;
  const char* validArgValues[] = {
      "EnumModulesValue1",
      "EnumModulesValue2",
  };
  if (!IsValidEnum(arg, validArgValues, base::size(validArgValues), "TestEnumModules", exceptionState)) {
    return;
  }

  TestInterfacePartial3Implementation::partialVoidTestEnumModulesArgMethod(*impl, arg);
}

static void partial2StaticVoidMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8StringResource<> value;
  value = info[0];
  if (!value.Prepare())
    return;

  TestInterfacePartial3Implementation::partial2StaticVoidMethod(value);
}

static void partial2StaticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (true) {
        partial2StaticVoidMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "partial2StaticVoidMethod");
  if (isArityError) {
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void partial2VoidTestEnumModulesRecordMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "partial2VoidTestEnumModulesRecordMethod");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  Vector<std::pair<String, String>> arg;
  arg = NativeValueTraits<IDLRecord<IDLString, IDLString>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartial3Implementation::partial2VoidTestEnumModulesRecordMethod(*impl, arg);
}

static void unscopableVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartial3Implementation::unscopableVoidMethod(*impl);
}

static void unionWithTypedefMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  UnsignedLongLongOrBooleanOrTestCallbackInterface result;
  TestInterfacePartial3Implementation::unionWithTypedefMethod(*impl, result);
  V8SetReturnValue(info, result);
}

static void partial4VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartial4::partial4VoidMethod(*impl);
}

static void partial4StaticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfacePartial4::partial4StaticVoidMethod();
}

}  // namespace test_interface_implementation_partial_v8_internal

void V8TestInterfacePartial::partial4LongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial4LongAttribute_Getter");

  test_interface_implementation_partial_v8_internal::partial4LongAttributeAttributeGetter(info);
}

void V8TestInterfacePartial::partial4LongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial4LongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_partial_v8_internal::partial4LongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfacePartial::partial4StaticLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial4StaticLongAttribute_Getter");

  test_interface_implementation_partial_v8_internal::partial4StaticLongAttributeAttributeGetter(info);
}

void V8TestInterfacePartial::partial4StaticLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial4StaticLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_partial_v8_internal::partial4StaticLongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfacePartial::partialVoidTestEnumModulesArgMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialVoidTestEnumModulesArgMethod");

  test_interface_implementation_partial_v8_internal::partialVoidTestEnumModulesArgMethodMethod(info);
}

void V8TestInterfacePartial::partial2VoidTestEnumModulesRecordMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2VoidTestEnumModulesRecordMethod");

  test_interface_implementation_partial_v8_internal::partial2VoidTestEnumModulesRecordMethodMethod(info);
}

void V8TestInterfacePartial::unscopableVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_unscopableVoidMethod");

  test_interface_implementation_partial_v8_internal::unscopableVoidMethodMethod(info);
}

void V8TestInterfacePartial::unionWithTypedefMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_unionWithTypedefMethod");

  test_interface_implementation_partial_v8_internal::unionWithTypedefMethodMethod(info);
}

void V8TestInterfacePartial::partial4VoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial4VoidMethod");

  test_interface_implementation_partial_v8_internal::partial4VoidMethodMethod(info);
}

void V8TestInterfacePartial::partial4StaticVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial4StaticVoidMethod");

  test_interface_implementation_partial_v8_internal::partial4StaticVoidMethodMethod(info);
}

static const V8DOMConfiguration::MethodConfiguration V8TestInterfaceMethods[] = {
    {"partialVoidTestEnumModulesArgMethod", V8TestInterfacePartial::partialVoidTestEnumModulesArgMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"partial2VoidTestEnumModulesRecordMethod", V8TestInterfacePartial::partial2VoidTestEnumModulesRecordMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"unscopableVoidMethod", V8TestInterfacePartial::unscopableVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"unionWithTypedefMethod", V8TestInterfacePartial::unionWithTypedefMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

void V8TestInterfacePartial::installV8TestInterfaceTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8TestInterface::installV8TestInterfaceTemplate(isolate, world, interfaceTemplate);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  static constexpr V8DOMConfiguration::ConstantConfiguration V8TestInterfaceConstants[] = {
      {"PARTIAL3_UNSIGNED_SHORT", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(0)},
  };
  V8DOMConfiguration::InstallConstants(
      isolate, interfaceTemplate, prototypeTemplate,
      V8TestInterfaceConstants, base::size(V8TestInterfaceConstants));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterfaceMethods, base::size(V8TestInterfaceMethods));

  // Custom signature

  V8TestInterfacePartial::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterfacePartial::InstallRuntimeEnabledFeaturesOnTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  V8TestInterface::InstallRuntimeEnabledFeaturesOnTemplate(isolate, world, interface_template);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.

  // Custom signature
}

void V8TestInterfacePartial::installOriginTrialPartialFeature(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::Object> instance, v8::Local<v8::Object> prototype, v8::Local<v8::Function> interface) {
  v8::Local<v8::FunctionTemplate> interfaceTemplate = V8TestInterface::wrapperTypeInfo.domTemplate(isolate, world);
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  ExecutionContext* executionContext = ToExecutionContext(isolate->GetCurrentContext());
  bool isSecureContext = (executionContext && executionContext->IsSecureContext());
  if (isSecureContext) {
    static const V8DOMConfiguration::AccessorConfiguration accessorpartial4LongAttributeConfiguration[] = {
      { "partial4LongAttribute", V8TestInterfacePartial::partial4LongAttributeAttributeGetterCallback, V8TestInterfacePartial::partial4LongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds }
    };
    for (const auto& accessorConfig : accessorpartial4LongAttributeConfiguration)
      V8DOMConfiguration::InstallAccessor(isolate, world, instance, prototype, interface, signature, accessorConfig);
  }
  if (isSecureContext) {
    static const V8DOMConfiguration::AccessorConfiguration accessorpartial4StaticLongAttributeConfiguration[] = {
      { "partial4StaticLongAttribute", V8TestInterfacePartial::partial4StaticLongAttributeAttributeGetterCallback, V8TestInterfacePartial::partial4StaticLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds }
    };
    for (const auto& accessorConfig : accessorpartial4StaticLongAttributeConfiguration)
      V8DOMConfiguration::InstallAccessor(isolate, world, instance, prototype, interface, signature, accessorConfig);
  }
  const V8DOMConfiguration::ConstantConfiguration constantPartial4UnsignedShortConfiguration = {"PARTIAL4_UNSIGNED_SHORT", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(4)};
  V8DOMConfiguration::InstallConstant(isolate, interface, prototype, constantPartial4UnsignedShortConfiguration);
  if (isSecureContext) {
    static const V8DOMConfiguration::MethodConfiguration methodPartial4StaticvoidmethodConfiguration[] = {
      {"partial4StaticVoidMethod", V8TestInterfacePartial::partial4StaticVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : methodPartial4StaticvoidmethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance, prototype, interface, signature, methodConfig);
  }
  if (isSecureContext) {
    static const V8DOMConfiguration::MethodConfiguration methodPartial4VoidmethodConfiguration[] = {
      {"partial4VoidMethod", V8TestInterfacePartial::partial4VoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : methodPartial4VoidmethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance, prototype, interface, signature, methodConfig);
  }
}

void V8TestInterfacePartial::installOriginTrialPartialFeature(ScriptState* scriptState, v8::Local<v8::Object> instance) {
  V8PerContextData* perContextData = scriptState->PerContextData();
  v8::Local<v8::Object> prototype = perContextData->PrototypeForType(&V8TestInterface::wrapperTypeInfo);
  v8::Local<v8::Function> interface = perContextData->ConstructorForType(&V8TestInterface::wrapperTypeInfo);
  ALLOW_UNUSED_LOCAL(interface);
  installOriginTrialPartialFeature(scriptState->GetIsolate(), scriptState->World(), instance, prototype, interface);
}

void V8TestInterfacePartial::installOriginTrialPartialFeature(ScriptState* scriptState) {
  installOriginTrialPartialFeature(scriptState, v8::Local<v8::Object>());
}

void V8TestInterfacePartial::InstallConditionalFeatures(
    v8::Local<v8::Context> context,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instanceObject,
    v8::Local<v8::Object> prototypeObject,
    v8::Local<v8::Function> interfaceObject,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  CHECK(!interfaceTemplate.IsEmpty());
  DCHECK((!prototypeObject.IsEmpty() && !interfaceObject.IsEmpty()) ||
         !instanceObject.IsEmpty());
  V8TestInterface::InstallConditionalFeatures(
      context, world, instanceObject, prototypeObject, interfaceObject, interfaceTemplate);

  v8::Isolate* isolate = context->GetIsolate();

  if (!prototypeObject.IsEmpty()) {
    v8::Local<v8::Name> unscopablesSymbol(v8::Symbol::GetUnscopables(isolate));
    v8::Local<v8::Object> unscopables;
    bool has_unscopables;
    if (prototypeObject->HasOwnProperty(context, unscopablesSymbol).To(&has_unscopables) && has_unscopables) {
      unscopables = prototypeObject->Get(context, unscopablesSymbol).ToLocalChecked().As<v8::Object>();
    } else {
      // Web IDL 3.6.3. Interface prototype object
      // https://heycam.github.io/webidl/#create-an-interface-prototype-object
      // step 8.1. Let unscopableObject be the result of performing
      //   ! ObjectCreate(null).
      unscopables = v8::Object::New(isolate);
      unscopables->SetPrototype(context, v8::Null(isolate)).ToChecked();
    }
    unscopables->CreateDataProperty(
        context, V8AtomicString(isolate, "unscopableVoidMethod"), v8::True(isolate))
        .FromJust();
    prototypeObject->CreateDataProperty(context, unscopablesSymbol, unscopables).FromJust();
  }
}

void V8TestInterfacePartial::initialize() {
  // Should be invoked from ModulesInitializer.
  V8TestInterface::UpdateWrapperTypeInfo(
      &V8TestInterfacePartial::installV8TestInterfaceTemplate,
      nullptr,
      &V8TestInterfacePartial::InstallRuntimeEnabledFeaturesOnTemplate,
      V8TestInterfacePartial::InstallConditionalFeatures);
  V8TestInterface::registerVoidMethodPartialOverloadMethodForPartialInterface(&test_interface_implementation_partial_v8_internal::voidMethodPartialOverloadMethod);
  V8TestInterface::registerStaticVoidMethodPartialOverloadMethodForPartialInterface(&test_interface_implementation_partial_v8_internal::staticVoidMethodPartialOverloadMethod);
  V8TestInterface::registerPromiseMethodPartialOverloadMethodForPartialInterface(&test_interface_implementation_partial_v8_internal::promiseMethodPartialOverloadMethod);
  V8TestInterface::registerStaticPromiseMethodPartialOverloadMethodForPartialInterface(&test_interface_implementation_partial_v8_internal::staticPromiseMethodPartialOverloadMethod);
  V8TestInterface::registerPartial2VoidMethodMethodForPartialInterface(&test_interface_implementation_partial_v8_internal::partial2VoidMethodMethod);
  V8TestInterface::registerPartial2StaticVoidMethodMethodForPartialInterface(&test_interface_implementation_partial_v8_internal::partial2StaticVoidMethodMethod);
}

}  // namespace blink
