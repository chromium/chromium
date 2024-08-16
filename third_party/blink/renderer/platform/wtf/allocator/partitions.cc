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

#include "base/allocator/partition_alloc_features.h"
#include "base/allocator/partition_alloc_support.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/strings/safe_sprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/oom.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_alloc.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_root.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace WTF {

const char* const Partitions::kAllocatedObjectPoolName =
    "partition_alloc/allocated_objects";

BASE_FEATURE(kBlinkUseLargeEmptySlotSpanRingForBufferRoot,
             "BlinkUseLargeEmptySlotSpanRingForBufferRoot",
#if BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

bool Partitions::initialized_ = false;

// These statics are inlined, so cannot be LazyInstances. We create the values,
// and then set the pointers correctly in Initialize().
partition_alloc::PartitionRoot* Partitions::fast_malloc_root_ = nullptr;
partition_alloc::PartitionRoot* Partitions::array_buffer_root_ = nullptr;
partition_alloc::PartitionRoot* Partitions::buffer_root_ = nullptr;

namespace {

// Reads feature configuration and returns a suitable
// `PartitionOptions`.
partition_alloc::PartitionOptions PartitionOptionsFromFeatures() {
  using base::features::BackupRefPtrEnabledProcesses;
  using base::features::BackupRefPtrMode;
  using partition_alloc::PartitionOptions;

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  const auto brp_mode = base::features::kBackupRefPtrModeParam.Get();
  const bool process_affected_by_brp_flag =
      base::features::kBackupRefPtrEnabledProcessesParam.Get() ==
          BackupRefPtrEnabledProcesses::kAllProcesses ||
      base::features::kBackupRefPtrEnabledProcessesParam.Get() ==
          BackupRefPtrEnabledProcesses::kBrowserAndRenderer;
  const bool enable_brp = base::FeatureList::IsEnabled(
                              base::features::kPartitionAllocBackupRefPtr) &&
                          (brp_mode == BackupRefPtrMode::kEnabled) &&
                          process_affected_by_brp_flag;
#else  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  const bool enable_brp = false;
#endif

  const auto brp_setting =
      enable_brp ? PartitionOptions::kEnabled : PartitionOptions::kDisabled;

  const bool enable_memory_tagging = base::allocator::PartitionAllocSupport::
      ShouldEnableMemoryTaggingInRendererProcess();
  const auto memory_tagging =
      enable_memory_tagging ? partition_alloc::PartitionOptions::kEnabled
                            : partition_alloc::PartitionOptions::kDisabled;
#if PA_BUILDFLAG(USE_FREELIST_DISPATCHER)
  const bool pool_offset_freelists_enabled =
      base::FeatureList::IsEnabled(base::features::kUsePoolOffsetFreelists);
#else
  const bool pool_offset_freelists_enabled = false;
#endif  // PA_BUILDFLAG(USE_FREELIST_DISPATCHER)
  const auto use_pool_offset_freelists =
      pool_offset_freelists_enabled
          ? partition_alloc::PartitionOptions::kEnabled
          : partition_alloc::PartitionOptions::kDisabled;
  // No need to call ChangeMemoryTaggingModeForAllThreadsPerProcess() as it will
  // be handled in ReconfigureAfterFeatureListInit().
  PartitionOptions opts;
  opts.star_scan_quarantine = PartitionOptions::kAllowed;
  opts.backup_ref_ptr = brp_setting;
  opts.memory_tagging = {.enabled = memory_tagging};
  opts.use_pool_offset_freelists = use_pool_offset_freelists;
  opts.use_small_single_slot_spans =
      base::FeatureList::IsEnabled(
          base::features::kPartitionAllocUseSmallSingleSlotSpans)
          ? partition_alloc::PartitionOptions::kEnabled
          : partition_alloc::PartitionOptions::kDisabled;
  return opts;
}

}  // namespace

// static
void Partitions::Initialize() {
  static bool initialized = InitializeOnce();
  DCHECK(initialized);
}

// static
bool Partitions::InitializeOnce() {
  using partition_alloc::PartitionOptions;

  partition_alloc::PartitionAllocGlobalInit(&Partitions::HandleOutOfMemory);

  auto options = PartitionOptionsFromFeatures();

  const auto actual_brp_setting = options.backup_ref_ptr;
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocDisableBRPInBufferPartition)) {
    options.backup_ref_ptr = PartitionOptions::kDisabled;
  }

  static base::NoDestructor<partition_alloc::PartitionAllocator>
      buffer_allocator(options);
  buffer_root_ = buffer_allocator->root();
  if (base::FeatureList::IsEnabled(
          kBlinkUseLargeEmptySlotSpanRingForBufferRoot)) {
    buffer_root_->EnableLargeEmptySlotSpanRing();
  }

  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocDisableBRPInBufferPartition)) {
    options.backup_ref_ptr = actual_brp_setting;
  }

  // FastMalloc doesn't provide isolation, only a (hopefully fast) malloc().
  // When PartitionAlloc is already the malloc() implementation, there is
  // nothing to do.
  //
  // Note that we could keep the two heaps separate, but each PartitionAlloc's
  // root has a cost, both in used memory and in virtual address space. Don't
  // pay it when we don't have to.
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  options.thread_cache = PartitionOptions::kEnabled;
  static base::NoDestructor<partition_alloc::PartitionAllocator>
      fast_malloc_allocator(options);
  fast_malloc_root_ = fast_malloc_allocator->root();
