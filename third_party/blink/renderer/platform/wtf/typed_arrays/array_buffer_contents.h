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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TYPED_ARRAYS_ARRAY_BUFFER_CONTENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TYPED_ARRAYS_ARRAY_BUFFER_CONTENTS_H_

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

class WTF_EXPORT ArrayBufferContents {
  DISALLOW_NEW();

 public:
  using AdjustAmountOfExternalAllocatedMemoryFunction = void (*)(int64_t diff);
  // Types that need to be used when injecting external memory.
  // DataHandle allows specifying a deleter which will be invoked when
  // DataHandle instance goes out of scope. If the data memory is allocated
  // using ArrayBufferContents::AllocateMemoryOrNull, it is necessary to specify
  // ArrayBufferContents::FreeMemory as the DataDeleter. Most clients would want
  // to use ArrayBufferContents::CreateDataHandle, which allocates memory and
  // specifies the correct deleter.
  using DataDeleter = void (*)(void* data, size_t length, void* info);

  class DataHandle {
    DISALLOW_COPY_AND_ASSIGN(DataHandle);

   public:
    DataHandle(void* data,
               size_t length,
               DataDeleter deleter,
               void* deleter_info)
        : data_(data),
          data_length_(length),
          deleter_(deleter),
          deleter_info_(deleter_info) {}
    // Move constructor
    DataHandle(DataHandle&& other) { *this = std::move(other); }
    ~DataHandle() {
      if (!data_)
        return;
      deleter_(data_, data_length_, deleter_info_);
    }

    // Move operator
    DataHandle& operator=(DataHandle&& other) {
      data_ = other.data_;
      data_length_ = other.data_length_;
      deleter_ = other.deleter_;
      deleter_info_ = other.deleter_info_;
      other.data_ = nullptr;
      return *this;
    }

    void* Data() const { return data_; }
    size_t DataLength() const { return data_length_; }

    operator bool() const { return data_; }

   private:
    void* data_;
    size_t data_length_;

    DataDeleter deleter_;
    void* deleter_info_;
  };

  enum InitializationPolicy { kZeroInitialize, kDontInitialize };

  enum SharingType {
    kNotShared,
    kShared,
  };

  ArrayBufferContents();
  ArrayBufferContents(size_t num_elements,
                      unsigned element_byte_size,
                      SharingType is_shared,
                      InitializationPolicy);
  ArrayBufferContents(DataHandle,
                      SharingType is_shared);
  ArrayBufferContents(ArrayBufferContents&&) = default;

  ~ArrayBufferContents();

  ArrayBufferContents& operator=(ArrayBufferContents&&) = default;

  void Neuter();

  void* Data() const {
    DCHECK(!IsShared());
    return DataMaybeShared();
  }
  void* DataShared() const {
    DCHECK(IsShared());
    return DataMaybeShared();
  }
  void* DataMaybeShared() const { return holder_ ? holder_->Data() : nullptr; }
  size_t DataLength() const { return holder_ ? holder_->DataLength() : 0; }
  bool IsShared() const { return holder_ ? holder_->IsShared() : false; }

  void Transfer(ArrayBufferContents& other);
  void ShareWith(ArrayBufferContents& other);
  void CopyTo(ArrayBufferContents& other);

  static void* AllocateMemoryOrNull(size_t, InitializationPolicy);
  static void FreeMemory(void*);
  static DataHandle CreateDataHandle(size_t, InitializationPolicy);
  static void Initialize(
      AdjustAmountOfExternalAllocatedMemoryFunction function) {
    DCHECK(IsMainThread());
    DCHECK_EQ(adjust_amount_of_external_allocated_memory_function_,
              DefaultAdjustAmountOfExternalAllocatedMemoryFunction);
    adjust_amount_of_external_allocated_memory_function_ = function;
  }

  void RegisterExternalAllocationWithCurrentContext() {
    if (holder_)
      holder_->RegisterExternalAllocationWithCurrentContext();
  }

  void UnregisterExternalAllocationWithCurrentContext() {
    if (holder_)
      holder_->UnregisterExternalAllocationWithCurrentContext();
  }

 private:
  static void* AllocateMemoryWithFlags(size_t, InitializationPolicy, int);

  static void DefaultAdjustAmountOfExternalAllocatedMemoryFunction(
      int64_t diff);

  class DataHolder : public ThreadSafeRefCounted<DataHolder> {
    DISALLOW_COPY_AND_ASSIGN(DataHolder);

   public:
    DataHolder();
    ~DataHolder();

    void AllocateNew(size_t length,
                     SharingType is_shared,
                     InitializationPolicy);
    void Adopt(DataHandle, SharingType is_shared);
    void CopyMemoryFrom(const DataHolder& source);

    const void* Data() const { return data_.Data(); }
    void* Data() { return data_.Data(); }
    size_t DataLength() const { return data_.DataLength(); }
    bool IsShared() const { return is_shared_ == kShared; }

    void RegisterExternalAllocationWithCurrentContext();
    void UnregisterExternalAllocationWithCurrentContext();

   private:
    void AdjustAmountOfExternalAllocatedMemory(int64_t diff) {
      has_registered_external_allocation_ =
          !has_registered_external_allocation_;
      DCHECK(!diff || (has_registered_external_allocation_ == (diff > 0)));
      CheckIfAdjustAmountOfExternalAllocatedMemoryIsConsistent();
      adjust_amount_of_external_allocated_memory_function_(diff);
    }

    void AdjustAmountOfExternalAllocatedMemory(size_t diff) {
      AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(diff));
    }

    void CheckIfAdjustAmountOfExternalAllocatedMemoryIsConsistent() {
      DCHECK(adjust_amount_of_external_allocated_memory_function_);

#if DCHECK_IS_ON()
      // Make sure that the function actually used is always the same.
      // Shouldn't be updated during its use.
      if (!last_used_adjust_amount_of_external_allocated_memory_function_) {
        last_used_adjust_amount_of_external_allocated_memory_function_ =
            adjust_amount_of_external_allocated_memory_function_;
      }
      DCHECK_EQ(adjust_amount_of_external_allocated_memory_function_,
                last_used_adjust_amount_of_external_allocated_memory_function_);
#endif
    }

    DataHandle data_;
    SharingType is_shared_;
    bool has_registered_external_allocation_;
  };

  scoped_refptr<DataHolder> holder_;
  static AdjustAmountOfExternalAllocatedMemoryFunction
      adjust_amount_of_external_allocated_memory_function_;
#if DCHECK_IS_ON()
  static AdjustAmountOfExternalAllocatedMemoryFunction
      last_used_adjust_amount_of_external_allocated_memory_function_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ArrayBufferContents);
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TYPED_ARRAYS_ARRAY_BUFFER_CONTENTS_H_
