// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off
#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_interface_constructor.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_test_interface_empty.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_context_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/wtf/get_ptr.h"

namespace blink {

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestInterfaceConstructor::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceConstructor::domTemplate,
    nullptr,
    "TestInterfaceConstructor",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

// This static member must be declared by DEFINE_WRAPPERTYPEINFO in TestInterfaceConstructor.h.
// For details, see the comment of DEFINE_WRAPPERTYPEINFO in
// platform/bindings/ScriptWrappable.h.
const WrapperTypeInfo& TestInterfaceConstructor::wrapper_type_info_ = V8TestInterfaceConstructor::wrapperTypeInfo;

// not [ActiveScriptWrappable]
static_assert(
    !std::is_base_of<ActiveScriptWrappableBase, TestInterfaceConstructor>::value,
    "TestInterfaceConstructor inherits from ActiveScriptWrappable<>, but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");
static_assert(
    std::is_same<decltype(&TestInterfaceConstructor::HasPendingActivity),
                 decltype(&ScriptWrappable::HasPendingActivity)>::value,
    "TestInterfaceConstructor is overriding hasPendingActivity(), but is not specifying "
    "[ActiveScriptWrappable] extended attribute in the IDL file.  "
    "Be consistent.");

namespace test_interface_constructor_v8_internal {

static void constructor1(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor_ConstructorCallback");

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor");
  ScriptState* scriptState = ScriptState::From(
      info.NewTarget().As<v8::Object>()->CreationContext());

  ExecutionContext* executionContext = ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext());
  Document& document = *To<Document>(ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext()));
  TestInterfaceConstructor* impl = TestInterfaceConstructor::Create(scriptState, executionContext, document, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructor::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

static void constructor2(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor_ConstructorCallback");

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor");
  ScriptState* scriptState = ScriptState::From(
      info.NewTarget().As<v8::Object>()->CreationContext());

  double doubleArg;
  V8StringResource<> stringArg;
  TestInterfaceEmpty* testInterfaceEmptyArg;
  Dictionary dictionaryArg;
  Vector<String> sequenceStringArg;
  Vector<Dictionary> sequenceDictionaryArg;
  HeapVector<LongOrTestDictionary> sequenceLongOrTestDictionaryArg;
  V8StringResource<kTreatNullAndUndefinedAsNullString> optionalUSVStringArg;
  Dictionary optionalDictionaryArg;
  TestInterfaceEmpty* optionalTestInterfaceEmptyArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  doubleArg = NativeValueTraits<IDLDouble>::NativeValue(info.GetIsolate(), info[0], exceptionState);
  if (exceptionState.HadException())
    return;

  stringArg = info[1];
  if (!stringArg.Prepare())
    return;

  testInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[2]);
  if (!testInterfaceEmptyArg) {
    exceptionState.ThrowTypeError("parameter 3 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  if (!info[3]->IsNullOrUndefined() && !info[3]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 4 ('dictionaryArg') is not an object.");
    return;
  }
  dictionaryArg = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[3], exceptionState);
  if (exceptionState.HadException())
    return;

  sequenceStringArg = NativeValueTraits<IDLSequence<IDLString>>::NativeValue(info.GetIsolate(), info[4], exceptionState);
  if (exceptionState.HadException())
    return;

  sequenceDictionaryArg = NativeValueTraits<IDLSequence<Dictionary>>::NativeValue(info.GetIsolate(), info[5], exceptionState);
  if (exceptionState.HadException())
    return;

  sequenceLongOrTestDictionaryArg = NativeValueTraits<IDLSequence<LongOrTestDictionary>>::NativeValue(info.GetIsolate(), info[6], exceptionState);
  if (exceptionState.HadException())
    return;

