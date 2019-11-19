// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/js_event_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_for_each_iterator_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_2.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_empty.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_mixin_2.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_partial.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_partial_2_implementation.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_partial_secure_context.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_mixin_3_implementation.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
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
WrapperTypeInfo v8_test_interface_wrapper_type_info = {
    gin::kEmbedderBlink,
    V8TestInterface::DomTemplate,
    V8TestInterface::InstallConditionalFeatures,
    "TestInterface",
    V8TestInterfaceEmpty::GetWrapperTypeInfo(),
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceImplementation.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceImplementation::wrapper_type_info_ = v8_test_interface_wrapper_type_info;

// [ActiveScriptWrappable]
static_assert(
    std::is_base_of<ActiveScriptWrappableBase, TestInterfaceImplementation>::value,
    "TestInterfaceImplementation does not inherit from ActiveScriptWrappable<>, but specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    !std::is_same<decltype(&TestInterfaceImplementation::HasPendingActivity),
                  decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceImplementation is not overriding hasPendingActivity(), but is specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_implementation_v8_internal {
static void (*voidMethodPartialOverloadMethodForPartialInterface)(const v8::FunctionCallbackInfo<v8::Value>&) = 0;
static void (*staticVoidMethodPartialOverloadMethodForPartialInterface)(const v8::FunctionCallbackInfo<v8::Value>&) = 0;
static void (*promiseMethodPartialOverloadMethodForPartialInterface)(const v8::FunctionCallbackInfo<v8::Value>&) = 0;
static void (*staticPromiseMethodPartialOverloadMethodForPartialInterface)(const v8::FunctionCallbackInfo<v8::Value>&) = 0;
static void (*partial2VoidMethodMethodForPartialInterface)(const v8::FunctionCallbackInfo<v8::Value>&) = 0;
static void (*partial2StaticVoidMethodMethodForPartialInterface)(const v8::FunctionCallbackInfo<v8::Value>&) = 0;

static void TestInterfaceAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->testInterfaceAttribute()), impl);
}

static void TestInterfaceAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "testInterfaceAttribute");

  // Prepare the value to be set.
  TestInterfaceImplementation* cpp_value = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8_value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cpp_value) {
    exception_state.ThrowTypeError("The provided value is not of type 'TestInterface'.");
    return;
  }

  impl->setTestInterfaceAttribute(cpp_value);
}

static void DoubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValue(info, impl->doubleAttribute());
}

static void DoubleAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "doubleAttribute");

  // Prepare the value to be set.
  double cpp_value = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setDoubleAttribute(cpp_value);
}

static void FloatAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValue(info, impl->floatAttribute());
}

static void FloatAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "floatAttribute");

  // Prepare the value to be set.
  float cpp_value = NativeValueTraits<IDLFloat>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setFloatAttribute(cpp_value);
}

static void UnrestrictedDoubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValue(info, impl->unrestrictedDoubleAttribute());
}

static void UnrestrictedDoubleAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "unrestrictedDoubleAttribute");

  // Prepare the value to be set.
  double cpp_value = NativeValueTraits<IDLUnrestrictedDouble>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setUnrestrictedDoubleAttribute(cpp_value);
}

static void UnrestrictedFloatAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValue(info, impl->unrestrictedFloatAttribute());
}

static void UnrestrictedFloatAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "unrestrictedFloatAttribute");

  // Prepare the value to be set.
  float cpp_value = NativeValueTraits<IDLUnrestrictedFloat>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setUnrestrictedFloatAttribute(cpp_value);
}

static void TestEnumAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, impl->testEnumAttribute(), info.GetIsolate());
}

static void TestEnumAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "testEnumAttribute");

  // Prepare the value to be set.
  V8StringResource<> cpp_value = v8_value;
  if (!cpp_value.Prepare())
    return;

  // Type check per: http://heycam.github.io/webidl/#dfn-attribute-setter
  // Returns undefined without setting the value if the value is invalid.
  DummyExceptionStateForTesting dummy_exception_state;
  {
    const char* const kValidValues[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
    if (!IsValidEnum(cpp_value, kValidValues, base::size(kValidValues),
                     "TestEnum", dummy_exception_state)) {
      ExecutionContext::ForCurrentRealm(info)->AddConsoleMessage(
          ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                                 mojom::ConsoleMessageLevel::kWarning,
                                 dummy_exception_state.Message()));
      return;
    }
  }

  impl->setTestEnumAttribute(cpp_value);
}

static void TestEnumOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueStringOrNull(info, impl->testEnumOrNullAttribute(), info.GetIsolate());
}

static void TestEnumOrNullAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "testEnumOrNullAttribute");

  // Prepare the value to be set.
  V8StringResource<kTreatNullAndUndefinedAsNullString> cpp_value = v8_value;
  if (!cpp_value.Prepare())
    return;

  // Type check per: http://heycam.github.io/webidl/#dfn-attribute-setter
  // Returns undefined without setting the value if the value is invalid.
  DummyExceptionStateForTesting dummy_exception_state;
  {
    const char* const kValidValues[] = {
      nullptr,
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
    if (!IsValidEnum(cpp_value, kValidValues, base::size(kValidValues),
                     "TestEnum", dummy_exception_state)) {
      ExecutionContext::ForCurrentRealm(info)->AddConsoleMessage(
          ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                                 mojom::ConsoleMessageLevel::kWarning,
                                 dummy_exception_state.Message()));
      return;
    }
  }

  impl->setTestEnumOrNullAttribute(cpp_value);
}

static void StringOrDoubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  StringOrDouble result;
  impl->stringOrDoubleAttribute(result);

  V8SetReturnValue(info, result);
}

static void StringOrDoubleAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "stringOrDoubleAttribute");

  // Prepare the value to be set.
  StringOrDouble cpp_value;
  V8StringOrDouble::ToImpl(info.GetIsolate(), v8_value, cpp_value, UnionTypeConversionMode::kNotNullable, exception_state);
  if (exception_state.HadException())
    return;

  impl->setStringOrDoubleAttribute(cpp_value);
}

static void WithExtendedAttributeStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, impl->withExtendedAttributeStringAttribute());
}

static void WithExtendedAttributeStringAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "withExtendedAttributeStringAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setWithExtendedAttributeStringAttribute(cpp_value);
}

static void UncapitalAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->CapitalImplementation()), impl);
}

static void UncapitalAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "uncapitalAttribute");

  // Prepare the value to be set.
  Implementation* cpp_value = V8Implementation::ToImplWithTypeCheck(info.GetIsolate(), v8_value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cpp_value) {
    exception_state.ThrowTypeError("The provided value is not of type 'Implementation'.");
    return;
  }

  impl->setCapitalImplementation(cpp_value);
}

static void ConditionalLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, impl->conditionalLongAttribute());
}

static void ConditionalLongAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "conditionalLongAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setConditionalLongAttribute(cpp_value);
}

static void ConditionalReadOnlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, impl->conditionalReadOnlyLongAttribute());
}

static void StaticStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueString(info, TestInterfaceImplementation::staticStringAttribute(), info.GetIsolate());
}

static void StaticStringAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // Prepare the value to be set.
  V8StringResource<> cpp_value = v8_value;
  if (!cpp_value.Prepare())
    return;

  TestInterfaceImplementation::setStaticStringAttribute(cpp_value);
}

static void StaticReturnDOMWrapperAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, WTF::GetPtr(TestInterfaceImplementation::staticReturnDOMWrapperAttribute()), info.GetIsolate()->GetCurrentContext()->Global());
}

static void StaticReturnDOMWrapperAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "staticReturnDOMWrapperAttribute");

  // Prepare the value to be set.
  TestInterfaceImplementation* cpp_value = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8_value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cpp_value) {
    exception_state.ThrowTypeError("The provided value is not of type 'TestInterface'.");
    return;
  }

  TestInterfaceImplementation::setStaticReturnDOMWrapperAttribute(cpp_value);
}

static void StaticReadOnlyStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueString(info, TestInterfaceImplementation::staticReadOnlyStringAttribute(), info.GetIsolate());
}

static void StaticReadOnlyReturnDOMWrapperAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, WTF::GetPtr(TestInterfaceImplementation::staticReadOnlyReturnDOMWrapperAttribute()), info.GetIsolate()->GetCurrentContext()->Global());
}

static void StaticConditionalReadOnlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueInt(info, TestInterfaceImplementation::staticConditionalReadOnlyLongAttribute());
}

static void StringNullAsEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, impl->stringNullAsEmptyAttribute(), info.GetIsolate());
}

static void StringNullAsEmptyAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<kTreatNullAsEmptyString> cpp_value = v8_value;
  if (!cpp_value.Prepare())
    return;

  impl->setStringNullAsEmptyAttribute(cpp_value);
}

static void UsvStringOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueStringOrNull(info, impl->usvStringOrNullAttribute(), info.GetIsolate());
}

static void UsvStringOrNullAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "usvStringOrNullAttribute");

  // Prepare the value to be set.
  V8StringResource<kTreatNullAndUndefinedAsNullString> cpp_value = NativeValueTraits<IDLUSVStringOrNull>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setUsvStringOrNullAttribute(cpp_value);
}

static void AlwaysExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, impl->alwaysExposedAttribute());
}

static void AlwaysExposedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "alwaysExposedAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setAlwaysExposedAttribute(cpp_value);
}

static void WorkerExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, impl->workerExposedAttribute());
}

static void WorkerExposedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "workerExposedAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setWorkerExposedAttribute(cpp_value);
}

static void WindowExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, impl->windowExposedAttribute());
}

static void WindowExposedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "windowExposedAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setWindowExposedAttribute(cpp_value);
}

static void LenientThisAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  // [LenientThis]
  // Make sure that info.Holder() really points to an instance if [LenientThis].
  if (!V8TestInterface::HasInstance(info.Holder(), info.GetIsolate()))
    return; // Return silently because of [LenientThis].

  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValue(info, impl->lenientThisAttribute().V8Value());
}

static void LenientThisAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [LenientThis]
  // Make sure that info.Holder() really points to an instance if [LenientThis].
  if (!V8TestInterface::HasInstance(holder, isolate))
    return; // Return silently because of [LenientThis].

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  // Prepare the value to be set.
  ScriptValue cpp_value = ScriptValue(info.GetIsolate(), v8_value);

  impl->setLenientThisAttribute(cpp_value);
}

static void AttributeWithSideEffectFreeGetterAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->attributeWithSideEffectFreeGetter());
}

static void AttributeWithSideEffectFreeGetterAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "attributeWithSideEffectFreeGetter");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setAttributeWithSideEffectFreeGetter(cpp_value);
}

static void SecureContextAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextAttribute());
}

static void SecureContextAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setSecureContextAttribute(cpp_value);
}

static void SecureContextRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextRuntimeEnabledAttribute());
}

static void SecureContextRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setSecureContextRuntimeEnabledAttribute(cpp_value);
}

static void SecureContextnessRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextnessRuntimeEnabledAttribute());
}

static void SecureContextnessRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextnessRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setSecureContextnessRuntimeEnabledAttribute(cpp_value);
}

static void SecureContextWindowExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWindowExposedAttribute());
}

static void SecureContextWindowExposedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextWindowExposedAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setSecureContextWindowExposedAttribute(cpp_value);
}

static void SecureContextWorkerExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWorkerExposedAttribute());
}

static void SecureContextWorkerExposedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextWorkerExposedAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setSecureContextWorkerExposedAttribute(cpp_value);
}

static void SecureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWindowExposedRuntimeEnabledAttribute());
}

static void SecureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextWindowExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setSecureContextWindowExposedRuntimeEnabledAttribute(cpp_value);
}

static void SecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWorkerExposedRuntimeEnabledAttribute());
}

static void SecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextWorkerExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  impl->setSecureContextWorkerExposedRuntimeEnabledAttribute(cpp_value);
}

static void MixinReadonlyStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, impl->mixinReadonlyStringAttribute(), info.GetIsolate());
}

static void MixinStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, impl->mixinStringAttribute(), info.GetIsolate());
}

static void MixinStringAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cpp_value = v8_value;
  if (!cpp_value.Prepare())
    return;

  impl->setMixinStringAttribute(cpp_value);
}

static void MixinNodeAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->mixinNodeAttribute()), impl);
}

static void MixinNodeAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "mixinNodeAttribute");

  // Prepare the value to be set.
  Node* cpp_value = V8Node::ToImplWithTypeCheck(info.GetIsolate(), v8_value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cpp_value) {
    exception_state.ThrowTypeError("The provided value is not of type 'Node'.");
    return;
  }

  impl->setMixinNodeAttribute(cpp_value);
}

static void MixinEventHandlerAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  EventListener* cpp_value(WTF::GetPtr(impl->mixinEventHandlerAttribute()));

  V8SetReturnValue(info, JSEventHandler::AsV8Value(info.GetIsolate(), impl, cpp_value));
}

static void MixinEventHandlerAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  // Prepare the value to be set.

  impl->setMixinEventHandlerAttribute(JSEventHandler::CreateOrNull(v8_value, JSEventHandler::HandlerType::kEventHandler));
}

static void MixinRuntimeEnabledNodeAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->mixinRuntimeEnabledNodeAttribute()), impl);
}

static void MixinRuntimeEnabledNodeAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "mixinRuntimeEnabledNodeAttribute");

  // Prepare the value to be set.
  Node* cpp_value = V8Node::ToImplWithTypeCheck(info.GetIsolate(), v8_value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cpp_value) {
    exception_state.ThrowTypeError("The provided value is not of type 'Node'.");
    return;
  }

  impl->setMixinRuntimeEnabledNodeAttribute(cpp_value);
}

static void Mixin2StringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, TestInterfaceMixin2::mixin2StringAttribute(*impl), info.GetIsolate());
}

static void Mixin2StringAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cpp_value = v8_value;
  if (!cpp_value.Prepare())
    return;

  TestInterfaceMixin2::setMixin2StringAttribute(*impl, cpp_value);
}

static void Mixin3StringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, TestMixin3Implementation::mixin3StringAttribute(*impl), info.GetIsolate());
}

static void Mixin3StringAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cpp_value = v8_value;
  if (!cpp_value.Prepare())
    return;

  TestMixin3Implementation::setMixin3StringAttribute(*impl, cpp_value);
}

static void PartialLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, TestInterfacePartial::partialLongAttribute(*impl));
}

static void PartialLongAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partialLongAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  TestInterfacePartial::setPartialLongAttribute(*impl, cpp_value);
}

static void PartialStaticLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueInt(info, TestInterfacePartial::partialStaticLongAttribute());
}

static void PartialStaticLongAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partialStaticLongAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  TestInterfacePartial::setPartialStaticLongAttribute(cpp_value);
}

static void PartialCallWithExecutionContextLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExecutionContext* execution_context = ExecutionContext::ForRelevantRealm(info);

  V8SetReturnValueInt(info, TestInterfacePartial::partialCallWithExecutionContextLongAttribute(execution_context, *impl));
}

static void PartialCallWithExecutionContextLongAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partialCallWithExecutionContextLongAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  ExecutionContext* execution_context = ExecutionContext::ForRelevantRealm(info);

  TestInterfacePartial::setPartialCallWithExecutionContextLongAttribute(execution_context, *impl, cpp_value);
}

static void PartialPartialEnumTypeAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, TestInterfacePartial::partialPartialEnumTypeAttribute(*impl), info.GetIsolate());
}

static void PartialPartialEnumTypeAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partialPartialEnumTypeAttribute");

  // Prepare the value to be set.
  V8StringResource<> cpp_value = v8_value;
  if (!cpp_value.Prepare())
    return;

  // Type check per: http://heycam.github.io/webidl/#dfn-attribute-setter
  // Returns undefined without setting the value if the value is invalid.
  DummyExceptionStateForTesting dummy_exception_state;
  {
    const char* const kValidValues[] = {
      "foo",
      "bar",
  };
    if (!IsValidEnum(cpp_value, kValidValues, base::size(kValidValues),
                     "PartialEnumType", dummy_exception_state)) {
      ExecutionContext::ForCurrentRealm(info)->AddConsoleMessage(
          ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                                 mojom::ConsoleMessageLevel::kWarning,
                                 dummy_exception_state.Message()));
      return;
    }
  }

  TestInterfacePartial::setPartialPartialEnumTypeAttribute(*impl, cpp_value);
}

static void PartialSecureContextLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, TestInterfacePartial::partialSecureContextLongAttribute(*impl));
}

static void PartialSecureContextLongAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextLongAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  TestInterfacePartial::setPartialSecureContextLongAttribute(*impl, cpp_value);
}

static void Partial2LongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, TestInterfacePartial2Implementation::partial2LongAttribute(*impl));
}

static void Partial2LongAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partial2LongAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  TestInterfacePartial2Implementation::setPartial2LongAttribute(*impl, cpp_value);
}

static void Partial2StaticLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueInt(info, TestInterfacePartial2Implementation::partial2StaticLongAttribute());
}

static void Partial2StaticLongAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partial2StaticLongAttribute");

  // Prepare the value to be set.
  int32_t cpp_value = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  TestInterfacePartial2Implementation::setPartial2StaticLongAttribute(cpp_value);
}

static void Partial2SecureContextAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartial2Implementation::partial2SecureContextAttribute(*impl));
}

static void Partial2SecureContextAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partial2SecureContextAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  TestInterfacePartial2Implementation::setPartial2SecureContextAttribute(*impl, cpp_value);
}

static void PartialSecureContextAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartialSecureContext::partialSecureContextAttribute(*impl));
}

static void PartialSecureContextAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  TestInterfacePartialSecureContext::setPartialSecureContextAttribute(*impl, cpp_value);
}

static void PartialSecureContextRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartialSecureContext::partialSecureContextRuntimeEnabledAttribute(*impl));
}

static void PartialSecureContextRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  TestInterfacePartialSecureContext::setPartialSecureContextRuntimeEnabledAttribute(*impl, cpp_value);
}

static void PartialSecureContextWindowExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartialSecureContext::partialSecureContextWindowExposedAttribute(*impl));
}

static void PartialSecureContextWindowExposedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextWindowExposedAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  TestInterfacePartialSecureContext::setPartialSecureContextWindowExposedAttribute(*impl, cpp_value);
}

static void PartialSecureContextWorkerExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartialSecureContext::partialSecureContextWorkerExposedAttribute(*impl));
}

static void PartialSecureContextWorkerExposedAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextWorkerExposedAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  TestInterfacePartialSecureContext::setPartialSecureContextWorkerExposedAttribute(*impl, cpp_value);
}

static void PartialSecureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartialSecureContext::partialSecureContextWindowExposedRuntimeEnabledAttribute(*impl));
}

static void PartialSecureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextWindowExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  TestInterfacePartialSecureContext::setPartialSecureContextWindowExposedRuntimeEnabledAttribute(*impl, cpp_value);
}

static void PartialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartialSecureContext::partialSecureContextWorkerExposedRuntimeEnabledAttribute(*impl));
}

static void PartialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(
    v8::Local<v8::Value> v8_value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exception_state(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextWorkerExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cpp_value = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8_value, exception_state);
  if (exception_state.HadException())
    return;

  TestInterfacePartialSecureContext::setPartialSecureContextWorkerExposedRuntimeEnabledAttribute(*impl, cpp_value);
}

static void VoidMethodTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyArg", "TestInterface", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* test_interface_empty_arg;
  test_interface_empty_arg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!test_interface_empty_arg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyArg", "TestInterface", ExceptionMessages::ArgumentNotOfType(0, "TestInterfaceEmpty")));
    return;
  }

  impl->voidMethodTestInterfaceEmptyArg(test_interface_empty_arg);
}

static void VoidMethodDoubleArgFloatArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "voidMethodDoubleArgFloatArg");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  double double_arg;
  float float_arg;
  double_arg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  float_arg = NativeValueTraits<IDLFloat>::NativeValue(info.GetIsolate(), info[1], exception_state);
  if (exception_state.HadException())
    return;

  impl->voidMethodDoubleArgFloatArg(double_arg, float_arg);
}

static void VoidMethodNullableAndOptionalObjectArgsMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNullableAndOptionalObjectArgs", "TestInterface", ExceptionMessages::NotEnoughArguments(2, info.Length())));
    return;
  }

  ScriptValue object_arg;
  ScriptValue nullable_object_arg;
  ScriptValue optional_object_arg;
  int num_args_passed = info.Length();
  while (num_args_passed > 0) {
    if (!info[num_args_passed - 1]->IsUndefined())
      break;
    --num_args_passed;
  }
  if (info[0]->IsObject()) {
    object_arg = ScriptValue(info.GetIsolate(), info[0]);
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNullableAndOptionalObjectArgs", "TestInterface", "parameter 1 ('objectArg') is not an object."));
    return;
  }

  if (info[1]->IsObject()) {
    nullable_object_arg = ScriptValue(info.GetIsolate(), info[1]);
  } else if (info[1]->IsNullOrUndefined()) {
    nullable_object_arg = ScriptValue(info.GetIsolate(), v8::Null(info.GetIsolate()));
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNullableAndOptionalObjectArgs", "TestInterface", "parameter 2 ('nullableObjectArg') is not an object."));
    return;
  }

  if (UNLIKELY(num_args_passed <= 2)) {
    impl->voidMethodNullableAndOptionalObjectArgs(object_arg, nullable_object_arg);
    return;
  }
  if (info[2]->IsObject()) {
    optional_object_arg = ScriptValue(info.GetIsolate(), info[2]);
  } else if (info[2]->IsUndefined()) {
    optional_object_arg = ScriptValue(info.GetIsolate(), v8::Undefined(info.GetIsolate()));
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNullableAndOptionalObjectArgs", "TestInterface", "parameter 3 ('optionalObjectArg') is not an object."));
    return;
  }

  impl->voidMethodNullableAndOptionalObjectArgs(object_arg, nullable_object_arg, optional_object_arg);
}

static void VoidMethodUnrestrictedDoubleArgUnrestrictedFloatArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "voidMethodUnrestrictedDoubleArgUnrestrictedFloatArg");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  double unrestricted_double_arg;
  float unrestricted_float_arg;
  unrestricted_double_arg = NativeValueTraits<IDLUnrestrictedDouble>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  unrestricted_float_arg = NativeValueTraits<IDLUnrestrictedFloat>::NativeValue(info.GetIsolate(), info[1], exception_state);
  if (exception_state.HadException())
    return;

  impl->voidMethodUnrestrictedDoubleArgUnrestrictedFloatArg(unrestricted_double_arg, unrestricted_float_arg);
}

static void VoidMethodTestEnumArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "voidMethodTestEnumArg");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> test_enum_arg;
  test_enum_arg = info[0];
  if (!test_enum_arg.Prepare())
    return;
  const char* const kValidTestEnumArgValues[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(test_enum_arg, kValidTestEnumArgValues, base::size(kValidTestEnumArgValues), "TestEnum", exception_state)) {
    return;
  }

  impl->voidMethodTestEnumArg(test_enum_arg);
}

static void VoidOptionalDictArgWithEmptyDefaultMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "voidOptionalDictArgWithEmptyDefault");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestDictionary* test_dict;
  if (!info[0]->IsNullOrUndefined() && !info[0]->IsObject()) {
    exception_state.ThrowTypeError("parameter 1 ('testDict') is not an object.");
    return;
  }
  test_dict = NativeValueTraits<TestDictionary>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  impl->voidOptionalDictArgWithEmptyDefault(test_dict);
}

static void VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->voidMethod();
}

static void VoidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->voidMethod();
}

static void AlwaysExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->alwaysExposedMethod();
}

static void WorkerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->workerExposedMethod();
}

static void WindowExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->windowExposedMethod();
}

static void OriginTrialWindowExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->originTrialWindowExposedMethod();
}

static void AlwaysExposedStaticMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation::alwaysExposedStaticMethod();
}

static void WorkerExposedStaticMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation::workerExposedStaticMethod();
}

static void WindowExposedStaticMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation::windowExposedStaticMethod();
}

static void StaticReturnDOMWrapperMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, TestInterfaceImplementation::staticReturnDOMWrapperMethod(), info.GetIsolate()->GetCurrentContext()->Global());
}

static void MethodWithExposedAndRuntimeEnabledFlagMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->methodWithExposedAndRuntimeEnabledFlag();
}

static void OverloadMethodWithExposedAndRuntimeEnabledFlag1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "overloadMethodWithExposedAndRuntimeEnabledFlag");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  int32_t long_arg;
  long_arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  impl->overloadMethodWithExposedAndRuntimeEnabledFlag(long_arg);
}

static void OverloadMethodWithExposedAndRuntimeEnabledFlag2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  V8StringResource<> string;
  string = info[0];
  if (!string.Prepare())
    return;

  impl->overloadMethodWithExposedAndRuntimeEnabledFlag(string);
}

static void OverloadMethodWithExposedAndRuntimeEnabledFlag3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  DOMWindow* window;
  window = ToDOMWindow(info.GetIsolate(), info[0]);
  if (!window) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadMethodWithExposedAndRuntimeEnabledFlag", "TestInterface", ExceptionMessages::ArgumentNotOfType(0, "Window")));
    return;
  }

  impl->overloadMethodWithExposedAndRuntimeEnabledFlag(window);
}

static void OverloadMethodWithExposedAndRuntimeEnabledFlagMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  scheduler::CooperativeSchedulingManager::Instance()->Safepoint();

  bool is_arity_error = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (RuntimeEnabledFeatures::RuntimeFeature2Enabled()) {
        if (V8Window::HasInstance(info[0], info.GetIsolate())) {
          OverloadMethodWithExposedAndRuntimeEnabledFlag3Method(info);
          return;
        }
      }
      if (info[0]->IsNumber()) {
        OverloadMethodWithExposedAndRuntimeEnabledFlag1Method(info);
        return;
      }
      if (RuntimeEnabledFeatures::RuntimeFeatureEnabled()) {
        if (true) {
          OverloadMethodWithExposedAndRuntimeEnabledFlag2Method(info);
          return;
        }
      }
      if (true) {
        OverloadMethodWithExposedAndRuntimeEnabledFlag1Method(info);
        return;
      }
      break;
    default:
      is_arity_error = true;
  }

  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "overloadMethodWithExposedAndRuntimeEnabledFlag");
  if (is_arity_error) {
    if (info.Length() < 1) {
      exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exception_state.ThrowTypeError("No function was found that matched the signature provided.");
}

static void MethodWithExposedHavingRuntimeEnabldFlagMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->methodWithExposedHavingRuntimeEnabldFlag();
}

