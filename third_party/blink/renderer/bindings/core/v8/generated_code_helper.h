// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides utilities to be used only by generated bindings code.
//
// CAUTION: Do not use this header outside generated bindings code.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_GENERATED_CODE_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_GENERATED_CODE_HELPER_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8.h"

namespace blink {

class DOMParser;
class Document;
class ExecutionContext;
class QualifiedName;
class Range;
class ScriptState;

CORE_EXPORT void V8ConstructorAttributeGetter(
    v8::Local<v8::Name> property_name,
    const v8::PropertyCallbackInfo<v8::Value>&,
    const WrapperTypeInfo*);

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

namespace bindings {

// Returns the length of arguments ignoring the undefined values at the end.
inline int NonUndefinedArgumentLength(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  for (int index = info.Length() - 1; 0 <= index; --index) {
    if (!info[index]->IsUndefined())
      return index + 1;
  }
  return 0;
}

template <typename T, typename... ExtraArgs>
typename IDLSequence<T>::ImplType VariadicArgumentsToNativeValues(
    v8::Isolate* isolate,
    const v8::FunctionCallbackInfo<v8::Value>& info,
    int start_index,
    ExceptionState& exception_state,
    ExtraArgs... extra_args) {
  using VectorType = typename IDLSequence<T>::ImplType;

  const int length = info.Length();
  if (start_index >= length)
    return VectorType();

  VectorType result;
  result.ReserveInitialCapacity(length - start_index);
  for (int i = start_index; i < length; ++i) {
    result.UncheckedAppend(NativeValueTraits<T>::ArgumentValue(
        isolate, i, info[i], exception_state, extra_args...));
    if (exception_state.HadException())
      return VectorType();
  }
  return std::move(result);
}

CORE_EXPORT base::Optional<size_t> FindIndexInEnumStringTable(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    base::span<const char* const> enum_value_table,
    const char* enum_type_name,
    ExceptionState& exception_state);

CORE_EXPORT base::Optional<size_t> FindIndexInEnumStringTable(
    const String& str_value,
    base::span<const char* const> enum_value_table);

CORE_EXPORT void ReportInvalidEnumSetToAttribute(
    v8::Isolate* isolate,
    const String& value,
    const String& enum_type_name,
    ExceptionState& exception_state);

CORE_EXPORT bool IsEsIterableObject(v8::Isolate* isolate,
                                    v8::Local<v8::Value> value,
                                    ExceptionState& exception_state);

CORE_EXPORT Document* ToDocumentFromExecutionContext(
    ExecutionContext* execution_context);

// This function is mostly used for EventTargets, and so this version is
// inlined. The less commonly used overloads are defined in the .cc file.
CORE_EXPORT inline ExecutionContext* ExecutionContextFromV8Wrappable(
    const EventTarget* event_target) {
  return event_target->GetExecutionContext();
}
CORE_EXPORT ExecutionContext* ExecutionContextFromV8Wrappable(
    const Range* range);
CORE_EXPORT ExecutionContext* ExecutionContextFromV8Wrappable(
    const DOMParser* parser);

CORE_EXPORT v8::MaybeLocal<v8::Value> CreateNamedConstructorFunction(
    ScriptState* script_state,
    v8::FunctionCallback callback,
    const char* func_name,
    int func_length,
    const WrapperTypeInfo* wrapper_type_info);

CORE_EXPORT void InstallUnscopablePropertyNames(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> prototype_object,
    base::span<const char* const> property_name_table);

CORE_EXPORT v8::Local<v8::Array> EnumerateIndexedProperties(
    v8::Isolate* isolate,
    uint32_t length);

// Performs the ES value to IDL value conversion of IDL dictionary member.
// Sets a dictionary member |value| and |presence| to the resulting values.
// Returns true on success, otherwise returns false and throws an exception.
//
// |try_block| must be the innermost v8::TryCatch and it's used to internally
// capture an exception, which is rethrown in |exception_state|.
template <typename NVTTag, bool is_required, typename T>
bool ConvertDictionaryMember(v8::Isolate* isolate,
                             v8::Local<v8::Context> current_context,
                             v8::Local<v8::Object> v8_dictionary,
                             v8::Local<v8::Name> v8_member_name,
                             const char* dictionary_name,
                             const char* member_name,
                             T& value,
                             bool& presence,
                             v8::TryCatch& try_block,
                             ExceptionState& exception_state) {
  v8::Local<v8::Value> v8_value;
  if (!v8_dictionary->Get(current_context, v8_member_name).ToLocal(&v8_value)) {
    exception_state.RethrowV8Exception(try_block.Exception());
    try_block.Reset();
    return false;
  }

  if (v8_value->IsUndefined()) {
    if (is_required) {
      exception_state.ThrowTypeError(ExceptionMessages::FailedToGet(
          member_name, dictionary_name, "Required member is undefined."));
      return false;
    }
    presence = false;
    return true;
  }

  value = NativeValueTraits<NVTTag>::NativeValue(isolate, v8_value,
                                                 exception_state);
  if (exception_state.HadException()) {
    return false;
  }
  presence = true;
  return true;
}

// [CSSProperty]
CORE_EXPORT void InstallCSSPropertyAttributes(
    v8::Isolate* isolate,
    const DOMWrapperWorld& world,
    v8::Local<v8::ObjectTemplate> instance_template,
    v8::Local<v8::ObjectTemplate> prototype_template,
    v8::Local<v8::FunctionTemplate> interface_template,
    v8::Local<v8::Signature> signature,
    base::span<const char* const> css_property_names);
CORE_EXPORT void CSSPropertyAttributeGet(
    const v8::FunctionCallbackInfo<v8::Value>& info);
CORE_EXPORT void CSSPropertyAttributeSet(
    const v8::FunctionCallbackInfo<v8::Value>& info);

// Common implementation to reduce the binary size of attribute set callbacks.
CORE_EXPORT void PerformAttributeSetCEReactionsReflectTypeBoolean(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const QualifiedName& content_attribute,
    const char* interface_name,
    const char* attribute_name);
CORE_EXPORT void PerformAttributeSetCEReactionsReflectTypeString(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const QualifiedName& content_attribute,
    const char* interface_name,
    const char* attribute_name);
CORE_EXPORT void
PerformAttributeSetCEReactionsReflectTypeStringLegacyNullToEmptyString(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const QualifiedName& content_attribute,
    const char* interface_name,
    const char* attribute_name);
CORE_EXPORT void PerformAttributeSetCEReactionsReflectTypeStringOrNull(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    const QualifiedName& content_attribute,
    const char* interface_name,
    const char* attribute_name);

}  // namespace bindings

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_GENERATED_CODE_HELPER_H_
