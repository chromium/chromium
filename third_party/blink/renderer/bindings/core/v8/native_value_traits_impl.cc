// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/custom/v8_custom_xpath_ns_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/js_event_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_ctype_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_xpath_ns_resolver.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "v8/include/v8-fast-api-calls.h"

namespace blink {

namespace bindings {

static_assert(static_cast<IntegerConversionConfiguration>(
                  IDLIntegerConvMode::kDefault) == kNormalConversion,
              "IDLIntegerConvMode::kDefault == kNormalConversion");
static_assert(static_cast<IntegerConversionConfiguration>(
                  IDLIntegerConvMode::kClamp) == kClamp,
              "IDLIntegerConvMode::kClamp == kClamp");
static_assert(static_cast<IntegerConversionConfiguration>(
                  IDLIntegerConvMode::kEnforceRange) == kEnforceRange,
              "IDLIntegerConvMode::kEnforceRange == kEnforceRange");

void NativeValueTraitsInterfaceNotOfType(
    const WrapperTypeInfo* wrapper_type_info,
    ExceptionState& exception_state) {
  exception_state.ThrowTypeError(ExceptionMessages::FailedToConvertJSValue(
      wrapper_type_info->interface_name));
}

void NativeValueTraitsInterfaceNotOfType(
    const WrapperTypeInfo* wrapper_type_info,
    int argument_index,
    ExceptionState& exception_state) {
  exception_state.ThrowTypeError(ExceptionMessages::ArgumentNotOfType(
      argument_index, wrapper_type_info->interface_name));
}

template <>
CORE_TEMPLATE_EXPORT typename NativeValueTraits<IDLSequence<IDLLong>>::ImplType
CreateIDLSequenceFromV8Array<IDLLong>(v8::Isolate* isolate,
                                      v8::Local<v8::Array> v8_array,
                                      ExceptionState& exception_state) {
  typename NativeValueTraits<IDLSequence<IDLLong>>::ImplType result;

  // https://webidl.spec.whatwg.org/#create-sequence-from-iterable
  const uint32_t length = v8_array->Length();
  if (length >
      NativeValueTraits<IDLSequence<IDLLong>>::ImplType::MaxCapacity()) {
    exception_state.ThrowRangeError("Array length exceeds supported limit.");
    return {};
  }

  result.ReserveInitialCapacity(length);
  result.resize(length);
  if (v8::TryToCopyAndConvertArrayToCppBuffer<
          V8CTypeTraits<IDLLong>::kCTypeInfo.GetId()>(v8_array, result.data(),
                                                      length)) {
    return result;
  }

  // Slow path
  return bindings::CreateIDLSequenceFromV8ArraySlow<IDLLong>(isolate, v8_array,
                                                             exception_state);
}

}  // namespace bindings

// EventHandler
EventListener* NativeValueTraits<IDLEventHandler>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return JSEventHandler::CreateOrNull(
      value, JSEventHandler::HandlerType::kEventHandler);
}

EventListener* NativeValueTraits<IDLOnBeforeUnloadEventHandler>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return JSEventHandler::CreateOrNull(
      value, JSEventHandler::HandlerType::kOnBeforeUnloadEventHandler);
}

EventListener* NativeValueTraits<IDLOnErrorEventHandler>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  return JSEventHandler::CreateOrNull(
      value, JSEventHandler::HandlerType::kOnErrorEventHandler);
}

// Workaround https://crbug.com/345529
XPathNSResolver* NativeValueTraits<XPathNSResolver>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (XPathNSResolver* xpath_ns_resolver =
          V8XPathNSResolver::ToImplWithTypeCheck(isolate, value)) {
    return xpath_ns_resolver;
  }
  if (value->IsObject()) {
    ScriptState* script_state = ScriptState::From(isolate->GetCurrentContext());
    return MakeGarbageCollected<V8CustomXPathNSResolver>(
        script_state, value.As<v8::Object>());
  }

  exception_state.ThrowTypeError(
      ExceptionMessages::FailedToConvertJSValue("XPathNSResolver"));
  return nullptr;
}

XPathNSResolver* NativeValueTraits<XPathNSResolver>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (XPathNSResolver* xpath_ns_resolver =
          V8XPathNSResolver::ToImplWithTypeCheck(isolate, value)) {
    return xpath_ns_resolver;
  }
  if (value->IsObject()) {
    ScriptState* script_state = ScriptState::From(isolate->GetCurrentContext());
    return MakeGarbageCollected<V8CustomXPathNSResolver>(
        script_state, value.As<v8::Object>());
  }

  exception_state.ThrowTypeError(
      ExceptionMessages::ArgumentNotOfType(argument_index, "XPathNSResolver"));
  return nullptr;
}

XPathNSResolver* NativeValueTraits<IDLNullable<XPathNSResolver>>::NativeValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (XPathNSResolver* xpath_ns_resolver =
          V8XPathNSResolver::ToImplWithTypeCheck(isolate, value)) {
    return xpath_ns_resolver;
  }
  if (value->IsObject()) {
    ScriptState* script_state = ScriptState::From(isolate->GetCurrentContext());
    return MakeGarbageCollected<V8CustomXPathNSResolver>(
        script_state, value.As<v8::Object>());
  }
  if (value->IsNullOrUndefined())
    return nullptr;

  exception_state.ThrowTypeError(
      ExceptionMessages::FailedToConvertJSValue("XPathNSResolver"));
  return nullptr;
}

XPathNSResolver* NativeValueTraits<IDLNullable<XPathNSResolver>>::ArgumentValue(
    v8::Isolate* isolate,
    int argument_index,
    v8::Local<v8::Value> value,
    ExceptionState& exception_state) {
  if (XPathNSResolver* xpath_ns_resolver =
          V8XPathNSResolver::ToImplWithTypeCheck(isolate, value)) {
    return xpath_ns_resolver;
  }
  if (value->IsObject()) {
    ScriptState* script_state = ScriptState::From(isolate->GetCurrentContext());
    return MakeGarbageCollected<V8CustomXPathNSResolver>(
        script_state, value.As<v8::Object>());
  }
  if (value->IsNullOrUndefined())
    return nullptr;

  exception_state.ThrowTypeError(
      ExceptionMessages::ArgumentNotOfType(argument_index, "XPathNSResolver"));
  return nullptr;
}

}  // namespace blink
