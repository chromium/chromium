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

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/wtf/typed_arrays/array_buffer_contents.h"

#include <string.h>
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace WTF {

void ArrayBufferContents::DefaultAdjustAmountOfExternalAllocatedMemoryFunction(
    int64_t diff) {
  // Do nothing by default.
}

ArrayBufferContents::AdjustAmountOfExternalAllocatedMemoryFunction
    ArrayBufferContents::adjust_amount_of_external_allocated_memory_function_ =
        DefaultAdjustAmountOfExternalAllocatedMemoryFunction;

#if DCHECK_IS_ON()
ArrayBufferContents::AdjustAmountOfExternalAllocatedMemoryFunction
    ArrayBufferContents::
        last_used_adjust_amount_of_external_allocated_memory_function_;
#endif

ArrayBufferContents::ArrayBufferContents()
    : holder_(base::AdoptRef(new DataHolder())) {}

ArrayBufferContents::ArrayBufferContents(
    size_t num_elements,
    unsigned element_byte_size,
    SharingType is_shared,
    ArrayBufferContents::InitializationPolicy policy)
    : holder_(base::AdoptRef(new DataHolder())) {
  // Do not allow 32-bit overflow of the total size.
  size_t total_size = num_elements * element_byte_size;
  if (num_elements) {
    if (total_size / num_elements != element_byte_size) {
      return;
    }
  }

  holder_->AllocateNew(total_size, is_shared, policy);
}

ArrayBufferContents::ArrayBufferContents(DataHandle data,
                                         SharingType is_shared)
    : holder_(base::AdoptRef(new DataHolder())) {
  if (data) {
    holder_->Adopt(std::move(data), is_shared);
  } else {
    // Allow null data if size is 0 bytes, make sure data is valid pointer.
    // (PartitionAlloc guarantees valid pointer for size 0)
    holder_->AllocateNew(0, is_shared, kZeroInitialize);
  }
}

ArrayBufferContents::~ArrayBufferContents() = default;

void ArrayBufferContents::Neuter() {
  holder_ = nullptr;
}

void ArrayBufferContents::Transfer(ArrayBufferContents& other) {
  DCHECK(!IsShared());
  DCHECK(!other.holder_->Data());
  other.holder_ = holder_;
  Neuter();
}

void ArrayBufferContents::ShareWith(ArrayBufferContents& other) {
  DCHECK(IsShared());
  DCHECK(!other.holder_->Data());
  other.holder_ = holder_;
}

void ArrayBufferContents::CopyTo(ArrayBufferContents& other) {
  DCHECK(!holder_->IsShared() && !other.holder_->IsShared());
  other.holder_->CopyMemoryFrom(*holder_);
}

void* ArrayBufferContents::AllocateMemoryWithFlags(size_t size,
                                                   InitializationPolicy policy,
                                                   int flags) {
  if (policy == kZeroInitialize) {
    flags |= base::PartitionAllocZeroFill;
  }
  void* data = PartitionAllocGenericFlags(
      Partitions::ArrayBufferPartition(), flags, size,
      WTF_HEAP_PROFILER_TYPE_NAME(ArrayBufferContents));
  return data;
}

void* ArrayBufferContents::AllocateMemoryOrNull(size_t size,
                                                InitializationPolicy policy) {
  return AllocateMemoryWithFlags(size, policy, base::PartitionAllocReturnNull);
}

void ArrayBufferContents::FreeMemory(void* data) {
  Partitions::ArrayBufferPartition()->Free(data);
}

ArrayBufferContents::DataHandle ArrayBufferContents::CreateDataHandle(
    size_t size,
    InitializationPolicy policy) {
  return DataHandle(
      ArrayBufferContents::AllocateMemoryOrNull(size, policy), size,
      [](void* buffer, size_t, void*) { FreeMemory(buffer); }, nullptr);
}

ArrayBufferContents::DataHolder::DataHolder()
    : data_(nullptr, 0, [](void*, size_t, void*) {}, nullptr),
      is_shared_(kNotShared),
      has_registered_external_allocation_(false) {}

ArrayBufferContents::DataHolder::~DataHolder() {
  if (has_registered_external_allocation_)
    AdjustAmountOfExternalAllocatedMemory(-static_cast<int64_t>(DataLength()));

  is_shared_ = kNotShared;
}

void ArrayBufferContents::DataHolder::AllocateNew(size_t length,
                                                  SharingType is_shared,
                                                  InitializationPolicy policy) {
  DCHECK(!data_);
  DCHECK(!has_registered_external_allocation_);

  data_ = CreateDataHandle(length, policy);
  if (!data_)
    return;

  is_shared_ = is_shared;

  RegisterExternalAllocationWithCurrentContext();
}

void ArrayBufferContents::DataHolder::Adopt(DataHandle data,
                                            SharingType is_shared) {
  DCHECK(!data_);
  DCHECK(!has_registered_external_allocation_);

  data_ = std::move(data);
  is_shared_ = is_shared;

  RegisterExternalAllocationWithCurrentContext();
}

void ArrayBufferContents::DataHolder::CopyMemoryFrom(const DataHolder& source) {
  DCHECK(!data_);
  DCHECK(!has_registered_external_allocation_);

  data_ = CreateDataHandle(source.DataLength(), kDontInitialize);
  if (!data_)
    return;

  memcpy(data_.Data(), source.Data(), source.DataLength());

  RegisterExternalAllocationWithCurrentContext();
}

void ArrayBufferContents::DataHolder::
    RegisterExternalAllocationWithCurrentContext() {
  DCHECK(!has_registered_external_allocation_);
  // Currently, we can only track an allocation if we have a single owner. For
  // shared data this is not true, hence do not attempt to track at all.
  // TODO(crbug.com/877055) Implement tracking of shared external allocations.
  if (IsShared())
    return;
  AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(DataLength()));
}

void ArrayBufferContents::DataHolder::
    UnregisterExternalAllocationWithCurrentContext() {
  if (!has_registered_external_allocation_)
    return;
  DCHECK(!IsShared());
  AdjustAmountOfExternalAllocatedMemory(-static_cast<int64_t>(DataLength()));
}

}  // namespace WTF
