// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is for functions that are used only by generated code.
// CAUTION:
// All functions defined in this file should be used by generated code only.
// If you want to use them from hand-written code, please find appropriate
// location and move them to that location.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_GENERATED_CODE_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_GENERATED_CODE_HELPER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8.h"

namespace blink {

class QualifiedName;
class ScriptState;
class SerializedScriptValue;

CORE_EXPORT void V8ConstructorAttributeGetter(
    v8::Local<v8::Name> property_name,
    const v8::PropertyCallbackInfo<v8::Value>&,
    const WrapperTypeInfo*);

CORE_EXPORT v8::Local<v8::Value> V8Deserialize(v8::Isolate*,
                                               SerializedScriptValue*);

// ExceptionToRejectPromiseScope converts a possible exception to a reject
// promise and returns the promise instead of throwing the exception.
//
// Promise-returning DOM operations are required to always return a promise
// and to never throw an exception.
// See also http://heycam.github.io/webidl/#es-operations
class CORE_EXPORT ExceptionToRejectPromiseScope final {
  STACK_ALLOCATED();

 public:
  ExceptionToRejectPromiseScope(const v8::FunctionCallbackInfo<v8::Value>& info,
                                ExceptionState& exception_state)
      : info_(info), exception_state_(exception_state) {}
  ~ExceptionToRejectPromiseScope() {
    if (!exception_state_.HadException())
      return;

    // As exceptions must always be created in the current realm, reject
    // promises must also be created in the current realm while regular promises
    // are created in the relevant realm of the context object.
    ScriptState* script_state = ScriptState::ForCurrentRealm(info_);
    V8SetReturnValue(
        info_, ScriptPromise::Reject(script_state, exception_state_).V8Value());
  }

 private:
  const v8::FunctionCallbackInfo<v8::Value>& info_;
  ExceptionState& exception_state_;
};

CORE_EXPORT bool IsCallbackFunctionRunnable(
    const ScriptState* callback_relevant_script_state,
    const ScriptState* incumbent_script_state);

CORE_EXPORT bool IsCallbackFunctionRunnableIgnoringPause(
    const ScriptState* callback_relevant_script_state,
    const ScriptState* incumbent_script_state);

using InstallTemplateFunction =
    void (*)(v8::Isolate* isolate,
             const DOMWrapperWorld& world,
             v8::Local<v8::FunctionTemplate> interface_template);

using InstallRuntimeEnabledFeaturesFunction =
    void (*)(v8::Isolate*,
             const DOMWrapperWorld&,
             v8::Local<v8::Object> instance,
             v8::Local<v8::Object> prototype,
             v8::Local<v8::Function> interface);

using InstallRuntimeEnabledFeaturesOnTemplateFunction = InstallTemplateFunction;

// Helpers for [CEReactions, Reflect] IDL attributes.
void V8SetReflectedBooleanAttribute(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const char* interface_name,
    const char* idl_attribute_name,
    const QualifiedName& content_attr);
void V8SetReflectedDOMStringAttribute(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const QualifiedName& content_attr);
void V8SetReflectedNullableDOMStringAttribute(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const QualifiedName& content_attr);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_GENERATED_CODE_HELPER_H_
