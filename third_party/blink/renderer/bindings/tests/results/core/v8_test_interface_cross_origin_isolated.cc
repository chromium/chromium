// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_cross_origin_isolated.h"

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
const WrapperTypeInfo v8_test_interface_cross_origin_isolated_wrapper_type_info = {
    gin::kEmbedderBlink,
    V8TestInterfaceCrossOriginIsolated::DomTemplate,
    V8TestInterfaceCrossOriginIsolated::InstallConditionalFeatures,
    "TestInterfaceCrossOriginIsolated",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceCrossOriginIsolated.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceCrossOriginIsolated::wrapper_type_info_ = v8_test_interface_cross_origin_isolated_wrapper_type_info;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceCrossOriginIsolated>::value,
    "TestInterfaceCrossOriginIsolated inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceCrossOriginIsolated::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceCrossOriginIsolated is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_cross_origin_isolated_v8_internal {

static void CrossOriginIsolatedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(holder);

  V8SetReturnValueBool(info, impl->crossOriginIsolatedAttribute());
}

static void CrossOriginIsolatedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceCrossOriginIsolated", "crossOriginIsolatedAttribute");

  // Prepare the value to be set.
  bool cpp_value{ NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state) };
  if (exception_state.HadException())
    return;

  impl->setCrossOriginIsolatedAttribute(cpp_value);
}

static void CrossOriginIsolatedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(holder);

  V8SetReturnValueBool(info, impl->crossOriginIsolatedRuntimeEnabledAttribute());
}

static void CrossOriginIsolatedRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceCrossOriginIsolated", "crossOriginIsolatedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value{ NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state) };
  if (exception_state.HadException())
    return;

  impl->setCrossOriginIsolatedRuntimeEnabledAttribute(cpp_value);
}

static void CrossOriginIsolatedWindowExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(holder);

  V8SetReturnValueBool(info, impl->crossOriginIsolatedWindowExposedAttribute());
}

static void CrossOriginIsolatedWindowExposedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceCrossOriginIsolated", "crossOriginIsolatedWindowExposedAttribute");

  // Prepare the value to be set.
  bool cpp_value{ NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state) };
  if (exception_state.HadException())
    return;

  impl->setCrossOriginIsolatedWindowExposedAttribute(cpp_value);
}

static void CrossOriginIsolatedWorkerExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(holder);

  V8SetReturnValueBool(info, impl->crossOriginIsolatedWorkerExposedAttribute());
}

static void CrossOriginIsolatedWorkerExposedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceCrossOriginIsolated", "crossOriginIsolatedWorkerExposedAttribute");

  // Prepare the value to be set.
  bool cpp_value{ NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state) };
  if (exception_state.HadException())
    return;

  impl->setCrossOriginIsolatedWorkerExposedAttribute(cpp_value);
}

static void CrossOriginIsolatedWindowExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(holder);

  V8SetReturnValueBool(info, impl->crossOriginIsolatedWindowExposedRuntimeEnabledAttribute());
}

static void CrossOriginIsolatedWindowExposedRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceCrossOriginIsolated", "crossOriginIsolatedWindowExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value{ NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state) };
  if (exception_state.HadException())
    return;

  impl->setCrossOriginIsolatedWindowExposedRuntimeEnabledAttribute(cpp_value);
}

static void CrossOriginIsolatedWorkerExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(holder);

  V8SetReturnValueBool(info, impl->crossOriginIsolatedWorkerExposedRuntimeEnabledAttribute());
}

static void CrossOriginIsolatedWorkerExposedRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceCrossOriginIsolated", "crossOriginIsolatedWorkerExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value{ NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state) };
  if (exception_state.HadException())
    return;

  impl->setCrossOriginIsolatedWorkerExposedRuntimeEnabledAttribute(cpp_value);
}

static void CrossOriginIsolatedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(info.Holder());

  impl->crossOriginIsolatedMethod();
}

static void CrossOriginIsolatedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(info.Holder());

  impl->crossOriginIsolatedRuntimeEnabledMethod();
}

static void CrossOriginIsolatedWindowExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(info.Holder());

  impl->crossOriginIsolatedWindowExposedMethod();
}

static void CrossOriginIsolatedWorkerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(info.Holder());

  impl->crossOriginIsolatedWorkerExposedMethod();
}

static void CrossOriginIsolatedWindowExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(info.Holder());

  impl->crossOriginIsolatedWindowExposedRuntimeEnabledMethod();
}

static void CrossOriginIsolatedWorkerExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCrossOriginIsolated* impl = V8TestInterfaceCrossOriginIsolated::ToImpl(info.Holder());

  impl->crossOriginIsolatedWorkerExposedRuntimeEnabledMethod();
}

}  // namespace test_interface_cross_origin_isolated_v8_internal

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedAttribute_Getter");

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedAttributeAttributeGetter(info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedRuntimeEnabledAttribute_Getter");

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWindowExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedWindowExposedAttribute_Getter");

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedWindowExposedAttributeAttributeGetter(info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWindowExposedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedWindowExposedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedWindowExposedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWorkerExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedWorkerExposedAttribute_Getter");

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedWorkerExposedAttributeAttributeGetter(info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWorkerExposedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedWorkerExposedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedWorkerExposedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWindowExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedWindowExposedRuntimeEnabledAttribute_Getter");

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedWindowExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWindowExposedRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedWindowExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedWindowExposedRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedWorkerExposedRuntimeEnabledAttribute_Getter");

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedWorkerExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedWorkerExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedWorkerExposedRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  BLINK_BINDINGS_TRACE_EVENT("TestInterfaceCrossOriginIsolated.crossOriginIsolatedMethod");
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedMethod");

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedMethodMethod(info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  BLINK_BINDINGS_TRACE_EVENT("TestInterfaceCrossOriginIsolated.crossOriginIsolatedRuntimeEnabledMethod");
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedRuntimeEnabledMethod");

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedRuntimeEnabledMethodMethod(info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWindowExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  BLINK_BINDINGS_TRACE_EVENT("TestInterfaceCrossOriginIsolated.crossOriginIsolatedWindowExposedMethod");
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedWindowExposedMethod");

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedWindowExposedMethodMethod(info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWorkerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  BLINK_BINDINGS_TRACE_EVENT("TestInterfaceCrossOriginIsolated.crossOriginIsolatedWorkerExposedMethod");
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedWorkerExposedMethod");

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedWorkerExposedMethodMethod(info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWindowExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  BLINK_BINDINGS_TRACE_EVENT("TestInterfaceCrossOriginIsolated.crossOriginIsolatedWindowExposedRuntimeEnabledMethod");
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedWindowExposedRuntimeEnabledMethod");

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedWindowExposedRuntimeEnabledMethodMethod(info);
}

void V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWorkerExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  BLINK_BINDINGS_TRACE_EVENT("TestInterfaceCrossOriginIsolated.crossOriginIsolatedWorkerExposedRuntimeEnabledMethod");
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCrossOriginIsolated_crossOriginIsolatedWorkerExposedRuntimeEnabledMethod");

  test_interface_cross_origin_isolated_v8_internal::CrossOriginIsolatedWorkerExposedRuntimeEnabledMethodMethod(info);
}

static void InstallV8TestInterfaceCrossOriginIsolatedTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interface_template, V8TestInterfaceCrossOriginIsolated::GetWrapperTypeInfo()->interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceCrossOriginIsolated::kInternalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.

  // Custom signature

  V8TestInterfaceCrossOriginIsolated::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interface_template);
}

void V8TestInterfaceCrossOriginIsolated::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceCrossOriginIsolated::DomTemplate(
    v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world, const_cast<WrapperTypeInfo*>(V8TestInterfaceCrossOriginIsolated::GetWrapperTypeInfo()),
      InstallV8TestInterfaceCrossOriginIsolatedTemplate);
}

bool V8TestInterfaceCrossOriginIsolated::HasInstance(v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(V8TestInterfaceCrossOriginIsolated::GetWrapperTypeInfo(), v8_value);
}

v8::Local<v8::Object> V8TestInterfaceCrossOriginIsolated::FindInstanceInPrototypeChain(
    v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(
      V8TestInterfaceCrossOriginIsolated::GetWrapperTypeInfo(), v8_value);
}

TestInterfaceCrossOriginIsolated* V8TestInterfaceCrossOriginIsolated::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

void V8TestInterfaceCrossOriginIsolated::InstallConditionalFeatures(
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
  bool is_cross_origin_isolated = (execution_context && execution_context->CrossOriginIsolatedCapability());

  if (!prototype_object.IsEmpty() || !interface_object.IsEmpty()) {
    if (execution_context && (is_cross_origin_isolated)) {
    }
    if (execution_context && (execution_context->IsWindow())) {
      if (execution_context && (is_cross_origin_isolated)) {
      }
    }
    if (execution_context && (execution_context->IsWorkerGlobalScope())) {
      if (execution_context && (is_cross_origin_isolated)) {
      }
    }
    if (execution_context && (is_cross_origin_isolated)) {
      {
        // Install crossOriginIsolatedMethod configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"crossOriginIsolatedMethod", V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& config : kConfigurations) {
          V8DOMConfiguration::InstallMethod(
              isolate, world, instance_object, prototype_object,
              interface_object, signature, config);
        }
      }
    }
    if (execution_context && (is_cross_origin_isolated)) {
      if (RuntimeEnabledFeatures::RuntimeFeature2Enabled()) {
        {
          // Install crossOriginIsolatedRuntimeEnabledMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"crossOriginIsolatedRuntimeEnabledMethod", V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (execution_context && (is_cross_origin_isolated)) {
      if (execution_context && (execution_context->IsWindow())) {
        {
          // Install crossOriginIsolatedWindowExposedMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"crossOriginIsolatedWindowExposedMethod", V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWindowExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (execution_context && (is_cross_origin_isolated)) {
      if (execution_context && (execution_context->IsWorkerGlobalScope())) {
        {
          // Install crossOriginIsolatedWorkerExposedMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"crossOriginIsolatedWorkerExposedMethod", V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWorkerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (execution_context && (is_cross_origin_isolated)) {
      if (execution_context && (execution_context->IsWindow())) {
        if (RuntimeEnabledFeatures::RuntimeFeature2Enabled()) {
          {
            // Install crossOriginIsolatedWindowExposedRuntimeEnabledMethod configuration
            const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
                {"crossOriginIsolatedWindowExposedRuntimeEnabledMethod", V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWindowExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
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
    if (execution_context && (is_cross_origin_isolated)) {
      if (execution_context && (execution_context->IsWorkerGlobalScope())) {
        if (RuntimeEnabledFeatures::RuntimeFeature2Enabled()) {
          {
            // Install crossOriginIsolatedWorkerExposedRuntimeEnabledMethod configuration
            const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
                {"crossOriginIsolatedWorkerExposedRuntimeEnabledMethod", V8TestInterfaceCrossOriginIsolated::CrossOriginIsolatedWorkerExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
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
