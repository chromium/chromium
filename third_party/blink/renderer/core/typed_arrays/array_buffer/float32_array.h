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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_FLOAT32_ARRAY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_FLOAT32_ARRAY_H_

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/typed_array_base.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

class Float32Array final : public TypedArrayBase<float> {
 public:
  static inline scoped_refptr<Float32Array> Create(unsigned length);
  static inline scoped_refptr<Float32Array> Create(const float* array,
                                                   unsigned length);
  static inline scoped_refptr<Float32Array> Create(scoped_refptr<ArrayBuffer>,
                                                   unsigned byte_offset,
                                                   unsigned length);

  static inline scoped_refptr<Float32Array> CreateOrNull(unsigned length);

  using TypedArrayBase<float>::Set;

  void Set(unsigned index, double value) {
    if (index >= TypedArrayBase<float>::length_)
      return;
    TypedArrayBase<float>::Data()[index] = static_cast<float>(value);
  }

  ViewType GetType() const override { return kTypeFloat32; }

 private:
  inline Float32Array(scoped_refptr<ArrayBuffer>,
                      unsigned byte_offset,
                      unsigned length);
  // Make constructor visible to superclass.
  friend class TypedArrayBase<float>;
};

scoped_refptr<Float32Array> Float32Array::Create(unsigned length) {
  return TypedArrayBase<float>::Create<Float32Array>(length);
}

scoped_refptr<Float32Array> Float32Array::Create(const float* array,
                                                 unsigned length) {
  return TypedArrayBase<float>::Create<Float32Array>(array, length);
}

scoped_refptr<Float32Array> Float32Array::Create(
    scoped_refptr<ArrayBuffer> buffer,
    unsigned byte_offset,
    unsigned length) {
  return TypedArrayBase<float>::Create<Float32Array>(std::move(buffer),
                                                     byte_offset, length);
}

scoped_refptr<Float32Array> Float32Array::CreateOrNull(unsigned length) {
  return TypedArrayBase<float>::CreateOrNull<Float32Array>(length);
}

Float32Array::Float32Array(scoped_refptr<ArrayBuffer> buffer,
                           unsigned byte_offset,
                           unsigned length)
    : TypedArrayBase<float>(std::move(buffer), byte_offset, length) {}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_FLOAT32_ARRAY_H_
