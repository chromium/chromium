/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_TYPED_ARRAY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_TYPED_ARRAY_BASE_H_

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_view.h"

namespace blink {

template <typename T>
class TypedArrayBase : public ArrayBufferView {
 public:
  typedef T ValueType;

  T* Data() const { return static_cast<T*>(BaseAddress()); }
  T* DataMaybeShared() const {
    return static_cast<T*>(BaseAddressMaybeShared());
  }

  bool Set(TypedArrayBase<T>* array, unsigned offset) {
    return SetImpl(array, offset * sizeof(T));
  }

  // Overridden from ArrayBufferView. This must be public because of
  // rules about inheritance of members in template classes, and
  // because it is accessed via pointers to subclasses.
  unsigned length() const { return length_; }

  unsigned ByteLength() const final { return length_ * sizeof(T); }

  unsigned TypeSize() const final { return sizeof(T); }

  // Invoked by the indexed getter. Does not perform range checks; caller
  // is responsible for doing so and returning undefined as necessary.
  T Item(unsigned index) const {
    SECURITY_DCHECK(index < TypedArrayBase<T>::length_);
    return TypedArrayBase<T>::Data()[index];
  }

 protected:
  TypedArrayBase(scoped_refptr<ArrayBuffer> buffer,
                 unsigned byte_offset,
                 unsigned length)
      : ArrayBufferView(std::move(buffer), byte_offset), length_(length) {}

  template <class Subclass>
  static scoped_refptr<Subclass> Create(unsigned length) {
    scoped_refptr<ArrayBuffer> buffer = ArrayBuffer::Create(length, sizeof(T));
    return Create<Subclass>(std::move(buffer), 0, length);
  }

  template <class Subclass>
  static scoped_refptr<Subclass> Create(const T* array, unsigned length) {
    scoped_refptr<Subclass> a = Create<Subclass>(length);
    if (a) {
      std::memcpy(a->Data(), array, a->ByteLength());
    }
    return a;
  }

  template <class Subclass>
  static scoped_refptr<Subclass> Create(scoped_refptr<ArrayBuffer> buffer,
                                        unsigned byte_offset,
                                        unsigned length) {
    CHECK(VerifySubRange<T>(buffer.get(), byte_offset, length));
    return base::AdoptRef(new Subclass(std::move(buffer), byte_offset, length));
  }

  template <class Subclass>
  static scoped_refptr<Subclass> CreateOrNull(unsigned length) {
    scoped_refptr<ArrayBuffer> buffer =
        ArrayBuffer::CreateOrNull(length, sizeof(T));
    if (!buffer)
      return nullptr;
    return Create<Subclass>(std::move(buffer), 0, length);
  }

  template <class Subclass>
  static scoped_refptr<Subclass> CreateUninitializedOrNull(unsigned length) {
    scoped_refptr<ArrayBuffer> buffer =
        ArrayBuffer::CreateUninitializedOrNull(length, sizeof(T));
    if (!buffer)
      return nullptr;
    return Create<Subclass>(std::move(buffer), 0, length);
  }

  void Detach() final {
    ArrayBufferView::Detach();
    length_ = 0;
  }

  // We do not want to have to access this via a virtual function in subclasses,
  // which is why it is protected rather than private.
  unsigned length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_TYPED_ARRAY_BASE_H_
