// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event_listener_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_2.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_empty.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_implements_2.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_implements_3_implementation.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_partial.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_partial_2_implementation.h"
#include "third_party/blink/renderer/bindings/tests/idls/core/test_interface_partial_secure_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
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
WrapperTypeInfo V8TestInterface::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterface::domTemplate,
    V8TestInterface::InstallConditionalFeatures,
    "TestInterface",
    &V8TestInterfaceEmpty::wrapperTypeInfo,
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
const WrapperTypeInfo& TestInterfaceImplementation::wrapper_type_info_ = V8TestInterface::wrapperTypeInfo;

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

static void testInterfaceAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->testInterfaceAttribute()), impl);
}

static void testInterfaceAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "testInterfaceAttribute");

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterface'.");
    return;
  }

  impl->setTestInterfaceAttribute(cppValue);
}

static void doubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValue(info, impl->doubleAttribute());
}

static void doubleAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "doubleAttribute");

  // Prepare the value to be set.
  double cppValue = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDoubleAttribute(cppValue);
}

static void floatAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValue(info, impl->floatAttribute());
}

static void floatAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "floatAttribute");

  // Prepare the value to be set.
  float cppValue = NativeValueTraits<IDLFloat>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setFloatAttribute(cppValue);
}

static void unrestrictedDoubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValue(info, impl->unrestrictedDoubleAttribute());
}

static void unrestrictedDoubleAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "unrestrictedDoubleAttribute");

  // Prepare the value to be set.
  double cppValue = NativeValueTraits<IDLUnrestrictedDouble>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnrestrictedDoubleAttribute(cppValue);
}

static void unrestrictedFloatAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValue(info, impl->unrestrictedFloatAttribute());
}

static void unrestrictedFloatAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "unrestrictedFloatAttribute");

  // Prepare the value to be set.
  float cppValue = NativeValueTraits<IDLUnrestrictedFloat>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnrestrictedFloatAttribute(cppValue);
}

static void testEnumAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, impl->testEnumAttribute(), info.GetIsolate());
}

static void testEnumAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "testEnumAttribute");

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  // Type check per: http://heycam.github.io/webidl/#dfn-attribute-setter
  // Returns undefined without setting the value if the value is invalid.
  DummyExceptionStateForTesting dummyExceptionState;
  const char* validValues[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(cppValue, validValues, base::size(validValues), "TestEnum", dummyExceptionState)) {
    ExecutionContext::ForCurrentRealm(info)->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                               dummyExceptionState.Message()));
    return;
  }

  impl->setTestEnumAttribute(cppValue);
}

static void testEnumOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueStringOrNull(info, impl->testEnumOrNullAttribute(), info.GetIsolate());
}

static void testEnumOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "testEnumOrNullAttribute");

  // Prepare the value to be set.
  V8StringResource<kTreatNullAndUndefinedAsNullString> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  // Type check per: http://heycam.github.io/webidl/#dfn-attribute-setter
  // Returns undefined without setting the value if the value is invalid.
  DummyExceptionStateForTesting dummyExceptionState;
  const char* validValues[] = {
      nullptr,
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(cppValue, validValues, base::size(validValues), "TestEnum", dummyExceptionState)) {
    ExecutionContext::ForCurrentRealm(info)->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                               dummyExceptionState.Message()));
    return;
  }

  impl->setTestEnumOrNullAttribute(cppValue);
}

static void stringOrDoubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  StringOrDouble result;
  impl->stringOrDoubleAttribute(result);

  V8SetReturnValue(info, result);
}

static void stringOrDoubleAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "stringOrDoubleAttribute");

  // Prepare the value to be set.
  StringOrDouble cppValue;
  V8StringOrDouble::ToImpl(info.GetIsolate(), v8Value, cppValue, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setStringOrDoubleAttribute(cppValue);
}

static void withExtendedAttributeStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, impl->withExtendedAttributeStringAttribute());
}

static void withExtendedAttributeStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "withExtendedAttributeStringAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLongEnforceRange>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setWithExtendedAttributeStringAttribute(cppValue);
}

static void uncapitalAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->CapitalImplementation()), impl);
}

static void uncapitalAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "uncapitalAttribute");

  // Prepare the value to be set.
  Implementation* cppValue = V8Implementation::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Implementation'.");
    return;
  }

  impl->setCapitalImplementation(cppValue);
}

static void conditionalLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, impl->conditionalLongAttribute());
}

static void conditionalLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "conditionalLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setConditionalLongAttribute(cppValue);
}

static void conditionalReadOnlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, impl->conditionalReadOnlyLongAttribute());
}

static void staticStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueString(info, TestInterfaceImplementation::staticStringAttribute(), info.GetIsolate());
}

static void staticStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  TestInterfaceImplementation::setStaticStringAttribute(cppValue);
}

static void staticReturnDOMWrapperAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, WTF::GetPtr(TestInterfaceImplementation::staticReturnDOMWrapperAttribute()), info.GetIsolate()->GetCurrentContext()->Global());
}

static void staticReturnDOMWrapperAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "staticReturnDOMWrapperAttribute");

  // Prepare the value to be set.
  TestInterfaceImplementation* cppValue = V8TestInterface::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterface'.");
    return;
  }

  TestInterfaceImplementation::setStaticReturnDOMWrapperAttribute(cppValue);
}

static void staticReadOnlyStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueString(info, TestInterfaceImplementation::staticReadOnlyStringAttribute(), info.GetIsolate());
}

static void staticReadOnlyReturnDOMWrapperAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, WTF::GetPtr(TestInterfaceImplementation::staticReadOnlyReturnDOMWrapperAttribute()), info.GetIsolate()->GetCurrentContext()->Global());
}

static void staticConditionalReadOnlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueInt(info, TestInterfaceImplementation::staticConditionalReadOnlyLongAttribute());
}

static void legacyInterfaceTypeCheckingAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->legacyInterfaceTypeCheckingAttribute()), impl);
}

static void legacyInterfaceTypeCheckingAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  // Prepare the value to be set.
  TestInterfaceEmpty* cppValue = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  impl->setLegacyInterfaceTypeCheckingAttribute(cppValue);
}

static void stringNullAsEmptyAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, impl->stringNullAsEmptyAttribute(), info.GetIsolate());
}

static void stringNullAsEmptyAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<kTreatNullAsEmptyString> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setStringNullAsEmptyAttribute(cppValue);
}

static void usvStringOrNullAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueStringOrNull(info, impl->usvStringOrNullAttribute(), info.GetIsolate());
}

static void usvStringOrNullAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "usvStringOrNullAttribute");

  // Prepare the value to be set.
  V8StringResource<kTreatNullAndUndefinedAsNullString> cppValue = NativeValueTraits<IDLUSVStringOrNull>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUsvStringOrNullAttribute(cppValue);
}

static void alwaysExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, impl->alwaysExposedAttribute());
}

static void alwaysExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "alwaysExposedAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setAlwaysExposedAttribute(cppValue);
}

static void workerExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, impl->workerExposedAttribute());
}

static void workerExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "workerExposedAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setWorkerExposedAttribute(cppValue);
}

static void windowExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, impl->windowExposedAttribute());
}

static void windowExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "windowExposedAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setWindowExposedAttribute(cppValue);
}

static void lenientThisAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  // [LenientThis]
  // Make sure that info.Holder() really points to an instance if [LenientThis].
  if (!V8TestInterface::hasInstance(info.Holder(), info.GetIsolate()))
    return; // Return silently because of [LenientThis].

  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValue(info, impl->lenientThisAttribute().V8Value());
}

static void lenientThisAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // [LenientThis]
  // Make sure that info.Holder() really points to an instance if [LenientThis].
  if (!V8TestInterface::hasInstance(holder, isolate))
    return; // Return silently because of [LenientThis].

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  // Prepare the value to be set.
  ScriptValue cppValue = ScriptValue(ScriptState::Current(info.GetIsolate()), v8Value);

  impl->setLenientThisAttribute(cppValue);
}

static void attributeWithSideEffectFreeGetterAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->attributeWithSideEffectFreeGetter());
}

static void attributeWithSideEffectFreeGetterAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "attributeWithSideEffectFreeGetter");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setAttributeWithSideEffectFreeGetter(cppValue);
}

static void secureContextAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextAttribute());
}

static void secureContextAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextAttribute(cppValue);
}

static void secureContextRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextRuntimeEnabledAttribute());
}

static void secureContextRuntimeEnabledAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextRuntimeEnabledAttribute(cppValue);
}

static void secureContextnessRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextnessRuntimeEnabledAttribute());
}

static void secureContextnessRuntimeEnabledAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextnessRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextnessRuntimeEnabledAttribute(cppValue);
}

static void secureContextWindowExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWindowExposedAttribute());
}

static void secureContextWindowExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextWindowExposedAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextWindowExposedAttribute(cppValue);
}

static void secureContextWorkerExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWorkerExposedAttribute());
}

static void secureContextWorkerExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextWorkerExposedAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextWorkerExposedAttribute(cppValue);
}

static void secureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWindowExposedRuntimeEnabledAttribute());
}

static void secureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextWindowExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextWindowExposedRuntimeEnabledAttribute(cppValue);
}

static void secureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, impl->secureContextWorkerExposedRuntimeEnabledAttribute());
}

static void secureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "secureContextWorkerExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setSecureContextWorkerExposedRuntimeEnabledAttribute(cppValue);
}

static void implementsStaticReadOnlyLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueInt(info, TestInterfaceImplementation::implementsStaticReadOnlyLongAttribute());
}

static void implementsStaticStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueString(info, TestInterfaceImplementation::implementsStaticStringAttribute(), info.GetIsolate());
}

static void implementsStaticStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  TestInterfaceImplementation::setImplementsStaticStringAttribute(cppValue);
}

static void implementsReadonlyStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, impl->implementsReadonlyStringAttribute(), info.GetIsolate());
}

static void implementsStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, impl->implementsStringAttribute(), info.GetIsolate());
}

static void implementsStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  impl->setImplementsStringAttribute(cppValue);
}

static void implementsNodeAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->implementsNodeAttribute()), impl);
}

static void implementsNodeAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "implementsNodeAttribute");

  // Prepare the value to be set.
  Node* cppValue = V8Node::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Node'.");
    return;
  }

  impl->setImplementsNodeAttribute(cppValue);
}

static void implementsEventHandlerAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  EventListener* cppValue(WTF::GetPtr(impl->implementsEventHandlerAttribute()));

  V8SetReturnValue(info, JSBasedEventListener::GetListenerOrNull(info.GetIsolate(), impl, cppValue));
}

static void implementsEventHandlerAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  // Prepare the value to be set.

  impl->setImplementsEventHandlerAttribute(V8EventListenerHelper::GetEventHandler(ScriptState::ForRelevantRealm(info), v8Value, JSEventHandler::HandlerType::kEventHandler, kListenerFindOrCreate));
}

static void implementsRuntimeEnabledNodeAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->implementsRuntimeEnabledNodeAttribute()), impl);
}

static void implementsRuntimeEnabledNodeAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "implementsRuntimeEnabledNodeAttribute");

  // Prepare the value to be set.
  Node* cppValue = V8Node::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'Node'.");
    return;
  }

  impl->setImplementsRuntimeEnabledNodeAttribute(cppValue);
}

static void implements2StaticStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueString(info, TestImplements2::implements2StaticStringAttribute(), info.GetIsolate());
}

static void implements2StaticStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  TestImplements2::setImplements2StaticStringAttribute(cppValue);
}

static void implements2StringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, TestImplements2::implements2StringAttribute(*impl), info.GetIsolate());
}

static void implements2StringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  TestImplements2::setImplements2StringAttribute(*impl, cppValue);
}

static void implements3StringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, TestImplements3Implementation::implements3StringAttribute(*impl), info.GetIsolate());
}

static void implements3StringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  TestImplements3Implementation::setImplements3StringAttribute(*impl, cppValue);
}

