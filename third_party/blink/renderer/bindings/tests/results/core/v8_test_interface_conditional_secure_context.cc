// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_conditional_secure_context.h"

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
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo v8_test_interface_conditional_secure_context_wrapper_type_info = {
    gin::kEmbedderBlink,
    V8TestInterfaceConditionalSecureContext::DomTemplate,
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
const WrapperTypeInfo& TestInterfaceConditionalSecureContext::wrapper_type_info_ = v8_test_interface_conditional_secure_context_wrapper_type_info;

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

static void SecureContextAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextAttribute());
}

static void SecureContextAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceConditionalSecureContext", "secureContextAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setSecureContextAttribute(cpp_value);
}

static void SecureContextRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextRuntimeEnabledAttribute());
}

static void SecureContextRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceConditionalSecureContext", "secureContextRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setSecureContextRuntimeEnabledAttribute(cpp_value);
}

static void SecureContextWindowExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWindowExposedAttribute());
}

static void SecureContextWindowExposedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceConditionalSecureContext", "secureContextWindowExposedAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setSecureContextWindowExposedAttribute(cpp_value);
}

static void SecureContextWorkerExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWorkerExposedAttribute());
}

static void SecureContextWorkerExposedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceConditionalSecureContext", "secureContextWorkerExposedAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setSecureContextWorkerExposedAttribute(cpp_value);
}

static void SecureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWindowExposedRuntimeEnabledAttribute());
}

static void SecureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceConditionalSecureContext", "secureContextWindowExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setSecureContextWindowExposedRuntimeEnabledAttribute(cpp_value);
}

static void SecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWorkerExposedRuntimeEnabledAttribute());
}

static void SecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceConditionalSecureContext", "secureContextWorkerExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setSecureContextWorkerExposedRuntimeEnabledAttribute(cpp_value);
}

static void SecureContextMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(info.Holder());

  impl->secureContextMethod();
}

static void SecureContextRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(info.Holder());

  impl->secureContextRuntimeEnabledMethod();
}

static void SecureContextWindowExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(info.Holder());

  impl->secureContextWindowExposedMethod();
}

static void SecureContextWorkerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(info.Holder());

  impl->secureContextWorkerExposedMethod();
}

static void SecureContextWindowExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(info.Holder());

  impl->secureContextWindowExposedRuntimeEnabledMethod();
}

static void SecureContextWorkerExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceConditionalSecureContext* impl = V8TestInterfaceConditionalSecureContext::ToImpl(info.Holder());

  impl->secureContextWorkerExposedRuntimeEnabledMethod();
}

}  // namespace test_interface_conditional_secure_context_v8_internal

