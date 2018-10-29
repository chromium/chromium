// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_conditional_secure_context.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestInterfaceConditionalSecureContext::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceConditionalSecureContext::domTemplate,
    V8TestInterfaceConditionalSecureContext::InstallConditionalFeatures,
    "TestInterfaceConditionalSecureContext",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceConditionalSecureContext.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceConditionalSecureContext::wrapper_type_info_ = V8TestInterfaceConditionalSecureContext::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceConditionalSecureContext>::value,
    "TestInterfaceConditionalSecureContext inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceConditionalSecureContext::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceConditionalSecureContext is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_conditional_secure_context_v8_internal {

static void secureContextAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextAttribute());
}

static void secureContextAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceConditionalSecureContext", "secureContextAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextAttribute(cppValue);
}

static void secureContextRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextRuntimeEnabledAttribute());
}

static void secureContextRuntimeEnabledAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceConditionalSecureContext", "secureContextRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextRuntimeEnabledAttribute(cppValue);
}

static void secureContextWindowExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWindowExposedAttribute());
}

static void secureContextWindowExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceConditionalSecureContext", "secureContextWindowExposedAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextWindowExposedAttribute(cppValue);
}

static void secureContextWorkerExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWorkerExposedAttribute());
}

static void secureContextWorkerExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceConditionalSecureContext", "secureContextWorkerExposedAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextWorkerExposedAttribute(cppValue);
}

static void secureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWindowExposedRuntimeEnabledAttribute());
}

static void secureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceConditionalSecureContext", "secureContextWindowExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextWindowExposedRuntimeEnabledAttribute(cppValue);
}

static void secureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWorkerExposedRuntimeEnabledAttribute());
}

static void secureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterfaceConditionalSecureContext", "secureContextWorkerExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextWorkerExposedRuntimeEnabledAttribute(cppValue);
}

static void secureContextMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(info.Holder());

  impl->secureContextMethod();
}

static void secureContextRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(info.Holder());

  impl->secureContextRuntimeEnabledMethod();
}

static void secureContextWindowExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(info.Holder());

  impl->secureContextWindowExposedMethod();
}

static void secureContextWorkerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(info.Holder());

  impl->secureContextWorkerExposedMethod();
}

static void secureContextWindowExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(info.Holder());

  impl->secureContextWindowExposedRuntimeEnabledMethod();
}

static void secureContextWorkerExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(info.Holder());

  impl->secureContextWorkerExposedRuntimeEnabledMethod();
}

}  // namespace test_interface_conditional_secure_context_v8_internal

void V8TestInterfaceConditionalSecureContext::secureContextAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextAttribute_Getter");

  test_interface_conditional_secure_context_v8_internal::secureContextAttributeAttributeGetter(info);
}

void V8TestInterfaceConditionalSecureContext::secureContextAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_conditional_secure_context_v8_internal::secureContextAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceConditionalSecureContext::secureContextRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextRuntimeEnabledAttribute_Getter");

  test_interface_conditional_secure_context_v8_internal::secureContextRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceConditionalSecureContext::secureContextRuntimeEnabledAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_conditional_secure_context_v8_internal::secureContextRuntimeEnabledAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceConditionalSecureContext::secureContextWindowExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWindowExposedAttribute_Getter");

  test_interface_conditional_secure_context_v8_internal::secureContextWindowExposedAttributeAttributeGetter(info);
}

void V8TestInterfaceConditionalSecureContext::secureContextWindowExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWindowExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_conditional_secure_context_v8_internal::secureContextWindowExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceConditionalSecureContext::secureContextWorkerExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWorkerExposedAttribute_Getter");

  test_interface_conditional_secure_context_v8_internal::secureContextWorkerExposedAttributeAttributeGetter(info);
}