#endif

  initialized_ = true;
  return initialized_;
}

// static
void Partitions::InitializeArrayBufferPartition() {
  CHECK(initialized_);
  CHECK(!ArrayBufferPartitionInitialized());

  // BackupRefPtr disallowed because it will prevent allocations from being 16B
  // aligned as required by ArrayBufferContents.
  static base::NoDestructor<partition_alloc::PartitionAllocator>
      array_buffer_allocator([]() {
        partition_alloc::PartitionOptions opts;
        opts.star_scan_quarantine = partition_alloc::PartitionOptions::kAllowed;
        opts.backup_ref_ptr = partition_alloc::PartitionOptions::kDisabled;
        // When the V8 virtual memory cage is enabled, the ArrayBuffer
        // partition must be placed inside of it. For that, PA's
        // ConfigurablePool is created inside the V8 Cage during
        // initialization. As such, here all we need to do is indicate that
        // we'd like to use that Pool if it has been created by now (if it
        // hasn't been created, the cage isn't enabled, and so we'll use the
        // default Pool).
        opts.use_configurable_pool =
            partition_alloc::PartitionOptions::kAllowed;
        opts.memory_tagging = {
            .enabled = partition_alloc::PartitionOptions::kDisabled};
        return opts;
      }());

  array_buffer_root_ = array_buffer_allocator->root();
}

// static
void Partitions::StartMemoryReclaimer(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  CHECK(IsMainThread());
  DCHECK(initialized_);

  base::allocator::StartMemoryReclaimer(task_runner);
}

// static
void Partitions::DumpMemoryStats(
    bool is_light_dump,
    partition_alloc::PartitionStatsDumper* partition_stats_dumper) {
  // Object model and rendering partitions are not thread safe and can be
  // accessed only on the main thread.
  DCHECK(IsMainThread());

  if (auto* fast_malloc_partition = FastMallocPartition()) {
    fast_malloc_partition->DumpStats("fast_malloc", is_light_dump,
                                     partition_stats_dumper);
  }
  if (ArrayBufferPartitionInitialized()) {
    ArrayBufferPartition()->DumpStats("array_buffer", is_light_dump,
                                      partition_stats_dumper);
  }
  BufferPartition()->DumpStats("buffer", is_light_dump, partition_stats_dumper);
}

namespace {

class LightPartitionStatsDumperImpl
    : public partition_alloc::PartitionStatsDumper {
 public:
  LightPartitionStatsDumperImpl() : total_active_bytes_(0) {}

  void PartitionDumpTotals(
      const char* partition_name,
      const partition_alloc::PartitionMemoryStats* memory_stats) override {
    total_active_bytes_ += memory_stats->total_active_bytes;
  }

  void PartitionsDumpBucketStats(
      const char* partition_name,
      const partition_alloc::PartitionBucketMemoryStats*) override {}

  size_t TotalActiveBytes() const { return total_active_bytes_; }

 private:
  size_t total_active_bytes_;
};

}  // namespace

// static
size_t Partitions::TotalSizeOfCommittedPages() {
  DCHECK(initialized_);
  size_t total_size = 0;
  // Racy reads below: this is fine to collect statistics.
  if (auto* fast_malloc_partition = FastMallocPartition()) {
    total_size +=
        TS_UNCHECKED_READ(fast_malloc_partition->total_size_of_committed_pages);
  }
  if (ArrayBufferPartitionInitialized()) {
    total_size += TS_UNCHECKED_READ(
        ArrayBufferPartition()->total_size_of_committed_pages);
  }
  total_size +=
      TS_UNCHECKED_READ(BufferPartition()->total_size_of_committed_pages);
  return total_size;
}

// static
size_t Partitions::TotalActiveBytes() {
  LightPartitionStatsDumperImpl dumper;
  WTF::Partitions::DumpMemoryStats(true, &dumper);
  return dumper.TotalActiveBytes();
}