static void WindowAndServiceWorkerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->windowAndServiceWorkerExposedMethod();
}

static void VoidMethodPartialOverload1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->voidMethodPartialOverload();
}

static void VoidMethodPartialOverload2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "voidMethodPartialOverload");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  double double_arg;
  double_arg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  impl->voidMethodPartialOverload(double_arg);
}

static void StaticVoidMethodPartialOverload1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation::staticVoidMethodPartialOverload();
}

static void PromiseMethodPartialOverload1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "promiseMethodPartialOverload");
  ExceptionToRejectPromiseScope reject_promise_scope(info, exception_state);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestInterface::HasInstance(info.Holder(), info.GetIsolate())) {
    exception_state.ThrowTypeError("Illegal invocation");
    return;
  }
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->promiseMethodPartialOverload().V8Value());
}

static void PromiseMethodPartialOverload2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "promiseMethodPartialOverload");
  ExceptionToRejectPromiseScope reject_promise_scope(info, exception_state);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestInterface::HasInstance(info.Holder(), info.GetIsolate())) {
    exception_state.ThrowTypeError("Illegal invocation");
    return;
  }
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  DOMWindow* window;
  window = ToDOMWindow(info.GetIsolate(), info[0]);
  if (!window) {
    exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(0, "Window"));
    return;
  }

  V8SetReturnValue(info, impl->promiseMethodPartialOverload(window).V8Value());
}

static void StaticPromiseMethodPartialOverload1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, TestInterfaceImplementation::staticPromiseMethodPartialOverload().V8Value());
}

static void OverloadMethodWithUnionTypeWithStringMember1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "overloadMethodWithUnionTypeWithStringMember");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  DoubleOrString union_arg;
  V8DoubleOrString::ToImpl(info.GetIsolate(), info[0], union_arg, UnionTypeConversionMode::kNotNullable, exception_state);
  if (exception_state.HadException())
    return;

  impl->overloadMethodWithUnionTypeWithStringMember(union_arg);
}

static void OverloadMethodWithUnionTypeWithStringMember2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "overloadMethodWithUnionTypeWithStringMember");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  bool bool_arg;
  bool_arg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  impl->overloadMethodWithUnionTypeWithStringMember(bool_arg);
}

static void OverloadMethodWithUnionTypeWithStringMemberMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  scheduler::CooperativeSchedulingManager::Instance()->Safepoint();

  bool is_arity_error = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (info[0]->IsBoolean()) {
        OverloadMethodWithUnionTypeWithStringMember2Method(info);
        return;
      }
      if (true) {
        OverloadMethodWithUnionTypeWithStringMember1Method(info);
        return;
      }
      if (true) {
        OverloadMethodWithUnionTypeWithStringMember2Method(info);
        return;
      }
      break;
    default:
      is_arity_error = true;
  }

  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "overloadMethodWithUnionTypeWithStringMember");
  if (is_arity_error) {
    if (info.Length() < 1) {
      exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exception_state.ThrowTypeError("No function was found that matched the signature provided.");
}

static void SideEffectFreeMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->sideEffectFreeMethod();
}

static void SecureContextMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextMethod();
}

static void SecureContextRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextRuntimeEnabledMethod();
}

static void SecureContextnessRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextnessRuntimeEnabledMethod();
}

static void SecureContextWindowExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextWindowExposedMethod();
}

static void SecureContextWorkerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextWorkerExposedMethod();
}

static void SecureContextWindowExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextWindowExposedRuntimeEnabledMethod();
}

static void SecureContextWorkerExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextWorkerExposedRuntimeEnabledMethod();
}

static void MethodWithNullableSequencesMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "methodWithNullableSequences");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 4)) {
    exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(4, info.Length()));
    return;
  }

  Vector<base::Optional<double>> numbers;
  Vector<String> strings;
  HeapVector<Member<Element>> elements;
  HeapVector<DoubleOrString> unions;
  numbers = NativeValueTraits<IDLSequence<IDLNullable<IDLDouble>>>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  strings = NativeValueTraits<IDLSequence<IDLStringOrNull>>::NativeValue(info.GetIsolate(), info[1], exception_state);
  if (exception_state.HadException())
    return;

  elements = NativeValueTraits<IDLSequence<IDLNullable<Element>>>::NativeValue(info.GetIsolate(), info[2], exception_state);
  if (exception_state.HadException())
    return;

  unions = NativeValueTraits<IDLSequence<IDLNullable<DoubleOrString>>>::NativeValue(info.GetIsolate(), info[3], exception_state);
  if (exception_state.HadException())
    return;

  impl->methodWithNullableSequences(numbers, strings, elements, unions);
}

static void MethodWithNullableRecordsMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "methodWithNullableRecords");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 4)) {
    exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(4, info.Length()));
    return;
  }

  Vector<std::pair<String, base::Optional<double>>> numbers;
  Vector<std::pair<String, String>> strings;
  HeapVector<std::pair<String, Member<Element>>> elements;
  HeapVector<std::pair<String, DoubleOrString>> unions;
  numbers = NativeValueTraits<IDLRecord<IDLString, IDLNullable<IDLDouble>>>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  strings = NativeValueTraits<IDLRecord<IDLString, IDLStringOrNull>>::NativeValue(info.GetIsolate(), info[1], exception_state);
  if (exception_state.HadException())
    return;

  elements = NativeValueTraits<IDLRecord<IDLString, IDLNullable<Element>>>::NativeValue(info.GetIsolate(), info[2], exception_state);
  if (exception_state.HadException())
    return;

  unions = NativeValueTraits<IDLRecord<IDLString, IDLNullable<DoubleOrString>>>::NativeValue(info.GetIsolate(), info[3], exception_state);
  if (exception_state.HadException())
    return;

  impl->methodWithNullableRecords(numbers, strings, elements, unions);
}

static void MixinVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->mixinVoidMethod();
}

static void MixinComplexMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "mixinComplexMethod");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  V8StringResource<> str_arg;
  TestInterfaceEmpty* test_interface_empty_arg;
  str_arg = info[0];
  if (!str_arg.Prepare())
    return;

  test_interface_empty_arg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!test_interface_empty_arg) {
    exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(1, "TestInterfaceEmpty"));
    return;
  }

  ExecutionContext* execution_context = ExecutionContext::ForRelevantRealm(info);
  TestInterfaceEmpty* result = impl->mixinComplexMethod(execution_context, str_arg, test_interface_empty_arg, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void Mixin2VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfaceMixin2::mixin2VoidMethod(*impl);
}

static void Mixin3VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestMixin3Implementation::mixin3VoidMethod(*impl);
}

static void PartialVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartial::partialVoidMethod(*impl);
}

static void PartialStaticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfacePartial::partialStaticVoidMethod();
}

static void PartialVoidMethodLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "partialVoidMethodLongArg");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t long_arg;
  long_arg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exception_state);
  if (exception_state.HadException())
    return;

  TestInterfacePartial::partialVoidMethodLongArg(*impl, long_arg);
}

static void PartialCallWithExecutionContextRaisesExceptionVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "partialCallWithExecutionContextRaisesExceptionVoidMethod");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  ExecutionContext* execution_context = ExecutionContext::ForRelevantRealm(info);
  TestInterfacePartial::partialCallWithExecutionContextRaisesExceptionVoidMethod(execution_context, *impl, exception_state);
  if (exception_state.HadException()) {
    return;
  }
}

static void PartialVoidMethodPartialCallbackTypeArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("partialVoidMethodPartialCallbackTypeArg", "TestInterface", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  ScriptValue partial_callback_type_arg;
  if (info[0]->IsFunction()) {
    partial_callback_type_arg = ScriptValue(info.GetIsolate(), info[0]);
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("partialVoidMethodPartialCallbackTypeArg", "TestInterface", "The callback provided as parameter 1 is not a function."));
    return;
  }

  TestInterfacePartial::partialVoidMethodPartialCallbackTypeArg(*impl, partial_callback_type_arg);
}

static void Partial2VoidMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartial2Implementation::partial2VoidMethod(*impl);
}

static void Partial2StaticVoidMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfacePartial2Implementation::partial2StaticVoidMethod();
}

static void Partial2SecureContextMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartial2Implementation::partial2SecureContextMethod(*impl);
}

static void PartialSecureContextMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartialSecureContext::partialSecureContextMethod(*impl);
}

static void PartialSecureContextRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartialSecureContext::partialSecureContextRuntimeEnabledMethod(*impl);
}

static void PartialSecureContextWindowExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartialSecureContext::partialSecureContextWindowExposedMethod(*impl);
}

static void PartialSecureContextWorkerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartialSecureContext::partialSecureContextWorkerExposedMethod(*impl);
}

static void PartialSecureContextWindowExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartialSecureContext::partialSecureContextWindowExposedRuntimeEnabledMethod(*impl);
}

static void PartialSecureContextWorkerExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartialSecureContext::partialSecureContextWorkerExposedRuntimeEnabledMethod(*impl);
}

static void VoidMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  scheduler::CooperativeSchedulingManager::Instance()->Safepoint();

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        VoidMethodPartialOverload1Method(info);
        return;
      }
      break;
    case 1:
      if (info[0]->IsNumber()) {
        VoidMethodPartialOverload2Method(info);
        return;
      }
      if (true) {
        VoidMethodPartialOverload2Method(info);
        return;
      }
      break;
  }

  DCHECK(voidMethodPartialOverloadMethodForPartialInterface);
  (voidMethodPartialOverloadMethodForPartialInterface)(info);
}

static void StaticVoidMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  scheduler::CooperativeSchedulingManager::Instance()->Safepoint();

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        StaticVoidMethodPartialOverload1Method(info);
        return;
      }
      break;
    case 1:
      break;
  }

  DCHECK(staticVoidMethodPartialOverloadMethodForPartialInterface);
  (staticVoidMethodPartialOverloadMethodForPartialInterface)(info);
}

static void PromiseMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  scheduler::CooperativeSchedulingManager::Instance()->Safepoint();

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        PromiseMethodPartialOverload1Method(info);
        return;
      }
      break;
    case 1:
      if (V8Window::HasInstance(info[0], info.GetIsolate())) {
        PromiseMethodPartialOverload2Method(info);
        return;
      }
      break;
  }

  DCHECK(promiseMethodPartialOverloadMethodForPartialInterface);
  (promiseMethodPartialOverloadMethodForPartialInterface)(info);
}

static void StaticPromiseMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  scheduler::CooperativeSchedulingManager::Instance()->Safepoint();

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        StaticPromiseMethodPartialOverload1Method(info);
        return;
      }
      break;
    case 1:
      break;
  }

  DCHECK(staticPromiseMethodPartialOverloadMethodForPartialInterface);
  (staticPromiseMethodPartialOverloadMethodForPartialInterface)(info);
}

static void Partial2VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  scheduler::CooperativeSchedulingManager::Instance()->Safepoint();

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        Partial2VoidMethod1Method(info);
        return;
      }
      break;
    case 1:
      break;
  }

  DCHECK(partial2VoidMethodMethodForPartialInterface);
  (partial2VoidMethodMethodForPartialInterface)(info);
}

static void Partial2StaticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  scheduler::CooperativeSchedulingManager::Instance()->Safepoint();

  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        Partial2StaticVoidMethod1Method(info);
        return;
      }
      break;
    case 1:
      break;
  }

  DCHECK(partial2StaticVoidMethodMethodForPartialInterface);
  (partial2StaticVoidMethodMethodForPartialInterface)(info);
}

static void KeysMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "keys");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  ScriptState* script_state = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->keysForBinding(script_state, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void ValuesMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "values");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  ScriptState* script_state = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->valuesForBinding(script_state, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void ForEachMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "forEach");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  ScriptState* script_state = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 1)) {
    exception_state.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8ForEachIteratorCallback* callback;
  ScriptValue this_arg;
  if (info[0]->IsFunction()) {
    callback = V8ForEachIteratorCallback::Create(info[0].As<v8::Function>());
  } else {
    exception_state.ThrowTypeError("The callback provided as parameter 1 is not a function.");
    return;
  }

  this_arg = ScriptValue(info.GetIsolate(), info[1]);

  impl->forEachForBinding(script_state, ScriptValue(info.GetIsolate(), info.Holder()), callback, this_arg, exception_state);
  if (exception_state.HadException()) {
    return;
  }
}

static void ToStringMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->toString(), info.GetIsolate());
}

static void IteratorMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exception_state(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "iterator");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  ScriptState* script_state = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->GetIterator(script_state, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void NamedPropertyGetter(const AtomicString& name,
                                const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());
  String result = impl->AnonymousNamedGetter(name);
  if (result.IsNull())
    return;
  V8SetReturnValueString(info, result, info.GetIsolate());
}

