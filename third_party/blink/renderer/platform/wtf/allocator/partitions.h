/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_ALLOCATOR_PARTITIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_ALLOCATOR_PARTITIONS_H_

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace base {
class PartitionStatsDumper;
struct PartitionRoot;
struct PartitionRootGeneric;
}  // namespace base

namespace WTF {

class WTF_EXPORT Partitions {
 public:
  // Name of allocator used by tracing for marking sub-allocations while take
  // memory snapshots.
  static const char* const kAllocatedObjectPoolName;

  static void Initialize();
  static void StartPeriodicReclaim(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ALWAYS_INLINE static base::PartitionRootGeneric* ArrayBufferPartition() {
    DCHECK(initialized_);
    return array_buffer_root_;
  }

  ALWAYS_INLINE static base::PartitionRootGeneric* BufferPartition() {
    DCHECK(initialized_);
    return buffer_root_;
  }

  ALWAYS_INLINE static base::PartitionRoot* LayoutPartition() {
    DCHECK(initialized_);
    return layout_root_;
  }

  ALWAYS_INLINE static size_t ComputeAllocationSize(size_t count, size_t size) {
    base::CheckedNumeric<size_t> total = count;
    total *= size;
    return total.ValueOrDie();
  }

  static size_t TotalSizeOfCommittedPages();

  static size_t TotalActiveBytes();

  static void DumpMemoryStats(bool is_light_dump, base::PartitionStatsDumper*);

  static void* BufferMalloc(size_t n, const char* type_name);
  static void* BufferTryRealloc(void* p, size_t n, const char* type_name);
  static void BufferFree(void* p);
  static size_t BufferActualSize(size_t n);

  static void* FastMalloc(size_t n, const char* type_name);
  static void* FastZeroedMalloc(size_t n, const char* type_name);
  static void FastFree(void* p);

  static void HandleOutOfMemory();

 private:
  ALWAYS_INLINE static base::PartitionRootGeneric* FastMallocPartition() {
    DCHECK(initialized_);
    return fast_malloc_root_;
  }

  static bool initialized_;

  // See Allocator.md for a description of these partitions.
  static base::PartitionRootGeneric* fast_malloc_root_;
  static base::PartitionRootGeneric* array_buffer_root_;
  static base::PartitionRootGeneric* buffer_root_;
  static base::PartitionRoot* layout_root_;
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_ALLOCATOR_PARTITIONS_H_
