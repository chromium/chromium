/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_UINT32_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_UINT32_ARRAY_H_

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/integral_typed_array_base.h"

namespace blink {

class ArrayBuffer;

class Uint32Array final : public IntegralTypedArrayBase<unsigned> {
 public:
  static inline scoped_refptr<Uint32Array> Create(unsigned length);
  static inline scoped_refptr<Uint32Array> Create(const unsigned* array,
                                                  unsigned length);
  static inline scoped_refptr<Uint32Array> Create(scoped_refptr<ArrayBuffer>,
                                                  unsigned byte_offset,
                                                  unsigned length);

  using TypedArrayBase<unsigned>::Set;
  using IntegralTypedArrayBase<unsigned>::Set;

  ViewType GetType() const override { return kTypeUint32; }

 private:
  inline Uint32Array(scoped_refptr<ArrayBuffer>,
                     unsigned byte_offset,
                     unsigned length);
  // Make constructor visible to superclass.
  friend class TypedArrayBase<unsigned>;
};

scoped_refptr<Uint32Array> Uint32Array::Create(unsigned length) {
  return TypedArrayBase<unsigned>::Create<Uint32Array>(length);
}

scoped_refptr<Uint32Array> Uint32Array::Create(const unsigned* array,
                                               unsigned length) {
  return TypedArrayBase<unsigned>::Create<Uint32Array>(array, length);
}

scoped_refptr<Uint32Array> Uint32Array::Create(
    scoped_refptr<ArrayBuffer> buffer,
    unsigned byte_offset,
    unsigned length) {
  return TypedArrayBase<unsigned>::Create<Uint32Array>(std::move(buffer),
                                                       byte_offset, length);
}

Uint32Array::Uint32Array(scoped_refptr<ArrayBuffer> buffer,
                         unsigned byte_offset,
                         unsigned length)
    : IntegralTypedArrayBase<unsigned>(std::move(buffer), byte_offset, length) {
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_UINT32_ARRAY_H_