static void implements3StaticStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueString(info, TestImplements3Implementation::implements3StaticStringAttribute(), info.GetIsolate());
}

static void implements3StaticStringAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  TestImplements3Implementation::setImplements3StaticStringAttribute(cppValue);
}

static void partialLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, TestInterfacePartial::partialLongAttribute(*impl));
}

static void partialLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partialLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartial::setPartialLongAttribute(*impl, cppValue);
}

static void partialStaticLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueInt(info, TestInterfacePartial::partialStaticLongAttribute());
}

static void partialStaticLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partialStaticLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartial::setPartialStaticLongAttribute(cppValue);
}

static void partialCallWithExecutionContextLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);

  V8SetReturnValueInt(info, TestInterfacePartial::partialCallWithExecutionContextLongAttribute(executionContext, *impl));
}

static void partialCallWithExecutionContextLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partialCallWithExecutionContextLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);

  TestInterfacePartial::setPartialCallWithExecutionContextLongAttribute(executionContext, *impl, cppValue);
}

static void partialPartialEnumTypeAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueString(info, TestInterfacePartial::partialPartialEnumTypeAttribute(*impl), info.GetIsolate());
}

static void partialPartialEnumTypeAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partialPartialEnumTypeAttribute");

  // Prepare the value to be set.
  V8StringResource<> cppValue = v8Value;
  if (!cppValue.Prepare())
    return;

  // Type check per: http://heycam.github.io/webidl/#dfn-attribute-setter
  // Returns undefined without setting the value if the value is invalid.
  DummyExceptionStateForTesting dummyExceptionState;
  const char* validValues[] = {
      "foo",
      "bar",
  };
  if (!IsValidEnum(cppValue, validValues, base::size(validValues), "PartialEnumType", dummyExceptionState)) {
    ExecutionContext::ForCurrentRealm(info)->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kWarningMessageLevel,
                               dummyExceptionState.Message()));
    return;
  }

  TestInterfacePartial::setPartialPartialEnumTypeAttribute(*impl, cppValue);
}

static void partialSecureContextLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, TestInterfacePartial::partialSecureContextLongAttribute(*impl));
}

static void partialSecureContextLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartial::setPartialSecureContextLongAttribute(*impl, cppValue);
}

static void partial2LongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueInt(info, TestInterfacePartial2Implementation::partial2LongAttribute(*impl));
}

static void partial2LongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partial2LongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartial2Implementation::setPartial2LongAttribute(*impl, cppValue);
}

static void partial2StaticLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueInt(info, TestInterfacePartial2Implementation::partial2StaticLongAttribute());
}

static void partial2StaticLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partial2StaticLongAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartial2Implementation::setPartial2StaticLongAttribute(cppValue);
}

static void partial2SecureContextAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartial2Implementation::partial2SecureContextAttribute(*impl));
}

static void partial2SecureContextAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partial2SecureContextAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartial2Implementation::setPartial2SecureContextAttribute(*impl, cppValue);
}

static void partialSecureContextAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartialSecureContext::partialSecureContextAttribute(*impl));
}

static void partialSecureContextAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartialSecureContext::setPartialSecureContextAttribute(*impl, cppValue);
}

static void partialSecureContextRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartialSecureContext::partialSecureContextRuntimeEnabledAttribute(*impl));
}

static void partialSecureContextRuntimeEnabledAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartialSecureContext::setPartialSecureContextRuntimeEnabledAttribute(*impl, cppValue);
}

static void partialSecureContextWindowExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartialSecureContext::partialSecureContextWindowExposedAttribute(*impl));
}

static void partialSecureContextWindowExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextWindowExposedAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartialSecureContext::setPartialSecureContextWindowExposedAttribute(*impl, cppValue);
}

static void partialSecureContextWorkerExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartialSecureContext::partialSecureContextWorkerExposedAttribute(*impl));
}

static void partialSecureContextWorkerExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextWorkerExposedAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartialSecureContext::setPartialSecureContextWorkerExposedAttribute(*impl, cppValue);
}

static void partialSecureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartialSecureContext::partialSecureContextWindowExposedRuntimeEnabledAttribute(*impl));
}

static void partialSecureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextWindowExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartialSecureContext::setPartialSecureContextWindowExposedRuntimeEnabledAttribute(*impl, cppValue);
}

static void partialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  V8SetReturnValueBool(info, TestInterfacePartialSecureContext::partialSecureContextWorkerExposedRuntimeEnabledAttribute(*impl));
}

static void partialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface", "partialSecureContextWorkerExposedRuntimeEnabledAttribute");

  // Prepare the value to be set.
  bool cppValue = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartialSecureContext::setPartialSecureContextWorkerExposedRuntimeEnabledAttribute(*impl, cppValue);
}

static void voidMethodTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyArg", "TestInterface", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyArg", "TestInterface", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->voidMethodTestInterfaceEmptyArg(testInterfaceEmptyArg);
}

static void voidMethodDoubleArgFloatArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "voidMethodDoubleArgFloatArg");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  double doubleArg;
  float floatArg;
  doubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  floatArg = NativeValueTraits<IDLFloat>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDoubleArgFloatArg(doubleArg, floatArg);
}

static void voidMethodNullableAndOptionalObjectArgsMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNullableAndOptionalObjectArgs", "TestInterface", ExceptionMessages::NotEnoughArguments(2, info.Length())));
    return;
  }

  ScriptValue objectArg;
  ScriptValue nullableObjectArg;
  ScriptValue optionalObjectArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  if (info[0]->IsObject()) {
    objectArg = ScriptValue(ScriptState::Current(info.GetIsolate()), info[0]);
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNullableAndOptionalObjectArgs", "TestInterface", "parameter 1 ('objectArg') is not an object."));
    return;
  }

  if (info[1]->IsObject()) {
    nullableObjectArg = ScriptValue(ScriptState::Current(info.GetIsolate()), info[1]);
  } else if (info[1]->IsNullOrUndefined()) {
    nullableObjectArg = ScriptValue(ScriptState::Current(info.GetIsolate()), v8::Null(info.GetIsolate()));
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNullableAndOptionalObjectArgs", "TestInterface", "parameter 2 ('nullableObjectArg') is not an object."));
    return;
  }

  if (UNLIKELY(numArgsPassed <= 2)) {
    impl->voidMethodNullableAndOptionalObjectArgs(objectArg, nullableObjectArg);
    return;
  }
  if (info[2]->IsObject()) {
    optionalObjectArg = ScriptValue(ScriptState::Current(info.GetIsolate()), info[2]);
  } else if (info[2]->IsUndefined()) {
    optionalObjectArg = ScriptValue(ScriptState::Current(info.GetIsolate()), v8::Undefined(info.GetIsolate()));
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodNullableAndOptionalObjectArgs", "TestInterface", "parameter 3 ('optionalObjectArg') is not an object."));
    return;
  }

  impl->voidMethodNullableAndOptionalObjectArgs(objectArg, nullableObjectArg, optionalObjectArg);
}

static void voidMethodUnrestrictedDoubleArgUnrestrictedFloatArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "voidMethodUnrestrictedDoubleArgUnrestrictedFloatArg");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  double unrestrictedDoubleArg;
  float unrestrictedFloatArg;
  unrestrictedDoubleArg = NativeValueTraits<IDLUnrestrictedDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  unrestrictedFloatArg = NativeValueTraits<IDLUnrestrictedFloat>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodUnrestrictedDoubleArgUnrestrictedFloatArg(unrestrictedDoubleArg, unrestrictedFloatArg);
}

static void voidMethodTestEnumArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "voidMethodTestEnumArg");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> testEnumArg;
  testEnumArg = info[0];
  if (!testEnumArg.Prepare())
    return;
  const char* validTestEnumArgValues[] = {
      "",
      "EnumValue1",
      "EnumValue2",
      "EnumValue3",
  };
  if (!IsValidEnum(testEnumArg, validTestEnumArgValues, base::size(validTestEnumArgValues), "TestEnum", exceptionState)) {
    return;
  }

  impl->voidMethodTestEnumArg(testEnumArg);
}

static void voidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->voidMethod();
}

static void voidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->voidMethod();
}

static void alwaysExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->alwaysExposedMethod();
}

static void workerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->workerExposedMethod();
}

static void windowExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->windowExposedMethod();
}

static void alwaysExposedStaticMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation::alwaysExposedStaticMethod();
}

static void workerExposedStaticMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation::workerExposedStaticMethod();
}

static void windowExposedStaticMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation::windowExposedStaticMethod();
}

static void staticReturnDOMWrapperMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, TestInterfaceImplementation::staticReturnDOMWrapperMethod(), info.GetIsolate()->GetCurrentContext()->Global());
}

static void methodWithExposedAndRuntimeEnabledFlagMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->methodWithExposedAndRuntimeEnabledFlag();
}

static void overloadMethodWithExposedAndRuntimeEnabledFlag1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "overloadMethodWithExposedAndRuntimeEnabledFlag");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadMethodWithExposedAndRuntimeEnabledFlag(longArg);
}

static void overloadMethodWithExposedAndRuntimeEnabledFlag2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  V8StringResource<> string;
  string = info[0];
  if (!string.Prepare())
    return;

  impl->overloadMethodWithExposedAndRuntimeEnabledFlag(string);
}

static void overloadMethodWithExposedAndRuntimeEnabledFlag3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  DOMWindow* window;
  window = ToDOMWindow(info.GetIsolate(), info[0]);
  if (!window) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("overloadMethodWithExposedAndRuntimeEnabledFlag", "TestInterface", "parameter 1 is not of type 'Window'."));
    return;
  }

  impl->overloadMethodWithExposedAndRuntimeEnabledFlag(window);
}

static void overloadMethodWithExposedAndRuntimeEnabledFlagMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (RuntimeEnabledFeatures::FeatureName2Enabled()) {
        if (V8Window::hasInstance(info[0], info.GetIsolate())) {
          overloadMethodWithExposedAndRuntimeEnabledFlag3Method(info);
          return;
        }
      }
      if (info[0]->IsNumber()) {
        overloadMethodWithExposedAndRuntimeEnabledFlag1Method(info);
        return;
      }
      if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
        if (true) {
          overloadMethodWithExposedAndRuntimeEnabledFlag2Method(info);
          return;
        }
      }
      if (true) {
        overloadMethodWithExposedAndRuntimeEnabledFlag1Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "overloadMethodWithExposedAndRuntimeEnabledFlag");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void methodWithExposedHavingRuntimeEnabldFlagMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->methodWithExposedHavingRuntimeEnabldFlag();
}

static void windowAndServiceWorkerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->windowAndServiceWorkerExposedMethod();
}

static void voidMethodPartialOverload1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->voidMethodPartialOverload();
}

static void voidMethodPartialOverload2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "voidMethodPartialOverload");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  double doubleArg;
  doubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodPartialOverload(doubleArg);
}

static void staticVoidMethodPartialOverload1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation::staticVoidMethodPartialOverload();
}

static void promiseMethodPartialOverload1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "promiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestInterface::hasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  V8SetReturnValue(info, impl->promiseMethodPartialOverload().V8Value());
}

static void promiseMethodPartialOverload2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "promiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::kDoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestInterface::hasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.ThrowTypeError("Illegal invocation");
    return;
  }
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  DOMWindow* window;
  window = ToDOMWindow(info.GetIsolate(), info[0]);
  if (!window) {
    exceptionState.ThrowTypeError("parameter 1 is not of type 'Window'.");
    return;
  }

  V8SetReturnValue(info, impl->promiseMethodPartialOverload(window).V8Value());
}

static void staticPromiseMethodPartialOverload1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValue(info, TestInterfaceImplementation::staticPromiseMethodPartialOverload().V8Value());
}

static void overloadMethodWithUnionTypeWithStringMember1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "overloadMethodWithUnionTypeWithStringMember");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  DoubleOrString unionArg;
  V8DoubleOrString::ToImpl(info.GetIsolate(), info[0], unionArg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadMethodWithUnionTypeWithStringMember(unionArg);
}

