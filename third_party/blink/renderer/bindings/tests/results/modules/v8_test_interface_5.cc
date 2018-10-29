// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/modules/v8_test_interface_5.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_empty.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_test_interface_5.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_void_callback_function_modules.h"
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
const WrapperTypeInfo V8TestInterface5::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterface5::domTemplate,
    V8TestInterface5::InstallConditionalFeatures,
    "TestInterface5",
    &V8TestInterfaceEmpty::wrapperTypeInfo,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterface5Implementation.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterface5Implementation::wrapper_type_info_ = V8TestInterface5::wrapperTypeInfo;

// [ActiveScriptWrappable]
static_assert(
    std::is_base_of<ActiveScriptWrappableBase, TestInterface5Implementation>::value,
    "TestInterface5Implementation does not inherit from ActiveScriptWrappable<>, but specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    !std::is_same<decltype(&TestInterface5Implementation::HasPendingActivity),
                  decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterface5Implementation is not overriding hasPendingActivity(), but is specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_5_implementation_v8_internal {

static void testInterfaceAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  V8SetReturnValueFast(info, WTF::GetPtr(impl->testInterfaceAttribute()), impl);
}

static void testInterfaceAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface5", "testInterfaceAttribute");

  // Prepare the value to be set.
  TestInterface5Implementation* cppValue = V8TestInterface5::ToImplWithTypeCheck(info.GetIsolate(), v8Value);

  // Type check per: http://heycam.github.io/webidl/#es-interface
  if (!cppValue) {
    exceptionState.ThrowTypeError("The provided value is not of type 'TestInterface5'.");
    return;
  }

  impl->setTestInterfaceAttribute(cppValue);
}

static void doubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  V8SetReturnValue(info, impl->doubleAttribute());
}

static void doubleAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface5", "doubleAttribute");

  // Prepare the value to be set.
  double cppValue = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setDoubleAttribute(cppValue);
}

static void floatAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  V8SetReturnValue(info, impl->floatAttribute());
}

static void floatAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface5", "floatAttribute");

  // Prepare the value to be set.
  float cppValue = NativeValueTraits<IDLFloat>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setFloatAttribute(cppValue);
}

static void unrestrictedDoubleAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  V8SetReturnValue(info, impl->unrestrictedDoubleAttribute());
}

static void unrestrictedDoubleAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface5", "unrestrictedDoubleAttribute");

  // Prepare the value to be set.
  double cppValue = NativeValueTraits<IDLUnrestrictedDouble>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnrestrictedDoubleAttribute(cppValue);
}

static void unrestrictedFloatAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  V8SetReturnValue(info, impl->unrestrictedFloatAttribute());
}

static void unrestrictedFloatAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface5", "unrestrictedFloatAttribute");

  // Prepare the value to be set.
  float cppValue = NativeValueTraits<IDLUnrestrictedFloat>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setUnrestrictedFloatAttribute(cppValue);
}

static void staticStringAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8SetReturnValueString(info, TestInterface5Implementation::staticStringAttribute(), info.GetIsolate());
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

  TestInterface5Implementation::setStaticStringAttribute(cppValue);
}

static void lengthAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  V8SetReturnValueUnsigned(info, impl->length());
}

static void alwaysExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  V8SetReturnValueInt(info, impl->alwaysExposedAttribute());
}

static void alwaysExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface5", "alwaysExposedAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setAlwaysExposedAttribute(cppValue);
}

static void workerExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  V8SetReturnValueInt(info, impl->workerExposedAttribute());
}

static void workerExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface5", "workerExposedAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setWorkerExposedAttribute(cppValue);
}

static void windowExposedAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  V8SetReturnValueInt(info, impl->windowExposedAttribute());
}

static void windowExposedAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  ALLOW_UNUSED_LOCAL(isolate);

  v8::Local<v8::Object> holder = info.Holder();
  ALLOW_UNUSED_LOCAL(holder);

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(holder);

  ExceptionState exceptionState(isolate, ExceptionState::kSetterContext, "TestInterface5", "windowExposedAttribute");

  // Prepare the value to be set.
  int32_t cppValue = NativeValueTraits<IDLLong>::NativeValue(info.GetIsolate(), v8Value, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->setWindowExposedAttribute(cppValue);
}

static void voidMethodTestInterfaceEmptyArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyArg", "TestInterface5", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  TestInterfaceEmpty* testInterfaceEmptyArg;
  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!testInterfaceEmptyArg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodTestInterfaceEmptyArg", "TestInterface5", "parameter 1 is not of type 'TestInterfaceEmpty'."));
    return;
  }

  impl->voidMethodTestInterfaceEmptyArg(testInterfaceEmptyArg);
}

static void voidMethodDoubleArgFloatArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface5", "voidMethodDoubleArgFloatArg");

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

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

static void voidMethodUnrestrictedDoubleArgUnrestrictedFloatArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface5", "voidMethodUnrestrictedDoubleArgUnrestrictedFloatArg");

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

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

static void voidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  impl->voidMethod();
}

static void voidMethodMethodForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  impl->voidMethod();
}

static void alwaysExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  impl->alwaysExposedMethod();
}

static void workerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  impl->workerExposedMethod();
}

static void windowExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  impl->windowExposedMethod();
}

static void alwaysExposedStaticMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation::alwaysExposedStaticMethod();
}

static void workerExposedStaticMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation::workerExposedStaticMethod();
}

static void windowExposedStaticMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation::windowExposedStaticMethod();
}

static void windowAndServiceWorkerExposedMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  impl->windowAndServiceWorkerExposedMethod();
}

static void voidMethodBooleanOrDOMStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface5", "voidMethodBooleanOrDOMStringArg");

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  BooleanOrString arg;
  V8BooleanOrString::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodBooleanOrDOMStringArg(arg);
}

static void voidMethodDoubleOrDOMStringArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kExecutionContext, "TestInterface5", "voidMethodDoubleOrDOMStringArg");

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  DoubleOrString arg;
  V8DoubleOrString::ToImpl(info.GetIsolate(), info[0], arg, UnionTypeConversionMode::kNotNullable, exceptionState);
  if (exceptionState.HadException())
    return;

  impl->voidMethodDoubleOrDOMStringArg(arg);
}

static void voidMethodVoidExperimentalCallbackFunctionMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodVoidExperimentalCallbackFunction", "TestInterface5", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  VoidExperimentalCallbackFunction* arg;
  arg = V8VoidExperimentalCallbackFunction::ToImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!arg) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodVoidExperimentalCallbackFunction", "TestInterface5", "parameter 1 is not of type 'VoidExperimentalCallbackFunction'."));
    return;
  }

  impl->voidMethodVoidExperimentalCallbackFunction(arg);
}

static void voidMethodVoidCallbackFunctionModulesArgMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodVoidCallbackFunctionModulesArg", "TestInterface5", ExceptionMessages::NotEnoughArguments(1, info.Length())));
    return;
  }

  V8VoidCallbackFunctionModules* arg;
  if (info[0]->IsFunction()) {
    arg = V8VoidCallbackFunctionModules::Create(info[0].As<v8::Function>());
  } else {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::FailedToExecute("voidMethodVoidCallbackFunctionModulesArg", "TestInterface5", "The callback provided as parameter 1 is not a function."));
    return;
  }

  impl->voidMethodVoidCallbackFunctionModulesArg(arg);
}

static void toStringMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  V8SetReturnValueString(info, impl->toString(), info.GetIsolate());
}

static void namedPropertyGetter(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());
  String result = impl->AnonymousNamedGetter(name);
  if (result.IsNull())
    return;
  V8SetReturnValueString(info, result, info.GetIsolate());
}

static void namedPropertyQuery(const AtomicString& name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  const CString& nameInUtf8 = name.Utf8();
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kGetterContext, "TestInterface5", nameInUtf8.data());

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  bool result = impl->NamedPropertyQuery(name, exceptionState);
  if (!result)
    return;
  // https://heycam.github.io/webidl/#LegacyPlatformObjectGetOwnProperty
  // 2.7. If |O| implements an interface with a named property setter, then set
  //      desc.[[Writable]] to true, otherwise set it to false.
  // 2.8. If |O| implements an interface with the
  //      [LegacyUnenumerableNamedProperties] extended attribute, then set
  //      desc.[[Enumerable]] to false, otherwise set it to true.
  V8SetReturnValueInt(info, v8::ReadOnly);
}