static void NamedPropertySetter(
    const AtomicString& name,
    v8::Local<v8::Value> v8_value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());
  V8StringResource<> property_value = v8_value;
  if (!property_value.Prepare())
    return;

  bool result = impl->AnonymousNamedSetter(name, property_value);
  if (!result)
    return;
  V8SetReturnValue(info, v8_value);
}

static void NamedPropertyDeleter(
    const AtomicString& name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  DeleteResult result = impl->AnonymousNamedDeleter(name);
  if (result == kDeleteUnknownProperty)
    return;
  V8SetReturnValue(info, result == kDeleteSuccess);
}

static void NamedPropertyQuery(
    const AtomicString& name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  const std::string& name_in_utf8 = name.Utf8();
  ExceptionState exception_state(
      info.GetIsolate(),
      ExceptionState::kGetterContext,
      "TestInterface",
      name_in_utf8.c_str());

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  bool result = impl->NamedPropertyQuery(name, exception_state);
  if (!result)
    return;
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // 2.7. If |O| implements an interface with a named property setter, then set
  //      desc.[[Writable]] to true, otherwise set it to false.
  // 2.8. If |O| implements an interface with the
  //      [LegacyUnenumerableNamedProperties] extended attribute, then set
  //      desc.[[Enumerable]] to false, otherwise set it to true.
  V8SetReturnValueInt(info, v8::None);
}

static void NamedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  ExceptionState exception_state(
      info.GetIsolate(),
      ExceptionState::kEnumerationContext,
      "TestInterface");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  Vector<String> names;
  impl->NamedPropertyEnumerator(names, exception_state);
  if (exception_state.HadException())
    return;
  V8SetReturnValue(info, ToV8(names, info.Holder(), info.GetIsolate()).As<v8::Array>());
}

static void IndexedPropertyGetter(
    uint32_t index,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  // We assume that all the implementations support length() method, although
  // the spec doesn't require that length() must exist.  It's okay that
  // the interface does not have length attribute as long as the
  // implementation supports length() member function.
  if (index >= impl->length())
    return;  // Returns undefined due to out-of-range.

  String result = impl->AnonymousIndexedGetter(index);
  V8SetReturnValueString(info, result, info.GetIsolate());
}

static void IndexedPropertyDescriptor(
    uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // Steps 1.1 to 1.2.4 are covered here: we rely on indexedPropertyGetter() to
  // call the getter function and check that |index| is a valid property index,
  // in which case it will have set info.GetReturnValue() to something other
  // than undefined.
  V8TestInterface::IndexedPropertyGetterCallback(index, info);
  v8::Local<v8::Value> getter_value = info.GetReturnValue().Get();
  if (!getter_value->IsUndefined()) {
    // 1.2.5. Let |desc| be a newly created Property Descriptor with no fields.
    // 1.2.6. Set desc.[[Value]] to the result of converting value to an
    //        ECMAScript value.
    // 1.2.7. If O implements an interface with an indexed property setter,
    //        then set desc.[[Writable]] to true, otherwise set it to false.
    v8::PropertyDescriptor desc(getter_value, true);
    // 1.2.8. Set desc.[[Enumerable]] and desc.[[Configurable]] to true.
    desc.set_enumerable(true);
    desc.set_configurable(true);
    // 1.2.9. Return |desc|.
    V8SetReturnValue(info, desc);
  }
}

static void IndexedPropertySetter(
    uint32_t index,
    v8::Local<v8::Value> v8_value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());
  V8StringResource<> property_value = v8_value;
  if (!property_value.Prepare())
    return;

  bool result = impl->AnonymousIndexedSetter(index, property_value);
  if (!result)
    return;
  V8SetReturnValue(info, v8_value);
}

static void IndexedPropertyDeleter(
    uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  DeleteResult result = impl->AnonymousIndexedDeleter(index);
  if (result == kDeleteUnknownProperty)
    return;
  V8SetReturnValue(info, result == kDeleteSuccess);
}

}  // namespace test_interface_implementation_v8_internal

void V8TestInterface::TestInterfaceAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testInterfaceAttribute_Getter");

  ExecutionContext* execution_context_for_measurement = CurrentExecutionContext(info.GetIsolate());
  UseCounter::Count(execution_context_for_measurement, WebFeature::kV8TestInterface_TestInterfaceAttribute_AttributeGetter);

  test_interface_implementation_v8_internal::TestInterfaceAttributeAttributeGetter(info);
}

void V8TestInterface::TestInterfaceAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testInterfaceAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kV8TestInterface_TestInterfaceAttribute_AttributeSetter);

  test_interface_implementation_v8_internal::TestInterfaceAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::TestInterfaceConstructorAttributeConstructorGetterCallback(
    v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testInterfaceConstructorAttribute_ConstructorGetterCallback");

  V8ConstructorAttributeGetter(property, info, V8TestInterface::GetWrapperTypeInfo());
}

void V8TestInterface::TestInterfaceConstructorGetterCallback(
    v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_TestInterface_ConstructorGetterCallback");

  V8ConstructorAttributeGetter(property, info, V8TestInterface::GetWrapperTypeInfo());
}

void V8TestInterface::TestInterface2ConstructorGetterCallback(
    v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_TestInterface2_ConstructorGetterCallback");

  V8ConstructorAttributeGetter(property, info, V8TestInterface2::GetWrapperTypeInfo());
}

void V8TestInterface::DoubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_doubleAttribute_Getter");

  test_interface_implementation_v8_internal::DoubleAttributeAttributeGetter(info);
}

void V8TestInterface::DoubleAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_doubleAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::DoubleAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::FloatAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_floatAttribute_Getter");

  test_interface_implementation_v8_internal::FloatAttributeAttributeGetter(info);
}

void V8TestInterface::FloatAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_floatAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::FloatAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::UnrestrictedDoubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_unrestrictedDoubleAttribute_Getter");

  test_interface_implementation_v8_internal::UnrestrictedDoubleAttributeAttributeGetter(info);
}

void V8TestInterface::UnrestrictedDoubleAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_unrestrictedDoubleAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::UnrestrictedDoubleAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::UnrestrictedFloatAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_unrestrictedFloatAttribute_Getter");

  test_interface_implementation_v8_internal::UnrestrictedFloatAttributeAttributeGetter(info);
}

void V8TestInterface::UnrestrictedFloatAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_unrestrictedFloatAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::UnrestrictedFloatAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::TestEnumAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testEnumAttribute_Getter");

  test_interface_implementation_v8_internal::TestEnumAttributeAttributeGetter(info);
}

void V8TestInterface::TestEnumAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testEnumAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::TestEnumAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::TestEnumOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testEnumOrNullAttribute_Getter");

  test_interface_implementation_v8_internal::TestEnumOrNullAttributeAttributeGetter(info);
}

void V8TestInterface::TestEnumOrNullAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testEnumOrNullAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::TestEnumOrNullAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::StringOrDoubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_stringOrDoubleAttribute_Getter");

  test_interface_implementation_v8_internal::StringOrDoubleAttributeAttributeGetter(info);
}

void V8TestInterface::StringOrDoubleAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_stringOrDoubleAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::StringOrDoubleAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::WithExtendedAttributeStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_withExtendedAttributeStringAttribute_Getter");

  test_interface_implementation_v8_internal::WithExtendedAttributeStringAttributeAttributeGetter(info);
}

void V8TestInterface::WithExtendedAttributeStringAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_withExtendedAttributeStringAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::WithExtendedAttributeStringAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::UncapitalAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_uncapitalAttribute_Getter");

  test_interface_implementation_v8_internal::UncapitalAttributeAttributeGetter(info);
}

void V8TestInterface::UncapitalAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_uncapitalAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::UncapitalAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::ConditionalLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_conditionalLongAttribute_Getter");

  test_interface_implementation_v8_internal::ConditionalLongAttributeAttributeGetter(info);
}

void V8TestInterface::ConditionalLongAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_conditionalLongAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::ConditionalLongAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::ConditionalReadOnlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_conditionalReadOnlyLongAttribute_Getter");

  test_interface_implementation_v8_internal::ConditionalReadOnlyLongAttributeAttributeGetter(info);
}

void V8TestInterface::StaticStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticStringAttribute_Getter");

  test_interface_implementation_v8_internal::StaticStringAttributeAttributeGetter(info);
}

void V8TestInterface::StaticStringAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticStringAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::StaticStringAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::StaticReturnDOMWrapperAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticReturnDOMWrapperAttribute_Getter");

  test_interface_implementation_v8_internal::StaticReturnDOMWrapperAttributeAttributeGetter(info);
}

void V8TestInterface::StaticReturnDOMWrapperAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticReturnDOMWrapperAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::StaticReturnDOMWrapperAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::StaticReadOnlyStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticReadOnlyStringAttribute_Getter");

  test_interface_implementation_v8_internal::StaticReadOnlyStringAttributeAttributeGetter(info);
}

void V8TestInterface::StaticReadOnlyReturnDOMWrapperAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticReadOnlyReturnDOMWrapperAttribute_Getter");

  test_interface_implementation_v8_internal::StaticReadOnlyReturnDOMWrapperAttributeAttributeGetter(info);
}

void V8TestInterface::StaticConditionalReadOnlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticConditionalReadOnlyLongAttribute_Getter");

  test_interface_implementation_v8_internal::StaticConditionalReadOnlyLongAttributeAttributeGetter(info);
}

void V8TestInterface::StringNullAsEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_stringNullAsEmptyAttribute_Getter");

  test_interface_implementation_v8_internal::StringNullAsEmptyAttributeAttributeGetter(info);
}

void V8TestInterface::StringNullAsEmptyAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_stringNullAsEmptyAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::StringNullAsEmptyAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::UsvStringOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_usvStringOrNullAttribute_Getter");

  test_interface_implementation_v8_internal::UsvStringOrNullAttributeAttributeGetter(info);
}

void V8TestInterface::UsvStringOrNullAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_usvStringOrNullAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::UsvStringOrNullAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::AlwaysExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_alwaysExposedAttribute_Getter");

  test_interface_implementation_v8_internal::AlwaysExposedAttributeAttributeGetter(info);
}

void V8TestInterface::AlwaysExposedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_alwaysExposedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::AlwaysExposedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::WorkerExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_workerExposedAttribute_Getter");

  test_interface_implementation_v8_internal::WorkerExposedAttributeAttributeGetter(info);
}

void V8TestInterface::WorkerExposedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_workerExposedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::WorkerExposedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::WindowExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_windowExposedAttribute_Getter");

  test_interface_implementation_v8_internal::WindowExposedAttributeAttributeGetter(info);
}

void V8TestInterface::WindowExposedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_windowExposedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::WindowExposedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::LenientThisAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_lenientThisAttribute_Getter");

  test_interface_implementation_v8_internal::LenientThisAttributeAttributeGetter(info);
}

void V8TestInterface::LenientThisAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_lenientThisAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::LenientThisAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::AttributeWithSideEffectFreeGetterAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_attributeWithSideEffectFreeGetter_Getter");

  test_interface_implementation_v8_internal::AttributeWithSideEffectFreeGetterAttributeGetter(info);
}

void V8TestInterface::AttributeWithSideEffectFreeGetterAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_attributeWithSideEffectFreeGetter_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::AttributeWithSideEffectFreeGetterAttributeSetter(v8_value, info);
}

void V8TestInterface::SecureContextAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextAttribute_Getter");

  test_interface_implementation_v8_internal::SecureContextAttributeAttributeGetter(info);
}

void V8TestInterface::SecureContextAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::SecureContextAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::SecureContextRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::SecureContextRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::SecureContextRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::SecureContextRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::SecureContextnessRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextnessRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::SecureContextnessRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::SecureContextnessRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextnessRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::SecureContextnessRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::SecureContextWindowExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWindowExposedAttribute_Getter");

  test_interface_implementation_v8_internal::SecureContextWindowExposedAttributeAttributeGetter(info);
}

void V8TestInterface::SecureContextWindowExposedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWindowExposedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::SecureContextWindowExposedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::SecureContextWorkerExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWorkerExposedAttribute_Getter");

  test_interface_implementation_v8_internal::SecureContextWorkerExposedAttributeAttributeGetter(info);
}

void V8TestInterface::SecureContextWorkerExposedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWorkerExposedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::SecureContextWorkerExposedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::SecureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWindowExposedRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::SecureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::SecureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWindowExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::SecureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWorkerExposedRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWorkerExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::MixinReadonlyStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixinReadonlyStringAttribute_Getter");

  test_interface_implementation_v8_internal::MixinReadonlyStringAttributeAttributeGetter(info);
}

void V8TestInterface::MixinStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixinStringAttribute_Getter");

  test_interface_implementation_v8_internal::MixinStringAttributeAttributeGetter(info);
}

void V8TestInterface::MixinStringAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixinStringAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::MixinStringAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::MixinNodeAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixinNodeAttribute_Getter");

  test_interface_implementation_v8_internal::MixinNodeAttributeAttributeGetter(info);
}

void V8TestInterface::MixinNodeAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixinNodeAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::MixinNodeAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::MixinEventHandlerAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixinEventHandlerAttribute_Getter");

  test_interface_implementation_v8_internal::MixinEventHandlerAttributeAttributeGetter(info);
}