static void overloadMethodWithUnionTypeWithStringMember2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "overloadMethodWithUnionTypeWithStringMember");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  bool boolArg;
  boolArg = NativeValueTraits<IDLBoolean>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->overloadMethodWithUnionTypeWithStringMember(boolArg);
}

static void overloadMethodWithUnionTypeWithStringMemberMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;

  switch (std::min(1, info.Length())) {
    case 1:
      if (info[0]->IsBoolean()) {
        overloadMethodWithUnionTypeWithStringMember2Method(info);
        return;
      }
      if (true) {
        overloadMethodWithUnionTypeWithStringMember1Method(info);
        return;
      }
      if (true) {
        overloadMethodWithUnionTypeWithStringMember2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "overloadMethodWithUnionTypeWithStringMember");
  if (isArityError) {
    if (info.Length() < 1) {
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
      return;
    }
  }
  exceptionState.ThrowTypeError("No function was found that matched the signature provided.");
}

static void legacyInterfaceTypeCheckingMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("legacyInterfaceTypeCheckingMethod", "TestInterface", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);

  impl->legacyInterfaceTypeCheckingMethod(testInterfaceEmptyArg);
}

static void sideEffectFreeMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->sideEffectFreeMethod();
}

static void secureContextMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextMethod();
}

static void secureContextRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextRuntimeEnabledMethod();
}

static void secureContextnessRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextnessRuntimeEnabledMethod();
}

static void secureContextWindowExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextWindowExposedMethod();
}

static void secureContextWorkerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextWorkerExposedMethod();
}

static void secureContextWindowExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextWindowExposedRuntimeEnabledMethod();
}

static void secureContextWorkerExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->secureContextWorkerExposedRuntimeEnabledMethod();
}

static void methodWithNullableSequencesMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "methodWithNullableSequences");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 4)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(4, info.Length()));
    return;
  }

  Vector<base::Optional<double>> numbers;
  Vector<String> strings;
  HeapVector<Member<Element>> elements;
  HeapVector<DoubleOrString> unions;
  numbers = NativeValueTraits<IDLSequence<IDLNullable<IDLDouble>>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  strings = NativeValueTraits<IDLSequence<IDLStringOrNull>>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  elements = NativeValueTraits<IDLSequence<IDLNullable<Element>>>::NativeValue(info.GetIsolate(), info[2], exceptionState);
  if (exceptionState.HadException())
    return;

  unions = NativeValueTraits<IDLSequence<IDLNullable<DoubleOrString>>>::NativeValue(info.GetIsolate(), info[3], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->methodWithNullableSequences(numbers, strings, elements, unions);
}

static void methodWithNullableRecordsMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "methodWithNullableRecords");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 4)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(4, info.Length()));
    return;
  }

  Vector<std::pair<String, base::Optional<double>>> numbers;
  Vector<std::pair<String, String>> strings;
  HeapVector<std::pair<String, Member<Element>>> elements;
  HeapVector<std::pair<String, DoubleOrString>> unions;
  numbers = NativeValueTraits<IDLRecord<IDLString, IDLNullable<IDLDouble>>>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  strings = NativeValueTraits<IDLRecord<IDLString, IDLStringOrNull>>::NativeValue(info.GetIsolate(), info[1], exceptionState);
  if (exceptionState.HadException())
    return;

  elements = NativeValueTraits<IDLRecord<IDLString, IDLNullable<Element>>>::NativeValue(info.GetIsolate(), info[2], exceptionState);
  if (exceptionState.HadException())
    return;

  unions = NativeValueTraits<IDLRecord<IDLString, IDLNullable<DoubleOrString>>>::NativeValue(info.GetIsolate(), info[3], exceptionState);
  if (exceptionState.HadException())
    return;

  impl->methodWithNullableRecords(numbers, strings, elements, unions);
}

static void implementsVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  impl->implementsVoidMethod();
}

static void implementsComplexMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "implementsComplexMethod");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(2, info.Length()));
    return;
  }

  V8StringResource<> strArg;
  TestInterfaceEmpty* testInterfaceEmptyArg;
  strArg = info[0];
  if (!strArg.Prepare())
    return;

  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[1]);
  if (!testInterfaceEmptyArg) {
    exceptionState.ThrowTypeError("parameter 2 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);
  TestInterfaceEmpty* result = impl->implementsComplexMethod(executionContext, strArg, testInterfaceEmptyArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void implementsStaticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation::implementsStaticVoidMethod();
}

static void implements2VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestImplements2::implements2VoidMethod(*impl);
}

static void implements3VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestImplements3Implementation::implements3VoidMethod(*impl);
}

static void implements3StaticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestImplements3Implementation::implements3StaticVoidMethod();
}

static void partialVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartial::partialVoidMethod(*impl);
}

static void partialStaticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfacePartial::partialStaticVoidMethod();
}

static void partialVoidMethodLongArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "partialVoidMethodLongArg");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  int32_t longArg;
  longArg = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  TestInterfacePartial::partialVoidMethodLongArg(*impl, longArg);
}

static void partialCallWithExecutionContextRaisesExceptionVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "partialCallWithExecutionContextRaisesExceptionVoidMethod");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  ExecutionContext* executionContext = ExecutionContext::ForRelevantRealm(info);
  TestInterfacePartial::partialCallWithExecutionContextRaisesExceptionVoidMethod(executionContext, *impl, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void partialVoidMethodPartialCallbackTypeArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("partialVoidMethodPartialCallbackTypeArg", "TestInterface", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  ScriptValue partialCallbackTypeArg;
  if (info[0]->IsFunction()) {
    partialCallbackTypeArg = ScriptValue(ScriptState::Current(info.GetIsolate()), info[0]);
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("partialVoidMethodPartialCallbackTypeArg", "TestInterface", "The callback provided as parameter 1 is not a function."));
    return;
  }

  TestInterfacePartial::partialVoidMethodPartialCallbackTypeArg(*impl, partialCallbackTypeArg);
}

static void partial2VoidMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartial2Implementation::partial2VoidMethod(*impl);
}

static void partial2StaticVoidMethod1Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfacePartial2Implementation::partial2StaticVoidMethod();
}

static void partial2SecureContextMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartial2Implementation::partial2SecureContextMethod(*impl);
}

static void partialSecureContextMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartialSecureContext::partialSecureContextMethod(*impl);
}

static void partialSecureContextRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartialSecureContext::partialSecureContextRuntimeEnabledMethod(*impl);
}

static void partialSecureContextWindowExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartialSecureContext::partialSecureContextWindowExposedMethod(*impl);
}

static void partialSecureContextWorkerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartialSecureContext::partialSecureContextWorkerExposedMethod(*impl);
}

static void partialSecureContextWindowExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartialSecureContext::partialSecureContextWindowExposedRuntimeEnabledMethod(*impl);
}

static void partialSecureContextWorkerExposedRuntimeEnabledMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  TestInterfacePartialSecureContext::partialSecureContextWorkerExposedRuntimeEnabledMethod(*impl);
}

static void voidMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        voidMethodPartialOverload1Method(info);
        return;
      }
      break;
    case 1:
      if (info[0]->IsNumber()) {
        voidMethodPartialOverload2Method(info);
        return;
      }
      if (true) {
        voidMethodPartialOverload2Method(info);
        return;
      }
      break;
  }

  DCHECK(voidMethodPartialOverloadMethodForPartialInterface);
  (voidMethodPartialOverloadMethodForPartialInterface)(info);
}

static void staticVoidMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        staticVoidMethodPartialOverload1Method(info);
        return;
      }
      break;
    case 1:
      break;
  }

  DCHECK(staticVoidMethodPartialOverloadMethodForPartialInterface);
  (staticVoidMethodPartialOverloadMethodForPartialInterface)(info);
}

static void promiseMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        promiseMethodPartialOverload1Method(info);
        return;
      }
      break;
    case 1:
      if (V8Window::hasInstance(info[0], info.GetIsolate())) {
        promiseMethodPartialOverload2Method(info);
        return;
      }
      break;
  }

  DCHECK(promiseMethodPartialOverloadMethodForPartialInterface);
  (promiseMethodPartialOverloadMethodForPartialInterface)(info);
}

static void staticPromiseMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        staticPromiseMethodPartialOverload1Method(info);
        return;
      }
      break;
    case 1:
      break;
  }

  DCHECK(staticPromiseMethodPartialOverloadMethodForPartialInterface);
  (staticPromiseMethodPartialOverloadMethodForPartialInterface)(info);
}

static void partial2VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        partial2VoidMethod1Method(info);
        return;
      }
      break;
    case 1:
      break;
  }

  DCHECK(partial2VoidMethodMethodForPartialInterface);
  (partial2VoidMethodMethodForPartialInterface)(info);
}

static void partial2StaticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  switch (std::min(1, info.Length())) {
    case 0:
      if (true) {
        partial2StaticVoidMethod1Method(info);
        return;
      }
      break;
    case 1:
      break;
  }

  DCHECK(partial2StaticVoidMethodMethodForPartialInterface);
  (partial2StaticVoidMethodMethodForPartialInterface)(info);
}

static void keysMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "keys");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->keysForBinding(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void valuesMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "values");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->valuesForBinding(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void forEachMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "forEach");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  ScriptValue callback;
  ScriptValue thisArg;
  if (info[0]->IsFunction()) {
    callback = ScriptValue(ScriptState::Current(info.GetIsolate()), info[0]);
  } else {
    exceptionState.ThrowTypeError("The callback provided as parameter 1 is not a function.");
    return;
  }

  thisArg = ScriptValue(ScriptState::Current(info.GetIsolate()), info[1]);

  impl->forEachForBinding(scriptState, ScriptValue(scriptState, info.Holder()), callback, thisArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
}

static void toJSONMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  ScriptValue result = impl->toJSONForBinding(scriptState);
  V8SetReturnValue(info, result.V8Value());
}

static void toStringMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->toString(), info.GetIsolate());
}

static void iteratorMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface", "iterator");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  ScriptState* scriptState = ScriptState::ForRelevantRealm(info);

  Iterator* result = impl->GetIterator(scriptState, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  V8SetReturnValue(info, result);
}

static void namedPropertyGetter(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());
  String result = impl->AnonymousNamedGetter(name);
  if (result.IsNull())
    return;
  V8SetReturnValueString(info, result, info.GetIsolate());
}

static void namedPropertySetter(const AtomicString& name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());
  V8StringResource<> propertyValue = v8Value;
  if (!propertyValue.Prepare())
    return;

  bool result = impl->AnonymousNamedSetter(name, propertyValue);
  if (!result)
    return;
  V8SetReturnValue(info, v8Value);
}

static void namedPropertyDeleter(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  DeleteResult result = impl->AnonymousNamedDeleter(name);
  if (result == kDeleteUnknownProperty)
    return;
  V8SetReturnValue(info, result == kDeleteSuccess);
}

static void namedPropertyQuery(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  const CString& nameInUtf8 = name.Utf8();
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestInterface", nameInUtf8.data());

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  bool result = impl->NamedPropertyQuery(name, exceptionState);
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

static void namedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kEnumerationContext, "TestInterface");

  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  Vector<String> names;
  impl->NamedPropertyEnumerator(names, exceptionState);
  if (exceptionState.HadException())
    return;
  V8SetReturnValue(info, ToV8(names, info.Holder(), info.GetIsolate()).As<v8::Array>());
}

static void indexedPropertyGetter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
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

static void indexedPropertyDescriptor(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // Steps 1.1 to 1.2.4 are covered here: we rely on indexedPropertyGetter() to
  // call the getter function and check that |index| is a valid property index,
  // in which case it will have set info.GetReturnValue() to something other
  // than undefined.
  V8TestInterface::indexedPropertyGetterCallback(index, info);
  v8::Local<v8::Value> getterValue = info.GetReturnValue().Get();
  if (!getterValue->IsUndefined()) {
    // 1.2.5. Let |desc| be a newly created Property Descriptor with no fields.
    // 1.2.6. Set desc.[[Value]] to the result of converting value to an
    //        ECMAScript value.
    // 1.2.7. If O implements an interface with an indexed property setter,
    //        then set desc.[[Writable]] to true, otherwise set it to false.
    v8::PropertyDescriptor desc(getterValue, true);
    // 1.2.8. Set desc.[[Enumerable]] and desc.[[Configurable]] to true.
    desc.set_enumerable(true);
    desc.set_configurable(true);
    // 1.2.9. Return |desc|.
    V8SetReturnValue(info, desc);
  }
}