static void namedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kEnumerationContext, "TestInterface5");

  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  Vector<String> names;
  impl->NamedPropertyEnumerator(names, exceptionState);
  if (exceptionState.HadException())
    return;
  V8SetReturnValue(info, ToV8(names, info.Holder(), info.GetIsolate()).As<v8::Array>());
}

static void indexedPropertyGetter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

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
  V8TestInterface5::indexedPropertyGetterCallback(index, info);
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
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());
  V8StringResource<> propertyValue = v8Value;
  if (!propertyValue.Prepare())
    return;

  bool result = impl->AnonymousIndexedSetter(index, propertyValue);
  if (!result)
    return;
  V8SetReturnValue(info, v8Value);
}

static void indexedPropertyDeleter(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  TestInterface5Implementation* impl = V8TestInterface5::ToImpl(info.Holder());

  DeleteResult result = impl->AnonymousIndexedDeleter(index);
  if (result == kDeleteUnknownProperty)
    return;
  V8SetReturnValue(info, result == kDeleteSuccess);
}

}  // namespace test_interface_5_implementation_v8_internal

void V8TestInterface5::testInterfaceAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_testInterfaceAttribute_Getter");

  test_interface_5_implementation_v8_internal::testInterfaceAttributeAttributeGetter(info);
}

void V8TestInterface5::testInterfaceAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_testInterfaceAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_5_implementation_v8_internal::testInterfaceAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface5::testInterfaceConstructorAttributeConstructorGetterCallback(v8::Local<v8::Name> property, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_testInterfaceConstructorAttribute_ConstructorGetterCallback");

  V8ConstructorAttributeGetter(property, info, &V8TestInterface5::wrapperTypeInfo);
}

void V8TestInterface5::doubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_doubleAttribute_Getter");

  test_interface_5_implementation_v8_internal::doubleAttributeAttributeGetter(info);
}

void V8TestInterface5::doubleAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_doubleAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_5_implementation_v8_internal::doubleAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface5::floatAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_floatAttribute_Getter");

  test_interface_5_implementation_v8_internal::floatAttributeAttributeGetter(info);
}

void V8TestInterface5::floatAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_floatAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_5_implementation_v8_internal::floatAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface5::unrestrictedDoubleAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_unrestrictedDoubleAttribute_Getter");

  test_interface_5_implementation_v8_internal::unrestrictedDoubleAttributeAttributeGetter(info);
}

void V8TestInterface5::unrestrictedDoubleAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_unrestrictedDoubleAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_5_implementation_v8_internal::unrestrictedDoubleAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface5::unrestrictedFloatAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_unrestrictedFloatAttribute_Getter");

  test_interface_5_implementation_v8_internal::unrestrictedFloatAttributeAttributeGetter(info);
}

void V8TestInterface5::unrestrictedFloatAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_unrestrictedFloatAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_5_implementation_v8_internal::unrestrictedFloatAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface5::staticStringAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_staticStringAttribute_Getter");

  test_interface_5_implementation_v8_internal::staticStringAttributeAttributeGetter(info);
}

void V8TestInterface5::staticStringAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_staticStringAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_5_implementation_v8_internal::staticStringAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface5::lengthAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_length_Getter");

  test_interface_5_implementation_v8_internal::lengthAttributeGetter(info);
}

void V8TestInterface5::alwaysExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_alwaysExposedAttribute_Getter");

  test_interface_5_implementation_v8_internal::alwaysExposedAttributeAttributeGetter(info);
}

void V8TestInterface5::alwaysExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_alwaysExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_5_implementation_v8_internal::alwaysExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface5::workerExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_workerExposedAttribute_Getter");

  test_interface_5_implementation_v8_internal::workerExposedAttributeAttributeGetter(info);
}