NOINLINE static void PartitionsOutOfMemoryUsing2G(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 2UL * 1024 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing1G(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 1UL * 1024 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing512M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 512 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing256M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 256 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing128M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 128 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing64M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 64 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing32M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 32 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsing16M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 16 * 1024 * 1024;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

NOINLINE static void PartitionsOutOfMemoryUsingLessThan16M(size_t size) {
  NO_CODE_FOLDING();
  size_t signature = 16 * 1024 * 1024 - 1;
  base::debug::Alias(&signature);
  OOM_CRASH(size);
}

// static
void* Partitions::BufferMalloc(size_t n, const char* type_name) {
  return BufferPartition()->Alloc(n, type_name);
}

// static
void* Partitions::BufferTryRealloc(void* p, size_t n, const char* type_name) {
  return BufferPartition()->Realloc<partition_alloc::AllocFlags::kReturnNull>(
      p, n, type_name);
}

// static
void Partitions::BufferFree(void* p) {
  BufferPartition()->Free(p);
}

// static
size_t Partitions::BufferPotentialCapacity(size_t n) {
  return BufferPartition()->AllocationCapacityFromRequestedSize(n);
}

// Ideally this would be removed when PartitionAlloc is malloc(), but there are
// quite a few callers. Just forward to the C functions instead.  Most of the
// usual callers will never reach here though, as USING_FAST_MALLOC() becomes a
// no-op.
// static
void* Partitions::FastMalloc(size_t n, const char* type_name) {
  auto* fast_malloc_partition = FastMallocPartition();
  if (fast_malloc_partition) [[unlikely]] {
    return fast_malloc_partition->Alloc(n, type_name);
  } else {
    return malloc(n);
  }
}

// static
void* Partitions::FastZeroedMalloc(size_t n, const char* type_name) {
  auto* fast_malloc_partition = FastMallocPartition();
  if (fast_malloc_partition) [[unlikely]] {
    return fast_malloc_partition
        ->AllocInline<partition_alloc::AllocFlags::kZeroFill>(n, type_name);
  } else {
    return calloc(n, 1);
  }
}

// static
void Partitions::FastFree(void* p) {
  auto* fast_malloc_partition = FastMallocPartition();
  if (fast_malloc_partition) [[unlikely]] {
    fast_malloc_partition->Free(p);
  } else {
    free(p);
  }
}

// static
void Partitions::HandleOutOfMemory(size_t size) {
  volatile size_t total_usage = TotalSizeOfCommittedPages();
  uint32_t alloc_page_error_code = partition_alloc::GetAllocPageErrorCode();
  base::debug::Alias(&alloc_page_error_code);

  // Report the total mapped size from PageAllocator. This is intended to
  // distinguish better between address space exhaustion and out of memory on 32
  // bit platforms. PartitionAlloc can use a lot of address space, as free pages
  // are not shared between buckets (see crbug.com/421387). There is already
  // reporting for this, however it only looks at the address space usage of a
  // single partition. This allows to look across all the partitions, and other
  // users such as V8.
  char value[24];
  // %d works for 64 bit types as well with SafeSPrintf(), see its unit tests
  // for an example.
  base::strings::SafeSPrintf(value, "%d",
                             partition_alloc::GetTotalMappedSize());
  static crash_reporter::CrashKeyString<24> g_page_allocator_mapped_size(
      "page-allocator-mapped-size");
  g_page_allocator_mapped_size.Set(value);

  if (total_usage >= 2UL * 1024 * 1024 * 1024) {
    PartitionsOutOfMemoryUsing2G(size);
  }
  if (total_usage >= 1UL * 1024 * 1024 * 1024) {
    PartitionsOutOfMemoryUsing1G(size);
  }
  if (total_usage >= 512 * 1024 * 1024) {
    PartitionsOutOfMemoryUsing512M(size);
  }
  if (total_usage >= 256 * 1024 * 1024) {
    PartitionsOutOfMemoryUsing256M(size);
  }
  if (total_usage >= 128 * 1024 * 1024) {
    PartitionsOutOfMemoryUsing128M(size);
  }
  if (total_usage >= 64 * 1024 * 1024) {
    PartitionsOutOfMemoryUsing64M(size);
  }
  if (total_usage >= 32 * 1024 * 1024) {
    PartitionsOutOfMemoryUsing32M(size);
  }
  if (total_usage >= 16 * 1024 * 1024) {
    PartitionsOutOfMemoryUsing16M(size);
  }
  PartitionsOutOfMemoryUsingLessThan16M(size);
}

// static
void Partitions::AdjustPartitionsForForeground() {
  DCHECK(initialized_);
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocAdjustSizeWhenInForeground)) {
    array_buffer_root_->AdjustForForeground();
    buffer_root_->AdjustForForeground();
    if (fast_malloc_root_) {
      fast_malloc_root_->AdjustForForeground();
    }
  }
}

// static
void Partitions::AdjustPartitionsForBackground() {
  DCHECK(initialized_);
  if (base::FeatureList::IsEnabled(
          base::features::kPartitionAllocAdjustSizeWhenInForeground)) {
    array_buffer_root_->AdjustForBackground();
    buffer_root_->AdjustForBackground();
    if (fast_malloc_root_) {
      fast_malloc_root_->AdjustForBackground();
    }
  }
}

}  // namespace WTF