static void indexedPropertySetter(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());
  V8StringResource<> propertyValue = v8Value;
  if (!propertyValue.Prepare())
    return;

  bool result = impl->AnonymousIndexedSetter(index, propertyValue);
  if (!result)
    return;
  V8SetReturnValue(info, v8Value);
}

static void indexedPropertyDeleter(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::ToImpl(info.Holder());

  DeleteResult result = impl->AnonymousIndexedDeleter(index);
  if (result == kDeleteUnknownProperty)
    return;
  V8SetReturnValue(info, result == kDeleteSuccess);
}

}  // namespace test_interface_implementation_v8_internal

void V8TestInterface::testInterfaceAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testInterfaceAttribute_Getter");

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kV8TestInterface_TestInterfaceAttribute_AttributeGetter);

  test_interface_implementation_v8_internal::testInterfaceAttributeAttributeGetter(info);
}

void V8TestInterface::testInterfaceAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testInterfaceAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kV8TestInterface_TestInterfaceAttribute_AttributeSetter);

  test_interface_implementation_v8_internal::testInterfaceAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::testInterfaceConstructorAttributeConstructorGetterCallback(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testInterfaceConstructorAttribute_ConstructorGetterCallback");

  V8ConstructorAttributeGetter(property, info, &V8TestInterface::wrapperTypeInfo);
}

void V8TestInterface::TestInterfaceConstructorGetterCallback(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_TestInterface_ConstructorGetterCallback");

  V8ConstructorAttributeGetter(property, info, &V8TestInterface::wrapperTypeInfo);
}

void V8TestInterface::TestInterface2ConstructorGetterCallback(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_TestInterface2_ConstructorGetterCallback");

  V8ConstructorAttributeGetter(property, info, &V8TestInterface2::wrapperTypeInfo);
}

void V8TestInterface::doubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_doubleAttribute_Getter");

  test_interface_implementation_v8_internal::doubleAttributeAttributeGetter(info);
}

void V8TestInterface::doubleAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_doubleAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::doubleAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::floatAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_floatAttribute_Getter");

  test_interface_implementation_v8_internal::floatAttributeAttributeGetter(info);
}

void V8TestInterface::floatAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_floatAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::floatAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::unrestrictedDoubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_unrestrictedDoubleAttribute_Getter");

  test_interface_implementation_v8_internal::unrestrictedDoubleAttributeAttributeGetter(info);
}

void V8TestInterface::unrestrictedDoubleAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_unrestrictedDoubleAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::unrestrictedDoubleAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::unrestrictedFloatAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_unrestrictedFloatAttribute_Getter");

  test_interface_implementation_v8_internal::unrestrictedFloatAttributeAttributeGetter(info);
}

void V8TestInterface::unrestrictedFloatAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_unrestrictedFloatAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::unrestrictedFloatAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::testEnumAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testEnumAttribute_Getter");

  test_interface_implementation_v8_internal::testEnumAttributeAttributeGetter(info);
}

void V8TestInterface::testEnumAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testEnumAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::testEnumAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::testEnumOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testEnumOrNullAttribute_Getter");

  test_interface_implementation_v8_internal::testEnumOrNullAttributeAttributeGetter(info);
}

void V8TestInterface::testEnumOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_testEnumOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::testEnumOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::stringOrDoubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_stringOrDoubleAttribute_Getter");

  test_interface_implementation_v8_internal::stringOrDoubleAttributeAttributeGetter(info);
}

void V8TestInterface::stringOrDoubleAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_stringOrDoubleAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::stringOrDoubleAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::withExtendedAttributeStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_withExtendedAttributeStringAttribute_Getter");

  test_interface_implementation_v8_internal::withExtendedAttributeStringAttributeAttributeGetter(info);
}

void V8TestInterface::withExtendedAttributeStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_withExtendedAttributeStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::withExtendedAttributeStringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::uncapitalAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_uncapitalAttribute_Getter");

  test_interface_implementation_v8_internal::uncapitalAttributeAttributeGetter(info);
}

void V8TestInterface::uncapitalAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_uncapitalAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::uncapitalAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::conditionalLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_conditionalLongAttribute_Getter");

  test_interface_implementation_v8_internal::conditionalLongAttributeAttributeGetter(info);
}

void V8TestInterface::conditionalLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_conditionalLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::conditionalLongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::conditionalReadOnlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_conditionalReadOnlyLongAttribute_Getter");

  test_interface_implementation_v8_internal::conditionalReadOnlyLongAttributeAttributeGetter(info);
}

void V8TestInterface::staticStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticStringAttribute_Getter");

  test_interface_implementation_v8_internal::staticStringAttributeAttributeGetter(info);
}

void V8TestInterface::staticStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::staticStringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::staticReturnDOMWrapperAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticReturnDOMWrapperAttribute_Getter");

  test_interface_implementation_v8_internal::staticReturnDOMWrapperAttributeAttributeGetter(info);
}

void V8TestInterface::staticReturnDOMWrapperAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticReturnDOMWrapperAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::staticReturnDOMWrapperAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::staticReadOnlyStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticReadOnlyStringAttribute_Getter");

  test_interface_implementation_v8_internal::staticReadOnlyStringAttributeAttributeGetter(info);
}

void V8TestInterface::staticReadOnlyReturnDOMWrapperAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticReadOnlyReturnDOMWrapperAttribute_Getter");

  test_interface_implementation_v8_internal::staticReadOnlyReturnDOMWrapperAttributeAttributeGetter(info);
}

void V8TestInterface::staticConditionalReadOnlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticConditionalReadOnlyLongAttribute_Getter");

  test_interface_implementation_v8_internal::staticConditionalReadOnlyLongAttributeAttributeGetter(info);
}

void V8TestInterface::legacyInterfaceTypeCheckingAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_legacyInterfaceTypeCheckingAttribute_Getter");

  test_interface_implementation_v8_internal::legacyInterfaceTypeCheckingAttributeAttributeGetter(info);
}

void V8TestInterface::legacyInterfaceTypeCheckingAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_legacyInterfaceTypeCheckingAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::legacyInterfaceTypeCheckingAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::stringNullAsEmptyAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_stringNullAsEmptyAttribute_Getter");

  test_interface_implementation_v8_internal::stringNullAsEmptyAttributeAttributeGetter(info);
}

void V8TestInterface::stringNullAsEmptyAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_stringNullAsEmptyAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::stringNullAsEmptyAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::usvStringOrNullAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_usvStringOrNullAttribute_Getter");

  test_interface_implementation_v8_internal::usvStringOrNullAttributeAttributeGetter(info);
}

void V8TestInterface::usvStringOrNullAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_usvStringOrNullAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::usvStringOrNullAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::alwaysExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_alwaysExposedAttribute_Getter");

  test_interface_implementation_v8_internal::alwaysExposedAttributeAttributeGetter(info);
}

void V8TestInterface::alwaysExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_alwaysExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::alwaysExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::workerExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_workerExposedAttribute_Getter");

  test_interface_implementation_v8_internal::workerExposedAttributeAttributeGetter(info);
}

void V8TestInterface::workerExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_workerExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::workerExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::windowExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_windowExposedAttribute_Getter");

  test_interface_implementation_v8_internal::windowExposedAttributeAttributeGetter(info);
}

void V8TestInterface::windowExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_windowExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::windowExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::lenientThisAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_lenientThisAttribute_Getter");

  test_interface_implementation_v8_internal::lenientThisAttributeAttributeGetter(info);
}

void V8TestInterface::lenientThisAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_lenientThisAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::lenientThisAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::attributeWithSideEffectFreeGetterAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_attributeWithSideEffectFreeGetter_Getter");

  test_interface_implementation_v8_internal::attributeWithSideEffectFreeGetterAttributeGetter(info);
}

void V8TestInterface::attributeWithSideEffectFreeGetterAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_attributeWithSideEffectFreeGetter_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::attributeWithSideEffectFreeGetterAttributeSetter(v8Value, info);
}

void V8TestInterface::secureContextAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextAttribute_Getter");

  test_interface_implementation_v8_internal::secureContextAttributeAttributeGetter(info);
}

void V8TestInterface::secureContextAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::secureContextAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::secureContextRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::secureContextRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::secureContextRuntimeEnabledAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::secureContextRuntimeEnabledAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::secureContextnessRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextnessRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::secureContextnessRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::secureContextnessRuntimeEnabledAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextnessRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::secureContextnessRuntimeEnabledAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::secureContextWindowExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWindowExposedAttribute_Getter");

  test_interface_implementation_v8_internal::secureContextWindowExposedAttributeAttributeGetter(info);
}

void V8TestInterface::secureContextWindowExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWindowExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::secureContextWindowExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::secureContextWorkerExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWorkerExposedAttribute_Getter");

  test_interface_implementation_v8_internal::secureContextWorkerExposedAttributeAttributeGetter(info);
}

void V8TestInterface::secureContextWorkerExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWorkerExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::secureContextWorkerExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::secureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWindowExposedRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::secureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::secureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWindowExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::secureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::secureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWorkerExposedRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::secureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::secureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWorkerExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::secureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::implementsStaticReadOnlyLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsStaticReadOnlyLongAttribute_Getter");

  test_interface_implementation_v8_internal::implementsStaticReadOnlyLongAttributeAttributeGetter(info);
}

void V8TestInterface::implementsStaticStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsStaticStringAttribute_Getter");

  test_interface_implementation_v8_internal::implementsStaticStringAttributeAttributeGetter(info);
}

void V8TestInterface::implementsStaticStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsStaticStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::implementsStaticStringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::implementsReadonlyStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsReadonlyStringAttribute_Getter");

  test_interface_implementation_v8_internal::implementsReadonlyStringAttributeAttributeGetter(info);
}

void V8TestInterface::implementsStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsStringAttribute_Getter");

  test_interface_implementation_v8_internal::implementsStringAttributeAttributeGetter(info);
}

void V8TestInterface::implementsStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::implementsStringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::implementsNodeAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsNodeAttribute_Getter");

  test_interface_implementation_v8_internal::implementsNodeAttributeAttributeGetter(info);
}

void V8TestInterface::implementsNodeAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsNodeAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::implementsNodeAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::implementsEventHandlerAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsEventHandlerAttribute_Getter");

  test_interface_implementation_v8_internal::implementsEventHandlerAttributeAttributeGetter(info);
}

void V8TestInterface::implementsEventHandlerAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsEventHandlerAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::implementsEventHandlerAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::implementsRuntimeEnabledNodeAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsRuntimeEnabledNodeAttribute_Getter");

  test_interface_implementation_v8_internal::implementsRuntimeEnabledNodeAttributeAttributeGetter(info);
}

void V8TestInterface::implementsRuntimeEnabledNodeAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsRuntimeEnabledNodeAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::implementsRuntimeEnabledNodeAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::implements2StaticStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implements2StaticStringAttribute_Getter");

  test_interface_implementation_v8_internal::implements2StaticStringAttributeAttributeGetter(info);
}

void V8TestInterface::implements2StaticStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implements2StaticStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::implements2StaticStringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::implements2StringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implements2StringAttribute_Getter");

  test_interface_implementation_v8_internal::implements2StringAttributeAttributeGetter(info);
}

void V8TestInterface::implements2StringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implements2StringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::implements2StringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::implements3StringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implements3StringAttribute_Getter");

  test_interface_implementation_v8_internal::implements3StringAttributeAttributeGetter(info);
}