void V8TestInterface5::workerExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_workerExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_5_implementation_v8_internal::workerExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface5::windowExposedAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_windowExposedAttribute_Getter");

  test_interface_5_implementation_v8_internal::windowExposedAttributeAttributeGetter(info);
}

void V8TestInterface5::windowExposedAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_windowExposedAttribute_Setter");

  v8::Local<v8::Value> v8Value = info[0];

  test_interface_5_implementation_v8_internal::windowExposedAttributeAttributeSetter(v8Value, info);
}

void V8TestInterface5::voidMethodTestInterfaceEmptyArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_voidMethodTestInterfaceEmptyArg");

  test_interface_5_implementation_v8_internal::voidMethodTestInterfaceEmptyArgMethod(info);
}

void V8TestInterface5::voidMethodDoubleArgFloatArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_voidMethodDoubleArgFloatArg");

  test_interface_5_implementation_v8_internal::voidMethodDoubleArgFloatArgMethod(info);
}

void V8TestInterface5::voidMethodUnrestrictedDoubleArgUnrestrictedFloatArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_voidMethodUnrestrictedDoubleArgUnrestrictedFloatArg");

  test_interface_5_implementation_v8_internal::voidMethodUnrestrictedDoubleArgUnrestrictedFloatArgMethod(info);
}

void V8TestInterface5::voidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_voidMethod");

  test_interface_5_implementation_v8_internal::voidMethodMethod(info);
}

void V8TestInterface5::voidMethodMethodCallbackForMainWorld(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_voidMethod");

  test_interface_5_implementation_v8_internal::voidMethodMethodForMainWorld(info);
}

void V8TestInterface5::alwaysExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_alwaysExposedMethod");

  test_interface_5_implementation_v8_internal::alwaysExposedMethodMethod(info);
}

void V8TestInterface5::workerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_workerExposedMethod");

  test_interface_5_implementation_v8_internal::workerExposedMethodMethod(info);
}

void V8TestInterface5::windowExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_windowExposedMethod");

  test_interface_5_implementation_v8_internal::windowExposedMethodMethod(info);
}

void V8TestInterface5::alwaysExposedStaticMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_alwaysExposedStaticMethod");

  test_interface_5_implementation_v8_internal::alwaysExposedStaticMethodMethod(info);
}

void V8TestInterface5::workerExposedStaticMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_workerExposedStaticMethod");

  test_interface_5_implementation_v8_internal::workerExposedStaticMethodMethod(info);
}

void V8TestInterface5::windowExposedStaticMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_windowExposedStaticMethod");

  test_interface_5_implementation_v8_internal::windowExposedStaticMethodMethod(info);
}

void V8TestInterface5::windowAndServiceWorkerExposedMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_windowAndServiceWorkerExposedMethod");

  test_interface_5_implementation_v8_internal::windowAndServiceWorkerExposedMethodMethod(info);
}

void V8TestInterface5::voidMethodBooleanOrDOMStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_voidMethodBooleanOrDOMStringArg");

  test_interface_5_implementation_v8_internal::voidMethodBooleanOrDOMStringArgMethod(info);
}

void V8TestInterface5::voidMethodDoubleOrDOMStringArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_voidMethodDoubleOrDOMStringArg");

  test_interface_5_implementation_v8_internal::voidMethodDoubleOrDOMStringArgMethod(info);
}

void V8TestInterface5::voidMethodVoidExperimentalCallbackFunctionMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_voidMethodVoidExperimentalCallbackFunction");

  test_interface_5_implementation_v8_internal::voidMethodVoidExperimentalCallbackFunctionMethod(info);
}

void V8TestInterface5::voidMethodVoidCallbackFunctionModulesArgMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_voidMethodVoidCallbackFunctionModulesArg");

  test_interface_5_implementation_v8_internal::voidMethodVoidCallbackFunctionModulesArgMethod(info);
}

void V8TestInterface5::toStringMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_toString");

  test_interface_5_implementation_v8_internal::toStringMethod(info);
}

void V8TestInterface5::namedPropertyGetterCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_NamedPropertyGetter");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_interface_5_implementation_v8_internal::namedPropertyGetter(propertyName, info);
}

