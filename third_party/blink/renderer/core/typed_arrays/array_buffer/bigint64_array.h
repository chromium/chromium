// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_BIGINT64_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_BIGINT64_ARRAY_H_

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/typed_array_base.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class BigInt64Array final : public TypedArrayBase<int64_t> {
 public:
  static inline scoped_refptr<BigInt64Array> Create(unsigned length);
  static inline scoped_refptr<BigInt64Array> Create(const int64_t* array,
                                                    unsigned length);
  static inline scoped_refptr<BigInt64Array> Create(scoped_refptr<ArrayBuffer>,
                                                    unsigned byte_offset,
                                                    unsigned length);

  // Should only be used when it is known the entire array will be filled. Do
  // not return these results directly to JavaScript without filling first.
  static inline scoped_refptr<BigInt64Array> CreateUninitialized(
      unsigned length);

  using TypedArrayBase<int64_t>::Set;

  void Set(unsigned index, int64_t value) {
    if (index >= TypedArrayBase<int64_t>::length_)
      return;
    TypedArrayBase<int64_t>::Data()[index] = value;
  }

  ViewType GetType() const override { return kTypeBigInt64; }

 private:
  inline BigInt64Array(scoped_refptr<ArrayBuffer>,
                       unsigned byte_offset,
                       unsigned length);
  // Make constructor visible to superclass.
  friend class TypedArrayBase<int64_t>;
};

scoped_refptr<BigInt64Array> BigInt64Array::Create(unsigned length) {
  return TypedArrayBase<int64_t>::Create<BigInt64Array>(length);
}

scoped_refptr<BigInt64Array> BigInt64Array::Create(const int64_t* array,
                                                   unsigned length) {
  return TypedArrayBase<int64_t>::Create<BigInt64Array>(array, length);
}

scoped_refptr<BigInt64Array> BigInt64Array::Create(
    scoped_refptr<ArrayBuffer> buffer,
    unsigned byte_offset,
    unsigned length) {
  return TypedArrayBase<int64_t>::Create<BigInt64Array>(std::move(buffer),
                                                        byte_offset, length);
}

BigInt64Array::BigInt64Array(scoped_refptr<ArrayBuffer> buffer,
                             unsigned byte_offset,
                             unsigned length)
    : TypedArrayBase<int64_t>(std::move(buffer), byte_offset, length) {}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_BIGINT64_ARRAY_H_
