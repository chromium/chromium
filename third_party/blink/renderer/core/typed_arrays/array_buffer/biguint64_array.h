// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_BIGUINT64_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_BIGUINT64_ARRAY_H_

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/typed_array_base.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class BigUint64Array final : public TypedArrayBase<uint64_t> {
 public:
  static inline scoped_refptr<BigUint64Array> Create(unsigned length);
  static inline scoped_refptr<BigUint64Array> Create(const uint64_t* array,
                                                     unsigned length);
  static inline scoped_refptr<BigUint64Array> Create(scoped_refptr<ArrayBuffer>,
                                                     unsigned byte_offset,
                                                     unsigned length);

  // Should only be used when it is known the entire array will be filled. Do
  // not return these results directly to JavaScript without filling first.
  static inline scoped_refptr<BigUint64Array> CreateUninitialized(
      unsigned length);

  using TypedArrayBase<uint64_t>::Set;

  void Set(unsigned index, uint64_t value) {
    if (index >= TypedArrayBase<uint64_t>::length_)
      return;
    TypedArrayBase<uint64_t>::Data()[index] = value;
  }

  ViewType GetType() const override { return kTypeBigUint64; }

 private:
  inline BigUint64Array(scoped_refptr<ArrayBuffer>,
                        unsigned byte_offset,
                        unsigned length);
  // Make constructor visible to superclass.
  friend class TypedArrayBase<uint64_t>;
};

scoped_refptr<BigUint64Array> BigUint64Array::Create(unsigned length) {
  return TypedArrayBase<uint64_t>::Create<BigUint64Array>(length);
}

scoped_refptr<BigUint64Array> BigUint64Array::Create(const uint64_t* array,
                                                     unsigned length) {
  return TypedArrayBase<uint64_t>::Create<BigUint64Array>(array, length);
}

scoped_refptr<BigUint64Array> BigUint64Array::Create(
    scoped_refptr<ArrayBuffer> buffer,
    unsigned byte_offset,
    unsigned length) {
  return TypedArrayBase<uint64_t>::Create<BigUint64Array>(std::move(buffer),
                                                          byte_offset, length);
}

BigUint64Array::BigUint64Array(scoped_refptr<ArrayBuffer> buffer,
                               unsigned byte_offset,
                               unsigned length)
    : TypedArrayBase<uint64_t>(std::move(buffer), byte_offset, length) {}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_BIGUINT64_ARRAY_H_
