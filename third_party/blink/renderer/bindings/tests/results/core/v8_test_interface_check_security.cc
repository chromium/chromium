// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_check_security.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_cross_origin_callback_info.h"
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
const WrapperTypeInfo v8_test_interface_check_security_wrapper_type_info = {
    gin::kEmbedderBlink,
    V8TestInterfaceCheckSecurity::DomTemplate,
    V8TestInterfaceCheckSecurity::InstallConditionalFeatures,
    "TestInterfaceCheckSecurity",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceCheckSecurity.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceCheckSecurity::wrapper_type_info_ = v8_test_interface_check_security_wrapper_type_info;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceCheckSecurity>::value,
    "TestInterfaceCheckSecurity inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceCheckSecurity::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceCheckSecurity is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_check_security_v8_internal {

static void ReadonlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  V8SetReturnValueInt(info, impl->readonlyLongAttribute());
}

static void LongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  V8SetReturnValueInt(info, impl->longAttribute());
}

static void LongAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceCheckSecurity", "longAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setLongAttribute(cpp_value);
}

static void DoNotCheckSecurityLongAttributeAttributeGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  V8SetReturnValueInt(info, impl->doNotCheckSecurityLongAttribute());
}

static void DoNotCheckSecurityLongAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::PropertyCallbackInfo<void>& info
) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceCheckSecurity", "doNotCheckSecurityLongAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setDoNotCheckSecurityLongAttribute(cpp_value);
}

static void DoNotCheckSecurityReadonlyLongAttributeAttributeGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  V8SetReturnValueInt(info, impl->doNotCheckSecurityReadonlyLongAttribute());
}

static void DoNotCheckSecurityOnSetterLongAttributeAttributeGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  V8SetReturnValueInt(info, impl->doNotCheckSecurityOnSetterLongAttribute());
}

static void DoNotCheckSecurityOnSetterLongAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const V8CrossOriginCallbackInfo& info
) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterfaceCheckSecurity", "doNotCheckSecurityOnSetterLongAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setDoNotCheckSecurityOnSetterLongAttribute(cpp_value);
}

static void DoNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(holder);

  V8SetReturnValueInt(info, impl->doNotCheckSecurityReplaceableReadonlyLongAttribute());
}

static void VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());

  impl->voidMethod();
}

static void DoNotCheckSecurityVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());

  impl->doNotCheckSecurityVoidMethod();
}

static void DoNotCheckSecurityVoidMethodOriginSafeMethodGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  V8PerIsolateData* data = V8PerIsolateData::From(isolate);
  const DOMWrapperWorld& world =
      DOMWrapperWorld::World(isolate->GetCurrentContext());
  v8::Local<v8::FunctionTemplate> interface_template =
      data->FindInterfaceTemplate(world, V8TestInterfaceCheckSecurity::GetWrapperTypeInfo());
  v8::Local<v8::Signature> signature =
      v8::Signature::New(isolate, interface_template);

  static int dom_template_key; // This address is used for a key to look up the dom template.
  v8::Local<v8::FunctionTemplate> method_template =
      data->FindOrCreateOperationTemplate(
          world,
          &dom_template_key,
          V8TestInterfaceCheckSecurity::DoNotCheckSecurityVoidMethodMethodCallback,
          v8::Local<v8::Value>(),
          signature,
          0);

  V8SetReturnValue(
      info,
      method_template->GetFunction(
          isolate->GetCurrentContext()).ToLocalChecked());
}

static void DoNotCheckSecurityPerWorldBindingsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());

  impl->doNotCheckSecurityPerWorldBindingsVoidMethod();
}

static void DoNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  V8PerIsolateData* data = V8PerIsolateData::From(isolate);
  const DOMWrapperWorld& world =
      DOMWrapperWorld::World(isolate->GetCurrentContext());
  v8::Local<v8::FunctionTemplate> interface_template =
      data->FindInterfaceTemplate(world, V8TestInterfaceCheckSecurity::GetWrapperTypeInfo());
  v8::Local<v8::Signature> signature =
      v8::Signature::New(isolate, interface_template);

  static int dom_template_key; // This address is used for a key to look up the dom template.
  v8::Local<v8::FunctionTemplate> method_template =
      data->FindOrCreateOperationTemplate(
          world,
          &dom_template_key,
          V8TestInterfaceCheckSecurity::DoNotCheckSecurityPerWorldBindingsVoidMethodMethodCallback,
          v8::Local<v8::Value>(),
          signature,
          0);

  V8SetReturnValue(
      info,
      method_template->GetFunction(
          isolate->GetCurrentContext()).ToLocalChecked());
}

