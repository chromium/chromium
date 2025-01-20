// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_ctype_traits.h"
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

namespace bindings::internal {

ByteSpanWithInlineStorage& ByteSpanWithInlineStorage::operator=(
    const ByteSpanWithInlineStorage& r) {
  if (r.span_.data() == r.inline_storage_) {
    auto span = base::span(inline_storage_);
    span.copy_from(base::span(r.inline_storage_));
    span_ = span.first(r.span_.size());
  } else {
    span_ = r.span_;
  }
  return *this;
}

}  // namespace bindings::internal

}  // namespace blink