void V8TestInterface::MixinEventHandlerAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixinEventHandlerAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::MixinEventHandlerAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::MixinRuntimeEnabledNodeAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixinRuntimeEnabledNodeAttribute_Getter");

  test_interface_implementation_v8_internal::MixinRuntimeEnabledNodeAttributeAttributeGetter(info);
}

void V8TestInterface::MixinRuntimeEnabledNodeAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixinRuntimeEnabledNodeAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::MixinRuntimeEnabledNodeAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::Mixin2StringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixin2StringAttribute_Getter");

  test_interface_implementation_v8_internal::Mixin2StringAttributeAttributeGetter(info);
}

void V8TestInterface::Mixin2StringAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixin2StringAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::Mixin2StringAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::Mixin3StringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixin3StringAttribute_Getter");

  test_interface_implementation_v8_internal::Mixin3StringAttributeAttributeGetter(info);
}

void V8TestInterface::Mixin3StringAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixin3StringAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::Mixin3StringAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::PartialLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialLongAttribute_Getter");

  test_interface_implementation_v8_internal::PartialLongAttributeAttributeGetter(info);
}

void V8TestInterface::PartialLongAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialLongAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::PartialLongAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::PartialStaticLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialStaticLongAttribute_Getter");

  test_interface_implementation_v8_internal::PartialStaticLongAttributeAttributeGetter(info);
}

void V8TestInterface::PartialStaticLongAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialStaticLongAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::PartialStaticLongAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::PartialCallWithExecutionContextLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialCallWithExecutionContextLongAttribute_Getter");

  test_interface_implementation_v8_internal::PartialCallWithExecutionContextLongAttributeAttributeGetter(info);
}

void V8TestInterface::PartialCallWithExecutionContextLongAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialCallWithExecutionContextLongAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::PartialCallWithExecutionContextLongAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::PartialPartialEnumTypeAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialPartialEnumTypeAttribute_Getter");

  test_interface_implementation_v8_internal::PartialPartialEnumTypeAttributeAttributeGetter(info);
}

void V8TestInterface::PartialPartialEnumTypeAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialPartialEnumTypeAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::PartialPartialEnumTypeAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::PartialSecureContextLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextLongAttribute_Getter");

  test_interface_implementation_v8_internal::PartialSecureContextLongAttributeAttributeGetter(info);
}

void V8TestInterface::PartialSecureContextLongAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextLongAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::PartialSecureContextLongAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::Partial2LongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2LongAttribute_Getter");

  test_interface_implementation_v8_internal::Partial2LongAttributeAttributeGetter(info);
}

void V8TestInterface::Partial2LongAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2LongAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::Partial2LongAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::Partial2StaticLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2StaticLongAttribute_Getter");

  test_interface_implementation_v8_internal::Partial2StaticLongAttributeAttributeGetter(info);
}

void V8TestInterface::Partial2StaticLongAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2StaticLongAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::Partial2StaticLongAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::Partial2SecureContextAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2SecureContextAttribute_Getter");

  test_interface_implementation_v8_internal::Partial2SecureContextAttributeAttributeGetter(info);
}

void V8TestInterface::Partial2SecureContextAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2SecureContextAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::Partial2SecureContextAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::PartialSecureContextAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextAttribute_Getter");

  test_interface_implementation_v8_internal::PartialSecureContextAttributeAttributeGetter(info);
}

void V8TestInterface::PartialSecureContextAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::PartialSecureContextAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::PartialSecureContextRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::PartialSecureContextRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::PartialSecureContextRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::PartialSecureContextRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::PartialSecureContextWindowExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWindowExposedAttribute_Getter");

  test_interface_implementation_v8_internal::PartialSecureContextWindowExposedAttributeAttributeGetter(info);
}

void V8TestInterface::PartialSecureContextWindowExposedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWindowExposedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::PartialSecureContextWindowExposedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::PartialSecureContextWorkerExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWorkerExposedAttribute_Getter");

  test_interface_implementation_v8_internal::PartialSecureContextWorkerExposedAttributeAttributeGetter(info);
}

void V8TestInterface::PartialSecureContextWorkerExposedAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWorkerExposedAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::PartialSecureContextWorkerExposedAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::PartialSecureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWindowExposedRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::PartialSecureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::PartialSecureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWindowExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::PartialSecureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::PartialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWorkerExposedRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::PartialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::PartialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWorkerExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8_value = info[0];

  test_interface_implementation_v8_internal::PartialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(v8_value, info);
}

void V8TestInterface::VoidMethodTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethodTestInterfaceEmptyArg");

  test_interface_implementation_v8_internal::VoidMethodTestInterfaceEmptyArgMethod(info);
}

void V8TestInterface::VoidMethodDoubleArgFloatArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethodDoubleArgFloatArg");

  test_interface_implementation_v8_internal::VoidMethodDoubleArgFloatArgMethod(info);
}

void V8TestInterface::VoidMethodNullableAndOptionalObjectArgsMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethodNullableAndOptionalObjectArgs");

  test_interface_implementation_v8_internal::VoidMethodNullableAndOptionalObjectArgsMethod(info);
}

void V8TestInterface::VoidMethodUnrestrictedDoubleArgUnrestrictedFloatArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethodUnrestrictedDoubleArgUnrestrictedFloatArg");

  test_interface_implementation_v8_internal::VoidMethodUnrestrictedDoubleArgUnrestrictedFloatArgMethod(info);
}

void V8TestInterface::VoidMethodTestEnumArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethodTestEnumArg");

  test_interface_implementation_v8_internal::VoidMethodTestEnumArgMethod(info);
}

void V8TestInterface::VoidOptionalDictArgWithEmptyDefaultMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidOptionalDictArgWithEmptyDefault");

  test_interface_implementation_v8_internal::VoidOptionalDictArgWithEmptyDefaultMethod(info);
}

void V8TestInterface::VoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethod");

  test_interface_implementation_v8_internal::VoidMethodMethod(info);
}

void V8TestInterface::VoidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethod");

  test_interface_implementation_v8_internal::VoidMethodMethodForMainWorld(info);
}

void V8TestInterface::AlwaysExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_alwaysExposedMethod");

  test_interface_implementation_v8_internal::AlwaysExposedMethodMethod(info);
}

void V8TestInterface::WorkerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_workerExposedMethod");

  test_interface_implementation_v8_internal::WorkerExposedMethodMethod(info);
}

void V8TestInterface::WindowExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_windowExposedMethod");

  test_interface_implementation_v8_internal::WindowExposedMethodMethod(info);
}

void V8TestInterface::OriginTrialWindowExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_originTrialWindowExposedMethod");

  test_interface_implementation_v8_internal::OriginTrialWindowExposedMethodMethod(info);
}

void V8TestInterface::AlwaysExposedStaticMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_alwaysExposedStaticMethod");

  test_interface_implementation_v8_internal::AlwaysExposedStaticMethodMethod(info);
}

void V8TestInterface::WorkerExposedStaticMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_workerExposedStaticMethod");

  test_interface_implementation_v8_internal::WorkerExposedStaticMethodMethod(info);
}

void V8TestInterface::WindowExposedStaticMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_windowExposedStaticMethod");

  test_interface_implementation_v8_internal::WindowExposedStaticMethodMethod(info);
}

void V8TestInterface::StaticReturnDOMWrapperMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticReturnDOMWrapperMethod");

  test_interface_implementation_v8_internal::StaticReturnDOMWrapperMethodMethod(info);
}

void V8TestInterface::MethodWithExposedAndRuntimeEnabledFlagMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_methodWithExposedAndRuntimeEnabledFlag");

  test_interface_implementation_v8_internal::MethodWithExposedAndRuntimeEnabledFlagMethod(info);
}

void V8TestInterface::OverloadMethodWithExposedAndRuntimeEnabledFlagMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_overloadMethodWithExposedAndRuntimeEnabledFlag");

  test_interface_implementation_v8_internal::OverloadMethodWithExposedAndRuntimeEnabledFlagMethod(info);
}

void V8TestInterface::MethodWithExposedHavingRuntimeEnabldFlagMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_methodWithExposedHavingRuntimeEnabldFlag");

  test_interface_implementation_v8_internal::MethodWithExposedHavingRuntimeEnabldFlagMethod(info);
}

void V8TestInterface::WindowAndServiceWorkerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_windowAndServiceWorkerExposedMethod");

  test_interface_implementation_v8_internal::WindowAndServiceWorkerExposedMethodMethod(info);
}

void V8TestInterface::OverloadMethodWithUnionTypeWithStringMemberMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_overloadMethodWithUnionTypeWithStringMember");

  test_interface_implementation_v8_internal::OverloadMethodWithUnionTypeWithStringMemberMethod(info);
}

void V8TestInterface::SideEffectFreeMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_sideEffectFreeMethod");

  test_interface_implementation_v8_internal::SideEffectFreeMethodMethod(info);
}

void V8TestInterface::SecureContextMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextMethod");

  test_interface_implementation_v8_internal::SecureContextMethodMethod(info);
}

void V8TestInterface::SecureContextRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::SecureContextRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::SecureContextnessRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextnessRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::SecureContextnessRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::SecureContextWindowExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWindowExposedMethod");

  test_interface_implementation_v8_internal::SecureContextWindowExposedMethodMethod(info);
}

void V8TestInterface::SecureContextWorkerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWorkerExposedMethod");

  test_interface_implementation_v8_internal::SecureContextWorkerExposedMethodMethod(info);
}

void V8TestInterface::SecureContextWindowExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWindowExposedRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::SecureContextWindowExposedRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::SecureContextWorkerExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWorkerExposedRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::SecureContextWorkerExposedRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::MethodWithNullableSequencesMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_methodWithNullableSequences");

  test_interface_implementation_v8_internal::MethodWithNullableSequencesMethod(info);
}

void V8TestInterface::MethodWithNullableRecordsMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_methodWithNullableRecords");

  test_interface_implementation_v8_internal::MethodWithNullableRecordsMethod(info);
}

void V8TestInterface::MixinVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixinVoidMethod");

  test_interface_implementation_v8_internal::MixinVoidMethodMethod(info);
}

void V8TestInterface::MixinComplexMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixinComplexMethod");

  test_interface_implementation_v8_internal::MixinComplexMethodMethod(info);
}

void V8TestInterface::MixinCustomVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixinCustomVoidMethod");

  V8TestInterface::MixinCustomVoidMethodMethodCustom(info);
}

void V8TestInterface::Mixin2VoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixin2VoidMethod");

  test_interface_implementation_v8_internal::Mixin2VoidMethodMethod(info);
}

void V8TestInterface::Mixin3VoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_mixin3VoidMethod");

  test_interface_implementation_v8_internal::Mixin3VoidMethodMethod(info);
}

void V8TestInterface::PartialVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialVoidMethod");

  test_interface_implementation_v8_internal::PartialVoidMethodMethod(info);
}

void V8TestInterface::PartialStaticVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialStaticVoidMethod");

  test_interface_implementation_v8_internal::PartialStaticVoidMethodMethod(info);
}

void V8TestInterface::PartialVoidMethodLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialVoidMethodLongArg");

  test_interface_implementation_v8_internal::PartialVoidMethodLongArgMethod(info);
}

void V8TestInterface::PartialCallWithExecutionContextRaisesExceptionVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialCallWithExecutionContextRaisesExceptionVoidMethod");

  test_interface_implementation_v8_internal::PartialCallWithExecutionContextRaisesExceptionVoidMethodMethod(info);
}

void V8TestInterface::PartialVoidMethodPartialCallbackTypeArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialVoidMethodPartialCallbackTypeArg");

  test_interface_implementation_v8_internal::PartialVoidMethodPartialCallbackTypeArgMethod(info);
}

void V8TestInterface::Partial2SecureContextMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2SecureContextMethod");

  test_interface_implementation_v8_internal::Partial2SecureContextMethodMethod(info);
}

void V8TestInterface::PartialSecureContextMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextMethod");

  test_interface_implementation_v8_internal::PartialSecureContextMethodMethod(info);
}

void V8TestInterface::PartialSecureContextRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::PartialSecureContextRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::PartialSecureContextWindowExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWindowExposedMethod");

  test_interface_implementation_v8_internal::PartialSecureContextWindowExposedMethodMethod(info);
}

void V8TestInterface::PartialSecureContextWorkerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWorkerExposedMethod");

  test_interface_implementation_v8_internal::PartialSecureContextWorkerExposedMethodMethod(info);
}

void V8TestInterface::PartialSecureContextWindowExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWindowExposedRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::PartialSecureContextWindowExposedRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::PartialSecureContextWorkerExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWorkerExposedRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::PartialSecureContextWorkerExposedRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::VoidMethodPartialOverloadMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethodPartialOverload");

  test_interface_implementation_v8_internal::VoidMethodPartialOverloadMethod(info);
}

void V8TestInterface::StaticVoidMethodPartialOverloadMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticVoidMethodPartialOverload");

  test_interface_implementation_v8_internal::StaticVoidMethodPartialOverloadMethod(info);
}