static void DoNotCheckSecurityPerWorldBindingsVoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());

  impl->doNotCheckSecurityPerWorldBindingsVoidMethod();
}

static void DoNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterForMainWorld(const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  V8PerIsolateData* data = V8PerIsolateData::From(isolate);
  const DOMWrapperWorld& world =
      DOMWrapperWorld::World(isolate->GetCurrentContext());
  v8::Local<v8::FunctionTemplate> interface_template =
      data->FindInterfaceTemplate(world, V8TestInterfaceCheckSecurity::GetWrapperTypeInfo());
  v8::Local<v8::Signature> signature =
      v8::Signature::New(isolate, interface_template);

  static int dom_template_key; // This address is used for a key to look up the dom template.
  v8::Local<v8::FunctionTemplate> method_template =
      data->FindOrCreateOperationTemplate(
          world,
          &dom_template_key,
          V8TestInterfaceCheckSecurity::DoNotCheckSecurityPerWorldBindingsVoidMethodMethodCallbackForMainWorld,
          v8::Local<v8::Value>(),
          signature,
          0);

  V8SetReturnValue(
      info,
      method_template->GetFunction(
          isolate->GetCurrentContext()).ToLocalChecked());
}

static void DoNotCheckSecurityUnforgeableVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());

  impl->doNotCheckSecurityUnforgeableVoidMethod();
}

static void DoNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  V8PerIsolateData* data = V8PerIsolateData::From(isolate);
  const DOMWrapperWorld& world =
      DOMWrapperWorld::World(isolate->GetCurrentContext());
  v8::Local<v8::FunctionTemplate> interface_template =
      data->FindInterfaceTemplate(world, V8TestInterfaceCheckSecurity::GetWrapperTypeInfo());
  v8::Local<v8::Signature> signature =
      v8::Signature::New(isolate, interface_template);

  static int dom_template_key; // This address is used for a key to look up the dom template.
  v8::Local<v8::FunctionTemplate> method_template =
      data->FindOrCreateOperationTemplate(
          world,
          &dom_template_key,
          V8TestInterfaceCheckSecurity::DoNotCheckSecurityUnforgeableVoidMethodMethodCallback,
          v8::Local<v8::Value>(),
          signature,
          0);

  V8SetReturnValue(
      info,
      method_template->GetFunction(
          isolate->GetCurrentContext()).ToLocalChecked());
}

static void DoNotCheckSecurityVoidOverloadMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());

  V8StringResource<> argument_1;
  V8StringResource<> argument_2;
  argument_1 = info[0];
  if (!argument_1.Prepare())
    return;

  argument_2 = info[1];
  if (!argument_2.Prepare())
    return;

  impl->doNotCheckSecurityVoidOverloadMethod(argument_1, argument_2);
}

static void DoNotCheckSecurityVoidOverloadMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceCheckSecurity", "doNotCheckSecurityVoidOverloadMethod");

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());

  V8StringResource<> argument_1;
  int32_t argument_2;
  int num_args_passed = info.Length();
  while (num_args_passed > 0) {
    if (!info[num_args_passed - 1]->IsUndefined())
      break;
    --num_args_passed;
  }
  argument_1 = info[0];
  if (!argument_1.Prepare())
    return;

  if (UNLIKELY(num_args_passed <= 1)) {
    impl->doNotCheckSecurityVoidOverloadMethod(argument_1);
    return;
  }
  argument_2 = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exception_state);
  if (exception_state.HadException())
    return;

  impl->doNotCheckSecurityVoidOverloadMethod(argument_1, argument_2);
}

static int DoNotCheckSecurityVoidOverloadMethodMethodLength() {
  if (RuntimeEnabledFeatures::RuntimeFeatureEnabled()) {
    return 1;
  }
  return 2;
}

static void DoNotCheckSecurityVoidOverloadMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  scheduler::CooperativeSchedulingManager::Instance()->Safepoint();

  bool is_arity_error = false;

  switch (std::min(2, info.Length())) {
    case 1:
      if (RuntimeEnabledFeatures::RuntimeFeatureEnabled()) {
        if (true) {
          DoNotCheckSecurityVoidOverloadMethod2Method(info);
          return;
        }
      }
      break;
    case 2:
      if (RuntimeEnabledFeatures::RuntimeFeatureEnabled()) {
        if (info[1]->IsUndefined()) {
          DoNotCheckSecurityVoidOverloadMethod2Method(info);
          return;
        }
      }
      if (RuntimeEnabledFeatures::RuntimeFeatureEnabled()) {
        if (info[1]->IsNumber()) {
          DoNotCheckSecurityVoidOverloadMethod2Method(info);
          return;
        }
      }
      if (true) {
        DoNotCheckSecurityVoidOverloadMethod1Method(info);
        return;
      }
      if (RuntimeEnabledFeatures::RuntimeFeatureEnabled()) {
        if (true) {
          DoNotCheckSecurityVoidOverloadMethod2Method(info);
          return;
        }
      }
      break;
    default:
      is_arity_error = true;
  }

  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceCheckSecurity", "doNotCheckSecurityVoidOverloadMethod");
  if (is_arity_error) {
    if (info.Length() < test_interface_check_security_v8_internal::DoNotCheckSecurityVoidOverloadMethodMethodLength()) {
      exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(test_interface_check_security_v8_internal::DoNotCheckSecurityVoidOverloadMethodMethodLength(), info.Length()));
      return;
    }
  }
  exception_state.ThrowTypeError("No function was found that matched the signature provided.");
}

static void DoNotCheckSecurityVoidOverloadMethodOriginSafeMethodGetter(const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  V8PerIsolateData* data = V8PerIsolateData::From(isolate);
  const DOMWrapperWorld& world =
      DOMWrapperWorld::World(isolate->GetCurrentContext());
  v8::Local<v8::FunctionTemplate> interface_template =
      data->FindInterfaceTemplate(world, V8TestInterfaceCheckSecurity::GetWrapperTypeInfo());
  v8::Local<v8::Signature> signature =
      v8::Signature::New(isolate, interface_template);

  static int dom_template_key; // This address is used for a key to look up the dom template.
  v8::Local<v8::FunctionTemplate> method_template =
      data->FindOrCreateOperationTemplate(
          world,
          &dom_template_key,
          V8TestInterfaceCheckSecurity::DoNotCheckSecurityVoidOverloadMethodMethodCallback,
          v8::Local<v8::Value>(),
          signature,
          test_interface_check_security_v8_internal::DoNotCheckSecurityVoidOverloadMethodMethodLength());

  V8SetReturnValue(
      info,
      method_template->GetFunction(
          isolate->GetCurrentContext()).ToLocalChecked());
}

static void SecureContextRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterfaceCheckSecurity", "secureContextRuntimeEnabledMethod");

  TestInterfaceCheckSecurity* impl = V8TestInterfaceCheckSecurity::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> arg;
  arg = info[0];
  if (!arg.Prepare())
    return;

  impl->secureContextRuntimeEnabledMethod(arg);
}

static const struct {
  using GetterCallback = void(*)(const v8::PropertyCallbackInfo<v8::Value>&);
  using SetterCallback = void(*)(v8::Local<v8::Value>, const V8CrossOriginCallbackInfo&);

  const char* const name;
  const GetterCallback getter;
  const SetterCallback setter;
} kCrossOriginAttributeTable[] = {
  {
    "doNotCheckSecurityLongAttribute",
    test_interface_check_security_v8_internal::DoNotCheckSecurityLongAttributeAttributeGetter,
    nullptr,
  },
  {
    "doNotCheckSecurityReadonlyLongAttribute",
    test_interface_check_security_v8_internal::DoNotCheckSecurityReadonlyLongAttributeAttributeGetter,
    nullptr,
  },
  {
    "doNotCheckSecurityOnSetterLongAttribute",
    nullptr,
    &test_interface_check_security_v8_internal::DoNotCheckSecurityOnSetterLongAttributeAttributeSetter,
  },
  {
    "doNotCheckSecurityReplaceableReadonlyLongAttribute",
    test_interface_check_security_v8_internal::DoNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetter,
    nullptr,
  },
};

