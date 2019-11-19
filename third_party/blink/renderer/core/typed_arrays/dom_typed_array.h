// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_TYPED_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_TYPED_ARRAY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/bigint64_array.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/biguint64_array.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/float32_array.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/float64_array.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/int16_array.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/int32_array.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/int8_array.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/uint16_array.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/uint32_array.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/uint8_array.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/uint8_clamped_array.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_shared_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "v8/include/v8.h"

namespace blink {

template <typename TypedArray, typename V8TypedArray>
class DOMTypedArray final : public DOMArrayBufferView {
  typedef DOMTypedArray<TypedArray, V8TypedArray> ThisType;
  DECLARE_WRAPPERTYPEINFO();

 public:
  typedef typename TypedArray::ValueType ValueType;

  static ThisType* Create(scoped_refptr<TypedArray> buffer_view) {
    return MakeGarbageCollected<ThisType>(std::move(buffer_view));
  }
  static ThisType* Create(unsigned length) {
    return Create(TypedArray::Create(length));
  }
  static ThisType* Create(const ValueType* array, unsigned length) {
    return Create(TypedArray::Create(array, length));
  }
  static ThisType* Create(scoped_refptr<ArrayBuffer> buffer,
                          unsigned byte_offset,
                          unsigned length) {
    return Create(TypedArray::Create(std::move(buffer), byte_offset, length));
  }
  static ThisType* Create(DOMArrayBufferBase* buffer,
                          unsigned byte_offset,
                          unsigned length) {
    scoped_refptr<TypedArray> buffer_view =
        TypedArray::Create(buffer->Buffer(), byte_offset, length);
    return MakeGarbageCollected<ThisType>(std::move(buffer_view), buffer);
  }

  static ThisType* CreateOrNull(unsigned length) {
    scoped_refptr<ArrayBuffer> buffer =
        ArrayBuffer::CreateOrNull(length, sizeof(ValueType));
    return buffer ? Create(std::move(buffer), 0, length) : nullptr;
  }

  static ThisType* CreateUninitializedOrNull(unsigned length) {
    scoped_refptr<ArrayBuffer> buffer =
        ArrayBuffer::CreateOrNull(length, sizeof(ValueType));
    return buffer ? Create(std::move(buffer), 0, length) : nullptr;
  }

  explicit DOMTypedArray(scoped_refptr<TypedArray> buffer_view)
      : DOMArrayBufferView(std::move(buffer_view)) {}
  DOMTypedArray(scoped_refptr<TypedArray> buffer_view,
                DOMArrayBufferBase* dom_array_buffer)
      : DOMArrayBufferView(std::move(buffer_view), dom_array_buffer) {}

  const TypedArray* View() const {
    return static_cast<const TypedArray*>(DOMArrayBufferView::View());
  }
  TypedArray* View() {
    return static_cast<TypedArray*>(DOMArrayBufferView::View());
  }

  ValueType* Data() const { return View()->Data(); }
  ValueType* DataMaybeShared() const { return View()->DataMaybeShared(); }
  unsigned length() const { return View()->length(); }
  // Invoked by the indexed getter. Does not perform range checks; caller
  // is responsible for doing so and returning undefined as necessary.
  ValueType Item(unsigned index) const { return View()->Item(index); }

  v8::Local<v8::Object> Wrap(v8::Isolate*,
                             v8::Local<v8::Object> creation_context) override;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<Int8Array, v8::Int8Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<Int16Array, v8::Int16Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<Int32Array, v8::Int32Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<Uint8Array, v8::Uint8Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<Uint8ClampedArray, v8::Uint8ClampedArray>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<Uint16Array, v8::Uint16Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<Uint32Array, v8::Uint32Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<BigInt64Array, v8::BigInt64Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<BigUint64Array, v8::BigUint64Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<Float32Array, v8::Float32Array>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT
    DOMTypedArray<Float64Array, v8::Float64Array>;

typedef DOMTypedArray<Int8Array, v8::Int8Array> DOMInt8Array;
typedef DOMTypedArray<Int16Array, v8::Int16Array> DOMInt16Array;
typedef DOMTypedArray<Int32Array, v8::Int32Array> DOMInt32Array;
typedef DOMTypedArray<Uint8Array, v8::Uint8Array> DOMUint8Array;
typedef DOMTypedArray<Uint8ClampedArray, v8::Uint8ClampedArray>
    DOMUint8ClampedArray;
typedef DOMTypedArray<Uint16Array, v8::Uint16Array> DOMUint16Array;
typedef DOMTypedArray<Uint32Array, v8::Uint32Array> DOMUint32Array;
typedef DOMTypedArray<BigInt64Array, v8::BigInt64Array> DOMBigInt64Array;
typedef DOMTypedArray<BigUint64Array, v8::BigUint64Array> DOMBigUint64Array;
typedef DOMTypedArray<Float32Array, v8::Float32Array> DOMFloat32Array;
typedef DOMTypedArray<Float64Array, v8::Float64Array> DOMFloat64Array;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_TYPED_ARRAY_H_
