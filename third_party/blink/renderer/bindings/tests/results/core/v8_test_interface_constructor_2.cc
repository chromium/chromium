// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_constructor_2.h"

#include <algorithm>

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
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo v8_test_interface_constructor_2_wrapper_type_info = {
    gin::kEmbedderBlink,
    V8TestInterfaceConstructor2::DomTemplate,
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
const WrapperTypeInfo& TestInterfaceConstructor2::wrapper_type_info_ = v8_test_interface_constructor_2_wrapper_type_info;

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

static void Constructor1(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor2_ConstructorCallback");

  V8StringResource<> string_arg;
  string_arg = info[0];
  if (!string_arg.Prepare())
    return;

  TestInterfaceConstructor2* impl = TestInterfaceConstructor2::Create(string_arg);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), V8TestInterfaceConstructor2::GetWrapperTypeInfo(), wrapper);
  V8SetReturnValue(info, wrapper);
}

static void Constructor2(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor2_ConstructorCallback");

  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor2");

  Dictionary dictionary_arg;
  if (!info[0]->IsNullOrUndefined() && !info[0]->IsObject()) {
    exception_state.ThrowTypeError("parameter 1 ('dictionaryArg') is not an object.");
    return;
  }
  dictionary_arg = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  TestInterfaceConstructor2* impl = TestInterfaceConstructor2::Create(dictionary_arg);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), V8TestInterfaceConstructor2::GetWrapperTypeInfo(), wrapper);
  V8SetReturnValue(info, wrapper);
}

static void Constructor3(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor2_ConstructorCallback");

  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor2");

  Vector<Vector<String>> string_sequence_sequence_arg;
  string_sequence_sequence_arg = NativeValueTraits<IDLSequence<IDLSequence<IDLString>>>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  TestInterfaceConstructor2* impl = TestInterfaceConstructor2::Create(string_sequence_sequence_arg);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), V8TestInterfaceConstructor2::GetWrapperTypeInfo(), wrapper);
  V8SetReturnValue(info, wrapper);
}

static void Constructor4(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor2_ConstructorCallback");

  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor2");

  TestInterfaceEmpty* test_interface_empty_arg;
  int32_t long_arg;
  V8StringResource<> default_undefined_optional_string_arg;
  V8StringResource<> default_null_string_optional_string_arg;
  Dictionary default_undefined_optional_dictionary_arg;
  V8StringResource<> optional_string_arg;
  int num_args_passed = info.Length();
  while (num_args_passed > 0) {
    if (!info[num_args_passed - 1]->IsUndefined())
      break;
    --num_args_passed;
  }
  test_interface_empty_arg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!test_interface_empty_arg) {
    exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(0, "TestInterfaceEmpty"));
    return;
  }

  long_arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[1], exception_state);
  if (exception_state.HadException())
    return;

  default_undefined_optional_string_arg = info[2];
  if (!default_undefined_optional_string_arg.Prepare())
    return;

  if (!info[3]->IsUndefined()) {
    default_null_string_optional_string_arg = info[3];
    if (!default_null_string_optional_string_arg.Prepare())
      return;
  } else {
    default_null_string_optional_string_arg = nullptr;
  }
  if (!info[4]->IsNullOrUndefined() && !info[4]->IsObject()) {
    exception_state.ThrowTypeError("parameter 5 ('defaultUndefinedOptionalDictionaryArg') is not an object.");
    return;
  }
  default_undefined_optional_dictionary_arg = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[4], exception_state);
  if (exception_state.HadException())
    return;

  if (UNLIKELY(num_args_passed <= 5)) {
    TestInterfaceConstructor2* impl = TestInterfaceConstructor2::Create(test_interface_empty_arg, long_arg, default_undefined_optional_string_arg, default_null_string_optional_string_arg, default_undefined_optional_dictionary_arg);
    v8::Local<v8::Object> wrapper = info.Holder();
    wrapper = impl->AssociateWithWrapper(info.GetIsolate(), V8TestInterfaceConstructor2::GetWrapperTypeInfo(), wrapper);
    V8SetReturnValue(info, wrapper);
    return;
  }
  optional_string_arg = info[5];
  if (!optional_string_arg.Prepare())
    return;

  TestInterfaceConstructor2* impl = TestInterfaceConstructor2::Create(test_interface_empty_arg, long_arg, default_undefined_optional_string_arg, default_null_string_optional_string_arg, default_undefined_optional_dictionary_arg, optional_string_arg);
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), V8TestInterfaceConstructor2::GetWrapperTypeInfo(), wrapper);
  V8SetReturnValue(info, wrapper);
}

