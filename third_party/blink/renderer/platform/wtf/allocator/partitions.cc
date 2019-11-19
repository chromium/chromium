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

#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_root_base.h"
#include "base/debug/alias.h"
#include "base/lazy_instance.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partition_allocator.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace WTF {

const char* const Partitions::kAllocatedObjectPoolName =
    "partition_alloc/allocated_objects";

static base::LazyInstance<base::subtle::SpinLock>::Leaky initialization_lock_ =
    LAZY_INSTANCE_INITIALIZER;
bool Partitions::initialized_ = false;

// These statics are inlined, so cannot be LazyInstances. We create
// LazyInstances below, and then set the pointers correctly in Initialize().
base::PartitionRootGeneric* Partitions::fast_malloc_root_ = nullptr;
base::PartitionRootGeneric* Partitions::array_buffer_root_ = nullptr;
base::PartitionRootGeneric* Partitions::buffer_root_ = nullptr;
base::PartitionRoot* Partitions::layout_root_ = nullptr;

static base::LazyInstance<base::PartitionAllocatorGeneric>::Leaky
    lazy_fast_malloc = LAZY_INSTANCE_INITIALIZER;
static base::LazyInstance<base::PartitionAllocatorGeneric>::Leaky
    lazy_array_buffer = LAZY_INSTANCE_INITIALIZER;
static base::LazyInstance<base::PartitionAllocatorGeneric>::Leaky lazy_buffer =
    LAZY_INSTANCE_INITIALIZER;
static base::LazyInstance<base::SizeSpecificPartitionAllocator<1024>>::Leaky
    lazy_layout = LAZY_INSTANCE_INITIALIZER;

void Partitions::Initialize() {
  base::subtle::SpinLock::Guard guard(initialization_lock_.Get());

  if (!initialized_) {
    base::PartitionAllocatorGeneric* fast_malloc_allocator =
        lazy_fast_malloc.Pointer();
    base::PartitionAllocatorGeneric* array_buffer_allocator =
        lazy_array_buffer.Pointer();
    base::PartitionAllocatorGeneric* buffer_allocator = lazy_buffer.Pointer();
    base::SizeSpecificPartitionAllocator<1024>* layout_allocator =
        lazy_layout.Pointer();

    base::PartitionAllocGlobalInit(&Partitions::HandleOutOfMemory);
    fast_malloc_allocator->init();
    array_buffer_allocator->init();
    buffer_allocator->init();
    layout_allocator->init();

    fast_malloc_root_ = fast_malloc_allocator->root();
    array_buffer_root_ = array_buffer_allocator->root();
    buffer_root_ = buffer_allocator->root();
    layout_root_ = layout_allocator->root();

    initialized_ = true;
  }
}

// static
void Partitions::StartPeriodicReclaim(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  CHECK(IsMainThread());
  if (!initialized_)
    return;

  base::PartitionAllocMemoryReclaimer::Instance()->Start(task_runner);
}

void Partitions::DumpMemoryStats(
    bool is_light_dump,
    base::PartitionStatsDumper* partition_stats_dumper) {
  // Object model and rendering partitions are not thread safe and can be
  // accessed only on the main thread.
  DCHECK(IsMainThread());

  FastMallocPartition()->DumpStats("fast_malloc", is_light_dump,
                                   partition_stats_dumper);
  ArrayBufferPartition()->DumpStats("array_buffer", is_light_dump,
                                    partition_stats_dumper);
  BufferPartition()->DumpStats("buffer", is_light_dump, partition_stats_dumper);
  LayoutPartition()->DumpStats("layout", is_light_dump, partition_stats_dumper);
}

namespace {

class LightPartitionStatsDumperImpl : public base::PartitionStatsDumper {
 public:
  LightPartitionStatsDumperImpl() : total_active_bytes_(0) {}

  void PartitionDumpTotals(
      const char* partition_name,
      const base::PartitionMemoryStats* memory_stats) override {
    total_active_bytes_ += memory_stats->total_active_bytes;
  }

  void PartitionsDumpBucketStats(
      const char* partition_name,
      const base::PartitionBucketMemoryStats*) override {}