  if (UNLIKELY(numArgsPassed <= 7)) {
    ExecutionContext* executionContext = ToExecutionContext(
        info.NewTarget().As<v8::Object>()->CreationContext());
    Document& document = *To<Document>(ToExecutionContext(
        info.NewTarget().As<v8::Object>()->CreationContext()));
    TestInterfaceConstructor* impl = TestInterfaceConstructor::Create(scriptState, executionContext, document, doubleArg, stringArg, testInterfaceEmptyArg, dictionaryArg, sequenceStringArg, sequenceDictionaryArg, sequenceLongOrTestDictionaryArg, exceptionState);
    if (exceptionState.HadException()) {
      return;
    }
    v8::Local<v8::Object> wrapper = info.Holder();
    wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructor::wrapperTypeInfo, wrapper);
    V8SetReturnValue(info, wrapper);
    return;
  }
  optionalUSVStringArg = NativeValueTraits<IDLUSVStringOrNull>::NativeValue(info.GetIsolate(), info[7], exceptionState);
  if (exceptionState.HadException())
    return;

  if (!info[8]->IsNullOrUndefined() && !info[8]->IsObject()) {
    exceptionState.ThrowTypeError("parameter 9 ('optionalDictionaryArg') is not an object.");
    return;
  }
  optionalDictionaryArg = NativeValueTraits<Dictionary>::NativeValue(info.GetIsolate(), info[8], exceptionState);
  if (exceptionState.HadException())
    return;

  optionalTestInterfaceEmptyArg = V8TestInterfaceEmpty::ToImplWithTypeCheck(info.GetIsolate(), info[9]);
  if (!optionalTestInterfaceEmptyArg) {
    exceptionState.ThrowTypeError("parameter 10 is not of type 'TestInterfaceEmpty'.");
    return;
  }

  ExecutionContext* executionContext = ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext());
  Document& document = *To<Document>(ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext()));
  TestInterfaceConstructor* impl = TestInterfaceConstructor::Create(scriptState, executionContext, document, doubleArg, stringArg, testInterfaceEmptyArg, dictionaryArg, sequenceStringArg, sequenceDictionaryArg, sequenceLongOrTestDictionaryArg, optionalUSVStringArg, optionalDictionaryArg, optionalTestInterfaceEmptyArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructor::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

static void constructor3(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor_ConstructorCallback");

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor");
  ScriptState* scriptState = ScriptState::From(
      info.NewTarget().As<v8::Object>()->CreationContext());

  V8StringResource<> arg;
  V8StringResource<> optArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  arg = info[0];
  if (!arg.Prepare())
    return;

  if (UNLIKELY(numArgsPassed <= 1)) {
    ExecutionContext* executionContext = ToExecutionContext(
        info.NewTarget().As<v8::Object>()->CreationContext());
    Document& document = *To<Document>(ToExecutionContext(
        info.NewTarget().As<v8::Object>()->CreationContext()));
    TestInterfaceConstructor* impl = TestInterfaceConstructor::Create(scriptState, executionContext, document, arg, exceptionState);
    if (exceptionState.HadException()) {
      return;
    }
    v8::Local<v8::Object> wrapper = info.Holder();
    wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructor::wrapperTypeInfo, wrapper);
    V8SetReturnValue(info, wrapper);
    return;
  }
  optArg = info[1];
  if (!optArg.Prepare())
    return;

  ExecutionContext* executionContext = ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext());
  Document& document = *To<Document>(ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext()));
  TestInterfaceConstructor* impl = TestInterfaceConstructor::Create(scriptState, executionContext, document, arg, optArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructor::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

static void constructor4(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor_ConstructorCallback");

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor");
  ScriptState* scriptState = ScriptState::From(
      info.NewTarget().As<v8::Object>()->CreationContext());

  V8StringResource<> arg;
  V8StringResource<> arg2;
  V8StringResource<> arg3;
  arg = info[0];
  if (!arg.Prepare())
    return;

  arg2 = info[1];
  if (!arg2.Prepare())
    return;

  arg3 = info[2];
  if (!arg3.Prepare())
    return;

  ExecutionContext* executionContext = ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext());
  Document& document = *To<Document>(ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext()));
  TestInterfaceConstructor* impl = TestInterfaceConstructor::Create(scriptState, executionContext, document, arg, arg2, arg3, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructor::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

static void constructor(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor");
  switch (std::min(10, info.Length())) {
    case 0:
      if (true) {
        test_interface_constructor_v8_internal::constructor1(info);
        return;
      }
      break;
    case 1:
      if (true) {
        test_interface_constructor_v8_internal::constructor3(info);
        return;
      }
      break;
    case 2:
      if (true) {
        test_interface_constructor_v8_internal::constructor3(info);
        return;
      }
      break;
    case 3:
      if (true) {
        test_interface_constructor_v8_internal::constructor4(info);
        return;
      }
      break;
    case 7:
      if (true) {
        test_interface_constructor_v8_internal::constructor2(info);
        return;
      }
      break;
    case 8:
      if (true) {
        test_interface_constructor_v8_internal::constructor2(info);
        return;
      }
      break;
    case 9:
      if (true) {
        test_interface_constructor_v8_internal::constructor2(info);
        return;
      }
      break;
    case 10:
      if (true) {
        test_interface_constructor_v8_internal::constructor2(info);
        return;
      }
      break;
    default:
      if (info.Length() >= 0) {
        exceptionState.ThrowTypeError(ExceptionMessages::InvalidArity("[0, 1, 2, 3, 7, 8, 9, 10]", info.Length()));
        return;
      }
      exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(0, info.Length()));
      return;
  }
  exceptionState.ThrowTypeError("No matching constructor signature.");
}

}  // namespace test_interface_constructor_v8_internal