void V8TestInterface5::namedPropertyQueryCallback(v8::Local<v8::Name> name, const v8::PropertyCallbackInfo<v8::Integer>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_NamedPropertyQuery");

  if (!name->IsString())
    return;
  const AtomicString& propertyName = ToCoreAtomicString(name.As<v8::String>());

  test_interface_5_implementation_v8_internal::namedPropertyQuery(propertyName, info);
}

void V8TestInterface5::namedPropertyEnumeratorCallback(const v8::PropertyCallbackInfo<v8::Array>& info) {
  test_interface_5_implementation_v8_internal::namedPropertyEnumerator(info);
}

void V8TestInterface5::indexedPropertyGetterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterface5Implementation_IndexedPropertyGetter");

  test_interface_5_implementation_v8_internal::indexedPropertyGetter(index, info);
}

void V8TestInterface5::indexedPropertyDescriptorCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_interface_5_implementation_v8_internal::indexedPropertyDescriptor(index, info);
}

void V8TestInterface5::indexedPropertySetterCallback(uint32_t index, v8::Local<v8::Value> v8Value, const v8::PropertyCallbackInfo<v8::Value>& info) {
  test_interface_5_implementation_v8_internal::indexedPropertySetter(index, v8Value, info);
}

void V8TestInterface5::indexedPropertyDeleterCallback(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) {
  test_interface_5_implementation_v8_internal::indexedPropertyDeleter(index, info);
}