void V8TestInterfaceConditionalSecureContext::secureContextWorkerExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWorkerExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_conditional_secure_context_v8_internal::secureContextWorkerExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceConditionalSecureContext::secureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWindowExposedRuntimeEnabledAttribute_Getter");

  test_interface_conditional_secure_context_v8_internal::secureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceConditionalSecureContext::secureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWindowExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_conditional_secure_context_v8_internal::secureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceConditionalSecureContext::secureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWorkerExposedRuntimeEnabledAttribute_Getter");

  test_interface_conditional_secure_context_v8_internal::secureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceConditionalSecureContext::secureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWorkerExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_conditional_secure_context_v8_internal::secureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(v8Value, info);
}

void V8TestInterfaceConditionalSecureContext::secureContextMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextMethod");

  test_interface_conditional_secure_context_v8_internal::secureContextMethodMethod(info);
}

void V8TestInterfaceConditionalSecureContext::secureContextRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextRuntimeEnabledMethod");

  test_interface_conditional_secure_context_v8_internal::secureContextRuntimeEnabledMethodMethod(info);
}

void V8TestInterfaceConditionalSecureContext::secureContextWindowExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWindowExposedMethod");

  test_interface_conditional_secure_context_v8_internal::secureContextWindowExposedMethodMethod(info);
}

void V8TestInterfaceConditionalSecureContext::secureContextWorkerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWorkerExposedMethod");

  test_interface_conditional_secure_context_v8_internal::secureContextWorkerExposedMethodMethod(info);
}

void V8TestInterfaceConditionalSecureContext::secureContextWindowExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWindowExposedRuntimeEnabledMethod");

  test_interface_conditional_secure_context_v8_internal::secureContextWindowExposedRuntimeEnabledMethodMethod(info);
}

void V8TestInterfaceConditionalSecureContext::secureContextWorkerExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWorkerExposedRuntimeEnabledMethod");

  test_interface_conditional_secure_context_v8_internal::secureContextWorkerExposedRuntimeEnabledMethodMethod(info);
}

static void installV8TestInterfaceConditionalSecureContextTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterfaceConditionalSecureContext::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceConditionalSecureContext::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.

  // Custom signature

  V8TestInterfaceConditionalSecureContext::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterfaceConditionalSecureContext::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceConditionalSecureContext::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterfaceConditionalSecureContextTemplate);
}

bool V8TestInterfaceConditionalSecureContext::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceConditionalSecureContext::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceConditionalSecureContext* V8TestInterfaceConditionalSecureContext::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceConditionalSecureContext* NativeValueTraits<TestInterfaceConditionalSecureContext>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceConditionalSecureContext* nativeValue = V8TestInterfaceConditionalSecureContext::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceConditionalSecureContext"));
  }
  return nativeValue;
}