static void Constructor(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor2");
  switch (std::min(6, info.Length())) {
    case 1:
      if (info[0]->IsArray()) {
        test_interface_constructor_2_v8_internal::Constructor3(info);
        return;
      }
      if (HasCallableIteratorSymbol(info.GetIsolate(), info[0], exception_state)) {
        test_interface_constructor_2_v8_internal::Constructor3(info);
        return;
      }
      if (exception_state.HadException()) {
        exception_state.RethrowV8Exception(exception_state.GetException());
        return;
      }
      if (info[0]->IsObject()) {
        test_interface_constructor_2_v8_internal::Constructor2(info);
        return;
      }
      if (true) {
        test_interface_constructor_2_v8_internal::Constructor1(info);
        return;
      }
      break;
    case 2:
      if (true) {
        test_interface_constructor_2_v8_internal::Constructor4(info);
        return;
      }
      break;
    case 3:
      if (true) {
        test_interface_constructor_2_v8_internal::Constructor4(info);
        return;
      }
      break;
    case 4:
      if (true) {
        test_interface_constructor_2_v8_internal::Constructor4(info);
        return;
      }
      break;
    case 5:
      if (true) {
        test_interface_constructor_2_v8_internal::Constructor4(info);
        return;
      }
      break;
    case 6:
      if (true) {
        test_interface_constructor_2_v8_internal::Constructor4(info);
        return;
      }
      break;
    default:
      exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
  }
  exception_state.ThrowTypeError("No matching constructor signature.");
}

CORE_EXPORT void ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor2_Constructor");

  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(
        info.GetIsolate(),
        ExceptionMessages::ConstructorNotCallableAsFunction("TestInterfaceConstructor2"));
    return;
  }

  if (ConstructorMode::Current(info.GetIsolate()) == ConstructorMode::kWrapExistingObject) {
    V8SetReturnValue(info, info.Holder());
    return;
  }

  test_interface_constructor_2_v8_internal::Constructor(info);
}

}  // namespace test_interface_constructor_2_v8_internal

static void InstallV8TestInterfaceConstructor2Template(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interface_template, V8TestInterfaceConstructor2::GetWrapperTypeInfo()->interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceConstructor2::kInternalFieldCount);
  interface_template->SetCallHandler(test_interface_constructor_2_v8_internal::ConstructorCallback);
  interface_template->SetLength(1);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.

  // Custom signature

  V8TestInterfaceConstructor2::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interface_template);
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceConstructor2::DomTemplate(
    v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world, const_cast<WrapperTypeInfo*>(V8TestInterfaceConstructor2::GetWrapperTypeInfo()),
      InstallV8TestInterfaceConstructor2Template);
}

bool V8TestInterfaceConstructor2::HasInstance(v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(V8TestInterfaceConstructor2::GetWrapperTypeInfo(), v8_value);
}

v8::Local<v8::Object> V8TestInterfaceConstructor2::FindInstanceInPrototypeChain(
    v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(
      V8TestInterfaceConstructor2::GetWrapperTypeInfo(), v8_value);
}

TestInterfaceConstructor2* V8TestInterfaceConstructor2::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceConstructor2* NativeValueTraits<TestInterfaceConstructor2>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestInterfaceConstructor2* native_value = V8TestInterfaceConstructor2::ToImplWithTypeCheck(isolate, value);
  if (!native_value) {
    exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceConstructor2"));
  }
  return native_value;
}

}  // namespace blink
