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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_ARRAY_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_ARRAY_BUFFER_H_

#include "base/allocator/partition_allocator/oom.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class ArrayBuffer;
class ArrayBufferView;

class CORE_EXPORT ArrayBuffer : public RefCounted<ArrayBuffer> {
  USING_FAST_MALLOC(ArrayBuffer);

 public:
  static inline scoped_refptr<ArrayBuffer> Create(size_t num_elements,
                                                  size_t element_byte_size);
  static inline scoped_refptr<ArrayBuffer> Create(ArrayBuffer*);
  static inline scoped_refptr<ArrayBuffer> Create(const void* source,
                                                  size_t byte_length);
  static inline scoped_refptr<ArrayBuffer> Create(ArrayBufferContents&);

  static inline scoped_refptr<ArrayBuffer> CreateOrNull(
      size_t num_elements,
      size_t element_byte_size);

  // Only for use by DOMArrayBuffer::CreateUninitializedOrNull().
  static inline scoped_refptr<ArrayBuffer> CreateUninitializedOrNull(
      size_t num_elements,
      size_t element_byte_size);

  static inline scoped_refptr<ArrayBuffer> CreateShared(
      size_t num_elements,
      size_t element_byte_size);
  static inline scoped_refptr<ArrayBuffer> CreateShared(const void* source,
                                                        size_t byte_length);

  inline void* Data();
  inline const void* Data() const;
  inline void* DataShared();
  inline const void* DataShared() const;
  inline void* DataMaybeShared();
  inline const void* DataMaybeShared() const;
  inline size_t ByteLengthAsSizeT() const;
  // This function is deprecated and should not be used. Use {ByteLengthAsSizeT}
  // instead.
  inline unsigned ByteLengthAsUnsigned() const;

  // Creates a new ArrayBuffer object with copy of bytes in this object
  // ranging from |begin| up to but not including |end|.
  inline scoped_refptr<ArrayBuffer> Slice(unsigned begin, unsigned end) const;

  void AddView(ArrayBufferView*);
  void RemoveView(ArrayBufferView*);

  bool Transfer(ArrayBufferContents&);
  bool ShareContentsWith(ArrayBufferContents&);
  // Documentation see DOMArrayBuffer.
  bool ShareNonSharedForInternalUse(ArrayBufferContents&);
  bool IsDetached() const { return is_detached_; }
  bool IsShared() const { return contents_.IsShared(); }

  ~ArrayBuffer() = default;

 protected:
  explicit ArrayBuffer(ArrayBufferContents&);

 private:
  static inline scoped_refptr<ArrayBuffer> Create(
      size_t num_elements,
      size_t element_byte_size,
      ArrayBufferContents::InitializationPolicy);
  static inline scoped_refptr<ArrayBuffer> CreateOrNull(
      size_t num_elements,
      size_t element_byte_size,
      ArrayBufferContents::InitializationPolicy);
  static inline scoped_refptr<ArrayBuffer> CreateShared(
      size_t num_elements,
      size_t element_byte_size,
      ArrayBufferContents::InitializationPolicy);

  inline unsigned ClampIndex(unsigned index) const;

  ArrayBufferContents contents_;
  HashSet<ArrayBufferView*> views_;
  bool is_detached_;
};

scoped_refptr<ArrayBuffer> ArrayBuffer::Create(size_t num_elements,
                                               size_t element_byte_size) {
  return Create(num_elements, element_byte_size,
                ArrayBufferContents::kZeroInitialize);
}

scoped_refptr<ArrayBuffer> ArrayBuffer::Create(ArrayBuffer* other) {
  // TODO(binji): support creating a SharedArrayBuffer by copying another
  // ArrayBuffer?
  DCHECK(!other->IsShared());
  return ArrayBuffer::Create(other->Data(), other->ByteLengthAsSizeT());
}

scoped_refptr<ArrayBuffer> ArrayBuffer::Create(const void* source,
                                               size_t byte_length) {
  ArrayBufferContents contents(byte_length, 1, ArrayBufferContents::kNotShared,
                               ArrayBufferContents::kDontInitialize);
  if (UNLIKELY(!contents.Data()))
    OOM_CRASH();
  scoped_refptr<ArrayBuffer> buffer = base::AdoptRef(new ArrayBuffer(contents));
  memcpy(buffer->Data(), source, byte_length);
  return buffer;
}

scoped_refptr<ArrayBuffer> ArrayBuffer::Create(ArrayBufferContents& contents) {
  CHECK(contents.DataLength() == 0 || contents.DataMaybeShared());
  return base::AdoptRef(new ArrayBuffer(contents));
}