static const struct {
  using ValueCallback = void(*)(const v8::PropertyCallbackInfo<v8::Value>&);

  const char* const name;
  const ValueCallback value;
} kCrossOriginOperationTable[] = {
  {"doNotCheckSecurityVoidMethod", &test_interface_check_security_v8_internal::DoNotCheckSecurityVoidMethodOriginSafeMethodGetter},
  {"doNotCheckSecurityPerWorldBindingsVoidMethod", &test_interface_check_security_v8_internal::DoNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetter},
  {"doNotCheckSecurityUnforgeableVoidMethod", &test_interface_check_security_v8_internal::DoNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodGetter},
  {"doNotCheckSecurityVoidOverloadMethod", &test_interface_check_security_v8_internal::DoNotCheckSecurityVoidOverloadMethodOriginSafeMethodGetter},
};
}  // namespace test_interface_check_security_v8_internal

void V8TestInterfaceCheckSecurity::ReadonlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_readonlyLongAttribute_Getter");

  test_interface_check_security_v8_internal::ReadonlyLongAttributeAttributeGetter(info);
}

void V8TestInterfaceCheckSecurity::LongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_longAttribute_Getter");

  test_interface_check_security_v8_internal::LongAttributeAttributeGetter(info);
}

void V8TestInterfaceCheckSecurity::LongAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_longAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_check_security_v8_internal::LongAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityLongAttributeAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityLongAttribute_Getter");

  test_interface_check_security_v8_internal::DoNotCheckSecurityLongAttributeAttributeGetter(info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityLongAttributeAttributeSetterCallback(
    v8::Local<v8::Name>, v8::Local<v8::Value> v8_value, const v8::PropertyCallbackInfo<void>& info
) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityLongAttribute_Setter");

  test_interface_check_security_v8_internal::DoNotCheckSecurityLongAttributeAttributeSetter(v8_value, info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityReadonlyLongAttributeAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityReadonlyLongAttribute_Getter");

  test_interface_check_security_v8_internal::DoNotCheckSecurityReadonlyLongAttributeAttributeGetter(info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityOnSetterLongAttributeAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityOnSetterLongAttribute_Getter");

  test_interface_check_security_v8_internal::DoNotCheckSecurityOnSetterLongAttributeAttributeGetter(info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityOnSetterLongAttributeAttributeSetterCallback(
    v8::Local<v8::Name>, v8::Local<v8::Value> v8_value, const v8::PropertyCallbackInfo<void>& info
) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityOnSetterLongAttribute_Setter");

  test_interface_check_security_v8_internal::DoNotCheckSecurityOnSetterLongAttributeAttributeSetter(
      v8_value, V8CrossOriginCallbackInfo(info));
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityReplaceableReadonlyLongAttribute_Getter");

  test_interface_check_security_v8_internal::DoNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetter(info);
}

void V8TestInterfaceCheckSecurity::VoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_voidMethod");

  test_interface_check_security_v8_internal::VoidMethodMethod(info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityVoidMethod");

  test_interface_check_security_v8_internal::DoNotCheckSecurityVoidMethodMethod(info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityVoidMethodOriginSafeMethodGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityVoidMethod_OriginSafeMethodGetter");

  test_interface_check_security_v8_internal::DoNotCheckSecurityVoidMethodOriginSafeMethodGetter(info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityPerWorldBindingsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityPerWorldBindingsVoidMethod");

  test_interface_check_security_v8_internal::DoNotCheckSecurityPerWorldBindingsVoidMethodMethod(info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityPerWorldBindingsVoidMethod_OriginSafeMethodGetter");

  test_interface_check_security_v8_internal::DoNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetter(info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityPerWorldBindingsVoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityPerWorldBindingsVoidMethod");

  test_interface_check_security_v8_internal::DoNotCheckSecurityPerWorldBindingsVoidMethodMethodForMainWorld(info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterCallbackForMainWorld(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityPerWorldBindingsVoidMethod_OriginSafeMethodGetter");

  test_interface_check_security_v8_internal::DoNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodGetterForMainWorld(info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityUnforgeableVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityUnforgeableVoidMethod");

  test_interface_check_security_v8_internal::DoNotCheckSecurityUnforgeableVoidMethodMethod(info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityUnforgeableVoidMethod_OriginSafeMethodGetter");

  test_interface_check_security_v8_internal::DoNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodGetter(info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityVoidOverloadMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityVoidOverloadMethod");

  test_interface_check_security_v8_internal::DoNotCheckSecurityVoidOverloadMethodMethod(info);
}

void V8TestInterfaceCheckSecurity::DoNotCheckSecurityVoidOverloadMethodOriginSafeMethodGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_doNotCheckSecurityVoidOverloadMethod_OriginSafeMethodGetter");

  test_interface_check_security_v8_internal::DoNotCheckSecurityVoidOverloadMethodOriginSafeMethodGetter(info);
}

void V8TestInterfaceCheckSecurity::SecureContextRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_secureContextRuntimeEnabledMethod");

  test_interface_check_security_v8_internal::SecureContextRuntimeEnabledMethodMethod(info);
}

bool V8TestInterfaceCheckSecurity::SecurityCheck(v8::Local<v8::Context> accessing_context, v8::Local<v8::Object> accessed_object, v8::Local<v8::Value> data) {
  #error "Unexpected security check for interface TestInterfaceCheckSecurity"
}

void V8TestInterfaceCheckSecurity::CrossOriginNamedGetter(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_CrossOriginNamedGetter");

  if (!name->IsString())
    return;
  const AtomicString& property_name = ToCoreAtomicString(name.As<v8::String>());

  for (const auto& attribute : test_interface_check_security_v8_internal::kCrossOriginAttributeTable) {
    if (property_name == attribute.name && attribute.getter) {
      attribute.getter(info);
      return;
    }
  }
  for (const auto& operation : test_interface_check_security_v8_internal::kCrossOriginOperationTable) {
    if (property_name == operation.name) {
      operation.value(info);
      return;
    }
  }

  // HTML 7.2.3.3 CrossOriginGetOwnPropertyHelper ( O, P )
  // https://html.spec.whatwg.org/C/#crossorigingetownpropertyhelper-(-o,-p-)
  // step 3. If P is "then", @@toStringTag, @@hasInstance, or
  //   @@isConcatSpreadable, then return PropertyDescriptor{ [[Value]]:
  //   undefined, [[Writable]]: false, [[Enumerable]]: false,
  //   [[Configurable]]: true }.
  if (property_name == "then") {
    V8SetReturnValue(info, v8::Undefined(info.GetIsolate()));
    return;
  }

  BindingSecurity::FailedAccessCheckFor(
      info.GetIsolate(),
      V8TestInterfaceCheckSecurity::GetWrapperTypeInfo(),
      info.Holder());
}

void V8TestInterfaceCheckSecurity::CrossOriginNamedSetter(v8::Local<v8::Name> name, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceCheckSecurity_CrossOriginNamedSetter");

  if (!name->IsString())
    return;
  const AtomicString& property_name = ToCoreAtomicString(name.As<v8::String>());

  for (const auto& attribute : test_interface_check_security_v8_internal::kCrossOriginAttributeTable) {
    if (property_name == attribute.name && attribute.setter) {
      attribute.setter(value, V8CrossOriginCallbackInfo(info));
      return;
    }
  }

  BindingSecurity::FailedAccessCheckFor(
      info.GetIsolate(),
      V8TestInterfaceCheckSecurity::GetWrapperTypeInfo(),
      info.Holder());
}

void V8TestInterfaceCheckSecurity::CrossOriginNamedEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  Vector<String> names;
  for (const auto& attribute : test_interface_check_security_v8_internal::kCrossOriginAttributeTable)
    names.push_back(attribute.name);
  for (const auto& operation : test_interface_check_security_v8_internal::kCrossOriginOperationTable)
    names.push_back(operation.name);

  // Use the current context as the creation context, as a cross-origin access
  // may involve an object that does not have a creation context.
  V8SetReturnValue(info,
                   ToV8(names, info.GetIsolate()->GetCurrentContext()->Global(),
                        info.GetIsolate()).As<v8::Array>());
}

static constexpr V8DOMConfiguration::MethodConfiguration kV8TestInterfaceCheckSecurityMethods[] = {
    {"voidMethod", V8TestInterfaceCheckSecurity::VoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

static void InstallV8TestInterfaceCheckSecurityTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interface_template, V8TestInterfaceCheckSecurity::GetWrapperTypeInfo()->interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceCheckSecurity::kInternalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Global object prototype chain consists of Immutable Prototype Exotic Objects
  prototype_template->SetImmutableProto();

  // Global objects are Immutable Prototype Exotic Objects
  instance_template->SetImmutableProto();

  // Register IDL constants, attributes and operations.
  static constexpr V8DOMConfiguration::AttributeConfiguration
  kAttributeConfigurations[] = {
      { "doNotCheckSecurityLongAttribute", V8TestInterfaceCheckSecurity::DoNotCheckSecurityLongAttributeAttributeGetterCallback, V8TestInterfaceCheckSecurity::DoNotCheckSecurityLongAttributeAttributeSetterCallback, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "doNotCheckSecurityReadonlyLongAttribute", V8TestInterfaceCheckSecurity::DoNotCheckSecurityReadonlyLongAttributeAttributeGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "doNotCheckSecurityOnSetterLongAttribute", V8TestInterfaceCheckSecurity::DoNotCheckSecurityOnSetterLongAttributeAttributeGetterCallback, V8TestInterfaceCheckSecurity::DoNotCheckSecurityOnSetterLongAttributeAttributeSetterCallback, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "doNotCheckSecurityReplaceableReadonlyLongAttribute", V8TestInterfaceCheckSecurity::DoNotCheckSecurityReplaceableReadonlyLongAttributeAttributeGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
  };
  V8DOMConfiguration::InstallAttributes(
      isolate, world, instance_template, prototype_template,
      kAttributeConfigurations, base::size(kAttributeConfigurations));
  static constexpr V8DOMConfiguration::AccessorConfiguration
  kAccessorConfigurations[] = {
      { "readonlyLongAttribute", V8TestInterfaceCheckSecurity::ReadonlyLongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "longAttribute", V8TestInterfaceCheckSecurity::LongAttributeAttributeGetterCallback, V8TestInterfaceCheckSecurity::LongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
  };
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instance_template, prototype_template, interface_template,
      signature, kAccessorConfigurations,
      base::size(kAccessorConfigurations));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instance_template, prototype_template, interface_template,
      signature, kV8TestInterfaceCheckSecurityMethods, base::size(kV8TestInterfaceCheckSecurityMethods));

  // Cross-origin access check
  instance_template->SetAccessCheckCallbackAndHandler(
      V8TestInterfaceCheckSecurity::SecurityCheck,
      v8::NamedPropertyHandlerConfiguration(
          V8TestInterfaceCheckSecurity::CrossOriginNamedGetter,
          V8TestInterfaceCheckSecurity::CrossOriginNamedSetter,
          nullptr,
          nullptr,
          V8TestInterfaceCheckSecurity::CrossOriginNamedEnumerator),
      v8::IndexedPropertyHandlerConfiguration(nullptr),
      v8::External::New(isolate, const_cast<WrapperTypeInfo*>(V8TestInterfaceCheckSecurity::GetWrapperTypeInfo())));

  // Custom signature
  static const V8DOMConfiguration::MethodConfiguration kDoNotCheckSecurityVoidMethodOriginSafeMethodConfiguration[] = {
      {
          "doNotCheckSecurityVoidMethod",
          V8TestInterfaceCheckSecurity::DoNotCheckSecurityVoidMethodMethodCallback,
          0,
          static_cast<v8::PropertyAttribute>(v8::None),
          V8DOMConfiguration::kOnInstance,
          V8DOMConfiguration::kCheckHolder,
          V8DOMConfiguration::kCheckAccess,
          V8DOMConfiguration::kHasSideEffect,
          V8DOMConfiguration::kAllWorlds,
      }
  };
  for (const auto& method_config : kDoNotCheckSecurityVoidMethodOriginSafeMethodConfiguration)
    V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, method_config);
  static const V8DOMConfiguration::MethodConfiguration kDoNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodConfiguration[] = {
      {
          "doNotCheckSecurityPerWorldBindingsVoidMethod",
          V8TestInterfaceCheckSecurity::DoNotCheckSecurityPerWorldBindingsVoidMethodMethodCallbackForMainWorld,
          0,
          static_cast<v8::PropertyAttribute>(v8::None),
          V8DOMConfiguration::kOnInstance,
          V8DOMConfiguration::kCheckHolder,
          V8DOMConfiguration::kCheckAccess,
          V8DOMConfiguration::kHasSideEffect,
          V8DOMConfiguration::MainWorld,
      },
      {
          "doNotCheckSecurityPerWorldBindingsVoidMethod",
          V8TestInterfaceCheckSecurity::DoNotCheckSecurityPerWorldBindingsVoidMethodMethodCallback,
          0,
          static_cast<v8::PropertyAttribute>(v8::None),
          V8DOMConfiguration::kOnInstance,
          V8DOMConfiguration::kCheckHolder,
          V8DOMConfiguration::kCheckAccess,
          V8DOMConfiguration::kHasSideEffect,
          V8DOMConfiguration::NonMainWorlds,
      }
  };
  for (const auto& method_config : kDoNotCheckSecurityPerWorldBindingsVoidMethodOriginSafeMethodConfiguration)
    V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, method_config);
  static const V8DOMConfiguration::MethodConfiguration kDoNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodConfiguration[] = {
      {
          "doNotCheckSecurityUnforgeableVoidMethod",
          V8TestInterfaceCheckSecurity::DoNotCheckSecurityUnforgeableVoidMethodMethodCallback,
          0,
          static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete),
          V8DOMConfiguration::kOnInstance,
          V8DOMConfiguration::kCheckHolder,
          V8DOMConfiguration::kCheckAccess,
          V8DOMConfiguration::kHasSideEffect,
          V8DOMConfiguration::kAllWorlds,
      }
  };
  for (const auto& method_config : kDoNotCheckSecurityUnforgeableVoidMethodOriginSafeMethodConfiguration)
    V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, method_config);
  static const V8DOMConfiguration::MethodConfiguration kDoNotCheckSecurityVoidOverloadMethodOriginSafeMethodConfiguration[] = {
      {
          "doNotCheckSecurityVoidOverloadMethod",
          V8TestInterfaceCheckSecurity::DoNotCheckSecurityVoidOverloadMethodMethodCallback,
          test_interface_check_security_v8_internal::DoNotCheckSecurityVoidOverloadMethodMethodLength(),
          static_cast<v8::PropertyAttribute>(v8::None),
          V8DOMConfiguration::kOnInstance,
          V8DOMConfiguration::kCheckHolder,
          V8DOMConfiguration::kCheckAccess,
          V8DOMConfiguration::kHasSideEffect,
          V8DOMConfiguration::kAllWorlds,
      }
  };
  for (const auto& method_config : kDoNotCheckSecurityVoidOverloadMethodOriginSafeMethodConfiguration)
    V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, method_config);

  V8TestInterfaceCheckSecurity::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interface_template);
}

void V8TestInterfaceCheckSecurity::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceCheckSecurity::DomTemplate(
    v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world, const_cast<WrapperTypeInfo*>(V8TestInterfaceCheckSecurity::GetWrapperTypeInfo()),
      InstallV8TestInterfaceCheckSecurityTemplate);
}

bool V8TestInterfaceCheckSecurity::HasInstance(v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(V8TestInterfaceCheckSecurity::GetWrapperTypeInfo(), v8_value);
}

v8::Local<v8::Object> V8TestInterfaceCheckSecurity::FindInstanceInPrototypeChain(
    v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(
      V8TestInterfaceCheckSecurity::GetWrapperTypeInfo(), v8_value);
}

TestInterfaceCheckSecurity* V8TestInterfaceCheckSecurity::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceCheckSecurity* NativeValueTraits<TestInterfaceCheckSecurity>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestInterfaceCheckSecurity* native_value = V8TestInterfaceCheckSecurity::ToImplWithTypeCheck(isolate, value);
  if (!native_value) {
    exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceCheckSecurity"));
  }
  return native_value;
}

void V8TestInterfaceCheckSecurity::InstallConditionalFeatures(
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

  if (!instance_object.IsEmpty()) {
    if (is_secure_context) {
      if (RuntimeEnabledFeatures::RuntimeFeatureEnabled()) {
        {
          // Install secureContextRuntimeEnabledMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"secureContextRuntimeEnabledMethod", V8TestInterfaceCheckSecurity::SecureContextRuntimeEnabledMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
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

}  // namespace blink