void V8TestInterfaceConditionalSecureContext::SecureContextAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextAttribute_Getter");

  test_interface_conditional_secure_context_v8_internal::SecureContextAttributeAttributeGetter(info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_conditional_secure_context_v8_internal::SecureContextAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextRuntimeEnabledAttribute_Getter");

  test_interface_conditional_secure_context_v8_internal::SecureContextRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_conditional_secure_context_v8_internal::SecureContextRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextWindowExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWindowExposedAttribute_Getter");

  test_interface_conditional_secure_context_v8_internal::SecureContextWindowExposedAttributeAttributeGetter(info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextWindowExposedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWindowExposedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_conditional_secure_context_v8_internal::SecureContextWindowExposedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextWorkerExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWorkerExposedAttribute_Getter");

  test_interface_conditional_secure_context_v8_internal::SecureContextWorkerExposedAttributeAttributeGetter(info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextWorkerExposedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWorkerExposedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_conditional_secure_context_v8_internal::SecureContextWorkerExposedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWindowExposedRuntimeEnabledAttribute_Getter");

  test_interface_conditional_secure_context_v8_internal::SecureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWindowExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_conditional_secure_context_v8_internal::SecureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWorkerExposedRuntimeEnabledAttribute_Getter");

  test_interface_conditional_secure_context_v8_internal::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWorkerExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_conditional_secure_context_v8_internal::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextMethod");

  test_interface_conditional_secure_context_v8_internal::SecureContextMethodMethod(info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextRuntimeEnabledMethod");

  test_interface_conditional_secure_context_v8_internal::SecureContextRuntimeEnabledMethodMethod(info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextWindowExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWindowExposedMethod");

  test_interface_conditional_secure_context_v8_internal::SecureContextWindowExposedMethodMethod(info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextWorkerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWorkerExposedMethod");

  test_interface_conditional_secure_context_v8_internal::SecureContextWorkerExposedMethodMethod(info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextWindowExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWindowExposedRuntimeEnabledMethod");

  test_interface_conditional_secure_context_v8_internal::SecureContextWindowExposedRuntimeEnabledMethodMethod(info);
}

void V8TestInterfaceConditionalSecureContext::SecureContextWorkerExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConditionalSecureContext_secureContextWorkerExposedRuntimeEnabledMethod");

  test_interface_conditional_secure_context_v8_internal::SecureContextWorkerExposedRuntimeEnabledMethodMethod(info);
}

static void InstallV8TestInterfaceConditionalSecureContextTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interface_template, V8TestInterfaceConditionalSecureContext::GetWrapperTypeInfo()->interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceConditionalSecureContext::kInternalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.

  // Custom signature

  V8TestInterfaceConditionalSecureContext::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interface_template);
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceConditionalSecureContext::DomTemplate(
    v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world, const_cast<WrapperTypeInfo*>(V8TestInterfaceConditionalSecureContext::GetWrapperTypeInfo()),
      InstallV8TestInterfaceConditionalSecureContextTemplate);
}

bool V8TestInterfaceConditionalSecureContext::HasInstance(v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(V8TestInterfaceConditionalSecureContext::GetWrapperTypeInfo(), v8_value);
}

v8::Local<v8::Object> V8TestInterfaceConditionalSecureContext::FindInstanceInPrototypeChain(
    v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(
      V8TestInterfaceConditionalSecureContext::GetWrapperTypeInfo(), v8_value);
}

TestInterfaceConditionalSecureContext* V8TestInterfaceConditionalSecureContext::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceConditionalSecureContext* NativeValueTraits<TestInterfaceConditionalSecureContext>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestInterfaceConditionalSecureContext* native_value = V8TestInterfaceConditionalSecureContext::ToImplWithTypeCheck(isolate, value);
  if (!native_value) {
    exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceConditionalSecureContext"));
  }
  return native_value;
}

void V8TestInterfaceConditionalSecureContext::InstallConditionalFeatures(
    v8::Local<v8::Context> context,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance_object,
    v8::Local<v8::Object> prototype_object,
    v8::Local<v8::Function> interface_object,
    v8::Local<v8::FunctionTemplate> interface_template) {
  CHECK(!interface_template.IsEmpty());
  DCHECK((!prototype_object.IsEmpty() && !interface_object.IsEmpty()) ||
         !instance_object.IsEmpty());

  v8::Isolate* isolate = context->GetIsolate();

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ExecutionContext* execution_context = ToExecutionContext(context);
  DCHECK(execution_context);
  bool is_secure_context = (execution_context && execution_context->IsSecureContext());

  if (!prototype_object.IsEmpty() || !interface_object.IsEmpty()) {
    if (is_secure_context || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      static constexpr V8DOMConfiguration::AccessorConfiguration
      kAccessorConfigurations[] = {
          { "secureContextAttribute", V8TestInterfaceConditionalSecureContext::SecureContextAttributeAttributeGetterCallback, V8TestInterfaceConditionalSecureContext::SecureContextAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      };
      V8DOMConfiguration::InstallAccessors(
          isolate, world, instance_object, prototype_object, interface_object,
          signature, kAccessorConfigurations,
          base::size(kAccessorConfigurations));

      if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
        static constexpr V8DOMConfiguration::AccessorConfiguration
        kAccessorConfigurations[] = {
            { "secureContextRuntimeEnabledAttribute", V8TestInterfaceConditionalSecureContext::SecureContextRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterfaceConditionalSecureContext::SecureContextRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instance_object, prototype_object, interface_object,
            signature, kAccessorConfigurations,
            base::size(kAccessorConfigurations));
      }
    }
    if (execution_context && (execution_context->IsDocument())) {
      if (is_secure_context || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
        static constexpr V8DOMConfiguration::AccessorConfiguration
        kAccessorConfigurations[] = {
            { "secureContextWindowExposedAttribute", V8TestInterfaceConditionalSecureContext::SecureContextWindowExposedAttributeAttributeGetterCallback, V8TestInterfaceConditionalSecureContext::SecureContextWindowExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instance_object, prototype_object, interface_object,
            signature, kAccessorConfigurations,
            base::size(kAccessorConfigurations));

        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          static constexpr V8DOMConfiguration::AccessorConfiguration
          kAccessorConfigurations[] = {
              { "secureContextWindowExposedRuntimeEnabledAttribute", V8TestInterfaceConditionalSecureContext::SecureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterfaceConditionalSecureContext::SecureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          };
          V8DOMConfiguration::InstallAccessors(
              isolate, world, instance_object, prototype_object, interface_object,
              signature, kAccessorConfigurations,
              base::size(kAccessorConfigurations));
        }
      }
    }
    if (execution_context && (execution_context->IsWorkerGlobalScope())) {
      if (is_secure_context || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
        static constexpr V8DOMConfiguration::AccessorConfiguration
        kAccessorConfigurations[] = {
            { "secureContextWorkerExposedAttribute", V8TestInterfaceConditionalSecureContext::SecureContextWorkerExposedAttributeAttributeGetterCallback, V8TestInterfaceConditionalSecureContext::SecureContextWorkerExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instance_object, prototype_object, interface_object,
            signature, kAccessorConfigurations,
            base::size(kAccessorConfigurations));

        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          static constexpr V8DOMConfiguration::AccessorConfiguration
          kAccessorConfigurations[] = {
              { "secureContextWorkerExposedRuntimeEnabledAttribute", V8TestInterfaceConditionalSecureContext::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterfaceConditionalSecureContext::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          };
          V8DOMConfiguration::InstallAccessors(
              isolate, world, instance_object, prototype_object, interface_object,
              signature, kAccessorConfigurations,
              base::size(kAccessorConfigurations));
        }
      }
    }
    if (is_secure_context || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      {
        // Install secureContextMethod configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"secureContextMethod", V8TestInterfaceConditionalSecureContext::SecureContextMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& config : kConfigurations) {
          V8DOMConfiguration::InstallMethod(
              isolate, world, instance_object, prototype_object,
              interface_object, signature, config);
        }
      }
    }
    if (is_secure_context || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
        {
          // Install secureContextRuntimeEnabledMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"secureContextRuntimeEnabledMethod", V8TestInterfaceConditionalSecureContext::SecureContextRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (is_secure_context || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      if (execution_context && (execution_context->IsDocument())) {
        {
          // Install secureContextWindowExposedMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"secureContextWindowExposedMethod", V8TestInterfaceConditionalSecureContext::SecureContextWindowExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (is_secure_context || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      if (execution_context && (execution_context->IsWorkerGlobalScope())) {
        {
          // Install secureContextWorkerExposedMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"secureContextWorkerExposedMethod", V8TestInterfaceConditionalSecureContext::SecureContextWorkerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (is_secure_context || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      if (execution_context && (execution_context->IsDocument())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          {
            // Install secureContextWindowExposedRuntimeEnabledMethod configuration
            const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
                {"secureContextWindowExposedRuntimeEnabledMethod", V8TestInterfaceConditionalSecureContext::SecureContextWindowExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
            };
            for (const auto& config : kConfigurations) {
              V8DOMConfiguration::InstallMethod(
                  isolate, world, instance_object, prototype_object,
                  interface_object, signature, config);
            }
          }
        }
      }
    }
    if (is_secure_context || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      if (execution_context && (execution_context->IsWorkerGlobalScope())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          {
            // Install secureContextWorkerExposedRuntimeEnabledMethod configuration
            const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
                {"secureContextWorkerExposedRuntimeEnabledMethod", V8TestInterfaceConditionalSecureContext::SecureContextWorkerExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
            };
            for (const auto& config : kConfigurations) {
              V8DOMConfiguration::InstallMethod(
                  isolate, world, instance_object, prototype_object,
                  interface_object, signature, config);
            }
          }
        }
      }
    }
  }
}

}  // namespace blink
