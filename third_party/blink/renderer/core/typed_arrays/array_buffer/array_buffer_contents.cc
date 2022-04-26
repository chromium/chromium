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

#include <cstring>

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/bits.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

namespace blink {

ArrayBufferContents::ArrayBufferContents(void* data,
                                         size_t length,
                                         DataDeleter deleter) {
  DCHECK(data || length == 0);

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
  std::memcpy(other.Data(), Data(), DataLength());
}

void* ArrayBufferContents::AllocateMemoryWithFlags(size_t size,
                                                   InitializationPolicy policy,
                                                   int flags) {
  // The array buffer contents are sometimes expected to be 16-byte aligned in
  // order to get the best optimization of SSE, especially in case of audio and
  // video buffers.  Hence, align the given size up to 16-byte boundary.
  // Technically speaking, 16-byte aligned size doesn't mean 16-byte aligned
  // address, but this heuristics works with the current implementation of
  // PartitionAlloc (and PartitionAlloc doesn't support a better way for now).
  if (base::kAlignment < 16) {  // base::kAlignment is a compile-time constant.
    size_t aligned_size = base::bits::AlignUp(size, 16);
    if (size == 0) {
      aligned_size = 16;
    }
    if (aligned_size >= size) {  // Only when no overflow
      size = aligned_size;
    }
  }

  if (policy == kZeroInitialize) {
    flags |= partition_alloc::AllocFlags::kZeroFill;
  }
  void* data = WTF::Partitions::ArrayBufferPartition()->AllocWithFlags(
      flags, size, WTF_HEAP_PROFILER_TYPE_NAME(ArrayBufferContents));
  if (base::kAlignment < 16) {
    char* ptr = reinterpret_cast<char*>(data);
    DCHECK_EQ(base::bits::AlignUp(ptr, 16), ptr)
        << "Pointer " << ptr << " not 16B aligned for size " << size;
  }
  InstanceCounters::IncrementCounter(
      InstanceCounters::kArrayBufferContentsCounter);
  return data;
}

void* ArrayBufferContents::AllocateMemoryOrNull(size_t size,
                                                InitializationPolicy policy) {
  return AllocateMemoryWithFlags(size, policy,
                                 partition_alloc::AllocFlags::kReturnNull);
}

void ArrayBufferContents::FreeMemory(void* data) {
  InstanceCounters::DecrementCounter(
      InstanceCounters::kArrayBufferContentsCounter);
  WTF::Partitions::ArrayBufferPartition()->Free(data);
}

}  // namespace blink
