/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_INT16_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_INT16_ARRAY_H_

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/integral_typed_array_base.h"

namespace blink {

class ArrayBuffer;

class Int16Array final : public IntegralTypedArrayBase<int16_t> {
 public:
  static inline scoped_refptr<Int16Array> Create(unsigned length);
  static inline scoped_refptr<Int16Array> Create(const int16_t* array,
                                                 unsigned length);
  static inline scoped_refptr<Int16Array> Create(scoped_refptr<ArrayBuffer>,
                                                 unsigned byte_offset,
                                                 unsigned length);

  using TypedArrayBase<int16_t>::Set;
  using IntegralTypedArrayBase<int16_t>::Set;

  ViewType GetType() const override { return kTypeInt16; }

 private:
  inline Int16Array(scoped_refptr<ArrayBuffer>,
                    unsigned byte_offset,
                    unsigned length);
  // Make constructor visible to superclass.
  friend class TypedArrayBase<int16_t>;
};

scoped_refptr<Int16Array> Int16Array::Create(unsigned length) {
  return TypedArrayBase<int16_t>::Create<Int16Array>(length);
}

scoped_refptr<Int16Array> Int16Array::Create(const int16_t* array,
                                             unsigned length) {
  return TypedArrayBase<int16_t>::Create<Int16Array>(array, length);
}

scoped_refptr<Int16Array> Int16Array::Create(scoped_refptr<ArrayBuffer> buffer,
                                             unsigned byte_offset,
                                             unsigned length) {
  return TypedArrayBase<int16_t>::Create<Int16Array>(std::move(buffer),
                                                     byte_offset, length);
}

Int16Array::Int16Array(scoped_refptr<ArrayBuffer> buffer,
                       unsigned byte_offset,
                       unsigned length)
    : IntegralTypedArrayBase<int16_t>(std::move(buffer), byte_offset, length) {}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_INT16_ARRAY_H_