// Suppress warning: global constructors, because struct WrapperTypeInfo is trivial
// and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestInterfaceConstructorConstructor::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestInterfaceConstructorConstructor::domTemplate,
    nullptr,
    "TestInterfaceConstructor",
    nullptr,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

static void V8TestInterfaceConstructorConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor_ConstructorCallback");

  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::ConstructorNotCallableAsFunction("Audio"));
    return;
  }

  if (ConstructorMode::Current(info.GetIsolate()) == ConstructorMode::kWrapExistingObject) {
    V8SetReturnValue(info, info.Holder());
    return;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::kConstructionContext, "TestInterfaceConstructor");
  ScriptState* scriptState = ScriptState::From(
      info.NewTarget().As<v8::Object>()->CreationContext());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.ThrowTypeError(ExceptionMessages::NotEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> arg;
  V8StringResource<> optArg;
  int numArgsPassed = info.Length();
  while (numArgsPassed > 0) {
    if (!info[numArgsPassed - 1]->IsUndefined())
      break;
    --numArgsPassed;
  }
  arg = info[0];
  if (!arg.Prepare())
    return;

  if (UNLIKELY(numArgsPassed <= 1)) {
    ExecutionContext* executionContext = ToExecutionContext(
        info.NewTarget().As<v8::Object>()->CreationContext());
    Document& document = *To<Document>(ToExecutionContext(
        info.NewTarget().As<v8::Object>()->CreationContext()));
    TestInterfaceConstructor* impl = TestInterfaceConstructor::CreateForJSConstructor(scriptState, executionContext, document, arg, exceptionState);
    if (exceptionState.HadException()) {
      return;
    }
    v8::Local<v8::Object> wrapper = info.Holder();
    wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructorConstructor::wrapperTypeInfo, wrapper);
    V8SetReturnValue(info, wrapper);
    return;
  }
  optArg = info[1];
  if (!optArg.Prepare())
    return;

  ExecutionContext* executionContext = ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext());
  Document& document = *To<Document>(ToExecutionContext(
      info.NewTarget().As<v8::Object>()->CreationContext()));
  TestInterfaceConstructor* impl = TestInterfaceConstructor::CreateForJSConstructor(scriptState, executionContext, document, arg, optArg, exceptionState);
  if (exceptionState.HadException()) {
    return;
  }
  v8::Local<v8::Object> wrapper = info.Holder();
  wrapper = impl->AssociateWithWrapper(info.GetIsolate(), &V8TestInterfaceConstructorConstructor::wrapperTypeInfo, wrapper);
  V8SetReturnValue(info, wrapper);
}

v8::Local<v8::FunctionTemplate> V8TestInterfaceConstructorConstructor::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  static int domTemplateKey; // This address is used for a key to look up the dom template.
  V8PerIsolateData* data = V8PerIsolateData::From(isolate);
  v8::Local<v8::FunctionTemplate> result = data->FindInterfaceTemplate(world, &domTemplateKey);
  if (!result.IsEmpty())
    return result;

  result = v8::FunctionTemplate::New(isolate, V8TestInterfaceConstructorConstructorCallback);
  v8::Local<v8::ObjectTemplate> instanceTemplate = result->InstanceTemplate();
  instanceTemplate->SetInternalFieldCount(V8TestInterfaceConstructor::internalFieldCount);
  result->SetClassName(V8AtomicString(isolate, "Audio"));
  result->Inherit(V8TestInterfaceConstructor::domTemplate(isolate, world));
  data->SetInterfaceTemplate(world, &domTemplateKey, result);
  return result;
}