void V8TestInterface::PromiseMethodPartialOverloadMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_promiseMethodPartialOverload");

  test_interface_implementation_v8_internal::PromiseMethodPartialOverloadMethod(info);
}

void V8TestInterface::StaticPromiseMethodPartialOverloadMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticPromiseMethodPartialOverload");

  test_interface_implementation_v8_internal::StaticPromiseMethodPartialOverloadMethod(info);
}

void V8TestInterface::Partial2VoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2VoidMethod");

  test_interface_implementation_v8_internal::Partial2VoidMethodMethod(info);
}

void V8TestInterface::Partial2StaticVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2StaticVoidMethod");

  test_interface_implementation_v8_internal::Partial2StaticVoidMethodMethod(info);
}

void V8TestInterface::KeysMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_keys");

  test_interface_implementation_v8_internal::KeysMethod(info);
}

void V8TestInterface::ValuesMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_values");

  test_interface_implementation_v8_internal::ValuesMethod(info);
}

void V8TestInterface::ForEachMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_forEach");

  test_interface_implementation_v8_internal::ForEachMethod(info);
}

void V8TestInterface::ToStringMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_toString");

  test_interface_implementation_v8_internal::ToStringMethod(info);
}

void V8TestInterface::IteratorMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_iterator");

  test_interface_implementation_v8_internal::IteratorMethod(info);
}

void V8TestInterface::NamedPropertyGetterCallback(
    v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_NamedPropertyGetter");

  if (!name->IsString())
    return;
  const AtomicString& property_name = ToCoreAtomicString(name.As<v8::String>());

  test_interface_implementation_v8_internal::NamedPropertyGetter(property_name, info);
}

void V8TestInterface::NamedPropertySetterCallback(
    v8::Local<v8::Name> name,
    v8::Local<v8::Value> v8_value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_NamedPropertySetter");

  if (!name->IsString())
    return;
  const AtomicString& property_name = ToCoreAtomicString(name.As<v8::String>());

  test_interface_implementation_v8_internal::NamedPropertySetter(property_name, v8_value, info);
}

void V8TestInterface::NamedPropertyDeleterCallback(
    v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  if (!name->IsString())
    return;
  const AtomicString& property_name = ToCoreAtomicString(name.As<v8::String>());

  test_interface_implementation_v8_internal::NamedPropertyDeleter(property_name, info);
}

void V8TestInterface::NamedPropertyQueryCallback(
    v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_NamedPropertyQuery");

  if (!name->IsString())
    return;
  const AtomicString& property_name = ToCoreAtomicString(name.As<v8::String>());

  test_interface_implementation_v8_internal::NamedPropertyQuery(property_name, info);
}

void V8TestInterface::NamedPropertyEnumeratorCallback(
    const v8::PropertyCallbackInfo<v8::Array>& info) {
  test_interface_implementation_v8_internal::NamedPropertyEnumerator(info);
}

void V8TestInterface::IndexedPropertyGetterCallback(
    uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_IndexedPropertyGetter");

  test_interface_implementation_v8_internal::IndexedPropertyGetter(index, info);
}

void V8TestInterface::IndexedPropertyDescriptorCallback(
    uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_interface_implementation_v8_internal::IndexedPropertyDescriptor(index, info);
}

void V8TestInterface::IndexedPropertySetterCallback(
    uint32_t index,
    v8::Local<v8::Value> v8_value,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_interface_implementation_v8_internal::IndexedPropertySetter(index, v8_value, info);
}

void V8TestInterface::IndexedPropertyDeleterCallback(
    uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  test_interface_implementation_v8_internal::IndexedPropertyDeleter(index, info);
}

void V8TestInterface::IndexedPropertyDefinerCallback(
    uint32_t index,
    const v8::PropertyDescriptor& desc,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  // https://heycam.github.io/webidl/#legacy-platform-object-defineownproperty
  // 3.9.3. [[DefineOwnProperty]]
  // step 1.1. If the result of calling IsDataDescriptor(Desc) is false, then
  //   return false.
  if (desc.has_get() || desc.has_set()) {
    V8SetReturnValue(info, v8::Null(info.GetIsolate()));
    if (info.ShouldThrowOnError()) {
      ExceptionState exception_state(info.GetIsolate(),
                                     ExceptionState::kIndexedSetterContext,
                                     "TestInterface");
      exception_state.ThrowTypeError("Accessor properties are not allowed.");
    }
    return;
  }

  // Return nothing and fall back to indexedPropertySetterCallback.
}