scoped_refptr<ArrayBuffer> ArrayBuffer::CreateOrNull(size_t num_elements,
                                                     size_t element_byte_size) {
  return CreateOrNull(num_elements, element_byte_size,
                      ArrayBufferContents::kZeroInitialize);
}

scoped_refptr<ArrayBuffer> ArrayBuffer::CreateUninitializedOrNull(
    size_t num_elements,
    size_t element_byte_size) {
  return CreateOrNull(num_elements, element_byte_size,
                      ArrayBufferContents::kDontInitialize);
}

scoped_refptr<ArrayBuffer> ArrayBuffer::Create(
    size_t num_elements,
    size_t element_byte_size,
    ArrayBufferContents::InitializationPolicy policy) {
  ArrayBufferContents contents(num_elements, element_byte_size,
                               ArrayBufferContents::kNotShared, policy);
  if (UNLIKELY(!contents.Data()))
    OOM_CRASH();
  return base::AdoptRef(new ArrayBuffer(contents));
}

scoped_refptr<ArrayBuffer> ArrayBuffer::CreateOrNull(
    size_t num_elements,
    size_t element_byte_size,
    ArrayBufferContents::InitializationPolicy policy) {
  ArrayBufferContents contents(num_elements, element_byte_size,
                               ArrayBufferContents::kNotShared, policy);
  if (!contents.Data())
    return nullptr;
  return base::AdoptRef(new ArrayBuffer(contents));
}

scoped_refptr<ArrayBuffer> ArrayBuffer::CreateShared(size_t num_elements,
                                                     size_t element_byte_size) {
  return CreateShared(num_elements, element_byte_size,
                      ArrayBufferContents::kZeroInitialize);
}

scoped_refptr<ArrayBuffer> ArrayBuffer::CreateShared(const void* source,
                                                     size_t byte_length) {
  ArrayBufferContents contents(byte_length, 1, ArrayBufferContents::kShared,
                               ArrayBufferContents::kDontInitialize);
  CHECK(contents.DataShared());
  scoped_refptr<ArrayBuffer> buffer = base::AdoptRef(new ArrayBuffer(contents));
  memcpy(buffer->DataShared(), source, byte_length);
  return buffer;
}

scoped_refptr<ArrayBuffer> ArrayBuffer::CreateShared(
    size_t num_elements,
    size_t element_byte_size,
    ArrayBufferContents::InitializationPolicy policy) {
  ArrayBufferContents contents(num_elements, element_byte_size,
                               ArrayBufferContents::kShared, policy);
  CHECK(contents.DataShared());
  return base::AdoptRef(new ArrayBuffer(contents));
}

void* ArrayBuffer::Data() {
  return contents_.Data();
}

const void* ArrayBuffer::Data() const {
  return contents_.Data();
}

void* ArrayBuffer::DataShared() {
  return contents_.DataShared();
}

const void* ArrayBuffer::DataShared() const {
  return contents_.DataShared();
}

void* ArrayBuffer::DataMaybeShared() {
  return contents_.DataMaybeShared();
}

const void* ArrayBuffer::DataMaybeShared() const {
  return contents_.DataMaybeShared();
}

size_t ArrayBuffer::ByteLengthAsSizeT() const {
  return contents_.DataLength();
}

// This function is deprecated and should not be used. Use {ByteLengthAsSizeT}
// instead.
unsigned ArrayBuffer::ByteLengthAsUnsigned() const {
  CHECK_LE(contents_.DataLength(),
           static_cast<size_t>(std::numeric_limits<unsigned>::max()));
  // TODO(dtapuska): Revisit this cast. ArrayBufferContents
  // uses size_t for storing data. Whereas ArrayBuffer IDL is
  // only uint32_t based.
  return static_cast<unsigned>(contents_.DataLength());
}

scoped_refptr<ArrayBuffer> ArrayBuffer::Slice(unsigned begin,
                                              unsigned end) const {
  begin = ClampIndex(begin);
  end = ClampIndex(end);
  size_t size = static_cast<size_t>(begin <= end ? end - begin : 0);
  return ArrayBuffer::Create(static_cast<const char*>(Data()) + begin, size);
}

unsigned ArrayBuffer::ClampIndex(unsigned index) const {
  return index < ByteLengthAsUnsigned() ? index : ByteLengthAsUnsigned();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_ARRAY_BUFFER_H_
