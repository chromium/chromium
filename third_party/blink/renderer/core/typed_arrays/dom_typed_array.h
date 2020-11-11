// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_TYPED_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_TYPED_ARRAY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_shared_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "v8/include/v8.h"

namespace blink {

namespace {
// Helper to verify that a given sub-range of an ArrayBuffer is within range.
template <typename T>
bool VerifySubRange(const DOMArrayBufferBase* buffer,
                    size_t byte_offset,
                    size_t num_elements) {
  if (!buffer)
    return false;
  if (sizeof(T) > 1 && byte_offset % sizeof(T))
    return false;
  if (byte_offset > buffer->ByteLength())
    return false;
  size_t remaining_elements = (buffer->ByteLength() - byte_offset) / sizeof(T);
  if (num_elements > remaining_elements)
    return false;
  return true;
}
}  // namespace

template <typename T, typename V8TypedArray, bool clamped = false>
class DOMTypedArray final : public DOMArrayBufferView {
  typedef DOMTypedArray<T, V8TypedArray, clamped> ThisType;
  DECLARE_WRAPPERTYPEINFO();

 public:
  typedef T ValueType;
  static ThisType* Create(DOMArrayBufferBase* buffer,
                          size_t byte_offset,
                          size_t length) {
    CHECK(VerifySubRange<ValueType>(buffer, byte_offset, length));
    return MakeGarbageCollected<ThisType>(buffer, byte_offset, length);
  }

  static ThisType* Create(size_t length) {
    DOMArrayBuffer* buffer = DOMArrayBuffer::Create(length, sizeof(ValueType));
    return Create(buffer, 0, length);
  }

  static ThisType* Create(const ValueType* array, size_t length) {
    DOMArrayBuffer* buffer =
        DOMArrayBuffer::Create(array, length * sizeof(ValueType));
    return Create(buffer, 0, length);
  }

  static ThisType* CreateOrNull(size_t length) {
    DOMArrayBuffer* buffer =
        DOMArrayBuffer::CreateOrNull(length, sizeof(ValueType));
    return buffer ? Create(std::move(buffer), 0, length) : nullptr;
  }

  static ThisType* CreateUninitializedOrNull(size_t length) {
    DOMArrayBuffer* buffer =
        DOMArrayBuffer::CreateUninitializedOrNull(length, sizeof(ValueType));
    return buffer ? Create(buffer, 0, length) : nullptr;
  }

  DOMTypedArray(DOMArrayBufferBase* dom_array_buffer,
                size_t byte_offset,
                size_t length)
      : DOMArrayBufferView(dom_array_buffer, byte_offset),
        raw_length_(length) {}

  ValueType* Data() const { return static_cast<ValueType*>(BaseAddress()); }

  ValueType* DataMaybeShared() const {
    return reinterpret_cast<ValueType*>(BaseAddressMaybeShared());
  }

  size_t length() const { return !IsDetached() ? raw_length_ : 0; }

  size_t byteLength() const final { return length() * sizeof(ValueType); }

  unsigned TypeSize() const final { return sizeof(ValueType); }

  DOMArrayBufferView::ViewType GetType() const override;

  // Invoked by the indexed getter. Does not perform range checks; caller
  // is responsible for doing so and returning undefined as necessary.
  ValueType Item(size_t index) const {
    SECURITY_DCHECK(index < length());
    return Data()[index];
  }

  v8::Local<v8::Value> Wrap(v8::Isolate*,
                            v8::Local<v8::Object> creation_context) override;

 private:
  // It may be stale after Detach. Use length() instead.
  size_t raw_length_;
};

#define FOREACH_VIEW_TYPE(V)                   \
  V(int8_t, v8::Int8Array, kTypeInt8)          \
  V(int16_t, v8::Int16Array, kTypeInt16)       \
  V(int32_t, v8::Int32Array, kTypeInt32)       \
  V(uint8_t, v8::Uint8Array, kTypeUint8)       \
  V(uint16_t, v8::Uint16Array, kTypeUint16)    \
  V(uint32_t, v8::Uint32Array, kTypeUint32)    \
  V(float, v8::Float32Array, kTypeFloat32)     \
  V(double, v8::Float64Array, kTypeFloat64)    \
  V(int64_t, v8::BigInt64Array, kTypeBigInt64) \
  V(uint64_t, v8::BigUint64Array, kTypeBigUint64)

#define GET_TYPE(c_type, v8_type, view_type)               \
  template <>                                              \
  inline DOMArrayBufferView::ViewType                      \
  DOMTypedArray<c_type, v8_type, false>::GetType() const { \
    return DOMArrayBufferView::view_type;                  \
  }

FOREACH_VIEW_TYPE(GET_TYPE)

#undef GET_TYPE
#undef FOREACH_VIEW_TYPE

template <>
inline DOMArrayBufferView::ViewType
DOMTypedArray<uint8_t, v8::Uint8ClampedArray, true>::GetType() const {
  return DOMArrayBufferView::kTypeUint8Clamped;
}

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<int8_t, v8::Int8Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<int16_t, v8::Int16Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<int32_t, v8::Int32Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<uint8_t, v8::Uint8Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<uint8_t, v8::Uint8ClampedArray, /*clamped=*/true>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<uint16_t, v8::Uint16Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<uint32_t, v8::Uint32Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<int64_t, v8::BigInt64Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<uint64_t, v8::BigUint64Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<float, v8::Float32Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<double, v8::Float64Array>;

typedef DOMTypedArray<int8_t, v8::Int8Array> DOMInt8Array;
typedef DOMTypedArray<int16_t, v8::Int16Array> DOMInt16Array;
typedef DOMTypedArray<int32_t, v8::Int32Array> DOMInt32Array;
typedef DOMTypedArray<uint8_t, v8::Uint8Array> DOMUint8Array;
typedef DOMTypedArray<uint8_t, v8::Uint8ClampedArray, /*clamped=*/true>
    DOMUint8ClampedArray;
typedef DOMTypedArray<uint16_t, v8::Uint16Array> DOMUint16Array;
typedef DOMTypedArray<uint32_t, v8::Uint32Array> DOMUint32Array;
typedef DOMTypedArray<int64_t, v8::BigInt64Array> DOMBigInt64Array;
typedef DOMTypedArray<uint64_t, v8::BigUint64Array> DOMBigUint64Array;
typedef DOMTypedArray<float, v8::Float32Array> DOMFloat32Array;
typedef DOMTypedArray<double, v8::Float64Array> DOMFloat64Array;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_TYPED_ARRAY_H_
