// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_direct_socket_enabled.h"

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
const WrapperTypeInfo v8_test_interface_direct_socket_enabled_wrapper_type_info = {
    gin::kEmbedderBlink,
    V8TestInterfaceDirectSocketEnabled::DomTemplate,
    V8TestInterfaceDirectSocketEnabled::InstallConditionalFeatures,
    "TestInterfaceDirectSocketEnabled",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceDirectSocketEnabled.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceDirectSocketEnabled::wrapper_type_info_ = v8_test_interface_direct_socket_enabled_wrapper_type_info;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceDirectSocketEnabled>::value,
    "TestInterfaceDirectSocketEnabled inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceDirectSocketEnabled::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceDirectSocketEnabled is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_direct_socket_enabled_v8_internal {

static void DirectSocketEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(holder);

  V8SetReturnValueBool(info, impl->directSocketEnabledAttribute());
}

static void DirectSocketEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceDirectSocketEnabled", "directSocketEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value{ NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state) };
  if (exception_state.HadException())
    return;

  impl->setDirectSocketEnabledAttribute(cpp_value);
}

static void DirectSocketEnabledRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(holder);

  V8SetReturnValueBool(info, impl->directSocketEnabledRuntimeEnabledAttribute());
}

static void DirectSocketEnabledRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceDirectSocketEnabled", "directSocketEnabledRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value{ NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state) };
  if (exception_state.HadException())
    return;

  impl->setDirectSocketEnabledRuntimeEnabledAttribute(cpp_value);
}

static void DirectSocketEnabledWindowExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(holder);

  V8SetReturnValueBool(info, impl->directSocketEnabledWindowExposedAttribute());
}

static void DirectSocketEnabledWindowExposedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceDirectSocketEnabled", "directSocketEnabledWindowExposedAttribute");

  // Prepare the value to be set.
  bool cpp_value{ NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state) };
  if (exception_state.HadException())
    return;

  impl->setDirectSocketEnabledWindowExposedAttribute(cpp_value);
}

static void DirectSocketEnabledWorkerExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(holder);

  V8SetReturnValueBool(info, impl->directSocketEnabledWorkerExposedAttribute());
}

static void DirectSocketEnabledWorkerExposedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceDirectSocketEnabled", "directSocketEnabledWorkerExposedAttribute");

  // Prepare the value to be set.
  bool cpp_value{ NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state) };
  if (exception_state.HadException())
    return;

  impl->setDirectSocketEnabledWorkerExposedAttribute(cpp_value);
}

static void DirectSocketEnabledWindowExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(holder);

  V8SetReturnValueBool(info, impl->directSocketEnabledWindowExposedRuntimeEnabledAttribute());
}

static void DirectSocketEnabledWindowExposedRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceDirectSocketEnabled", "directSocketEnabledWindowExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value{ NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state) };
  if (exception_state.HadException())
    return;

  impl->setDirectSocketEnabledWindowExposedRuntimeEnabledAttribute(cpp_value);
}

static void DirectSocketEnabledWorkerExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(holder);

  V8SetReturnValueBool(info, impl->directSocketEnabledWorkerExposedRuntimeEnabledAttribute());
}

static void DirectSocketEnabledWorkerExposedRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceDirectSocketEnabled", "directSocketEnabledWorkerExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value{ NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state) };
  if (exception_state.HadException())
    return;

  impl->setDirectSocketEnabledWorkerExposedRuntimeEnabledAttribute(cpp_value);
}

static void DirectSocketEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(info.Holder());

  impl->directSocketEnabledMethod();
}

static void DirectSocketEnabledRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(info.Holder());

  impl->directSocketEnabledRuntimeEnabledMethod();
}

static void DirectSocketEnabledWindowExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(info.Holder());

  impl->directSocketEnabledWindowExposedMethod();
}

static void DirectSocketEnabledWorkerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(info.Holder());

  impl->directSocketEnabledWorkerExposedMethod();
}

static void DirectSocketEnabledWindowExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(info.Holder());

  impl->directSocketEnabledWindowExposedRuntimeEnabledMethod();
}

static void DirectSocketEnabledWorkerExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceDirectSocketEnabled* impl = V8TestInterfaceDirectSocketEnabled::ToImpl(info.Holder());

  impl->directSocketEnabledWorkerExposedRuntimeEnabledMethod();
}

}  // namespace test_interface_direct_socket_enabled_v8_internal

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledAttribute_Getter");

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledRuntimeEnabledAttribute_Getter");

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWindowExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledWindowExposedAttribute_Getter");

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledWindowExposedAttributeAttributeGetter(info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWindowExposedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledWindowExposedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledWindowExposedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWorkerExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledWorkerExposedAttribute_Getter");

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledWorkerExposedAttributeAttributeGetter(info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWorkerExposedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledWorkerExposedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledWorkerExposedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWindowExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledWindowExposedRuntimeEnabledAttribute_Getter");

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledWindowExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWindowExposedRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledWindowExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledWindowExposedRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledWorkerExposedRuntimeEnabledAttribute_Getter");

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledWorkerExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledWorkerExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledWorkerExposedRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  BLINK_BINDINGS_TRACE_EVENT("TestInterfaceDirectSocketEnabled.directSocketEnabledMethod");
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledMethod");

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledMethodMethod(info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  BLINK_BINDINGS_TRACE_EVENT("TestInterfaceDirectSocketEnabled.directSocketEnabledRuntimeEnabledMethod");
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledRuntimeEnabledMethod");

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledRuntimeEnabledMethodMethod(info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWindowExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  BLINK_BINDINGS_TRACE_EVENT("TestInterfaceDirectSocketEnabled.directSocketEnabledWindowExposedMethod");
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledWindowExposedMethod");

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledWindowExposedMethodMethod(info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWorkerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  BLINK_BINDINGS_TRACE_EVENT("TestInterfaceDirectSocketEnabled.directSocketEnabledWorkerExposedMethod");
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledWorkerExposedMethod");

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledWorkerExposedMethodMethod(info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWindowExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  BLINK_BINDINGS_TRACE_EVENT("TestInterfaceDirectSocketEnabled.directSocketEnabledWindowExposedRuntimeEnabledMethod");
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledWindowExposedRuntimeEnabledMethod");

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledWindowExposedRuntimeEnabledMethodMethod(info);
}

void V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWorkerExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  BLINK_BINDINGS_TRACE_EVENT("TestInterfaceDirectSocketEnabled.directSocketEnabledWorkerExposedRuntimeEnabledMethod");
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceDirectSocketEnabled_directSocketEnabledWorkerExposedRuntimeEnabledMethod");

  test_interface_direct_socket_enabled_v8_internal::DirectSocketEnabledWorkerExposedRuntimeEnabledMethodMethod(info);
}

static void InstallV8TestInterfaceDirectSocketEnabledTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interface_template, V8TestInterfaceDirectSocketEnabled::GetWrapperTypeInfo()->interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceDirectSocketEnabled::kInternalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.

  // Custom signature

  V8TestInterfaceDirectSocketEnabled::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interface_template);
}

void V8TestInterfaceDirectSocketEnabled::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceDirectSocketEnabled::DomTemplate(
    v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world, const_cast<WrapperTypeInfo*>(V8TestInterfaceDirectSocketEnabled::GetWrapperTypeInfo()),
      InstallV8TestInterfaceDirectSocketEnabledTemplate);
}

bool V8TestInterfaceDirectSocketEnabled::HasInstance(v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(V8TestInterfaceDirectSocketEnabled::GetWrapperTypeInfo(), v8_value);
}

v8::Local<v8::Object> V8TestInterfaceDirectSocketEnabled::FindInstanceInPrototypeChain(
    v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(
      V8TestInterfaceDirectSocketEnabled::GetWrapperTypeInfo(), v8_value);
}

TestInterfaceDirectSocketEnabled* V8TestInterfaceDirectSocketEnabled::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

void V8TestInterfaceDirectSocketEnabled::InstallConditionalFeatures(
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
  bool is_direct_socket_enabled = (execution_context && execution_context->DirectSocketCapability());

  if (!prototype_object.IsEmpty() || !interface_object.IsEmpty()) {
    if (execution_context && (is_direct_socket_enabled)) {
    }
    if (execution_context && (execution_context->IsWindow())) {
      if (execution_context && (is_direct_socket_enabled)) {
      }
    }
    if (execution_context && (execution_context->IsWorkerGlobalScope())) {
      if (execution_context && (is_direct_socket_enabled)) {
      }
    }
    if (execution_context && (is_direct_socket_enabled)) {
      {
        // Install directSocketEnabledMethod configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"directSocketEnabledMethod", V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& config : kConfigurations) {
          V8DOMConfiguration::InstallMethod(
              isolate, world, instance_object, prototype_object,
              interface_object, signature, config);
        }
      }
    }
    if (execution_context && (is_direct_socket_enabled)) {
      if (RuntimeEnabledFeatures::RuntimeFeature2Enabled()) {
        {
          // Install directSocketEnabledRuntimeEnabledMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"directSocketEnabledRuntimeEnabledMethod", V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (execution_context && (is_direct_socket_enabled)) {
      if (execution_context && (execution_context->IsWindow())) {
        {
          // Install directSocketEnabledWindowExposedMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"directSocketEnabledWindowExposedMethod", V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWindowExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (execution_context && (is_direct_socket_enabled)) {
      if (execution_context && (execution_context->IsWorkerGlobalScope())) {
        {
          // Install directSocketEnabledWorkerExposedMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"directSocketEnabledWorkerExposedMethod", V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWorkerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (execution_context && (is_direct_socket_enabled)) {
      if (execution_context && (execution_context->IsWindow())) {
        if (RuntimeEnabledFeatures::RuntimeFeature2Enabled()) {
          {
            // Install directSocketEnabledWindowExposedRuntimeEnabledMethod configuration
            const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
                {"directSocketEnabledWindowExposedRuntimeEnabledMethod", V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWindowExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
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
    if (execution_context && (is_direct_socket_enabled)) {
      if (execution_context && (execution_context->IsWorkerGlobalScope())) {
        if (RuntimeEnabledFeatures::RuntimeFeature2Enabled()) {
          {
            // Install directSocketEnabledWorkerExposedRuntimeEnabledMethod configuration
            const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
                {"directSocketEnabledWorkerExposedRuntimeEnabledMethod", V8TestInterfaceDirectSocketEnabled::DirectSocketEnabledWorkerExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
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