void V8TestInterfaceConstructorConstructor::NamedConstructorAttributeGetter(
    v8::Local<v8::Name> propertyName,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Context> creationContext = info.Holder()->CreationContext();
  V8PerContextData* perContextData = V8PerContextData::From(creationContext);
  if (!perContextData) {
    // TODO(yukishiino): Return a valid named constructor even after the context is detached
    return;
  }

  v8::Local<v8::Function> namedConstructor = perContextData->ConstructorForType(&V8TestInterfaceConstructorConstructor::wrapperTypeInfo);

  // Set the prototype of named constructors to the regular constructor.
  auto privateProperty = V8PrivateProperty::GetNamedConstructorInitialized(info.GetIsolate());
  v8::Local<v8::Context> currentContext = info.GetIsolate()->GetCurrentContext();
  v8::Local<v8::Value> privateValue;

  if (!privateProperty.GetOrUndefined(namedConstructor).ToLocal(&privateValue) || privateValue->IsUndefined()) {
    v8::Local<v8::Function> interface = perContextData->ConstructorForType(&V8TestInterfaceConstructor::wrapperTypeInfo);
    v8::Local<v8::Value> interfacePrototype = interface->Get(currentContext, V8AtomicString(info.GetIsolate(), "prototype")).ToLocalChecked();
    bool result = namedConstructor->Set(currentContext, V8AtomicString(info.GetIsolate(), "prototype"), interfacePrototype).ToChecked();
    if (!result)
      return;
    privateProperty.Set(namedConstructor, v8::True(info.GetIsolate()));
  }

  V8SetReturnValue(info, namedConstructor);
}

void V8TestInterfaceConstructor::constructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  RUNTIME_CALL_TIMER_SCOPE_DISABLED_BY_DEFAULT(info.GetIsolate(), "Blink_TestInterfaceConstructor_Constructor");

  UseCounter::Count(CurrentExecutionContext(info.GetIsolate()), WebFeature::kTestFeature);
  if (!info.IsConstructCall()) {
    V8ThrowException::ThrowTypeError(info.GetIsolate(), ExceptionMessages::ConstructorNotCallableAsFunction("TestInterfaceConstructor"));
    return;
  }

  if (ConstructorMode::Current(info.GetIsolate()) == ConstructorMode::kWrapExistingObject) {
    V8SetReturnValue(info, info.Holder());
    return;
  }

  test_interface_constructor_v8_internal::constructor(info);
}

static void installV8TestInterfaceConstructorTemplate(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(isolate, interfaceTemplate, V8TestInterfaceConstructor::wrapperTypeInfo.interface_name, v8::Local<v8::FunctionTemplate>(), V8TestInterfaceConstructor::internalFieldCount);
  interfaceTemplate->SetCallHandler(V8TestInterfaceConstructor::constructorCallback);
  interfaceTemplate->SetLength(0);

  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register IDL constants, attributes and operations.

  // Custom signature

  V8TestInterfaceConstructor::InstallRuntimeEnabledFeaturesOnTemplate(
      isolate, world, interfaceTemplate);
}

void V8TestInterfaceConstructor::InstallRuntimeEnabledFeaturesOnTemplate(
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

v8::Local<v8::FunctionTemplate> V8TestInterfaceConstructor::domTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(isolate, world, const_cast<WrapperTypeInfo*>(&wrapperTypeInfo), installV8TestInterfaceConstructorTemplate);
}

bool V8TestInterfaceConstructor::hasInstance(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->HasInstance(&wrapperTypeInfo, v8Value);
}

v8::Local<v8::Object> V8TestInterfaceConstructor::findInstanceInPrototypeChain(v8::Local<v8::Value> v8Value, v8::Isolate* isolate) {
  return V8PerIsolateData::From(isolate)->FindInstanceInPrototypeChain(&wrapperTypeInfo, v8Value);
}

TestInterfaceConstructor* V8TestInterfaceConstructor::ToImplWithTypeCheck(v8::Isolate* isolate, v8::Local<v8::Value> value) {
  return hasInstance(value, isolate) ? ToImpl(v8::Local<v8::Object>::Cast(value)) : nullptr;
}

TestInterfaceConstructor* NativeValueTraits<TestInterfaceConstructor>::NativeValue(v8::Isolate* isolate, v8::Local<v8::Value> value, ExceptionState& exceptionState) {
  TestInterfaceConstructor* nativeValue = V8TestInterfaceConstructor::ToImplWithTypeCheck(isolate, value);
  if (!nativeValue) {
    exceptionState.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
        "TestInterfaceConstructor"));
  }
  return nativeValue;
}

}  // namespace blink