void V8TestInterface::implements3StringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implements3StringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::implements3StringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::implements3StaticStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implements3StaticStringAttribute_Getter");

  test_interface_implementation_v8_internal::implements3StaticStringAttributeAttributeGetter(info);
}

void V8TestInterface::implements3StaticStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implements3StaticStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::implements3StaticStringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partialLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialLongAttribute_Getter");

  test_interface_implementation_v8_internal::partialLongAttributeAttributeGetter(info);
}

void V8TestInterface::partialLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partialLongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partialStaticLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialStaticLongAttribute_Getter");

  test_interface_implementation_v8_internal::partialStaticLongAttributeAttributeGetter(info);
}

void V8TestInterface::partialStaticLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialStaticLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partialStaticLongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partialCallWithExecutionContextLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialCallWithExecutionContextLongAttribute_Getter");

  test_interface_implementation_v8_internal::partialCallWithExecutionContextLongAttributeAttributeGetter(info);
}

void V8TestInterface::partialCallWithExecutionContextLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialCallWithExecutionContextLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partialCallWithExecutionContextLongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partialPartialEnumTypeAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialPartialEnumTypeAttribute_Getter");

  test_interface_implementation_v8_internal::partialPartialEnumTypeAttributeAttributeGetter(info);
}

void V8TestInterface::partialPartialEnumTypeAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialPartialEnumTypeAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partialPartialEnumTypeAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partialSecureContextLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextLongAttribute_Getter");

  test_interface_implementation_v8_internal::partialSecureContextLongAttributeAttributeGetter(info);
}

void V8TestInterface::partialSecureContextLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partialSecureContextLongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partial2LongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2LongAttribute_Getter");

  test_interface_implementation_v8_internal::partial2LongAttributeAttributeGetter(info);
}

void V8TestInterface::partial2LongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2LongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partial2LongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partial2StaticLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2StaticLongAttribute_Getter");

  test_interface_implementation_v8_internal::partial2StaticLongAttributeAttributeGetter(info);
}

void V8TestInterface::partial2StaticLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2StaticLongAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partial2StaticLongAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partial2SecureContextAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2SecureContextAttribute_Getter");

  test_interface_implementation_v8_internal::partial2SecureContextAttributeAttributeGetter(info);
}

void V8TestInterface::partial2SecureContextAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2SecureContextAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partial2SecureContextAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partialSecureContextAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextAttribute_Getter");

  test_interface_implementation_v8_internal::partialSecureContextAttributeAttributeGetter(info);
}

void V8TestInterface::partialSecureContextAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partialSecureContextAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partialSecureContextRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::partialSecureContextRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::partialSecureContextRuntimeEnabledAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partialSecureContextRuntimeEnabledAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partialSecureContextWindowExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWindowExposedAttribute_Getter");

  test_interface_implementation_v8_internal::partialSecureContextWindowExposedAttributeAttributeGetter(info);
}

void V8TestInterface::partialSecureContextWindowExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWindowExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partialSecureContextWindowExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partialSecureContextWorkerExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWorkerExposedAttribute_Getter");

  test_interface_implementation_v8_internal::partialSecureContextWorkerExposedAttributeAttributeGetter(info);
}

void V8TestInterface::partialSecureContextWorkerExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWorkerExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partialSecureContextWorkerExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partialSecureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWindowExposedRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::partialSecureContextWindowExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::partialSecureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWindowExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partialSecureContextWindowExposedRuntimeEnabledAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::partialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWorkerExposedRuntimeEnabledAttribute_Getter");

  test_interface_implementation_v8_internal::partialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetter(info);
}

void V8TestInterface::partialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWorkerExposedRuntimeEnabledAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_implementation_v8_internal::partialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface::voidMethodTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethodTestInterfaceEmptyArg");

  test_interface_implementation_v8_internal::voidMethodTestInterfaceEmptyArgMethod(info);
}

void V8TestInterface::voidMethodDoubleArgFloatArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethodDoubleArgFloatArg");

  test_interface_implementation_v8_internal::voidMethodDoubleArgFloatArgMethod(info);
}

void V8TestInterface::voidMethodNullableAndOptionalObjectArgsMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethodNullableAndOptionalObjectArgs");

  test_interface_implementation_v8_internal::voidMethodNullableAndOptionalObjectArgsMethod(info);
}

void V8TestInterface::voidMethodUnrestrictedDoubleArgUnrestrictedFloatArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethodUnrestrictedDoubleArgUnrestrictedFloatArg");

  test_interface_implementation_v8_internal::voidMethodUnrestrictedDoubleArgUnrestrictedFloatArgMethod(info);
}

void V8TestInterface::voidMethodTestEnumArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethodTestEnumArg");

  test_interface_implementation_v8_internal::voidMethodTestEnumArgMethod(info);
}

void V8TestInterface::voidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethod");

  test_interface_implementation_v8_internal::voidMethodMethod(info);
}

void V8TestInterface::voidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethod");

  test_interface_implementation_v8_internal::voidMethodMethodForMainWorld(info);
}

void V8TestInterface::alwaysExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_alwaysExposedMethod");

  test_interface_implementation_v8_internal::alwaysExposedMethodMethod(info);
}

void V8TestInterface::workerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_workerExposedMethod");

  test_interface_implementation_v8_internal::workerExposedMethodMethod(info);
}

void V8TestInterface::windowExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_windowExposedMethod");

  test_interface_implementation_v8_internal::windowExposedMethodMethod(info);
}

void V8TestInterface::alwaysExposedStaticMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_alwaysExposedStaticMethod");

  test_interface_implementation_v8_internal::alwaysExposedStaticMethodMethod(info);
}

void V8TestInterface::workerExposedStaticMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_workerExposedStaticMethod");

  test_interface_implementation_v8_internal::workerExposedStaticMethodMethod(info);
}

void V8TestInterface::windowExposedStaticMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_windowExposedStaticMethod");

  test_interface_implementation_v8_internal::windowExposedStaticMethodMethod(info);
}

void V8TestInterface::staticReturnDOMWrapperMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticReturnDOMWrapperMethod");

  test_interface_implementation_v8_internal::staticReturnDOMWrapperMethodMethod(info);
}

void V8TestInterface::methodWithExposedAndRuntimeEnabledFlagMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_methodWithExposedAndRuntimeEnabledFlag");

  test_interface_implementation_v8_internal::methodWithExposedAndRuntimeEnabledFlagMethod(info);
}

void V8TestInterface::overloadMethodWithExposedAndRuntimeEnabledFlagMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_overloadMethodWithExposedAndRuntimeEnabledFlag");

  test_interface_implementation_v8_internal::overloadMethodWithExposedAndRuntimeEnabledFlagMethod(info);
}

void V8TestInterface::methodWithExposedHavingRuntimeEnabldFlagMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_methodWithExposedHavingRuntimeEnabldFlag");

  test_interface_implementation_v8_internal::methodWithExposedHavingRuntimeEnabldFlagMethod(info);
}

void V8TestInterface::windowAndServiceWorkerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_windowAndServiceWorkerExposedMethod");

  test_interface_implementation_v8_internal::windowAndServiceWorkerExposedMethodMethod(info);
}

void V8TestInterface::overloadMethodWithUnionTypeWithStringMemberMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_overloadMethodWithUnionTypeWithStringMember");

  test_interface_implementation_v8_internal::overloadMethodWithUnionTypeWithStringMemberMethod(info);
}

void V8TestInterface::legacyInterfaceTypeCheckingMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_legacyInterfaceTypeCheckingMethod");

  test_interface_implementation_v8_internal::legacyInterfaceTypeCheckingMethodMethod(info);
}

void V8TestInterface::sideEffectFreeMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_sideEffectFreeMethod");

  test_interface_implementation_v8_internal::sideEffectFreeMethodMethod(info);
}

void V8TestInterface::secureContextMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextMethod");

  test_interface_implementation_v8_internal::secureContextMethodMethod(info);
}

void V8TestInterface::secureContextRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::secureContextRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::secureContextnessRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextnessRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::secureContextnessRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::secureContextWindowExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWindowExposedMethod");

  test_interface_implementation_v8_internal::secureContextWindowExposedMethodMethod(info);
}

void V8TestInterface::secureContextWorkerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWorkerExposedMethod");

  test_interface_implementation_v8_internal::secureContextWorkerExposedMethodMethod(info);
}

void V8TestInterface::secureContextWindowExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWindowExposedRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::secureContextWindowExposedRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::secureContextWorkerExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_secureContextWorkerExposedRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::secureContextWorkerExposedRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::methodWithNullableSequencesMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_methodWithNullableSequences");

  test_interface_implementation_v8_internal::methodWithNullableSequencesMethod(info);
}

void V8TestInterface::methodWithNullableRecordsMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_methodWithNullableRecords");

  test_interface_implementation_v8_internal::methodWithNullableRecordsMethod(info);
}

void V8TestInterface::implementsVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsVoidMethod");

  test_interface_implementation_v8_internal::implementsVoidMethodMethod(info);
}

void V8TestInterface::implementsComplexMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsComplexMethod");

  test_interface_implementation_v8_internal::implementsComplexMethodMethod(info);
}

void V8TestInterface::implementsCustomVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsCustomVoidMethod");

  V8TestInterface::implementsCustomVoidMethodMethodCustom(info);
}

void V8TestInterface::implementsStaticVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implementsStaticVoidMethod");

  test_interface_implementation_v8_internal::implementsStaticVoidMethodMethod(info);
}

void V8TestInterface::implements2VoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implements2VoidMethod");

  test_interface_implementation_v8_internal::implements2VoidMethodMethod(info);
}

void V8TestInterface::implements3VoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implements3VoidMethod");

  test_interface_implementation_v8_internal::implements3VoidMethodMethod(info);
}

void V8TestInterface::implements3StaticVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_implements3StaticVoidMethod");

  test_interface_implementation_v8_internal::implements3StaticVoidMethodMethod(info);
}

void V8TestInterface::partialVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialVoidMethod");

  test_interface_implementation_v8_internal::partialVoidMethodMethod(info);
}

void V8TestInterface::partialStaticVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialStaticVoidMethod");

  test_interface_implementation_v8_internal::partialStaticVoidMethodMethod(info);
}

void V8TestInterface::partialVoidMethodLongArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialVoidMethodLongArg");

  test_interface_implementation_v8_internal::partialVoidMethodLongArgMethod(info);
}

void V8TestInterface::partialCallWithExecutionContextRaisesExceptionVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialCallWithExecutionContextRaisesExceptionVoidMethod");

  test_interface_implementation_v8_internal::partialCallWithExecutionContextRaisesExceptionVoidMethodMethod(info);
}

void V8TestInterface::partialVoidMethodPartialCallbackTypeArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialVoidMethodPartialCallbackTypeArg");

  test_interface_implementation_v8_internal::partialVoidMethodPartialCallbackTypeArgMethod(info);
}

void V8TestInterface::partial2SecureContextMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2SecureContextMethod");

  test_interface_implementation_v8_internal::partial2SecureContextMethodMethod(info);
}

void V8TestInterface::partialSecureContextMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextMethod");

  test_interface_implementation_v8_internal::partialSecureContextMethodMethod(info);
}

void V8TestInterface::partialSecureContextRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::partialSecureContextRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::partialSecureContextWindowExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWindowExposedMethod");

  test_interface_implementation_v8_internal::partialSecureContextWindowExposedMethodMethod(info);
}

void V8TestInterface::partialSecureContextWorkerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWorkerExposedMethod");

  test_interface_implementation_v8_internal::partialSecureContextWorkerExposedMethodMethod(info);
}

void V8TestInterface::partialSecureContextWindowExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWindowExposedRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::partialSecureContextWindowExposedRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::partialSecureContextWorkerExposedRuntimeEnabledMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partialSecureContextWorkerExposedRuntimeEnabledMethod");

  test_interface_implementation_v8_internal::partialSecureContextWorkerExposedRuntimeEnabledMethodMethod(info);
}

