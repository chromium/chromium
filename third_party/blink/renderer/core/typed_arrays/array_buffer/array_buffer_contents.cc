/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "build/build_config.h"

#include <string.h>
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

ArrayBufferContents::ArrayBufferContents(void* data,
                                         size_t length,
                                         DataDeleter deleter) {
  if (!data) {
    return;
  }
  backing_store_ =
      v8::ArrayBuffer::NewBackingStore(data, length, deleter, nullptr);
}

ArrayBufferContents::ArrayBufferContents(
    size_t num_elements,
    size_t element_byte_size,
    SharingType is_shared,
    ArrayBufferContents::InitializationPolicy policy) {
  auto checked_length =
      base::CheckedNumeric<size_t>(num_elements) * element_byte_size;
  if (!checked_length.IsValid()) {
    // The requested size is too big, we cannot allocate the memory and
    // therefore just return.
    return;
  }
  size_t length = checked_length.ValueOrDie();
  void* data = AllocateMemoryOrNull(length, policy);
  if (!data) {
    return;
  }
  auto deleter = [](void* data, size_t, void*) { FreeMemory(data); };
  if (is_shared == kNotShared) {
    backing_store_ =
        v8::ArrayBuffer::NewBackingStore(data, length, deleter, nullptr);
  } else {
    backing_store_ =
        v8::SharedArrayBuffer::NewBackingStore(data, length, deleter, nullptr);
  }
}

ArrayBufferContents::ArrayBufferContents(
    std::shared_ptr<v8::BackingStore> backing_store) {
  if (!backing_store || backing_store->Data()) {
    backing_store_ = std::move(backing_store);
    return;
  }
  // ArrayBufferContents has to guarantee that Data() provides a valid pointer,
  // even when DataSize() is '0'. That's why we create a new BackingStore here.

  // TODO(ahaas): Remove this code here once nullptr is a valid result for
  // Data().
  CHECK_EQ(backing_store->ByteLength(), 0u);
  void* data = AllocateMemoryOrNull(0, kDontInitialize);
  CHECK_NE(data, nullptr);
  DataDeleter deleter = [](void* data, size_t, void*) { FreeMemory(data); };
  if (!backing_store->IsShared()) {
    backing_store_ =
        v8::ArrayBuffer::NewBackingStore(data, 0, deleter, nullptr);
  } else {
    backing_store_ =
        v8::SharedArrayBuffer::NewBackingStore(data, 0, deleter, nullptr);
  }
}

ArrayBufferContents::~ArrayBufferContents() = default;

void ArrayBufferContents::Detach() {
  backing_store_.reset();
}

void ArrayBufferContents::Reset() {
  backing_store_.reset();
}

void ArrayBufferContents::Transfer(ArrayBufferContents& other) {
  DCHECK(!IsShared());
  DCHECK(!other.Data());
  other.backing_store_ = std::move(backing_store_);
}

void ArrayBufferContents::ShareWith(ArrayBufferContents& other) {
  DCHECK(IsShared());
  DCHECK(!other.Data());
  other.backing_store_ = backing_store_;
}

void ArrayBufferContents::ShareNonSharedForInternalUse(
    ArrayBufferContents& other) {
  DCHECK(!IsShared());
  DCHECK(!other.Data());
  DCHECK(Data());
  other.backing_store_ = backing_store_;
}

void ArrayBufferContents::CopyTo(ArrayBufferContents& other) {
  other = ArrayBufferContents(
      DataLength(), 1, IsShared() ? kShared : kNotShared, kDontInitialize);
  if (!IsValid() || !other.IsValid())
    return;
  memcpy(other.Data(), Data(), DataLength());
}

void* ArrayBufferContents::AllocateMemoryWithFlags(size_t size,
                                                   InitializationPolicy policy,
                                                   int flags) {
  if (policy == kZeroInitialize) {
    flags |= base::PartitionAllocZeroFill;
  }
  void* data = PartitionAllocGenericFlags(
      WTF::Partitions::ArrayBufferPartition(), flags, size,
      WTF_HEAP_PROFILER_TYPE_NAME(ArrayBufferContents));
  return data;
}

void* ArrayBufferContents::AllocateMemoryOrNull(size_t size,
                                                InitializationPolicy policy) {
  return AllocateMemoryWithFlags(size, policy, base::PartitionAllocReturnNull);
}

void ArrayBufferContents::FreeMemory(void* data) {
  WTF::Partitions::ArrayBufferPartition()->Free(data);
}

}  // namespace blink