void V8TestInterface5::indexedPropertyDefinerCallback(
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
                                    "TestInterface5");
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
static const V8DOMConfiguration::AttributeConfiguration V8TestInterface5Attributes[] = {
    { "testInterfaceConstructorAttribute", V8TestInterface5::testInterfaceConstructorAttributeConstructorGetterCallback, nullptr, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnInstance, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kReplaceWithDataProperty, V8DOMConfiguration::kAllWorlds },
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

static const V8DOMConfiguration::AccessorConfiguration V8TestInterface5Accessors[] = {
    { "testInterfaceAttribute", V8TestInterface5::testInterfaceAttributeAttributeGetterCallback, V8TestInterface5::testInterfaceAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "doubleAttribute", V8TestInterface5::doubleAttributeAttributeGetterCallback, V8TestInterface5::doubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "floatAttribute", V8TestInterface5::floatAttributeAttributeGetterCallback, V8TestInterface5::floatAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unrestrictedDoubleAttribute", V8TestInterface5::unrestrictedDoubleAttributeAttributeGetterCallback, V8TestInterface5::unrestrictedDoubleAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "unrestrictedFloatAttribute", V8TestInterface5::unrestrictedFloatAttributeAttributeGetterCallback, V8TestInterface5::unrestrictedFloatAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "staticStringAttribute", V8TestInterface5::staticStringAttributeAttributeGetterCallback, V8TestInterface5::staticStringAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "length", V8TestInterface5::lengthAttributeGetterCallback, nullptr, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::ReadOnly), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
    { "alwaysExposedAttribute", V8TestInterface5::alwaysExposedAttributeAttributeGetterCallback, V8TestInterface5::alwaysExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
};

static const V8DOMConfiguration::MethodConfiguration V8TestInterface5Methods[] = {
    {"voidMethodTestInterfaceEmptyArg", V8TestInterface5::voidMethodTestInterfaceEmptyArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleArgFloatArg", V8TestInterface5::voidMethodDoubleArgFloatArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodUnrestrictedDoubleArgUnrestrictedFloatArg", V8TestInterface5::voidMethodUnrestrictedDoubleArgUnrestrictedFloatArgMethodCallback, 2, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethod", V8TestInterface5::voidMethodMethodCallbackForMainWorld, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kMainWorld},
    {"voidMethod", V8TestInterface5::voidMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kNonMainWorlds},
    {"alwaysExposedMethod", V8TestInterface5::alwaysExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"alwaysExposedStaticMethod", V8TestInterface5::alwaysExposedStaticMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodBooleanOrDOMStringArg", V8TestInterface5::voidMethodBooleanOrDOMStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodDoubleOrDOMStringArg", V8TestInterface5::voidMethodDoubleOrDOMStringArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodVoidExperimentalCallbackFunction", V8TestInterface5::voidMethodVoidExperimentalCallbackFunctionMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"voidMethodVoidCallbackFunctionModulesArg", V8TestInterface5::voidMethodVoidCallbackFunctionModulesArgMethodCallback, 1, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
    {"toString", V8TestInterface5::toStringMethodCallback, 0, static_cast<v8::PropertyAttribute>(v8::DontEnum), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds},
};

static void installV8TestInterface5Template(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterface5::wrapperTypeInfo.interface_name, V8TestInterfaceEmpty::domTemplate(isolate, world), V8TestInterface5::internalFieldCount);

  if (!RuntimeEnabledFeatures::FeatureNameEnabled()) {
    return;
  }

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.
  static constexpr V8DOMConfiguration::ConstantConfiguration V8TestInterface5Constants[] = {
      {"UNSIGNED_LONG", V8DOMConfiguration::kConstantTypeUnsignedLong, static_cast<int>(0)},
      {"CONST_JAVASCRIPT", V8DOMConfiguration::kConstantTypeShort, static_cast<int>(1)},
  };
  V8DOMConfiguration::InstallConstants(
      isolate, interfaceTemplate, prototypeTemplate,
      V8TestInterface5Constants, base::size(V8TestInterface5Constants));
  V8DOMConfiguration::InstallAttributes(
      isolate, world, instanceTemplate, prototypeTemplate,
      V8TestInterface5Attributes, base::size(V8TestInterface5Attributes));
  V8DOMConfiguration::InstallAccessors(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterface5Accessors, base::size(V8TestInterface5Accessors));
  V8DOMConfiguration::InstallMethods(
      isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate,
      signature, V8TestInterface5Methods, base::size(V8TestInterface5Methods));

  // Indexed properties
  v8::IndexedPropertyHandlerConfiguration indexedPropertyHandlerConfig(
      V8TestInterface5::indexedPropertyGetterCallback,
      V8TestInterface5::indexedPropertySetterCallback,
      V8TestInterface5::indexedPropertyDescriptorCallback,
      V8TestInterface5::indexedPropertyDeleterCallback,
      IndexedPropertyEnumerator<TestInterface5Implementation>,
      V8TestInterface5::indexedPropertyDefinerCallback,
      v8::Local<v8::Value>(),
      v8::PropertyHandlerFlags::kNone);
  instanceTemplate->SetHandler(indexedPropertyHandlerConfig);
  // Named properties
  v8::NamedPropertyHandlerConfiguration namedPropertyHandlerConfig(V8TestInterface5::namedPropertyGetterCallback, nullptr, V8TestInterface5::namedPropertyQueryCallback, nullptr, V8TestInterface5::namedPropertyEnumeratorCallback, v8::Local<v8::Value>(), static_cast<v8::PropertyHandlerFlags>(int(v8::PropertyHandlerFlags::kOnlyInterceptStrings) | int(v8::PropertyHandlerFlags::kNonMasking)));
  instanceTemplate->SetHandler(namedPropertyHandlerConfig);

  // Array iterator (@@iterator)
  prototypeTemplate->SetIntrinsicDataProperty(v8::Symbol::GetIterator(isolate), v8::kArrayProto_values, v8::DontEnum);
  // For value iterators, the properties below must originally be set to the corresponding ones in %ArrayPrototype%.
  // See https://heycam.github.io/webidl/#es-iterators.
  prototypeTemplate->SetIntrinsicDataProperty(V8AtomicString(isolate, "entries"), v8::kArrayProto_entries);
  prototypeTemplate->SetIntrinsicDataProperty(V8AtomicString(isolate, "forEach"), v8::kArrayProto_forEach);
  prototypeTemplate->SetIntrinsicDataProperty(V8AtomicString(isolate, "keys"), v8::kArrayProto_keys);
  prototypeTemplate->SetIntrinsicDataProperty(V8AtomicString(isolate, "values"), v8::kArrayProto_values);

  instanceTemplate->SetCallAsFunctionHandler(V8TestInterface5::legacyCallCustom);

  // Custom signature

  V8TestInterface5::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterface5::InstallRuntimeEnabledFeaturesOnTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interface_template) {
  if (!RuntimeEnabledFeatures::FeatureNameEnabled()) {
    return;
  }

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interface_template);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instance_template = interface_template->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instance_template);
  v8::Local<v8::ObjectTemplate> prototype_template = interface_template->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototype_template);

  // Register IDL constants, attributes and operations.

  // Custom signature
}

v8::Local<v8::FunctionTemplate> V8TestInterface5::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterface5Template);
}