void V8TestInterface::voidMethodPartialOverloadMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_voidMethodPartialOverload");

  test_interface_implementation_v8_internal::voidMethodPartialOverloadMethod(info);
}

void V8TestInterface::staticVoidMethodPartialOverloadMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticVoidMethodPartialOverload");

  test_interface_implementation_v8_internal::staticVoidMethodPartialOverloadMethod(info);
}

void V8TestInterface::promiseMethodPartialOverloadMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_promiseMethodPartialOverload");

  test_interface_implementation_v8_internal::promiseMethodPartialOverloadMethod(info);
}

void V8TestInterface::staticPromiseMethodPartialOverloadMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_staticPromiseMethodPartialOverload");

  test_interface_implementation_v8_internal::staticPromiseMethodPartialOverloadMethod(info);
}

void V8TestInterface::partial2VoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2VoidMethod");

  test_interface_implementation_v8_internal::partial2VoidMethodMethod(info);
}

void V8TestInterface::partial2StaticVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_partial2StaticVoidMethod");

  test_interface_implementation_v8_internal::partial2StaticVoidMethodMethod(info);
}

void V8TestInterface::keysMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_keys");

  test_interface_implementation_v8_internal::keysMethod(info);
}

void V8TestInterface::valuesMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_values");

  test_interface_implementation_v8_internal::valuesMethod(info);
}

void V8TestInterface::forEachMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_forEach");

  test_interface_implementation_v8_internal::forEachMethod(info);
}

void V8TestInterface::toJSONMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_toJSON");

  test_interface_implementation_v8_internal::toJSONMethod(info);
}

void V8TestInterface::toStringMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_toString");

  test_interface_implementation_v8_internal::toStringMethod(info);
}

void V8TestInterface::iteratorMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_iterator");

  test_interface_implementation_v8_internal::iteratorMethod(info);
}

void V8TestInterface::namedPropertyGetterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_NamedPropertyGetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_interface_implementation_v8_internal::namedPropertyGetter(propertyName, info);
}

void V8TestInterface::namedPropertySetterCallback(v8::Local<v8::Name> name, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_NamedPropertySetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_interface_implementation_v8_internal::namedPropertySetter(propertyName, v8Value, info);
}

void V8TestInterface::namedPropertyDeleterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_interface_implementation_v8_internal::namedPropertyDeleter(propertyName, info);
}

void V8TestInterface::namedPropertyQueryCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_NamedPropertyQuery");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_interface_implementation_v8_internal::namedPropertyQuery(propertyName, info);
}

void V8TestInterface::namedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info) {
  test_interface_implementation_v8_internal::namedPropertyEnumerator(info);
}

void V8TestInterface::indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceImplementation_IndexedPropertyGetter");

  test_interface_implementation_v8_internal::indexedPropertyGetter(index, info);
}

void V8TestInterface::indexedPropertyDescriptorCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_interface_implementation_v8_internal::indexedPropertyDescriptor(index, info);
}

void V8TestInterface::indexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_interface_implementation_v8_internal::indexedPropertySetter(index, v8Value, info);
}

void V8TestInterface::indexedPropertyDeleterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  test_interface_implementation_v8_internal::indexedPropertyDeleter(index, info);
}

void V8TestInterface::indexedPropertyDefinerCallback(
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
      ExceptionState exceptionState(info.GetIsolate(),
                                    ExceptionState::kIndexedSetterContext,
                                    "TestInterface");
      exceptionState.ThrowTypeError("Accessor properties are not allowed.");
    }
    return;
  }

  // Return nothing and fall back to indexedPropertySetterCallback.
}