void V8TestInterfaceConditionalSecureContext::InstallConditionalFeatures(
    v8::Local<v8::Context> context,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instanceObject,
    v8::Local<v8::Object> prototypeObject,
    v8::Local<v8::Function> interfaceObject,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  CHECK(!interfaceTemplate.IsEmpty());
  DCHECK((!prototypeObject.IsEmpty() && !interfaceObject.IsEmpty()) ||
         !instanceObject.IsEmpty());

  v8::Isolate* isolate = context->GetIsolate();

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ExecutionContext* executionContext = ToExecutionContext(context);
  DCHECK(executionContext);
  bool isSecureContext = (executionContext && executionContext->IsSecureContext());

  if (!prototypeObject.IsEmpty() || !interfaceObject.IsEmpty()) {
    if (isSecureContext || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
          { "secureContextAttribute", V8TestInterfaceConditionalSecureContext::secureContextAttributeAttributeGetterCallback, V8TestInterfaceConditionalSecureContext::secureContextAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      };
      V8DOMConfiguration::InstallAccessors(
          isolate, world, instanceObject, prototypeObject, interfaceObject,
          signature, accessor_configurations,
          base::size(accessor_configurations));
      if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
        static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
            { "secureContextRuntimeEnabledAttribute", V8TestInterfaceConditionalSecureContext::secureContextRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterfaceConditionalSecureContext::secureContextRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instanceObject, prototypeObject, interfaceObject,
            signature, accessor_configurations,
            base::size(accessor_configurations));
      }
    }
    if (executionContext && (executionContext->IsDocument())) {
      if (isSecureContext || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
        static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
            { "secureContextWindowExposedAttribute", V8TestInterfaceConditionalSecureContext::secureContextWindowExposedAttributeAttributeGetterCallback, V8TestInterfaceConditionalSecureContext::secureContextWindowExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instanceObject, prototypeObject, interfaceObject,
            signature, accessor_configurations,
            base::size(accessor_configurations));
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
              { "secureContextWindowExposedRuntimeEnabledAttribute", V8TestInterfaceConditionalSecureContext::secureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterfaceConditionalSecureContext::secureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          };
          V8DOMConfiguration::InstallAccessors(
              isolate, world, instanceObject, prototypeObject, interfaceObject,
              signature, accessor_configurations,
              base::size(accessor_configurations));
        }
      }
    }
    if (executionContext && (executionContext->IsWorkerGlobalScope())) {
      if (isSecureContext || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
        static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
            { "secureContextWorkerExposedAttribute", V8TestInterfaceConditionalSecureContext::secureContextWorkerExposedAttributeAttributeGetterCallback, V8TestInterfaceConditionalSecureContext::secureContextWorkerExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instanceObject, prototypeObject, interfaceObject,
            signature, accessor_configurations,
            base::size(accessor_configurations));
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
              { "secureContextWorkerExposedRuntimeEnabledAttribute", V8TestInterfaceConditionalSecureContext::secureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterfaceConditionalSecureContext::secureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          };
          V8DOMConfiguration::InstallAccessors(
              isolate, world, instanceObject, prototypeObject, interfaceObject,
              signature, accessor_configurations,
              base::size(accessor_configurations));
        }
      }
    }
    if (isSecureContext || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      const V8DOMConfiguration::MethodConfiguration secureContextMethodMethodConfiguration[] = {
        {"secureContextMethod", V8TestInterfaceConditionalSecureContext::secureContextMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : secureContextMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (isSecureContext || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
        const V8DOMConfiguration::MethodConfiguration secureContextRuntimeEnabledMethodMethodConfiguration[] = {
          {"secureContextRuntimeEnabledMethod", V8TestInterfaceConditionalSecureContext::secureContextRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& methodConfig : secureContextRuntimeEnabledMethodMethodConfiguration)
          V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
      }
    }
    if (isSecureContext || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      if (executionContext && (executionContext->IsDocument())) {
        const V8DOMConfiguration::MethodConfiguration secureContextWindowExposedMethodMethodConfiguration[] = {
          {"secureContextWindowExposedMethod", V8TestInterfaceConditionalSecureContext::secureContextWindowExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& methodConfig : secureContextWindowExposedMethodMethodConfiguration)
          V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
      }
    }
    if (isSecureContext || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      if (executionContext && (executionContext->IsWorkerGlobalScope())) {
        const V8DOMConfiguration::MethodConfiguration secureContextWorkerExposedMethodMethodConfiguration[] = {
          {"secureContextWorkerExposedMethod", V8TestInterfaceConditionalSecureContext::secureContextWorkerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& methodConfig : secureContextWorkerExposedMethodMethodConfiguration)
          V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
      }
    }
    if (isSecureContext || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      if (executionContext && (executionContext->IsDocument())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          const V8DOMConfiguration::MethodConfiguration secureContextWindowExposedRuntimeEnabledMethodMethodConfiguration[] = {
            {"secureContextWindowExposedRuntimeEnabledMethod", V8TestInterfaceConditionalSecureContext::secureContextWindowExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& methodConfig : secureContextWindowExposedRuntimeEnabledMethodMethodConfiguration)
            V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
        }
      }
    }
    if (isSecureContext || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      if (executionContext && (executionContext->IsWorkerGlobalScope())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          const V8DOMConfiguration::MethodConfiguration secureContextWorkerExposedRuntimeEnabledMethodMethodConfiguration[] = {
            {"secureContextWorkerExposedRuntimeEnabledMethod", V8TestInterfaceConditionalSecureContext::secureContextWorkerExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& methodConfig : secureContextWorkerExposedRuntimeEnabledMethodMethodConfiguration)
            V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
        }
      }
    }
  }
}

}  // namespace blink
