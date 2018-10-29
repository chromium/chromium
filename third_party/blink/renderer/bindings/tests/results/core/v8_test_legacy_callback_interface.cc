// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/callback_interface.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#include "third_party/blink/renderer/bindings/tests/results/core/v8_test_legacy_callback_interface.h"

#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_configuration.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"

namespace blink {

// Support of "legacy callback interface"

// Suppress warning: global constructors, because struct WrapperTypeInfo is
// trivial and does not depend on another global objects.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif
const WrapperTypeInfo V8TestLegacyCallbackInterface::wrapperTypeInfo = {
    gin::kEmbedderBlink,
    V8TestLegacyCallbackInterface::DomTemplate,
    nullptr,
    "TestLegacyCallbackInterface",
    nullptr,
    WrapperTypeInfo::kWrapperTypeNoPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
};
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

static void InstallV8TestLegacyCallbackInterfaceTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::FunctionTemplate> interface_template) {
  // Legacy callback interface must not have a prototype object.
  interface_template->RemovePrototype();

  // Initialize the interface object's template.
  V8DOMConfiguration::InitializeDOMInterfaceTemplate(
      isolate, interface_template,
      V8TestLegacyCallbackInterface::wrapperTypeInfo.interface_name,
      v8::Local<v8::FunctionTemplate>(),
      kV8DefaultWrapperInternalFieldCount);
  interface_template->SetLength(0);

  // Register IDL constants.
  v8::Local<v8::FunctionTemplate> interfaceTemplate = interface_template;
  v8::Local<v8::ObjectTemplate> prototypeTemplate =
      interface_template->PrototypeTemplate();
  static constexpr V8DOMConfiguration::ConstantConfiguration V8TestLegacyCallbackInterfaceConstants[] = {
      {"CONST_VALUE_USHORT_42", V8DOMConfiguration::kConstantTypeUnsignedShort, static_cast<int>(42)},
  };
  V8DOMConfiguration::InstallConstants(
      isolate, interfaceTemplate, prototypeTemplate,
      V8TestLegacyCallbackInterfaceConstants, base::size(V8TestLegacyCallbackInterfaceConstants));
  static_assert(42 == TestLegacyCallbackInterface::kConstValueUshort42, "the value of TestLegacyCallbackInterface_kConstValueUshort42 does not match with implementation");
}

// static
v8::Local<v8::FunctionTemplate> V8TestLegacyCallbackInterface::DomTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world) {
  return V8DOMConfiguration::DomClassTemplate(
      isolate,
      world,
      const_cast<WrapperTypeInfo*>(&wrapperTypeInfo),
      InstallV8TestLegacyCallbackInterfaceTemplate);
}

const char* V8TestLegacyCallbackInterface::NameInHeapSnapshot() const {
  return "V8TestLegacyCallbackInterface";
}

// static
V8TestLegacyCallbackInterface* V8TestLegacyCallbackInterface::CreateOrNull(v8::Local<v8::Object> callback_object) {
  v8::Local<v8::Context> creation_context = callback_object->CreationContext();
  // When |callback_object| is an object in RemoteContext (i.e. RemoteInstance),
  // the object has no creation context, and no way to proceed.
  // TODO(crbug.com/886588): Make CreateOrNull into Create removing the early
  // return with nullptr.
  if (creation_context.IsEmpty())
    return nullptr;

  return new V8TestLegacyCallbackInterface(callback_object, creation_context);
}

