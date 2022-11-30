// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_TYPED_FLEXIBLE_ARRAY_BUFFER_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_TYPED_FLEXIBLE_ARRAY_BUFFER_VIEW_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/typed_arrays/flexible_array_buffer_view.h"

namespace blink {

template <typename ValueType, bool clamped = false>
class TypedFlexibleArrayBufferView final : public FlexibleArrayBufferView {
  STACK_ALLOCATED();

 public:
  TypedFlexibleArrayBufferView() = default;
  TypedFlexibleArrayBufferView(const TypedFlexibleArrayBufferView&) = default;
  TypedFlexibleArrayBufferView(TypedFlexibleArrayBufferView&&) = default;
  TypedFlexibleArrayBufferView(v8::Local<v8::ArrayBufferView> array_buffer_view)
      : FlexibleArrayBufferView(array_buffer_view) {}
  ~TypedFlexibleArrayBufferView() = default;

  ValueType* DataMaybeOnStack() const {
    return static_cast<ValueType*>(BaseAddressMaybeOnStack());
  }

  size_t length() const {
    DCHECK_EQ(ByteLength() % sizeof(ValueType), 0u);
    return ByteLength() / sizeof(ValueType);
  }
};

using FlexibleInt8Array = TypedFlexibleArrayBufferView<int8_t>;
using FlexibleInt16Array = TypedFlexibleArrayBufferView<int16_t>;
using FlexibleInt32Array = TypedFlexibleArrayBufferView<int32_t>;
using FlexibleUint8Array = TypedFlexibleArrayBufferView<uint8_t>;
using FlexibleUint8ClampedArray = TypedFlexibleArrayBufferView<uint8_t, true>;
using FlexibleUint16Array = TypedFlexibleArrayBufferView<uint16_t>;
using FlexibleUint32Array = TypedFlexibleArrayBufferView<uint32_t>;
using FlexibleBigInt64Array = TypedFlexibleArrayBufferView<int64_t>;
using FlexibleBigUint64Array = TypedFlexibleArrayBufferView<uint64_t>;
using FlexibleFloat32Array = TypedFlexibleArrayBufferView<float>;
using FlexibleFloat64Array = TypedFlexibleArrayBufferView<double>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_TYPED_FLEXIBLE_ARRAY_BUFFER_VIEW_H_
