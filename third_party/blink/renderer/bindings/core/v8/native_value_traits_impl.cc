// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_ctype_traits.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/string_resource.h"
#include "third_party/blink/renderer/platform/bindings/to_blink_string.h"
#include "v8/include/v8-fast-api-calls.h"

namespace blink {

namespace bindings {

StringView NativeValueTraitsStringAdapter::ToStringView() const& {
  if (v8_string_.IsEmpty()) [[unlikely]] {
    return wtf_string_;
  }

  // Be very careful in this code to ensure it is RVO friendly. Accidentally
  // breaking RVO will degrade some of the blink_perf benchmarks by a few
  // percent. This includes moving the StringTraits<>::FromStringResource() call
  // into GetExternalizedString() as it becomes impossible for the calling code
  // to satisfy all RVO constraints.
  if (StringResourceBase* string_resource =
          StringResourceBase::GetExternalizedString(isolate_, v8_string_)) {
    // Note that we request (and keep) a non-atomic String, but the underlying
    // string is likely already atomic as per externalization logic below.
    // Transition from a WTF::String with an atomic impl to WTF::AtomicString
    // is trivial and can be done on the blink impl side as required.
    wtf_string_ = string_resource->GetWTFString();
    return wtf_string_.Impl();
  }

  uint32_t length = v8_string_->Length();
  if (!length) [[unlikely]] {
    return StringView(g_empty_atom);
  }

  // Note that this code path looks very similar to ToBlinkString(). The
  // critical difference in ToStringView(), if `can_externalize` is false,
  // there is no attempt to create either an AtomicString or an String. This
  // can very likely avoid a heap allocation and definitely avoids refcount
  // churn which can be significantly faster in some hot paths.
  const bool is_one_byte = v8_string_->IsOneByte();
  const v8::String::Encoding requested_encoding =
      is_one_byte ? v8::String::ONE_BYTE_ENCODING
                  : v8::String::TWO_BYTE_ENCODING;
  bool can_externalize = v8_string_->CanMakeExternal(requested_encoding);
  if (can_externalize) [[likely]] {
    // An AtomicString is always used here for externalization. Using a String
    // would avoid the AtomicStringTable insert however it also means APIs
    // consuming the returned StringView must do O(l) operations on equality
    // checking.
    //
    // Given that externalization implies reuse of the string, taking the single
    // O(l) hit to insert into the AtomicStringTable ends up being faster in
    // most cases.
    //
    // If the caller explicitly wants a String, then using ToBlinkString<String>
    // is the better option.
    //
    // If the caller wants a disposable serialization where it knows the
    // v8::String is unlikely to be re-projected into Blink (seems rare?) then
    // calling this with kDoNotExternalize and relying on the
    // StringView::StackBackingStore yields the most efficient code.
    AtomicString blink_string =
        ToBlinkString<AtomicString>(isolate_, v8_string_, kExternalize);
    if (v8_string_->IsExternal()) {
      return StringView(blink_string.Impl());
    }
  }

  // The string has not been externalized. Serialize into
  // `string_view_backing_store_` and return.
  //
  // Note on platforms with v8 pointer compression, this is the hot path
  // for short strings like "id" as those are never externalized whereas on
  // platforms without pointer compression GetExternalizedString() is the hot
  // path.
  //
  // This is particularly important when optimizing for blink_perf.bindings as
  // x64 vs ARM performance will have very different behavior; x64 has
  // pointer compression but ARM does not. Since a common string used in the
  // {get,set}-attribute benchmarks is "id", this means optimizations
  // that affect the microbenchmark in one architecture likely have no effect
  // (or even a negative effect due to different expectations in branch
  // prediction) in the other.
  //
  // When pointer compression is on, short strings always cause a
  // serialization to Blink and thus if there are 1000 runs of an API
  // asking to convert the same `v8_string` to a Blink string, each run will
  // behavior similarly.
  //
  // When pointer compression is off, the first run will externalize the string
  // going through this path, but subsequent runs will enter the
  // GetExternalizedString() path and be much faster as it is just extracting
  // a pointer.
  //
  // Confusingly, the ARM and x64 absolute numbers for the benchmarks look
  // similar (80-90 runs/s on a pixel2 and a Lenovo P920). This can give the
  // mistaken belief that they are related numbers even though they are
  // testing almost entirely completely different codepaths. When optimizing
  // this code, it is instructive to increase the test attribute name string
  // length. Using something like something like "abcd1234" will make all
  // platforms externalize and x64 will likely run much much faster (local
  // test sees 260 runs/s on a x64 P920).
  //
  // TODO(ajwong): Revisit if the length restriction on externalization makes
  // sense. It's odd that pointer compression changes externalization
  // behavior.
  if (is_one_byte) {
    base::span<LChar> lchar = string_view_backing_store_.Realloc<LChar>(length);
    v8_string_->WriteOneByteV2(isolate_, 0, length, lchar.data());
    return StringView(lchar);
  }

  base::span<UChar> uchar = string_view_backing_store_.Realloc<UChar>(length);
  static_assert(sizeof(UChar) == sizeof(uint16_t),
                "UChar isn't the same as uint16_t");
  v8_string_->WriteV2(isolate_, 0, length,
                      reinterpret_cast<uint16_t*>(uchar.data()));
  return StringView(uchar);
}

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

bool ThrowIfResizable(v8::Local<v8::ArrayBuffer> array_buffer,
                      ExceptionState& exception_state) {
  if (array_buffer->IsResizableByUserJavaScript()) {
    exception_state.ThrowTypeError(
        "The provided ArrayBuffer value must not be resizable");
    return false;
  }
  return true;
}

bool ThrowIfResizable(v8::Local<v8::SharedArrayBuffer> shared_array_buffer,
                      ExceptionState& exception_state) {
  if (shared_array_buffer->GetBackingStore()->IsResizableByUserJavaScript()) {
    exception_state.ThrowTypeError(
        "The provided SharedArrayBuffer value must not be resizable");
    return false;
  }
  return true;
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

}  // namespace blink