static constexpr V8DOMConfiguration::MethodConfiguration kV8TestInterfaceMethods[] = {
    {"voidMethodTestInterfaceEmptyArg", V8TestInterface::VoidMethodTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleArgFloatArg", V8TestInterface::VoidMethodDoubleArgFloatArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodNullableAndOptionalObjectArgs", V8TestInterface::VoidMethodNullableAndOptionalObjectArgsMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUnrestrictedDoubleArgUnrestrictedFloatArg", V8TestInterface::VoidMethodUnrestrictedDoubleArgUnrestrictedFloatArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestEnumArg", V8TestInterface::VoidMethodTestEnumArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidOptionalDictArgWithEmptyDefault", V8TestInterface::VoidOptionalDictArgWithEmptyDefaultMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethod", V8TestInterface::VoidMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"voidMethod", V8TestInterface::VoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"alwaysExposedMethod", V8TestInterface::AlwaysExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"alwaysExposedStaticMethod", V8TestInterface::AlwaysExposedStaticMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"staticReturnDOMWrapperMethod", V8TestInterface::StaticReturnDOMWrapperMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadMethodWithUnionTypeWithStringMember", V8TestInterface::OverloadMethodWithUnionTypeWithStringMemberMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"sideEffectFreeMethod", V8TestInterface::SideEffectFreeMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasNoSideEffect, V8DOMConfiguration::kAllWorlds},
    {"methodWithNullableSequences", V8TestInterface::MethodWithNullableSequencesMethodCallback, 4, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"methodWithNullableRecords", V8TestInterface::MethodWithNullableRecordsMethodCallback, 4, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"mixinVoidMethod", V8TestInterface::MixinVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"mixinComplexMethod", V8TestInterface::MixinComplexMethodMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"mixinCustomVoidMethod", V8TestInterface::MixinCustomVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"mixin3VoidMethod", V8TestInterface::Mixin3VoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodPartialOverload", V8TestInterface::VoidMethodPartialOverloadMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"staticVoidMethodPartialOverload", V8TestInterface::StaticVoidMethodPartialOverloadMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"promiseMethodPartialOverload", V8TestInterface::PromiseMethodPartialOverloadMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"staticPromiseMethodPartialOverload", V8TestInterface::StaticPromiseMethodPartialOverloadMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"partial2VoidMethod", V8TestInterface::Partial2VoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"partial2StaticVoidMethod", V8TestInterface::Partial2StaticVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"keys", V8TestInterface::KeysMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"values", V8TestInterface::ValuesMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"forEach", V8TestInterface::ForEachMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"toString", V8TestInterface::ToStringMethodCallback, 0, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

void V8TestInterface::InstallV8TestInterfaceTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interface_template, V8TestInterface::GetWrapperTypeInfo()->interface_name, V8TestInterfaceEmpty::DomTemplate(isolate, world), V8TestInterface::kInternalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.
  {
    static constexpr V8DOMConfiguration::ConstantConfiguration kConstants[] = {
        {"UNSIGNED_LONG", V8DOMConfiguration::kConstantTypeUnsignedLong, static_cast<int>(0)},
        {"CONST_JAVASCRIPT", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(1)},
        {"MIXIN_CONSTANT_1", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(1)},
        {"MIXIN_CONSTANT_2", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(2)},
        {"PARTIAL2_UNSIGNED_SHORT", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(0)},
    };
    V8DOMConfiguration::InstallConstants(
        isolate, interface_template, prototype_template,
        kConstants, base::size(kConstants));
  }
  static constexpr V8DOMConfiguration::AttributeConfiguration
  kAttributeConfigurations[] = {
      { "testInterfaceConstructorAttribute", V8TestInterface::TestInterfaceConstructorAttributeConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kReplaceWithDataProperty, V8DOMConfiguration::kAllWorlds },
      { "TestInterface", V8TestInterface::TestInterfaceConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kReplaceWithDataProperty, V8DOMConfiguration::kAllWorlds },
      { "TestInterface2", V8TestInterface::TestInterface2ConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kReplaceWithDataProperty, V8DOMConfiguration::kAllWorlds },
  };
  V8DOMConfiguration::InstallAttributes(
      isolate, world, instance_template, prototype_template,
      kAttributeConfigurations, base::size(kAttributeConfigurations));
  static constexpr V8DOMConfiguration::AccessorConfiguration
  kAccessorConfigurations[] = {
      { "testInterfaceAttribute", V8TestInterface::TestInterfaceAttributeAttributeGetterCallback, V8TestInterface::TestInterfaceAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "doubleAttribute", V8TestInterface::DoubleAttributeAttributeGetterCallback, V8TestInterface::DoubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "floatAttribute", V8TestInterface::FloatAttributeAttributeGetterCallback, V8TestInterface::FloatAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "unrestrictedDoubleAttribute", V8TestInterface::UnrestrictedDoubleAttributeAttributeGetterCallback, V8TestInterface::UnrestrictedDoubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "unrestrictedFloatAttribute", V8TestInterface::UnrestrictedFloatAttributeAttributeGetterCallback, V8TestInterface::UnrestrictedFloatAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "testEnumAttribute", V8TestInterface::TestEnumAttributeAttributeGetterCallback, V8TestInterface::TestEnumAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "testEnumOrNullAttribute", V8TestInterface::TestEnumOrNullAttributeAttributeGetterCallback, V8TestInterface::TestEnumOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "stringOrDoubleAttribute", V8TestInterface::StringOrDoubleAttributeAttributeGetterCallback, V8TestInterface::StringOrDoubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "withExtendedAttributeStringAttribute", V8TestInterface::WithExtendedAttributeStringAttributeAttributeGetterCallback, V8TestInterface::WithExtendedAttributeStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "uncapitalAttribute", V8TestInterface::UncapitalAttributeAttributeGetterCallback, V8TestInterface::UncapitalAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "staticStringAttribute", V8TestInterface::StaticStringAttributeAttributeGetterCallback, V8TestInterface::StaticStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "staticReturnDOMWrapperAttribute", V8TestInterface::StaticReturnDOMWrapperAttributeAttributeGetterCallback, V8TestInterface::StaticReturnDOMWrapperAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "staticReadOnlyStringAttribute", V8TestInterface::StaticReadOnlyStringAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "staticReadOnlyReturnDOMWrapperAttribute", V8TestInterface::StaticReadOnlyReturnDOMWrapperAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "stringNullAsEmptyAttribute", V8TestInterface::StringNullAsEmptyAttributeAttributeGetterCallback, V8TestInterface::StringNullAsEmptyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "usvStringOrNullAttribute", V8TestInterface::UsvStringOrNullAttributeAttributeGetterCallback, V8TestInterface::UsvStringOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "alwaysExposedAttribute", V8TestInterface::AlwaysExposedAttributeAttributeGetterCallback, V8TestInterface::AlwaysExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "lenientThisAttribute", V8TestInterface::LenientThisAttributeAttributeGetterCallback, V8TestInterface::LenientThisAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "attributeWithSideEffectFreeGetter", V8TestInterface::AttributeWithSideEffectFreeGetterAttributeGetterCallback, V8TestInterface::AttributeWithSideEffectFreeGetterAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasNoSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "mixinReadonlyStringAttribute", V8TestInterface::MixinReadonlyStringAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "mixinStringAttribute", V8TestInterface::MixinStringAttributeAttributeGetterCallback, V8TestInterface::MixinStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "mixinNodeAttribute", V8TestInterface::MixinNodeAttributeAttributeGetterCallback, V8TestInterface::MixinNodeAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "mixinEventHandlerAttribute", V8TestInterface::MixinEventHandlerAttributeAttributeGetterCallback, V8TestInterface::MixinEventHandlerAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "mixin3StringAttribute", V8TestInterface::Mixin3StringAttributeAttributeGetterCallback, V8TestInterface::Mixin3StringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "partial2LongAttribute", V8TestInterface::Partial2LongAttributeAttributeGetterCallback, V8TestInterface::Partial2LongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      { "partial2StaticLongAttribute", V8TestInterface::Partial2StaticLongAttributeAttributeGetterCallback, V8TestInterface::Partial2StaticLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
  };
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instance_template, prototype_template, interface_template,
      signature, kAccessorConfigurations,
      base::size(kAccessorConfigurations));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instance_template, prototype_template, interface_template,
      signature, kV8TestInterfaceMethods, base::size(kV8TestInterfaceMethods));

  // Indexed properties
  v8::IndexedPropertyHandlerConfiguration indexedPropertyHandlerConfig(
      V8TestInterface::IndexedPropertyGetterCallback,
      V8TestInterface::IndexedPropertySetterCallback,
      V8TestInterface::IndexedPropertyDescriptorCallback,
      V8TestInterface::IndexedPropertyDeleterCallback,
      IndexedPropertyEnumerator<TestInterfaceImplementation>,
      V8TestInterface::IndexedPropertyDefinerCallback,
      v8::Local<v8::Value>(),
      v8::PropertyHandlerFlags::kNone);
  instance_template->SetHandler(indexedPropertyHandlerConfig);
  // Named properties
  v8::NamedPropertyHandlerConfiguration namedPropertyHandlerConfig(V8TestInterface::NamedPropertyGetterCallback, V8TestInterface::NamedPropertySetterCallback, V8TestInterface::NamedPropertyQueryCallback, V8TestInterface::NamedPropertyDeleterCallback, V8TestInterface::NamedPropertyEnumeratorCallback, v8::Local<v8::Value>(), static_cast<v8::PropertyHandlerFlags>(int(v8::PropertyHandlerFlags::kOnlyInterceptStrings) | int(v8::PropertyHandlerFlags::kNonMasking)));
  instance_template->SetHandler(namedPropertyHandlerConfig);

  // Iterator (@@iterator)
  static const V8DOMConfiguration::SymbolKeyedMethodConfiguration
  kSymbolKeyedIteratorConfiguration = {
      v8::Symbol::GetIterator,
      "entries",
      V8TestInterface::IteratorMethodCallback,
      0,
      v8::DontEnum,
      V8DOMConfiguration::kOnPrototype,
      V8DOMConfiguration::kCheckHolder,
      V8DOMConfiguration::kDoNotCheckAccess,
      V8DOMConfiguration::kHasSideEffect
  };
  V8DOMConfiguration::InstallMethod(
      isolate, world, prototype_template, signature,
      kSymbolKeyedIteratorConfiguration);

  instance_template->SetCallAsFunctionHandler(V8TestInterface::LegacyCallCustom);

  // Custom signature
}

void V8TestInterface::InstallRuntimeEnabledFeaturesOnTemplate(
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
  if (RuntimeEnabledFeatures::PartialRuntimeFeatureEnabled()) {
    static constexpr V8DOMConfiguration::ConstantConfiguration kConfigurations[] = {
        {"PARTIAL_UNSIGNED_SHORT", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(0)},
        {"PARTIAL_DOUBLE", V8DOMConfiguration::kConstantTypeDouble, static_cast<double>(3.14)},
    };
    V8DOMConfiguration::InstallConstants(
        isolate, interface_template, prototype_template,
        kConfigurations, base::size(kConfigurations));
  }

  if (RuntimeEnabledFeatures::Mixin2RuntimeFeatureEnabled()) {
    static constexpr V8DOMConfiguration::AccessorConfiguration
    kAccessorConfigurations[] = {
        { "mixin2StringAttribute", V8TestInterface::Mixin2StringAttributeAttributeGetterCallback, V8TestInterface::Mixin2StringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    };
    V8DOMConfiguration::InstallAccessors(
        isolate, world, instance_template, prototype_template, interface_template,
        signature, kAccessorConfigurations,
        base::size(kAccessorConfigurations));
  }
  if (RuntimeEnabledFeatures::MixinRuntimeFeatureEnabled()) {
    static constexpr V8DOMConfiguration::AccessorConfiguration
    kAccessorConfigurations[] = {
        { "mixinRuntimeEnabledNodeAttribute", V8TestInterface::MixinRuntimeEnabledNodeAttributeAttributeGetterCallback, V8TestInterface::MixinRuntimeEnabledNodeAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    };
    V8DOMConfiguration::InstallAccessors(
        isolate, world, instance_template, prototype_template, interface_template,
        signature, kAccessorConfigurations,
        base::size(kAccessorConfigurations));
  }
  if (RuntimeEnabledFeatures::PartialRuntimeFeatureEnabled()) {
    static constexpr V8DOMConfiguration::AccessorConfiguration
    kAccessorConfigurations[] = {
        { "partialCallWithExecutionContextLongAttribute", V8TestInterface::PartialCallWithExecutionContextLongAttributeAttributeGetterCallback, V8TestInterface::PartialCallWithExecutionContextLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "partialLongAttribute", V8TestInterface::PartialLongAttributeAttributeGetterCallback, V8TestInterface::PartialLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "partialPartialEnumTypeAttribute", V8TestInterface::PartialPartialEnumTypeAttributeAttributeGetterCallback, V8TestInterface::PartialPartialEnumTypeAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "partialStaticLongAttribute", V8TestInterface::PartialStaticLongAttributeAttributeGetterCallback, V8TestInterface::PartialStaticLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    };
    V8DOMConfiguration::InstallAccessors(
        isolate, world, instance_template, prototype_template, interface_template,
        signature, kAccessorConfigurations,
        base::size(kAccessorConfigurations));
  }
  if (RuntimeEnabledFeatures::RuntimeFeatureEnabled()) {
    static constexpr V8DOMConfiguration::AccessorConfiguration
    kAccessorConfigurations[] = {
        { "conditionalReadOnlyLongAttribute", V8TestInterface::ConditionalReadOnlyLongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "staticConditionalReadOnlyLongAttribute", V8TestInterface::StaticConditionalReadOnlyLongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "conditionalLongAttribute", V8TestInterface::ConditionalLongAttributeAttributeGetterCallback, V8TestInterface::ConditionalLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    };
    V8DOMConfiguration::InstallAccessors(
        isolate, world, instance_template, prototype_template, interface_template,
        signature, kAccessorConfigurations,
        base::size(kAccessorConfigurations));
  }

  // Custom signature
  if (RuntimeEnabledFeatures::Mixin2RuntimeFeatureEnabled()) {
    {
      // Install mixin2VoidMethod configuration
      constexpr V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
          {"mixin2VoidMethod", V8TestInterface::Mixin2VoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& config : kConfigurations) {
        V8DOMConfiguration::InstallMethod(
            isolate, world, instance_template, prototype_template,
            interface_template, signature, config);
      }
    }
  }
  if (RuntimeEnabledFeatures::PartialRuntimeFeatureEnabled()) {
    {
      // Install partialVoidMethod configuration
      constexpr V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
          {"partialVoidMethod", V8TestInterface::PartialVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& config : kConfigurations) {
        V8DOMConfiguration::InstallMethod(
            isolate, world, instance_template, prototype_template,
            interface_template, signature, config);
      }
    }
  }
  if (RuntimeEnabledFeatures::PartialRuntimeFeatureEnabled()) {
    {
      // Install partialStaticVoidMethod configuration
      constexpr V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
          {"partialStaticVoidMethod", V8TestInterface::PartialStaticVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& config : kConfigurations) {
        V8DOMConfiguration::InstallMethod(
            isolate, world, instance_template, prototype_template,
            interface_template, signature, config);
      }
    }
  }
  if (RuntimeEnabledFeatures::PartialRuntimeFeatureEnabled()) {
    {
      // Install partialVoidMethodLongArg configuration
      constexpr V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
          {"partialVoidMethodLongArg", V8TestInterface::PartialVoidMethodLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& config : kConfigurations) {
        V8DOMConfiguration::InstallMethod(
            isolate, world, instance_template, prototype_template,
            interface_template, signature, config);
      }
    }
  }
  if (RuntimeEnabledFeatures::PartialRuntimeFeatureEnabled()) {
    {
      // Install partialCallWithExecutionContextRaisesExceptionVoidMethod configuration
      constexpr V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
          {"partialCallWithExecutionContextRaisesExceptionVoidMethod", V8TestInterface::PartialCallWithExecutionContextRaisesExceptionVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& config : kConfigurations) {
        V8DOMConfiguration::InstallMethod(
            isolate, world, instance_template, prototype_template,
            interface_template, signature, config);
      }
    }
  }
  if (RuntimeEnabledFeatures::PartialRuntimeFeatureEnabled()) {
    {
      // Install partialVoidMethodPartialCallbackTypeArg configuration
      constexpr V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
          {"partialVoidMethodPartialCallbackTypeArg", V8TestInterface::PartialVoidMethodPartialCallbackTypeArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& config : kConfigurations) {
        V8DOMConfiguration::InstallMethod(
            isolate, world, instance_template, prototype_template,
            interface_template, signature, config);
      }
    }
  }
}

void V8TestInterface::InstallTestFeature(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::Object> instance,
    v8::Local<v8::Object> prototype,
    v8::Local<v8::Function> interface) {
  v8::Local<v8::FunctionTemplate> interface_template =
      V8TestInterface::GetWrapperTypeInfo()->DomTemplate(isolate, world);
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  ExecutionContext* execution_context = ToExecutionContext(isolate->GetCurrentContext());
  if (execution_context && (execution_context->IsDocument())) {
    static constexpr V8DOMConfiguration::MethodConfiguration
    kOriginTrialWindowExposedMethodConfigurations[] = {
        {"originTrialWindowExposedMethod", V8TestInterface::OriginTrialWindowExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& config : kOriginTrialWindowExposedMethodConfigurations) {
      V8DOMConfiguration::InstallMethod(
          isolate, world, instance, prototype,
          interface, signature, config);
    }
  }
}

void V8TestInterface::InstallTestFeature(
    ScriptState* script_state, v8::Local<v8::Object> instance) {
  V8PerContextData* per_context_data = script_state->PerContextData();
  v8::Local<v8::Object> prototype = per_context_data->PrototypeForType(
      V8TestInterface::GetWrapperTypeInfo());
  v8::Local<v8::Function> interface = per_context_data->ConstructorForType(
      V8TestInterface::GetWrapperTypeInfo());
  ALLOW_UNUSED_LOCAL(interface);
  InstallTestFeature(script_state->GetIsolate(), script_state->World(), instance, prototype, interface);
}

void V8TestInterface::InstallTestFeature(ScriptState* script_state) {
  InstallTestFeature(script_state, v8::Local<v8::Object>());
}

v8::Local<v8::FunctionTemplate> V8TestInterface::DomTemplate(
    v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate, world, const_cast<WrapperTypeInfo*>(V8TestInterface::GetWrapperTypeInfo()),
      V8TestInterface::install_v8_test_interface_template_function_);
}

bool V8TestInterface::HasInstance(v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(V8TestInterface::GetWrapperTypeInfo(), v8_value);
}

v8::Local<v8::Object> V8TestInterface::FindInstanceInPrototypeChain(
    v8::Local<v8::Value> v8_value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(
      V8TestInterface::GetWrapperTypeInfo(), v8_value);
}

TestInterfaceImplementation* V8TestInterface::ToImplWithTypeCheck(
    v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return HasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceImplementation* NativeValueTraits<TestInterfaceImplementation>::NativeValue(
    v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exception_state) {
  TestInterfaceImplementation* native_value = V8TestInterface::ToImplWithTypeCheck(isolate, value);
  if (!native_value) {
    exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterface"));
  }
  return native_value;
}

void V8TestInterface::InstallConditionalFeatures(
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
    if (is_secure_context) {
      static constexpr V8DOMConfiguration::AccessorConfiguration
      kAccessorConfigurations[] = {
          { "partial2SecureContextAttribute", V8TestInterface::Partial2SecureContextAttributeAttributeGetterCallback, V8TestInterface::Partial2SecureContextAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          { "partialSecureContextAttribute", V8TestInterface::PartialSecureContextAttributeAttributeGetterCallback, V8TestInterface::PartialSecureContextAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          { "secureContextAttribute", V8TestInterface::SecureContextAttributeAttributeGetterCallback, V8TestInterface::SecureContextAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      };
      V8DOMConfiguration::InstallAccessors(
          isolate, world, instance_object, prototype_object, interface_object,
          signature, kAccessorConfigurations,
          base::size(kAccessorConfigurations));

      if (RuntimeEnabledFeatures::PartialRuntimeFeatureEnabled()) {
        static constexpr V8DOMConfiguration::AccessorConfiguration
        kAccessorConfigurations[] = {
            { "partialSecureContextLongAttribute", V8TestInterface::PartialSecureContextLongAttributeAttributeGetterCallback, V8TestInterface::PartialSecureContextLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instance_object, prototype_object, interface_object,
            signature, kAccessorConfigurations,
            base::size(kAccessorConfigurations));
      }
      if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
        static constexpr V8DOMConfiguration::AccessorConfiguration
        kAccessorConfigurations[] = {
            { "partialSecureContextRuntimeEnabledAttribute", V8TestInterface::PartialSecureContextRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::PartialSecureContextRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
            { "secureContextRuntimeEnabledAttribute", V8TestInterface::SecureContextRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::SecureContextRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instance_object, prototype_object, interface_object,
            signature, kAccessorConfigurations,
            base::size(kAccessorConfigurations));
      }
    }
    if (is_secure_context || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      static constexpr V8DOMConfiguration::AccessorConfiguration
      kAccessorConfigurations[] = {
          { "secureContextnessRuntimeEnabledAttribute", V8TestInterface::SecureContextnessRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::SecureContextnessRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      };
      V8DOMConfiguration::InstallAccessors(
          isolate, world, instance_object, prototype_object, interface_object,
          signature, kAccessorConfigurations,
          base::size(kAccessorConfigurations));
    }
    if (execution_context && (execution_context->IsDocument())) {
      static constexpr V8DOMConfiguration::AccessorConfiguration
      kAccessorConfigurations[] = {
          { "windowExposedAttribute", V8TestInterface::WindowExposedAttributeAttributeGetterCallback, V8TestInterface::WindowExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      };
      V8DOMConfiguration::InstallAccessors(
          isolate, world, instance_object, prototype_object, interface_object,
          signature, kAccessorConfigurations,
          base::size(kAccessorConfigurations));

      if (is_secure_context) {
        static constexpr V8DOMConfiguration::AccessorConfiguration
        kAccessorConfigurations[] = {
            { "partialSecureContextWindowExposedAttribute", V8TestInterface::PartialSecureContextWindowExposedAttributeAttributeGetterCallback, V8TestInterface::PartialSecureContextWindowExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
            { "secureContextWindowExposedAttribute", V8TestInterface::SecureContextWindowExposedAttributeAttributeGetterCallback, V8TestInterface::SecureContextWindowExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instance_object, prototype_object, interface_object,
            signature, kAccessorConfigurations,
            base::size(kAccessorConfigurations));

        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          static constexpr V8DOMConfiguration::AccessorConfiguration
          kAccessorConfigurations[] = {
              { "partialSecureContextWindowExposedRuntimeEnabledAttribute", V8TestInterface::PartialSecureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::PartialSecureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
              { "secureContextWindowExposedRuntimeEnabledAttribute", V8TestInterface::SecureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::SecureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          };
          V8DOMConfiguration::InstallAccessors(
              isolate, world, instance_object, prototype_object, interface_object,
              signature, kAccessorConfigurations,
              base::size(kAccessorConfigurations));
        }
      }
    }
    if (execution_context && (execution_context->IsWorkerGlobalScope())) {
      static constexpr V8DOMConfiguration::AccessorConfiguration
      kAccessorConfigurations[] = {
          { "workerExposedAttribute", V8TestInterface::WorkerExposedAttributeAttributeGetterCallback, V8TestInterface::WorkerExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      };
      V8DOMConfiguration::InstallAccessors(
          isolate, world, instance_object, prototype_object, interface_object,
          signature, kAccessorConfigurations,
          base::size(kAccessorConfigurations));

      if (is_secure_context) {
        static constexpr V8DOMConfiguration::AccessorConfiguration
        kAccessorConfigurations[] = {
            { "partialSecureContextWorkerExposedAttribute", V8TestInterface::PartialSecureContextWorkerExposedAttributeAttributeGetterCallback, V8TestInterface::PartialSecureContextWorkerExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
            { "secureContextWorkerExposedAttribute", V8TestInterface::SecureContextWorkerExposedAttributeAttributeGetterCallback, V8TestInterface::SecureContextWorkerExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instance_object, prototype_object, interface_object,
            signature, kAccessorConfigurations,
            base::size(kAccessorConfigurations));

        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          static constexpr V8DOMConfiguration::AccessorConfiguration
          kAccessorConfigurations[] = {
              { "partialSecureContextWorkerExposedRuntimeEnabledAttribute", V8TestInterface::PartialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::PartialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
              { "secureContextWorkerExposedRuntimeEnabledAttribute", V8TestInterface::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::SecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          };
          V8DOMConfiguration::InstallAccessors(
              isolate, world, instance_object, prototype_object, interface_object,
              signature, kAccessorConfigurations,
              base::size(kAccessorConfigurations));
        }
      }
    }
    if (execution_context && (execution_context->IsWorkerGlobalScope())) {
      {
        // Install workerExposedMethod configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"workerExposedMethod", V8TestInterface::WorkerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& config : kConfigurations) {
          V8DOMConfiguration::InstallMethod(
              isolate, world, instance_object, prototype_object,
              interface_object, signature, config);
        }
      }
    }
    if (execution_context && (execution_context->IsDocument())) {
      {
        // Install windowExposedMethod configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"windowExposedMethod", V8TestInterface::WindowExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& config : kConfigurations) {
          V8DOMConfiguration::InstallMethod(
              isolate, world, instance_object, prototype_object,
              interface_object, signature, config);
        }
      }
    }
    if (execution_context && (execution_context->IsDocument())) {
      if (RuntimeEnabledFeatures::RuntimeFeatureEnabled()) {
        {
          // Install methodWithExposedAndRuntimeEnabledFlag configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"methodWithExposedAndRuntimeEnabledFlag", V8TestInterface::MethodWithExposedAndRuntimeEnabledFlagMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (execution_context && (execution_context->IsDocument())) {
      {
        // Install overloadMethodWithExposedAndRuntimeEnabledFlag configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"overloadMethodWithExposedAndRuntimeEnabledFlag", V8TestInterface::OverloadMethodWithExposedAndRuntimeEnabledFlagMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& config : kConfigurations) {
          V8DOMConfiguration::InstallMethod(
              isolate, world, instance_object, prototype_object,
              interface_object, signature, config);
        }
      }
    }
    if (execution_context && ((execution_context->IsDocument() && RuntimeEnabledFeatures::FeatureNameEnabled()) || (execution_context->IsWorkerGlobalScope() && RuntimeEnabledFeatures::FeatureName2Enabled()))) {
      {
        // Install methodWithExposedHavingRuntimeEnabldFlag configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"methodWithExposedHavingRuntimeEnabldFlag", V8TestInterface::MethodWithExposedHavingRuntimeEnabldFlagMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& config : kConfigurations) {
          V8DOMConfiguration::InstallMethod(
              isolate, world, instance_object, prototype_object,
              interface_object, signature, config);
        }
      }
    }
    if (execution_context && (execution_context->IsDocument() || execution_context->IsServiceWorkerGlobalScope())) {
      {
        // Install windowAndServiceWorkerExposedMethod configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"windowAndServiceWorkerExposedMethod", V8TestInterface::WindowAndServiceWorkerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& config : kConfigurations) {
          V8DOMConfiguration::InstallMethod(
              isolate, world, instance_object, prototype_object,
              interface_object, signature, config);
        }
      }
    }
    if (is_secure_context) {
      {
        // Install secureContextMethod configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"secureContextMethod", V8TestInterface::SecureContextMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& config : kConfigurations) {
          V8DOMConfiguration::InstallMethod(
              isolate, world, instance_object, prototype_object,
              interface_object, signature, config);
        }
      }
    }
    if (is_secure_context) {
      if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
        {
          // Install secureContextRuntimeEnabledMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"secureContextRuntimeEnabledMethod", V8TestInterface::SecureContextRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
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
      {
        // Install secureContextnessRuntimeEnabledMethod configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"secureContextnessRuntimeEnabledMethod", V8TestInterface::SecureContextnessRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& config : kConfigurations) {
          V8DOMConfiguration::InstallMethod(
              isolate, world, instance_object, prototype_object,
              interface_object, signature, config);
        }
      }
    }
    if (is_secure_context) {
      if (execution_context && (execution_context->IsDocument())) {
        {
          // Install secureContextWindowExposedMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"secureContextWindowExposedMethod", V8TestInterface::SecureContextWindowExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (is_secure_context) {
      if (execution_context && (execution_context->IsWorkerGlobalScope())) {
        {
          // Install secureContextWorkerExposedMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"secureContextWorkerExposedMethod", V8TestInterface::SecureContextWorkerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (is_secure_context) {
      if (execution_context && (execution_context->IsDocument())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          {
            // Install secureContextWindowExposedRuntimeEnabledMethod configuration
            const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
                {"secureContextWindowExposedRuntimeEnabledMethod", V8TestInterface::SecureContextWindowExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
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
    if (is_secure_context) {
      if (execution_context && (execution_context->IsWorkerGlobalScope())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          {
            // Install secureContextWorkerExposedRuntimeEnabledMethod configuration
            const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
                {"secureContextWorkerExposedRuntimeEnabledMethod", V8TestInterface::SecureContextWorkerExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
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
    if (is_secure_context) {
      {
        // Install partial2SecureContextMethod configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"partial2SecureContextMethod", V8TestInterface::Partial2SecureContextMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& config : kConfigurations) {
          V8DOMConfiguration::InstallMethod(
              isolate, world, instance_object, prototype_object,
              interface_object, signature, config);
        }
      }
    }
    if (is_secure_context) {
      {
        // Install partialSecureContextMethod configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"partialSecureContextMethod", V8TestInterface::PartialSecureContextMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& config : kConfigurations) {
          V8DOMConfiguration::InstallMethod(
              isolate, world, instance_object, prototype_object,
              interface_object, signature, config);
        }
      }
    }
    if (is_secure_context) {
      if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
        {
          // Install partialSecureContextRuntimeEnabledMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"partialSecureContextRuntimeEnabledMethod", V8TestInterface::PartialSecureContextRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (is_secure_context) {
      if (execution_context && (execution_context->IsDocument())) {
        {
          // Install partialSecureContextWindowExposedMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"partialSecureContextWindowExposedMethod", V8TestInterface::PartialSecureContextWindowExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (is_secure_context) {
      if (execution_context && (execution_context->IsWorkerGlobalScope())) {
        {
          // Install partialSecureContextWorkerExposedMethod configuration
          const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
              {"partialSecureContextWorkerExposedMethod", V8TestInterface::PartialSecureContextWorkerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& config : kConfigurations) {
            V8DOMConfiguration::InstallMethod(
                isolate, world, instance_object, prototype_object,
                interface_object, signature, config);
          }
        }
      }
    }
    if (is_secure_context) {
      if (execution_context && (execution_context->IsDocument())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          {
            // Install partialSecureContextWindowExposedRuntimeEnabledMethod configuration
            const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
                {"partialSecureContextWindowExposedRuntimeEnabledMethod", V8TestInterface::PartialSecureContextWindowExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
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
    if (is_secure_context) {
      if (execution_context && (execution_context->IsWorkerGlobalScope())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          {
            // Install partialSecureContextWorkerExposedRuntimeEnabledMethod configuration
            const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
                {"partialSecureContextWorkerExposedRuntimeEnabledMethod", V8TestInterface::PartialSecureContextWorkerExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
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
    if (execution_context && (execution_context->IsWorkerGlobalScope())) {
      {
        // Install workerExposedStaticMethod configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"workerExposedStaticMethod", V8TestInterface::WorkerExposedStaticMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& config : kConfigurations) {
          V8DOMConfiguration::InstallMethod(
              isolate, world, instance_object, prototype_object,
              interface_object, signature, config);
        }
      }
    }
    if (execution_context && (execution_context->IsDocument())) {
      {
        // Install windowExposedStaticMethod configuration
        const V8DOMConfiguration::MethodConfiguration kConfigurations[] = {
            {"windowExposedStaticMethod", V8TestInterface::WindowExposedStaticMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
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

InstallRuntimeEnabledFeaturesOnTemplateFunction
V8TestInterface::install_runtime_enabled_features_on_template_function_ =
    &V8TestInterface::InstallRuntimeEnabledFeaturesOnTemplate;

InstallTemplateFunction
V8TestInterface::install_v8_test_interface_template_function_ =
    &V8TestInterface::InstallV8TestInterfaceTemplate;

void V8TestInterface::UpdateWrapperTypeInfo(
    InstallTemplateFunction install_template_function,
    InstallRuntimeEnabledFeaturesFunction install_runtime_enabled_features_function,
    InstallRuntimeEnabledFeaturesOnTemplateFunction install_runtime_enabled_features_on_template_function,
    InstallConditionalFeaturesFunction install_conditional_features_function) {
  V8TestInterface::install_v8_test_interface_template_function_ =
      install_template_function;

  CHECK(install_runtime_enabled_features_on_template_function);
  V8TestInterface::install_runtime_enabled_features_on_template_function_ =
      install_runtime_enabled_features_on_template_function;

  if (install_conditional_features_function) {
    V8TestInterface::GetWrapperTypeInfo()->install_conditional_features_function =
        install_conditional_features_function;
  }
}

void V8TestInterface::RegisterVoidMethodPartialOverloadMethodForPartialInterface(void (*method)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  test_interface_implementation_v8_internal::voidMethodPartialOverloadMethodForPartialInterface = method;
}

void V8TestInterface::RegisterStaticVoidMethodPartialOverloadMethodForPartialInterface(void (*method)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  test_interface_implementation_v8_internal::staticVoidMethodPartialOverloadMethodForPartialInterface = method;
}

void V8TestInterface::RegisterPromiseMethodPartialOverloadMethodForPartialInterface(void (*method)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  test_interface_implementation_v8_internal::promiseMethodPartialOverloadMethodForPartialInterface = method;
}

void V8TestInterface::RegisterStaticPromiseMethodPartialOverloadMethodForPartialInterface(void (*method)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  test_interface_implementation_v8_internal::staticPromiseMethodPartialOverloadMethodForPartialInterface = method;
}

void V8TestInterface::RegisterPartial2VoidMethodMethodForPartialInterface(void (*method)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  test_interface_implementation_v8_internal::partial2VoidMethodMethodForPartialInterface = method;
}

void V8TestInterface::RegisterPartial2StaticVoidMethodMethodForPartialInterface(void (*method)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  test_interface_implementation_v8_internal::partial2StaticVoidMethodMethodForPartialInterface = method;
}

}  // namespace blink