// Suppress warning: global constructors, because AttributeConfiguration is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
static const V8DOMConfiguration::AttributeConfiguration V8TestInterfaceAttributes[] = {
    { "testInterfaceConstructorAttribute", V8TestInterface::testInterfaceConstructorAttributeConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kReplaceWithDataProperty, V8DOMConfiguration::kAllWorlds },
    { "TestInterface", V8TestInterface::TestInterfaceConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kReplaceWithDataProperty, V8DOMConfiguration::kAllWorlds },
    { "TestInterface2", V8TestInterface::TestInterface2ConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kReplaceWithDataProperty, V8DOMConfiguration::kAllWorlds },
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

static const V8DOMConfiguration::AccessorConfiguration V8TestInterfaceAccessors[] = {
    { "testInterfaceAttribute", V8TestInterface::testInterfaceAttributeAttributeGetterCallback, V8TestInterface::testInterfaceAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "doubleAttribute", V8TestInterface::doubleAttributeAttributeGetterCallback, V8TestInterface::doubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "floatAttribute", V8TestInterface::floatAttributeAttributeGetterCallback, V8TestInterface::floatAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unrestrictedDoubleAttribute", V8TestInterface::unrestrictedDoubleAttributeAttributeGetterCallback, V8TestInterface::unrestrictedDoubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unrestrictedFloatAttribute", V8TestInterface::unrestrictedFloatAttributeAttributeGetterCallback, V8TestInterface::unrestrictedFloatAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testEnumAttribute", V8TestInterface::testEnumAttributeAttributeGetterCallback, V8TestInterface::testEnumAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "testEnumOrNullAttribute", V8TestInterface::testEnumOrNullAttributeAttributeGetterCallback, V8TestInterface::testEnumOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "stringOrDoubleAttribute", V8TestInterface::stringOrDoubleAttributeAttributeGetterCallback, V8TestInterface::stringOrDoubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "withExtendedAttributeStringAttribute", V8TestInterface::withExtendedAttributeStringAttributeAttributeGetterCallback, V8TestInterface::withExtendedAttributeStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "uncapitalAttribute", V8TestInterface::uncapitalAttributeAttributeGetterCallback, V8TestInterface::uncapitalAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "staticStringAttribute", V8TestInterface::staticStringAttributeAttributeGetterCallback, V8TestInterface::staticStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "staticReturnDOMWrapperAttribute", V8TestInterface::staticReturnDOMWrapperAttributeAttributeGetterCallback, V8TestInterface::staticReturnDOMWrapperAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "staticReadOnlyStringAttribute", V8TestInterface::staticReadOnlyStringAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "staticReadOnlyReturnDOMWrapperAttribute", V8TestInterface::staticReadOnlyReturnDOMWrapperAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "legacyInterfaceTypeCheckingAttribute", V8TestInterface::legacyInterfaceTypeCheckingAttributeAttributeGetterCallback, V8TestInterface::legacyInterfaceTypeCheckingAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "stringNullAsEmptyAttribute", V8TestInterface::stringNullAsEmptyAttributeAttributeGetterCallback, V8TestInterface::stringNullAsEmptyAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "usvStringOrNullAttribute", V8TestInterface::usvStringOrNullAttributeAttributeGetterCallback, V8TestInterface::usvStringOrNullAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "alwaysExposedAttribute", V8TestInterface::alwaysExposedAttributeAttributeGetterCallback, V8TestInterface::alwaysExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "lenientThisAttribute", V8TestInterface::lenientThisAttributeAttributeGetterCallback, V8TestInterface::lenientThisAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "attributeWithSideEffectFreeGetter", V8TestInterface::attributeWithSideEffectFreeGetterAttributeGetterCallback, V8TestInterface::attributeWithSideEffectFreeGetterAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasNoSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "implementsStaticReadOnlyLongAttribute", V8TestInterface::implementsStaticReadOnlyLongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "implementsStaticStringAttribute", V8TestInterface::implementsStaticStringAttributeAttributeGetterCallback, V8TestInterface::implementsStaticStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "implementsReadonlyStringAttribute", V8TestInterface::implementsReadonlyStringAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "implementsStringAttribute", V8TestInterface::implementsStringAttributeAttributeGetterCallback, V8TestInterface::implementsStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "implementsNodeAttribute", V8TestInterface::implementsNodeAttributeAttributeGetterCallback, V8TestInterface::implementsNodeAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "implementsEventHandlerAttribute", V8TestInterface::implementsEventHandlerAttributeAttributeGetterCallback, V8TestInterface::implementsEventHandlerAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "implements3StringAttribute", V8TestInterface::implements3StringAttributeAttributeGetterCallback, V8TestInterface::implements3StringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "implements3StaticStringAttribute", V8TestInterface::implements3StaticStringAttributeAttributeGetterCallback, V8TestInterface::implements3StaticStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "partial2LongAttribute", V8TestInterface::partial2LongAttributeAttributeGetterCallback, V8TestInterface::partial2LongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "partial2StaticLongAttribute", V8TestInterface::partial2StaticLongAttributeAttributeGetterCallback, V8TestInterface::partial2StaticLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
};

static const V8DOMConfiguration::MethodConfiguration V8TestInterfaceMethods[] = {
    {"voidMethodTestInterfaceEmptyArg", V8TestInterface::voidMethodTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleArgFloatArg", V8TestInterface::voidMethodDoubleArgFloatArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodNullableAndOptionalObjectArgs", V8TestInterface::voidMethodNullableAndOptionalObjectArgsMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUnrestrictedDoubleArgUnrestrictedFloatArg", V8TestInterface::voidMethodUnrestrictedDoubleArgUnrestrictedFloatArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodTestEnumArg", V8TestInterface::voidMethodTestEnumArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethod", V8TestInterface::voidMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"voidMethod", V8TestInterface::voidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"alwaysExposedMethod", V8TestInterface::alwaysExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"alwaysExposedStaticMethod", V8TestInterface::alwaysExposedStaticMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"staticReturnDOMWrapperMethod", V8TestInterface::staticReturnDOMWrapperMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"overloadMethodWithUnionTypeWithStringMember", V8TestInterface::overloadMethodWithUnionTypeWithStringMemberMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"legacyInterfaceTypeCheckingMethod", V8TestInterface::legacyInterfaceTypeCheckingMethodMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"sideEffectFreeMethod", V8TestInterface::sideEffectFreeMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasNoSideEffect, V8DOMConfiguration::kAllWorlds},
    {"methodWithNullableSequences", V8TestInterface::methodWithNullableSequencesMethodCallback, 4, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"methodWithNullableRecords", V8TestInterface::methodWithNullableRecordsMethodCallback, 4, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"implementsVoidMethod", V8TestInterface::implementsVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"implementsComplexMethod", V8TestInterface::implementsComplexMethodMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"implementsCustomVoidMethod", V8TestInterface::implementsCustomVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"implementsStaticVoidMethod", V8TestInterface::implementsStaticVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"implements3VoidMethod", V8TestInterface::implements3VoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"implements3StaticVoidMethod", V8TestInterface::implements3StaticVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodPartialOverload", V8TestInterface::voidMethodPartialOverloadMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"staticVoidMethodPartialOverload", V8TestInterface::staticVoidMethodPartialOverloadMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"promiseMethodPartialOverload", V8TestInterface::promiseMethodPartialOverloadMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"staticPromiseMethodPartialOverload", V8TestInterface::staticPromiseMethodPartialOverloadMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kDoNotCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"partial2VoidMethod", V8TestInterface::partial2VoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"partial2StaticVoidMethod", V8TestInterface::partial2StaticVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"keys", V8TestInterface::keysMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"values", V8TestInterface::valuesMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"forEach", V8TestInterface::forEachMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"toJSON", V8TestInterface::toJSONMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"toString", V8TestInterface::toStringMethodCallback, 0, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

void V8TestInterface::installV8TestInterfaceTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterface::wrapperTypeInfo.interface_name, V8TestInterfaceEmpty::domTemplate(isolate, world), V8TestInterface::internalFieldCount);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  static constexpr V8DOMConfiguration::ConstantConfiguration V8TestInterfaceConstants[] = {
      {"UNSIGNED_LONG", V8DOMConfiguration::kConstantTypeUnsignedLong, static_cast<int>(0)},
      {"CONST_JAVASCRIPT", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(1)},
      {"IMPLEMENTS_CONSTANT_1", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(1)},
      {"IMPLEMENTS_CONSTANT_2", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(2)},
      {"PARTIAL2_UNSIGNED_SHORT", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(0)},
  };
  V8DOMConfiguration::InstallConstants(
      isolate, interfaceTemplate, prototypeTemplate,
      V8TestInterfaceConstants, base::size(V8TestInterfaceConstants));
  V8DOMConfiguration::InstallAttributes(
      isolate, world, instanceTemplate, prototypeTemplate,
      V8TestInterfaceAttributes, base::size(V8TestInterfaceAttributes));
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterfaceAccessors, base::size(V8TestInterfaceAccessors));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterfaceMethods, base::size(V8TestInterfaceMethods));

  // Indexed properties
  v8::IndexedPropertyHandlerConfiguration indexedPropertyHandlerConfig(
      V8TestInterface::indexedPropertyGetterCallback,
      V8TestInterface::indexedPropertySetterCallback,
      V8TestInterface::indexedPropertyDescriptorCallback,
      V8TestInterface::indexedPropertyDeleterCallback,
      IndexedPropertyEnumerator<TestInterfaceImplementation>,
      V8TestInterface::indexedPropertyDefinerCallback,
      v8::Local<v8::Value>(),
      v8::PropertyHandlerFlags::kNone);
  instanceTemplate->SetHandler(indexedPropertyHandlerConfig);
  // Named properties
  v8::NamedPropertyHandlerConfiguration namedPropertyHandlerConfig(V8TestInterface::namedPropertyGetterCallback, V8TestInterface::namedPropertySetterCallback, V8TestInterface::namedPropertyQueryCallback, V8TestInterface::namedPropertyDeleterCallback, V8TestInterface::namedPropertyEnumeratorCallback, v8::Local<v8::Value>(), static_cast<v8::PropertyHandlerFlags>(int(v8::PropertyHandlerFlags::kOnlyInterceptStrings) | int(v8::PropertyHandlerFlags::kNonMasking)));
  instanceTemplate->SetHandler(namedPropertyHandlerConfig);

  // Iterator (@@iterator)
  static const V8DOMConfiguration::SymbolKeyedMethodConfiguration
  symbolKeyedIteratorConfiguration = {
      v8::Symbol::GetIterator,
      "entries",
      V8TestInterface::iteratorMethodCallback,
      0,
      v8::DontEnum,
      V8DOMConfiguration::kOnPrototype,
      V8DOMConfiguration::kCheckHolder,
      V8DOMConfiguration::kDoNotCheckAccess,
      V8DOMConfiguration::kHasSideEffect
  };
  V8DOMConfiguration::InstallMethod(isolate, world, prototypeTemplate, signature, symbolKeyedIteratorConfiguration);

  instanceTemplate->SetCallAsFunctionHandler(V8TestInterface::legacyCallCustom);

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
  if (RuntimeEnabledFeatures::PartialFeatureNameEnabled()) {
    static const V8DOMConfiguration::ConstantConfiguration constant_configurations[] = {
        {"PARTIAL_UNSIGNED_SHORT", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(0)},
        {"PARTIAL_DOUBLE", V8DOMConfiguration::kConstantTypeDouble, static_cast<double>(3.14)},
    };
    V8DOMConfiguration::InstallConstants(
        isolate, interface_template, prototype_template,
        constant_configurations, base::size(constant_configurations));
  }

  if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
    static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
        { "conditionalReadOnlyLongAttribute", V8TestInterface::conditionalReadOnlyLongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "staticConditionalReadOnlyLongAttribute", V8TestInterface::staticConditionalReadOnlyLongAttributeAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "conditionalLongAttribute", V8TestInterface::conditionalLongAttributeAttributeGetterCallback, V8TestInterface::conditionalLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    };
    V8DOMConfiguration::InstallAccessors(
        isolate, world, instance_template, prototype_template, interface_template,
        signature, accessor_configurations,
        base::size(accessor_configurations));
  }
  if (RuntimeEnabledFeatures::Implements2FeatureNameEnabled()) {
    static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
        { "implements2StaticStringAttribute", V8TestInterface::implements2StaticStringAttributeAttributeGetterCallback, V8TestInterface::implements2StaticStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "implements2StringAttribute", V8TestInterface::implements2StringAttributeAttributeGetterCallback, V8TestInterface::implements2StringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    };
    V8DOMConfiguration::InstallAccessors(
        isolate, world, instance_template, prototype_template, interface_template,
        signature, accessor_configurations,
        base::size(accessor_configurations));
  }
  if (RuntimeEnabledFeatures::ImplementsFeatureNameEnabled()) {
    static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
        { "implementsRuntimeEnabledNodeAttribute", V8TestInterface::implementsRuntimeEnabledNodeAttributeAttributeGetterCallback, V8TestInterface::implementsRuntimeEnabledNodeAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    };
    V8DOMConfiguration::InstallAccessors(
        isolate, world, instance_template, prototype_template, interface_template,
        signature, accessor_configurations,
        base::size(accessor_configurations));
  }
  if (RuntimeEnabledFeatures::PartialFeatureNameEnabled()) {
    static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
        { "partialCallWithExecutionContextLongAttribute", V8TestInterface::partialCallWithExecutionContextLongAttributeAttributeGetterCallback, V8TestInterface::partialCallWithExecutionContextLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "partialLongAttribute", V8TestInterface::partialLongAttributeAttributeGetterCallback, V8TestInterface::partialLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "partialPartialEnumTypeAttribute", V8TestInterface::partialPartialEnumTypeAttributeAttributeGetterCallback, V8TestInterface::partialPartialEnumTypeAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        { "partialStaticLongAttribute", V8TestInterface::partialStaticLongAttributeAttributeGetterCallback, V8TestInterface::partialStaticLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    };
    V8DOMConfiguration::InstallAccessors(
        isolate, world, instance_template, prototype_template, interface_template,
        signature, accessor_configurations,
        base::size(accessor_configurations));
  }

  // Custom signature
  if (RuntimeEnabledFeatures::Implements2FeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration implements2VoidMethodMethodConfiguration[] = {
      {"implements2VoidMethod", V8TestInterface::implements2VoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : implements2VoidMethodMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
  if (RuntimeEnabledFeatures::PartialFeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration partialVoidMethodMethodConfiguration[] = {
      {"partialVoidMethod", V8TestInterface::partialVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : partialVoidMethodMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
  if (RuntimeEnabledFeatures::PartialFeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration partialStaticVoidMethodMethodConfiguration[] = {
      {"partialStaticVoidMethod", V8TestInterface::partialStaticVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : partialStaticVoidMethodMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
  if (RuntimeEnabledFeatures::PartialFeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration partialVoidMethodLongArgMethodConfiguration[] = {
      {"partialVoidMethodLongArg", V8TestInterface::partialVoidMethodLongArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : partialVoidMethodLongArgMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
  if (RuntimeEnabledFeatures::PartialFeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration partialCallWithExecutionContextRaisesExceptionVoidMethodMethodConfiguration[] = {
      {"partialCallWithExecutionContextRaisesExceptionVoidMethod", V8TestInterface::partialCallWithExecutionContextRaisesExceptionVoidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : partialCallWithExecutionContextRaisesExceptionVoidMethodMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
  if (RuntimeEnabledFeatures::PartialFeatureNameEnabled()) {
    const V8DOMConfiguration::MethodConfiguration partialVoidMethodPartialCallbackTypeArgMethodConfiguration[] = {
      {"partialVoidMethodPartialCallbackTypeArg", V8TestInterface::partialVoidMethodPartialCallbackTypeArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
    };
    for (const auto& methodConfig : partialVoidMethodPartialCallbackTypeArgMethodConfiguration)
      V8DOMConfiguration::InstallMethod(isolate, world, instance_template, prototype_template, interface_template, signature, methodConfig);
  }
}

v8::Local<v8::FunctionTemplate> V8TestInterface::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), V8TestInterface::installV8TestInterfaceTemplateFunction);
}

bool V8TestInterface::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterface::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceImplementation* V8TestInterface::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceImplementation* NativeValueTraits<TestInterfaceImplementation>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceImplementation* nativeValue = V8TestInterface::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterface"));
  }
  return nativeValue;
}

void V8TestInterface::InstallConditionalFeatures(
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
    if (isSecureContext) {
      static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
          { "partial2SecureContextAttribute", V8TestInterface::partial2SecureContextAttributeAttributeGetterCallback, V8TestInterface::partial2SecureContextAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          { "partialSecureContextAttribute", V8TestInterface::partialSecureContextAttributeAttributeGetterCallback, V8TestInterface::partialSecureContextAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          { "secureContextAttribute", V8TestInterface::secureContextAttributeAttributeGetterCallback, V8TestInterface::secureContextAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      };
      V8DOMConfiguration::InstallAccessors(
          isolate, world, instanceObject, prototypeObject, interfaceObject,
          signature, accessor_configurations,
          base::size(accessor_configurations));
      if (RuntimeEnabledFeatures::PartialFeatureNameEnabled()) {
        static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
            { "partialSecureContextLongAttribute", V8TestInterface::partialSecureContextLongAttributeAttributeGetterCallback, V8TestInterface::partialSecureContextLongAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instanceObject, prototypeObject, interfaceObject,
            signature, accessor_configurations,
            base::size(accessor_configurations));
      }
      if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
        static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
            { "partialSecureContextRuntimeEnabledAttribute", V8TestInterface::partialSecureContextRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::partialSecureContextRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
            { "secureContextRuntimeEnabledAttribute", V8TestInterface::secureContextRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::secureContextRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instanceObject, prototypeObject, interfaceObject,
            signature, accessor_configurations,
            base::size(accessor_configurations));
      }
    }
    if (isSecureContext || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
          { "secureContextnessRuntimeEnabledAttribute", V8TestInterface::secureContextnessRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::secureContextnessRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      };
      V8DOMConfiguration::InstallAccessors(
          isolate, world, instanceObject, prototypeObject, interfaceObject,
          signature, accessor_configurations,
          base::size(accessor_configurations));
    }
    if (executionContext && (executionContext->IsDocument())) {
      static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
          { "windowExposedAttribute", V8TestInterface::windowExposedAttributeAttributeGetterCallback, V8TestInterface::windowExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      };
      V8DOMConfiguration::InstallAccessors(
          isolate, world, instanceObject, prototypeObject, interfaceObject,
          signature, accessor_configurations,
          base::size(accessor_configurations));
      if (isSecureContext) {
        static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
            { "partialSecureContextWindowExposedAttribute", V8TestInterface::partialSecureContextWindowExposedAttributeAttributeGetterCallback, V8TestInterface::partialSecureContextWindowExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
            { "secureContextWindowExposedAttribute", V8TestInterface::secureContextWindowExposedAttributeAttributeGetterCallback, V8TestInterface::secureContextWindowExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instanceObject, prototypeObject, interfaceObject,
            signature, accessor_configurations,
            base::size(accessor_configurations));
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
              { "partialSecureContextWindowExposedRuntimeEnabledAttribute", V8TestInterface::partialSecureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::partialSecureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
              { "secureContextWindowExposedRuntimeEnabledAttribute", V8TestInterface::secureContextWindowExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::secureContextWindowExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          };
          V8DOMConfiguration::InstallAccessors(
              isolate, world, instanceObject, prototypeObject, interfaceObject,
              signature, accessor_configurations,
              base::size(accessor_configurations));
        }
      }
    }
    if (executionContext && (executionContext->IsWorkerGlobalScope())) {
      static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
          { "workerExposedAttribute", V8TestInterface::workerExposedAttributeAttributeGetterCallback, V8TestInterface::workerExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      };
      V8DOMConfiguration::InstallAccessors(
          isolate, world, instanceObject, prototypeObject, interfaceObject,
          signature, accessor_configurations,
          base::size(accessor_configurations));
      if (isSecureContext) {
        static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
            { "partialSecureContextWorkerExposedAttribute", V8TestInterface::partialSecureContextWorkerExposedAttributeAttributeGetterCallback, V8TestInterface::partialSecureContextWorkerExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
            { "secureContextWorkerExposedAttribute", V8TestInterface::secureContextWorkerExposedAttributeAttributeGetterCallback, V8TestInterface::secureContextWorkerExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
        };
        V8DOMConfiguration::InstallAccessors(
            isolate, world, instanceObject, prototypeObject, interfaceObject,
            signature, accessor_configurations,
            base::size(accessor_configurations));
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
              { "partialSecureContextWorkerExposedRuntimeEnabledAttribute", V8TestInterface::partialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::partialSecureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
              { "secureContextWorkerExposedRuntimeEnabledAttribute", V8TestInterface::secureContextWorkerExposedRuntimeEnabledAttributeAttributeGetterCallback, V8TestInterface::secureContextWorkerExposedRuntimeEnabledAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
          };
          V8DOMConfiguration::InstallAccessors(
              isolate, world, instanceObject, prototypeObject, interfaceObject,
              signature, accessor_configurations,
              base::size(accessor_configurations));
        }
      }
    }
    if (executionContext && (executionContext->IsWorkerGlobalScope())) {
      const V8DOMConfiguration::MethodConfiguration workerExposedMethodMethodConfiguration[] = {
        {"workerExposedMethod", V8TestInterface::workerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : workerExposedMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (executionContext && (executionContext->IsDocument())) {
      const V8DOMConfiguration::MethodConfiguration windowExposedMethodMethodConfiguration[] = {
        {"windowExposedMethod", V8TestInterface::windowExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : windowExposedMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (executionContext && (executionContext->IsDocument())) {
      if (RuntimeEnabledFeatures::FeatureNameEnabled()) {
        const V8DOMConfiguration::MethodConfiguration methodWithExposedAndRuntimeEnabledFlagMethodConfiguration[] = {
          {"methodWithExposedAndRuntimeEnabledFlag", V8TestInterface::methodWithExposedAndRuntimeEnabledFlagMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& methodConfig : methodWithExposedAndRuntimeEnabledFlagMethodConfiguration)
          V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
      }
    }
    if (executionContext && (executionContext->IsDocument())) {
      const V8DOMConfiguration::MethodConfiguration overloadMethodWithExposedAndRuntimeEnabledFlagMethodConfiguration[] = {
        {"overloadMethodWithExposedAndRuntimeEnabledFlag", V8TestInterface::overloadMethodWithExposedAndRuntimeEnabledFlagMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : overloadMethodWithExposedAndRuntimeEnabledFlagMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (executionContext && ((executionContext->IsDocument() && RuntimeEnabledFeatures::FeatureNameEnabled()) || (executionContext->IsWorkerGlobalScope() && RuntimeEnabledFeatures::FeatureName2Enabled()))) {
      const V8DOMConfiguration::MethodConfiguration methodWithExposedHavingRuntimeEnabldFlagMethodConfiguration[] = {
        {"methodWithExposedHavingRuntimeEnabldFlag", V8TestInterface::methodWithExposedHavingRuntimeEnabldFlagMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : methodWithExposedHavingRuntimeEnabldFlagMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (executionContext && (executionContext->IsDocument() || executionContext->IsServiceWorkerGlobalScope())) {
      const V8DOMConfiguration::MethodConfiguration windowAndServiceWorkerExposedMethodMethodConfiguration[] = {
        {"windowAndServiceWorkerExposedMethod", V8TestInterface::windowAndServiceWorkerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : windowAndServiceWorkerExposedMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (isSecureContext) {
      const V8DOMConfiguration::MethodConfiguration secureContextMethodMethodConfiguration[] = {
        {"secureContextMethod", V8TestInterface::secureContextMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : secureContextMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (isSecureContext) {
      if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
        const V8DOMConfiguration::MethodConfiguration secureContextRuntimeEnabledMethodMethodConfiguration[] = {
          {"secureContextRuntimeEnabledMethod", V8TestInterface::secureContextRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& methodConfig : secureContextRuntimeEnabledMethodMethodConfiguration)
          V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
      }
    }
    if (isSecureContext || !RuntimeEnabledFeatures::SecureContextnessFeatureEnabled()) {
      const V8DOMConfiguration::MethodConfiguration secureContextnessRuntimeEnabledMethodMethodConfiguration[] = {
        {"secureContextnessRuntimeEnabledMethod", V8TestInterface::secureContextnessRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : secureContextnessRuntimeEnabledMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (isSecureContext) {
      if (executionContext && (executionContext->IsDocument())) {
        const V8DOMConfiguration::MethodConfiguration secureContextWindowExposedMethodMethodConfiguration[] = {
          {"secureContextWindowExposedMethod", V8TestInterface::secureContextWindowExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& methodConfig : secureContextWindowExposedMethodMethodConfiguration)
          V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
      }
    }
    if (isSecureContext) {
      if (executionContext && (executionContext->IsWorkerGlobalScope())) {
        const V8DOMConfiguration::MethodConfiguration secureContextWorkerExposedMethodMethodConfiguration[] = {
          {"secureContextWorkerExposedMethod", V8TestInterface::secureContextWorkerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& methodConfig : secureContextWorkerExposedMethodMethodConfiguration)
          V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
      }
    }
    if (isSecureContext) {
      if (executionContext && (executionContext->IsDocument())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          const V8DOMConfiguration::MethodConfiguration secureContextWindowExposedRuntimeEnabledMethodMethodConfiguration[] = {
            {"secureContextWindowExposedRuntimeEnabledMethod", V8TestInterface::secureContextWindowExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& methodConfig : secureContextWindowExposedRuntimeEnabledMethodMethodConfiguration)
            V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
        }
      }
    }
    if (isSecureContext) {
      if (executionContext && (executionContext->IsWorkerGlobalScope())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          const V8DOMConfiguration::MethodConfiguration secureContextWorkerExposedRuntimeEnabledMethodMethodConfiguration[] = {
            {"secureContextWorkerExposedRuntimeEnabledMethod", V8TestInterface::secureContextWorkerExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& methodConfig : secureContextWorkerExposedRuntimeEnabledMethodMethodConfiguration)
            V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
        }
      }
    }
    if (isSecureContext) {
      const V8DOMConfiguration::MethodConfiguration partial2SecureContextMethodMethodConfiguration[] = {
        {"partial2SecureContextMethod", V8TestInterface::partial2SecureContextMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : partial2SecureContextMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (isSecureContext) {
      const V8DOMConfiguration::MethodConfiguration partialSecureContextMethodMethodConfiguration[] = {
        {"partialSecureContextMethod", V8TestInterface::partialSecureContextMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : partialSecureContextMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (isSecureContext) {
      if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
        const V8DOMConfiguration::MethodConfiguration partialSecureContextRuntimeEnabledMethodMethodConfiguration[] = {
          {"partialSecureContextRuntimeEnabledMethod", V8TestInterface::partialSecureContextRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& methodConfig : partialSecureContextRuntimeEnabledMethodMethodConfiguration)
          V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
      }
    }
    if (isSecureContext) {
      if (executionContext && (executionContext->IsDocument())) {
        const V8DOMConfiguration::MethodConfiguration partialSecureContextWindowExposedMethodMethodConfiguration[] = {
          {"partialSecureContextWindowExposedMethod", V8TestInterface::partialSecureContextWindowExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& methodConfig : partialSecureContextWindowExposedMethodMethodConfiguration)
          V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
      }
    }
    if (isSecureContext) {
      if (executionContext && (executionContext->IsWorkerGlobalScope())) {
        const V8DOMConfiguration::MethodConfiguration partialSecureContextWorkerExposedMethodMethodConfiguration[] = {
          {"partialSecureContextWorkerExposedMethod", V8TestInterface::partialSecureContextWorkerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
        };
        for (const auto& methodConfig : partialSecureContextWorkerExposedMethodMethodConfiguration)
          V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
      }
    }
    if (isSecureContext) {
      if (executionContext && (executionContext->IsDocument())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          const V8DOMConfiguration::MethodConfiguration partialSecureContextWindowExposedRuntimeEnabledMethodMethodConfiguration[] = {
            {"partialSecureContextWindowExposedRuntimeEnabledMethod", V8TestInterface::partialSecureContextWindowExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& methodConfig : partialSecureContextWindowExposedRuntimeEnabledMethodMethodConfiguration)
            V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
        }
      }
    }
    if (isSecureContext) {
      if (executionContext && (executionContext->IsWorkerGlobalScope())) {
        if (RuntimeEnabledFeatures::SecureFeatureEnabled()) {
          const V8DOMConfiguration::MethodConfiguration partialSecureContextWorkerExposedRuntimeEnabledMethodMethodConfiguration[] = {
            {"partialSecureContextWorkerExposedRuntimeEnabledMethod", V8TestInterface::partialSecureContextWorkerExposedRuntimeEnabledMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
          };
          for (const auto& methodConfig : partialSecureContextWorkerExposedRuntimeEnabledMethodMethodConfiguration)
            V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
        }
      }
    }
    if (executionContext && (executionContext->IsWorkerGlobalScope())) {
      const V8DOMConfiguration::MethodConfiguration workerExposedStaticMethodMethodConfiguration[] = {
        {"workerExposedStaticMethod", V8TestInterface::workerExposedStaticMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : workerExposedStaticMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (executionContext && (executionContext->IsDocument())) {
      const V8DOMConfiguration::MethodConfiguration windowExposedStaticMethodMethodConfiguration[] = {
        {"windowExposedStaticMethod", V8TestInterface::windowExposedStaticMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : windowExposedStaticMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
  }
}

InstallRuntimeEnabledFeaturesOnTemplateFunction
V8TestInterface::install_runtime_enabled_features_on_template_function_ =
    &V8TestInterface::InstallRuntimeEnabledFeaturesOnTemplate;

InstallTemplateFunction V8TestInterface::installV8TestInterfaceTemplateFunction =
    &V8TestInterface::installV8TestInterfaceTemplate;

void V8TestInterface::UpdateWrapperTypeInfo(
    InstallTemplateFunction install_template_function,
    InstallRuntimeEnabledFeaturesFunction install_runtime_enabled_features_function,
    InstallRuntimeEnabledFeaturesOnTemplateFunction install_runtime_enabled_features_on_template_function,
    InstallConditionalFeaturesFunction install_conditional_features_function) {
  V8TestInterface::installV8TestInterfaceTemplateFunction =
      install_template_function;

  CHECK(install_runtime_enabled_features_on_template_function);
  V8TestInterface::install_runtime_enabled_features_on_template_function_ =
      install_runtime_enabled_features_on_template_function;

  if (install_conditional_features_function) {
    V8TestInterface::wrapperTypeInfo.install_conditional_features_function =
        install_conditional_features_function;
  }
}

void V8TestInterface::registerVoidMethodPartialOverloadMethodForPartialInterface(void (*method)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  test_interface_implementation_v8_internal::voidMethodPartialOverloadMethodForPartialInterface = method;
}

void V8TestInterface::registerStaticVoidMethodPartialOverloadMethodForPartialInterface(void (*method)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  test_interface_implementation_v8_internal::staticVoidMethodPartialOverloadMethodForPartialInterface = method;
}

void V8TestInterface::registerPromiseMethodPartialOverloadMethodForPartialInterface(void (*method)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  test_interface_implementation_v8_internal::promiseMethodPartialOverloadMethodForPartialInterface = method;
}

void V8TestInterface::registerStaticPromiseMethodPartialOverloadMethodForPartialInterface(void (*method)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  test_interface_implementation_v8_internal::staticPromiseMethodPartialOverloadMethodForPartialInterface = method;
}

void V8TestInterface::registerPartial2VoidMethodMethodForPartialInterface(void (*method)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  test_interface_implementation_v8_internal::partial2VoidMethodMethodForPartialInterface = method;
}

void V8TestInterface::registerPartial2StaticVoidMethodMethodForPartialInterface(void (*method)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  test_interface_implementation_v8_internal::partial2StaticVoidMethodMethodForPartialInterface = method;
}

}  // namespace blink