bool V8TestInterface5::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterface5::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterface5Implementation* V8TestInterface5::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterface5Implementation* NativeValueTraits<TestInterface5Implementation>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterface5Implementation* nativeValue = V8TestInterface5::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterface5"));
  }
  return nativeValue;
}

void V8TestInterface5::InstallConditionalFeatures(
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

  if (!prototypeObject.IsEmpty() || !interfaceObject.IsEmpty()) {
    if (executionContext && (executionContext->IsDocument())) {
      static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
          { "windowExposedAttribute", V8TestInterface5::windowExposedAttributeAttributeGetterCallback, V8TestInterface5::windowExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      };
      V8DOMConfiguration::InstallAccessors(
          isolate, world, instanceObject, prototypeObject, interfaceObject,
          signature, accessor_configurations,
          base::size(accessor_configurations));
    }
    if (executionContext && (executionContext->IsWorkerGlobalScope())) {
      static const V8DOMConfiguration::AccessorConfiguration accessor_configurations[] = {
          { "workerExposedAttribute", V8TestInterface5::workerExposedAttributeAttributeGetterCallback, V8TestInterface5::workerExposedAttributeAttributeSetterCallback, V8PrivateProperty::kNoCachedAccessor, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAlwaysCallGetter, V8DOMConfiguration::kAllWorlds },
      };
      V8DOMConfiguration::InstallAccessors(
          isolate, world, instanceObject, prototypeObject, interfaceObject,
          signature, accessor_configurations,
          base::size(accessor_configurations));
    }
    if (executionContext && (executionContext->IsWorkerGlobalScope())) {
      const V8DOMConfiguration::MethodConfiguration workerExposedMethodMethodConfiguration[] = {
        {"workerExposedMethod", V8TestInterface5::workerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : workerExposedMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (executionContext && (executionContext->IsDocument())) {
      const V8DOMConfiguration::MethodConfiguration windowExposedMethodMethodConfiguration[] = {
        {"windowExposedMethod", V8TestInterface5::windowExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : windowExposedMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (executionContext && (executionContext->IsDocument() || executionContext->IsServiceWorkerGlobalScope())) {
      const V8DOMConfiguration::MethodConfiguration windowAndServiceWorkerExposedMethodMethodConfiguration[] = {
        {"windowAndServiceWorkerExposedMethod", V8TestInterface5::windowAndServiceWorkerExposedMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnPrototype, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : windowAndServiceWorkerExposedMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (executionContext && (executionContext->IsWorkerGlobalScope())) {
      const V8DOMConfiguration::MethodConfiguration workerExposedStaticMethodMethodConfiguration[] = {
        {"workerExposedStaticMethod", V8TestInterface5::workerExposedStaticMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : workerExposedStaticMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
    if (executionContext && (executionContext->IsDocument())) {
      const V8DOMConfiguration::MethodConfiguration windowExposedStaticMethodMethodConfiguration[] = {
        {"windowExposedStaticMethod", V8TestInterface5::windowExposedStaticMethodMethodCallback, 0, v8::None, V8DOMConfiguration::kOnInterface, V8DOMConfiguration::kCheckHolder, V8DOMConfiguration::kDoNotCheckAccess, V8DOMConfiguration::kHasSideEffect, V8DOMConfiguration::kAllWorlds}
      };
      for (const auto& methodConfig : windowExposedStaticMethodMethodConfiguration)
        V8DOMConfiguration::InstallMethod(isolate, world, instanceObject, prototypeObject, interfaceObject, signature, methodConfig);
    }
  }
}

}  // namespace blink