v8::Maybe<uint16_t> V8TestLegacyCallbackInterface::acceptNode(ScriptWrappable* callback_this_value, Node* node) {
  if (!IsCallbackFunctionRunnable(CallbackRelevantScriptState(),
                                  IncumbentScriptState())) {
    // Wrapper-tracing for the callback function makes the function object and
    // its creation context alive. Thus it's safe to use the creation context
    // of the callback function here.
    v8::HandleScope handle_scope(GetIsolate());
    v8::Local<v8::Object> callback_object = CallbackObject();
    CHECK(!callback_object.IsEmpty());
    v8::Context::Scope context_scope(callback_object->CreationContext());
    V8ThrowException::ThrowError(
        GetIsolate(),
        ExceptionMessages::FailedToExecute(
            "acceptNode",
            "TestLegacyCallbackInterface",
            "The provided callback is no longer runnable."));
    return v8::Nothing<uint16_t>();
  }

  // step: Prepare to run script with relevant settings.
  ScriptState::Scope callback_relevant_context_scope(
      CallbackRelevantScriptState());
  // step: Prepare to run a callback with stored settings.
  v8::Context::BackupIncumbentScope backup_incumbent_scope(
      IncumbentScriptState()->GetContext());

  v8::Local<v8::Function> function;
  if (IsCallbackObjectCallable()) {
    // step 9.1. If value's interface is a single operation callback interface
    //   and !IsCallable(O) is true, then set X to O.
    function = CallbackObject().As<v8::Function>();
  } else {
    // step 9.2.1. Let getResult be Get(O, opName).
    // step 9.2.2. If getResult is an abrupt completion, set completion to
    //   getResult and jump to the step labeled return.
    v8::Local<v8::Value> value;
    if (!CallbackObject()->Get(CallbackRelevantScriptState()->GetContext(),
                               V8String(GetIsolate(), "acceptNode"))
        .ToLocal(&value)) {
      return v8::Nothing<uint16_t>();
    }
    // step 10. If !IsCallable(X) is false, then set completion to a new
    //   Completion{[[Type]]: throw, [[Value]]: a newly created TypeError
    //   object, [[Target]]: empty}, and jump to the step labeled return.
    if (!value->IsFunction()) {
      V8ThrowException::ThrowTypeError(
          GetIsolate(),
          ExceptionMessages::FailedToExecute(
              "acceptNode",
              "TestLegacyCallbackInterface",
              "The provided callback is not callable."));
      return v8::Nothing<uint16_t>();
    }
    function = value.As<v8::Function>();
  }

  v8::Local<v8::Value> this_arg;
  if (!IsCallbackObjectCallable()) {
    // step 11. If value's interface is not a single operation callback
    //   interface, or if !IsCallable(O) is false, set thisArg to O (overriding
    //   the provided value).
    this_arg = CallbackObject();
  } else if (!callback_this_value) {
    // step 2. If thisArg was not given, let thisArg be undefined.
    this_arg = v8::Undefined(GetIsolate());
  } else {
    this_arg = ToV8(callback_this_value, CallbackRelevantScriptState());
  }

  // step: Let esArgs be the result of converting args to an ECMAScript
  //   arguments list. If this throws an exception, set completion to the
  //   completion value representing the thrown exception and jump to the step
  //   labeled return.
  v8::Local<v8::Object> argument_creation_context =
      CallbackRelevantScriptState()->GetContext()->Global();
  ALLOW_UNUSED_LOCAL(argument_creation_context);
  v8::Local<v8::Value> v8_node = ToV8(node, argument_creation_context, GetIsolate());
  constexpr int argc = 1;
  v8::Local<v8::Value> argv[] = { v8_node };
  static_assert(static_cast<size_t>(argc) == base::size(argv), "size mismatch");

  v8::Local<v8::Value> call_result;
  // step: Let callResult be Call(X, thisArg, esArgs).
  if (!V8ScriptRunner::CallFunction(
          function,
          ExecutionContext::From(CallbackRelevantScriptState()),
          this_arg,
          argc,
          argv,
          GetIsolate()).ToLocal(&call_result)) {
    // step: If callResult is an abrupt completion, set completion to callResult
    //   and jump to the step labeled return.
    return v8::Nothing<uint16_t>();
  }

  // step: Set completion to the result of converting callResult.[[Value]] to
  //   an IDL value of the same type as the operation's return type.
  {
    ExceptionState exception_state(GetIsolate(),
                                   ExceptionState::kExecutionContext,
                                   "TestLegacyCallbackInterface",
                                   "acceptNode");
    auto native_result =
        NativeValueTraits<IDLUnsignedShort>::NativeValue(
            GetIsolate(), call_result, exception_state);
    if (exception_state.HadException())
      return v8::Nothing<uint16_t>();
    else
      return v8::Just<uint16_t>(native_result);
  }
}

v8::Maybe<uint16_t> V8PersistentCallbackInterface<V8TestLegacyCallbackInterface>::acceptNode(ScriptWrappable* callback_this_value, Node* node) {
  return Proxy()->acceptNode(
      callback_this_value, node);
}

}  // namespace blink