  size_t TotalActiveBytes() const { return total_active_bytes_; }

 private:
  size_t total_active_bytes_;
};

}  // namespace

size_t Partitions::TotalSizeOfCommittedPages() {
  DCHECK(initialized_);
  size_t total_size = 0;
  total_size += FastMallocPartition()->total_size_of_committed_pages;
  total_size += ArrayBufferPartition()->total_size_of_committed_pages;
  total_size += BufferPartition()->total_size_of_committed_pages;
  total_size += LayoutPartition()->total_size_of_committed_pages;
  return total_size;
}

size_t Partitions::TotalActiveBytes() {
  LightPartitionStatsDumperImpl dumper;
  WTF::Partitions::DumpMemoryStats(true, &dumper);
  return dumper.TotalActiveBytes();
}

static NOINLINE void PartitionsOutOfMemoryUsing2G() {
  size_t signature = 2UL * 1024 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NOINLINE void PartitionsOutOfMemoryUsing1G() {
  size_t signature = 1UL * 1024 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NOINLINE void PartitionsOutOfMemoryUsing512M() {
  size_t signature = 512 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NOINLINE void PartitionsOutOfMemoryUsing256M() {
  size_t signature = 256 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NOINLINE void PartitionsOutOfMemoryUsing128M() {
  size_t signature = 128 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NOINLINE void PartitionsOutOfMemoryUsing64M() {
  size_t signature = 64 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NOINLINE void PartitionsOutOfMemoryUsing32M() {
  size_t signature = 32 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NOINLINE void PartitionsOutOfMemoryUsing16M() {
  size_t signature = 16 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH();
}

static NOINLINE void PartitionsOutOfMemoryUsingLessThan16M() {
  size_t signature = 16 * 1024 * 1024 - 1;
  base::debug::Alias(&signature);
  DLOG(FATAL) << "ParitionAlloc: out of memory with < 16M usage (error:"
              << base::GetAllocPageErrorCode() << ")";
}

void* Partitions::BufferMalloc(size_t n, const char* type_name) {
  return BufferPartition()->Alloc(n, type_name);
}

void* Partitions::BufferTryRealloc(void* p, size_t n, const char* type_name) {
  return BufferPartition()->TryRealloc(p, n, type_name);
}

void Partitions::BufferFree(void* p) {
  BufferPartition()->Free(p);
}

size_t Partitions::BufferActualSize(size_t n) {
  return BufferPartition()->ActualSize(n);
}

void* Partitions::FastMalloc(size_t n, const char* type_name) {
  return FastMallocPartition()->Alloc(n, type_name);
}

void* Partitions::FastZeroedMalloc(size_t n, const char* type_name) {
  return FastMallocPartition()->AllocFlags(base::PartitionAllocZeroFill, n,
                                           type_name);
}

void Partitions::FastFree(void* p) {
  FastMallocPartition()->Free(p);
}

void Partitions::HandleOutOfMemory() {
  volatile size_t total_usage = TotalSizeOfCommittedPages();
  uint32_t alloc_page_error_code = base::GetAllocPageErrorCode();
  base::debug::Alias(&alloc_page_error_code);

  if (total_usage >= 2UL * 1024 * 1024 * 1024)
    PartitionsOutOfMemoryUsing2G();
  if (total_usage >= 1UL * 1024 * 1024 * 1024)
    PartitionsOutOfMemoryUsing1G();
  if (total_usage >= 512 * 1024 * 1024)
    PartitionsOutOfMemoryUsing512M();
  if (total_usage >= 256 * 1024 * 1024)
    PartitionsOutOfMemoryUsing256M();
  if (total_usage >= 128 * 1024 * 1024)
    PartitionsOutOfMemoryUsing128M();
  if (total_usage >= 64 * 1024 * 1024)
    PartitionsOutOfMemoryUsing64M();
  if (total_usage >= 32 * 1024 * 1024)
    PartitionsOutOfMemoryUsing32M();
  if (total_usage >= 16 * 1024 * 1024)
    PartitionsOutOfMemoryUsing16M();
  PartitionsOutOfMemoryUsingLessThan16M();
}

}  // namespace WTF
