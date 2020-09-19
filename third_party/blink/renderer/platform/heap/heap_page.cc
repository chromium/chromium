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

#include "third_party/blink/renderer/platform/heap/heap_page.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/auto_reset.h"
#include "base/trace_event/process_memory_dump.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/heap_compact.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/marking_verifier.h"
#include "third_party/blink/renderer/platform/heap/page_bloom_filter.h"
#include "third_party/blink/renderer/platform/heap/page_memory.h"
#include "third_party/blink/renderer/platform/heap/page_pool.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/memory_pressure_listener.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/container_annotations.h"
#include "third_party/blink/renderer/platform/wtf/leak_annotations.h"

#ifdef ANNOTATE_CONTIGUOUS_CONTAINER

// When finalizing a non-inlined vector backing store/container, remove
// its contiguous container annotation. Required as it will not be destructed
// from its Vector.
#define ASAN_RETIRE_CONTAINER_ANNOTATION(object, objectSize)          \
  do {                                                                \
    BasePage* page = PageFromObject(object);                          \
    DCHECK(page);                                                     \
    bool is_container =                                               \
        ThreadHeap::IsVectorArenaIndex(page->Arena()->ArenaIndex());  \
    if (!is_container && page->IsLargeObjectPage())                   \
      is_container =                                                  \
          static_cast<LargeObjectPage*>(page)->IsVectorBackingPage(); \
    if (is_container)                                                 \
      ANNOTATE_DELETE_BUFFER(object, objectSize, 0);                  \
  } while (0)

// A vector backing store represented by a large object is marked
// so that when it is finalized, its ASan annotation will be
// correctly retired.
#define ASAN_MARK_LARGE_VECTOR_CONTAINER(arena, large_object)            \
  if (ThreadHeap::IsVectorArenaIndex(arena->ArenaIndex())) {             \
    BasePage* large_page = PageFromObject(large_object);                 \
    DCHECK(large_page->IsLargeObjectPage());                             \
    static_cast<LargeObjectPage*>(large_page)->SetIsVectorBackingPage(); \
  }
#else
#define ASAN_RETIRE_CONTAINER_ANNOTATION(payload, payloadSize)
#define ASAN_MARK_LARGE_VECTOR_CONTAINER(arena, largeObject)
#endif

namespace blink {

void HeapObjectHeader::Finalize(Address object, size_t object_size) {
  DCHECK(!IsInConstruction<HeapObjectHeader::AccessMode::kAtomic>());
  HeapAllocHooks::FreeHookIfEnabled(object);
  const GCInfo& gc_info = GCInfo::From(GcInfoIndex());
  if (gc_info.finalize)
    gc_info.finalize(object);

  ASAN_RETIRE_CONTAINER_ANNOTATION(object, object_size);
}

bool HeapObjectHeader::HasNonTrivialFinalizer() const {
  return GCInfo::From(GcInfoIndex()).finalize;
}

const char* HeapObjectHeader::Name() const {
  return GCInfo::From(GcInfoIndex()).name(Payload()).value;
}

BaseArena::BaseArena(ThreadState* state, int index)
    : thread_state_(state), index_(index) {}

BaseArena::~BaseArena() {
  DCHECK(swept_pages_.IsEmpty());
  DCHECK(unswept_pages_.IsEmpty());
  DCHECK(swept_unfinalized_pages_.IsEmpty());
  DCHECK(swept_unfinalized_empty_pages_.IsEmpty());
}

void BaseArena::RemoveAllPages() {
  ClearFreeLists();

  DCHECK(SweepingAndFinalizationCompleted());
  while (BasePage* page = swept_pages_.Pop()) {
    page->RemoveFromHeap();
  }
}

void BaseArena::CollectStatistics(std::string name,
                                  ThreadState::Statistics* stats) {
  ThreadState::Statistics::ArenaStatistics arena_stats;

  ResetAllocationPoint();

  if (!NameClient::HideInternalName()) {
    const size_t num_types = GCInfoTable::Get().NumberOfGCInfos();
    arena_stats.object_stats.num_types = num_types;
    arena_stats.object_stats.type_name.resize(num_types);
    arena_stats.object_stats.type_count.resize(num_types);
    arena_stats.object_stats.type_bytes.resize(num_types);
  }

  arena_stats.name = std::move(name);
  DCHECK(unswept_pages_.IsEmpty());
  for (BasePage* page : swept_pages_) {
    page->CollectStatistics(&arena_stats);
  }
  CollectFreeListStatistics(&arena_stats.free_list_stats);
  stats->used_size_bytes += arena_stats.used_size_bytes;
  stats->committed_size_bytes += arena_stats.committed_size_bytes;
  stats->arena_stats.emplace_back(std::move(arena_stats));
}

void NormalPageArena::CollectFreeListStatistics(
    ThreadState::Statistics::FreeListStatistics* stats) {
  free_list_.CollectStatistics(stats);
}

#if DCHECK_IS_ON()
BasePage* BaseArena::FindPageFromAddress(ConstAddress address) const {
  for (BasePage* page : swept_pages_) {
    if (page->Contains(address))
      return page;
  }
  for (BasePage* page : unswept_pages_) {
    if (page->Contains(address))
      return page;
  }
  for (BasePage* page : swept_unfinalized_pages_) {
    if (page->Contains(address))
      return page;
  }
  for (BasePage* page : swept_unfinalized_empty_pages_) {
    if (page->Contains(address))
      return page;
  }
  return nullptr;
}
#endif

void BaseArena::MakeConsistentForGC() {
#if DCHECK_IS_ON()
  DCHECK(IsConsistentForGC());
#endif

  // We should not start a new GC until we finish sweeping in the current GC.
  CHECK(SweepingAndFinalizationCompleted());

  HeapCompact* heap_compactor = GetThreadState()->Heap().Compaction();
  if (!heap_compactor->IsCompactingArena(ArenaIndex()))
    return;

  for (BasePage* page : swept_pages_) {
    if (!page->IsLargeObjectPage())
      heap_compactor->AddCompactingPage(page);
  }
}

void BaseArena::MakeConsistentForMutator() {
  ClearFreeLists();
#if DCHECK_IS_ON()
  DCHECK(IsConsistentForGC());
#endif
  DCHECK(swept_pages_.IsEmpty());

  // Drop marks from marked objects and rebuild free lists in preparation for
  // resuming the executions of mutators.
  for (BasePage* page : unswept_pages_) {
    page->MakeConsistentForMutator();
    page->MarkAsSwept();
  }

  swept_pages_.MoveFrom(std::move(unswept_pages_));
  DCHECK(SweepingAndFinalizationCompleted());

  VerifyObjectStartBitmap();
}

void BaseArena::Unmark() {
  DCHECK(GetThreadState()->InAtomicMarkingPause());
  DCHECK(SweepingAndFinalizationCompleted());

  for (BasePage* page : swept_pages_) {
    page->Unmark();
  }
}

size_t BaseArena::ObjectPayloadSizeForTesting() {
#if DCHECK_IS_ON()
  DCHECK(IsConsistentForGC());
#endif
  // DCHECK(SweepingCompleted());

  size_t object_payload_size = 0;
  for (BasePage* page : unswept_pages_) {
    object_payload_size += page->ObjectPayloadSizeForTesting();
  }
  return object_payload_size;
}

void BaseArena::PrepareForSweep(BlinkGC::CollectionType collection_type) {
  DCHECK(GetThreadState()->InAtomicMarkingPause());
  DCHECK(SweepingAndFinalizationCompleted());

  ClearFreeLists();

  // Verification depends on the allocation point being cleared.
  VerifyObjectStartBitmap();

  if (collection_type == BlinkGC::CollectionType::kMinor) {
    auto** first_young =
        std::partition(swept_pages_.begin(), swept_pages_.end(),
                       [](BasePage* page) { return !page->IsYoung(); });
    for (auto** it = first_young; it != swept_pages_.end(); ++it) {
      BasePage* page = *it;
      page->MarkAsUnswept();
      page->SetAsYoung(false);
      unswept_pages_.Push(page);
    }
    swept_pages_.erase(first_young, swept_pages_.end());
    return;
  }

  for (BasePage* page : swept_pages_) {
    page->MarkAsUnswept();
  }
  // Move all pages to a list of unswept pages.
  unswept_pages_.MoveFrom(std::move(swept_pages_));
  DCHECK(swept_pages_.IsEmpty());
}

#if defined(ADDRESS_SANITIZER)
void BaseArena::PoisonUnmarkedObjects() {
  for (BasePage* page : unswept_pages_) {
    page->PoisonUnmarkedObjects();
  }
}
#endif

Address BaseArena::LazySweep(size_t allocation_size, size_t gc_info_index) {
  // If there are no pages to be swept, return immediately.
  if (SweepingAndFinalizationCompleted())
    return nullptr;

  CHECK(GetThreadState()->IsSweepingInProgress());

  // lazySweepPages() can be called recursively if finalizers invoked in
  // page->Sweep() allocate memory and the allocation triggers
  // lazySweepPages(). This check prevents the sweeping from being executed
  // recursively.
  if (GetThreadState()->SweepForbidden())
    return nullptr;

  ThreadHeapStatsCollector::EnabledScope stats_scope(
      GetThreadState()->Heap().stats_collector(),
      ThreadHeapStatsCollector::kLazySweepOnAllocation);
  ThreadState::SweepForbiddenScope sweep_forbidden(GetThreadState());
  ScriptForbiddenScope script_forbidden;
  return LazySweepPages(allocation_size, gc_info_index);
}

bool BaseArena::SweepUnsweptPageOnConcurrentThread(BasePage* page) {
  const bool is_empty = page->Sweep(FinalizeType::kDeferred);
  if (is_empty) {
    swept_unfinalized_empty_pages_.PushLocked(page);
  } else {
    swept_unfinalized_pages_.PushLocked(page);
  }
  return is_empty;
}

bool BaseArena::SweepUnsweptPage(BasePage* page) {
  const bool is_empty = page->Sweep(FinalizeType::kInlined);
  if (is_empty) {
    page->FinalizeSweep(SweepResult::kPageEmpty);
  } else {
    // First, we add page to the list of swept pages
    // so that the FindPageFromAddress check is happy.
    swept_pages_.PushLocked(page);
    page->FinalizeSweep(SweepResult::kPageNotEmpty);
  }
  return is_empty;
}

bool BaseArena::LazySweepWithDeadline(base::TimeTicks deadline) {
  // It might be heavy to call
  // Platform::current()->monotonicallyIncreasingTimeSeconds() per page (i.e.,
  // 128 KB sweep or one LargeObject sweep), so we check the deadline per 10
  // pages.
  static constexpr size_t kDeadlineCheckInterval = 10;

  CHECK(GetThreadState()->IsSweepingInProgress());
  DCHECK(GetThreadState()->SweepForbidden());
  DCHECK(ScriptForbiddenScope::IsScriptForbidden());

  size_t page_count = 1;
  // First, process empty pages to faster reduce memory footprint.
  while (BasePage* page = swept_unfinalized_empty_pages_.PopLocked()) {
    page->FinalizeSweep(SweepResult::kPageEmpty);
    if (page_count % kDeadlineCheckInterval == 0) {
      if (deadline <= base::TimeTicks::Now()) {
        // Deadline has come.
        return SweepingAndFinalizationCompleted();
      }
    }
    page_count++;
  }
  // Second, execute finalizers to leave more work for concurrent sweeper.
  while (BasePage* page = swept_unfinalized_pages_.PopLocked()) {
    swept_pages_.PushLocked(page);
    page->FinalizeSweep(SweepResult::kPageNotEmpty);
    if (page_count % kDeadlineCheckInterval == 0) {
      if (deadline <= base::TimeTicks::Now()) {
        // Deadline has come.
        return SweepingAndFinalizationCompleted();
      }
    }
    page_count++;
  }
  // Help concurrent sweeper.
  while (BasePage* page = unswept_pages_.PopLocked()) {
    SweepUnsweptPage(page);
    if (page_count % kDeadlineCheckInterval == 0) {
      if (deadline <= base::TimeTicks::Now()) {
        // Deadline has come.
        return SweepingAndFinalizationCompleted();
      }
    }
    page_count++;
  }

  return true;
}

void BaseArena::InvokeFinalizersOnSweptPages() {
  DCHECK(GetThreadState()->CheckThread());
  DCHECK(GetThreadState()->IsSweepingInProgress());
  DCHECK(GetThreadState()->SweepForbidden());
  while (BasePage* page = swept_unfinalized_pages_.PopLocked()) {
    swept_pages_.PushLocked(page);
    page->FinalizeSweep(SweepResult::kPageNotEmpty);
  }
  while (BasePage* page = swept_unfinalized_empty_pages_.PopLocked()) {
    page->FinalizeSweep(SweepResult::kPageEmpty);
  }
}

bool BaseArena::ConcurrentSweepOnePage() {
  BasePage* page = unswept_pages_.PopLocked();
  if (!page)
    return true;
  SweepUnsweptPageOnConcurrentThread(page);
  return false;
}

void BaseArena::CompleteSweep() {
  CHECK(GetThreadState()->IsSweepingInProgress());
  DCHECK(GetThreadState()->SweepForbidden());
  DCHECK(ScriptForbiddenScope::IsScriptForbidden());

  // Some phases, e.g. verification, require iterability of a page.
  MakeIterable();

  // First, finalize pages that have been processed by concurrent sweepers.
  InvokeFinalizersOnSweptPages();

  // Then, sweep and finalize pages.
  while (BasePage* page = unswept_pages_.PopLocked()) {
    SweepUnsweptPage(page);
  }

  // Verify object start bitmap after all freelists have been merged.
  VerifyObjectStartBitmap();
}

Address BaseArena::AllocateLargeObject(size_t allocation_size,
                                       size_t gc_info_index) {
  LargeObjectArena* large_object_arena = static_cast<LargeObjectArena*>(
      GetThreadState()->Heap().Arena(BlinkGC::kLargeObjectArenaIndex));
  Address large_object = large_object_arena->AllocateLargeObjectPage(
      allocation_size, gc_info_index);
  ASAN_MARK_LARGE_VECTOR_CONTAINER(this, large_object);
  return large_object;
}

NormalPageArena::NormalPageArena(ThreadState* state, int index)
    : BaseArena(state, index),
      current_allocation_point_(nullptr),
      remaining_allocation_size_(0),
      promptly_freed_size_(0) {}

void NormalPageArena::AddToFreeList(Address address, size_t size) {
#if DCHECK_IS_ON()
  DCHECK(FindPageFromAddress(address));
  DCHECK(FindPageFromAddress(address + size - 1));
#endif
  free_list_.Add(address, size);
  static_cast<NormalPage*>(PageFromObject(address))
      ->object_start_bit_map()
      ->SetBit<HeapObjectHeader::AccessMode::kAtomic>(address);
}

void NormalPageArena::MakeConsistentForGC() {
  BaseArena::MakeConsistentForGC();

  // Remove linear allocation area.
  SetAllocationPoint(nullptr, 0);
}

void NormalPageArena::ClearFreeLists() {
  SetAllocationPoint(nullptr, 0);
  free_list_.Clear();
  promptly_freed_size_ = 0;
}

void NormalPageArena::MakeIterable() {
  SetAllocationPoint(nullptr, 0);
}

size_t NormalPageArena::ArenaSize() {
  size_t size = 0;
  for (BasePage* page : swept_pages_) {
    size += page->size();
  }
  LOG_HEAP_FREELIST_VERBOSE()
      << "Heap size: " << size << "(" << ArenaIndex() << ")";
  return size;
}

size_t NormalPageArena::FreeListSize() {
  size_t free_size = free_list_.FreeListSize();
  LOG_HEAP_FREELIST_VERBOSE()
      << "Free size: " << free_size << "(" << ArenaIndex() << ")";
  return free_size;
}

void NormalPageArena::SweepAndCompact() {
  ThreadHeap& heap = GetThreadState()->Heap();
  if (!heap.Compaction()->IsCompactingArena(ArenaIndex()))
    return;

  if (SweepingCompleted()) {
    heap.Compaction()->FinishedArenaCompaction(this, 0, 0);
    return;
  }

  // Compaction is performed in-place, sliding objects down over unused
  // holes for a smaller heap page footprint and improved locality.
  // A "compaction pointer" is consequently kept, pointing to the next
  // available address to move objects down to. It will belong to one
  // of the already sweep-compacted pages for this arena, but as compaction
  // proceeds, it will not belong to the same page as the one being
  // currently compacted.
  //
  // The compaction pointer is represented by the
  // |(currentPage, allocationPoint)| pair, with |allocationPoint|
  // being the offset into |currentPage|, making up the next
  // available location. When the compaction of an arena page causes the
  // compaction pointer to exhaust the current page it is compacting into,
  // page compaction will advance the current page of the compaction
  // pointer, as well as the allocation point.
  //
  // By construction, the page compaction can be performed without having
  // to allocate any new pages. So to arrange for the page compaction's
  // supply of freed, available pages, we chain them together after each
  // has been "compacted from". The page compaction will then reuse those
  // as needed, and once finished, the chained, available pages can be
  // released back to the OS.
  //
  // To ease the passing of the compaction state when iterating over an
  // arena's pages, package it up into a |CompactionContext|.
  NormalPage::CompactionContext context;
  context.compacted_pages_ = &swept_pages_;

  while (BasePage* page = unswept_pages_.Pop()) {
    // Large objects do not belong to this arena.
    DCHECK(!page->IsLargeObjectPage());
    NormalPage* normal_page = static_cast<NormalPage*>(page);
    normal_page->MarkAsSwept();
    // If not the first page, add |normalPage| onto the available pages chain.
    if (!context.current_page_) {
      context.current_page_ = normal_page;
    } else {
      context.available_pages_.Push(normal_page);
    }
    normal_page->SweepAndCompact(context);
  }

  // All pages were empty; nothing to compact.
  if (!context.current_page_) {
    heap.Compaction()->FinishedArenaCompaction(this, 0, 0);
    return;
  }

  size_t freed_size = 0;
  size_t freed_page_count = 0;

  // If the current page hasn't been allocated into, add it to the available
  // list, for subsequent release below.
  size_t allocation_point = context.allocation_point_;
  if (!allocation_point) {
    context.available_pages_.Push(context.current_page_);
  } else {
    NormalPage* current_page = context.current_page_;
    swept_pages_.Push(current_page);
    if (allocation_point != current_page->PayloadSize()) {
      // Put the remainder of the page onto the free list.
      freed_size = current_page->PayloadSize() - allocation_point;
      Address payload = current_page->Payload();
      SET_MEMORY_INACCESSIBLE(payload + allocation_point, freed_size);
      current_page->ArenaForNormalPage()->AddToFreeList(
          payload + allocation_point, freed_size);
    }
  }

  // Return available pages to the free page pool, decommitting them from
  // the pagefile.
#if DEBUG_HEAP_COMPACTION
  std::stringstream stream;
#endif
  while (BasePage* available_pages = context.available_pages_.Pop()) {
    size_t page_size = available_pages->size();
#if DEBUG_HEAP_COMPACTION
    if (!freed_page_count)
      stream << "Releasing:";
    stream << " [" << available_pages << ", "
           << static_cast<void*>(reinterpret_cast<char*>(available_pages) +
                                 page_size)
           << "]";
#endif
    freed_size += page_size;
    freed_page_count++;
#if !(DCHECK_IS_ON() || defined(LEAK_SANITIZER) || \
      defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER))
    // Clear out the page before adding it to the free page pool, which
    // decommits it. Recommitting the page must find a zeroed page later.
    // We cannot assume that the OS will hand back a zeroed page across
    // its "decommit" operation.
    //
    // If in a debug setting, the unused page contents will have been
    // zapped already; leave it in that state.
    DCHECK(!available_pages->IsLargeObjectPage());
    NormalPage* unused_page = static_cast<NormalPage*>(available_pages);
    memset(unused_page->Payload(), 0, unused_page->PayloadSize());
#endif
    available_pages->RemoveFromHeap();
  }
#if DEBUG_HEAP_COMPACTION
  if (freed_page_count)
    LOG_HEAP_COMPACTION() << stream.str();
#endif
  heap.Compaction()->FinishedArenaCompaction(this, freed_page_count,
                                             freed_size);

  VerifyObjectStartBitmap();
}

void NormalPageArena::VerifyObjectStartBitmap() {
#if DCHECK_IS_ON()
  // Verifying object start bitmap requires iterability of pages. As compaction
  // may set up a new we have to reset here.
  SetAllocationPoint(nullptr, 0);
  for (BasePage* page : swept_pages_) {
    static_cast<NormalPage*>(page)
        ->VerifyObjectStartBitmapIsConsistentWithPayload();
  }
#endif  // DCHECK_IS_ON()
}

void BaseArena::VerifyMarking() {
#if DCHECK_IS_ON()
  // We cannot rely on other marking phases to clear the allocation area as
  // for incremental marking the application is running between steps and
  // might set up a new area. For large object arenas this is a no-op.
  ResetAllocationPoint();

  DCHECK(swept_unfinalized_pages_.IsEmpty());
  DCHECK(swept_unfinalized_empty_pages_.IsEmpty());
  // There may be objects on |swept_pages_| as pre-finalizers may allocate.
  // These objects may point to other object on |swept_pages_| or marked objects
  // on |unswept_pages_| but may never point to a dead (unmarked) object in
  // |unswept_pages_|.
  for (BasePage* page : swept_pages_) {
    page->VerifyMarking();
  }
  for (BasePage* page : unswept_pages_) {
    page->VerifyMarking();
  }
#endif  // DCHECK_IS_ON()
}

#if DCHECK_IS_ON()
bool NormalPageArena::IsConsistentForGC() {
  // A thread heap is consistent for sweeping if none of the pages to be swept
  // contain a freelist block or the current allocation point.
  FreeListEntry* entry = free_list_.FindEntry([this](FreeListEntry* entry) {
    return PagesToBeSweptContains(entry->GetAddress());
  });
  if (entry)
    return false;

  if (HasCurrentAllocationArea()) {
    if (PagesToBeSweptContains(CurrentAllocationPoint()))
      return false;
  }
  return true;
}

bool NormalPageArena::PagesToBeSweptContains(ConstAddress address) const {
  for (BasePage* page : unswept_pages_) {
    if (page->Contains(address))
      return true;
  }
  return false;
}
#endif

void NormalPageArena::AllocatePage() {
  PageMemory* page_memory =
      GetThreadState()->Heap().GetFreePagePool()->Take(ArenaIndex());

  if (!page_memory) {
    // Allocate a memory region for blinkPagesPerRegion pages that
    // will each have the following layout.
    //
    //    [ guard os page | ... payload ... | guard os page ]
    //    ^---{ aligned to blink page size }
    PageMemoryRegion* region = PageMemoryRegion::AllocateNormalPages(
        GetThreadState()->Heap().GetRegionTree());

    // Setup the PageMemory object for each of the pages in the region.
    for (size_t i = 0; i < kBlinkPagesPerRegion; ++i) {
      PageMemory* memory = PageMemory::SetupPageMemoryInRegion(
          region, i * kBlinkPageSize, BlinkPagePayloadSize());
      // Take the first possible page ensuring that this thread actually
      // gets a page and add the rest to the page pool.
      if (!page_memory) {
        bool result = memory->Commit();
        // If you hit the CHECK, it will mean that you're hitting the limit
        // of the number of mmapped regions the OS can support
        // (e.g., /proc/sys/vm/max_map_count in Linux) or on that Windows you
        // have exceeded the max commit charge across all processes for the
        // system.
        CHECK(result);
        page_memory = memory;
      } else {
        GetThreadState()->Heap().GetFreePagePool()->Add(ArenaIndex(), memory);
      }
    }
  }
  NormalPage* page =
      new (page_memory->WritableStart()) NormalPage(page_memory, this);
  swept_pages_.PushLocked(page);

  ThreadHeap& heap = GetThreadState()->Heap();
  heap.stats_collector()->IncreaseAllocatedSpace(page->size());
  heap.page_bloom_filter()->Add(page->GetAddress());
#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
  // Allow the following addToFreeList() to add the newly allocated memory
  // to the free list.
  ASAN_UNPOISON_MEMORY_REGION(page->Payload(), page->PayloadSize());
  Address address = page->Payload();
  for (size_t i = 0; i < page->PayloadSize(); i++)
    address[i] = kReuseAllowedZapValue;
  ASAN_POISON_MEMORY_REGION(page->Payload(), page->PayloadSize());
#endif
  AddToFreeList(page->Payload(), page->PayloadSize());
  SynchronizedStore(page);
}

void NormalPageArena::FreePage(NormalPage* page) {
  ThreadHeap& heap = GetThreadState()->Heap();
  heap.stats_collector()->DecreaseAllocatedSpace(page->size());
  heap.page_bloom_filter()->Remove(page->GetAddress());

  PageMemory* memory = page->Storage();
  page->~NormalPage();
  GetThreadState()->Heap().GetFreePagePool()->Add(ArenaIndex(), memory);
}

PlatformAwareObjectStartBitmap::PlatformAwareObjectStartBitmap(Address offset)
    : ObjectStartBitmap(offset) {}

ObjectStartBitmap::ObjectStartBitmap(Address offset) : offset_(offset) {
  Clear();
}

void ObjectStartBitmap::Clear() {
  memset(&object_start_bit_map_, 0, kReservedForBitmap);
}

void NormalPageArena::PromptlyFreeObject(HeapObjectHeader* header) {
  DCHECK(!GetThreadState()->IsMarkingInProgress());
  DCHECK(!GetThreadState()->SweepForbidden());
  Address address = reinterpret_cast<Address>(header);
  Address payload = header->Payload();
  size_t size = header->size();
  size_t payload_size = header->PayloadSize();
  DCHECK_GT(size, 0u);
#if DCHECK_IS_ON()
  DCHECK_EQ(PageFromObject(address), FindPageFromAddress(address));
#endif
  {
    ThreadState::SweepForbiddenScope forbidden_scope(GetThreadState());
    header->Finalize(payload, payload_size);
    if (IsObjectAllocatedAtAllocationPoint(header)) {
      current_allocation_point_ -= size;
      DCHECK_EQ(address, current_allocation_point_);
      remaining_allocation_size_ += size;
      SET_MEMORY_INACCESSIBLE(address, size);
      // Memory that is part of the allocation point is not allowed to be part
      // of the object start bit map.
      reinterpret_cast<NormalPage*>(PageFromObject(header))
          ->object_start_bit_map()
          ->ClearBit(address);
      return;
    }
    DCHECK(!header->IsMarked());
    PromptlyFreeObjectInFreeList(header, size);
  }
}

void NormalPageArena::PromptlyFreeObjectInFreeList(HeapObjectHeader* header,
                                                   size_t size) {
  DCHECK(!header->IsMarked());
  Address address = reinterpret_cast<Address>(header);
  NormalPage* page = reinterpret_cast<NormalPage*>(PageFromObject(header));
  if (page->HasBeenSwept()) {
    Address payload = header->Payload();
    size_t payload_size = header->PayloadSize();
    // If the page has been swept a promptly freed object may be adjacent
    // to other free list entries. We make the object available for future
    // allocation right away by adding it to the free list and increase the
    // promptly_freed_size_ counter which may result in coalescing later.
    SET_MEMORY_INACCESSIBLE(payload, payload_size);
    CHECK_MEMORY_INACCESSIBLE(payload, payload_size);
    AddToFreeList(address, size);
    promptly_freed_size_ += size;
  }
  GetThreadState()->Heap().stats_collector()->DecreaseAllocatedObjectSize(size);
}

bool NormalPageArena::ExpandObject(HeapObjectHeader* header, size_t new_size) {
  // It's possible that Vector requests a smaller expanded size because
  // Vector::shrinkCapacity can set a capacity smaller than the actual payload
  // size.
  if (header->PayloadSize() >= new_size)
    return true;
  size_t allocation_size = ThreadHeap::AllocationSizeFromSize(new_size);
  DCHECK_GT(allocation_size, header->size());
  size_t expand_size = allocation_size - header->size();
  if (IsObjectAllocatedAtAllocationPoint(header) &&
      expand_size <= remaining_allocation_size_) {
    current_allocation_point_ += expand_size;
    DCHECK_GE(remaining_allocation_size_, expand_size);
    remaining_allocation_size_ -= expand_size;
    // Unpoison the memory used for the object (payload).
    SET_MEMORY_ACCESSIBLE(header->PayloadEnd(), expand_size);
    header->SetSize(allocation_size);
#if DCHECK_IS_ON()
    DCHECK(FindPageFromAddress(header->PayloadEnd() - 1));
#endif
    return true;
  }
  return false;
}

bool NormalPageArena::ShrinkObject(HeapObjectHeader* header, size_t new_size) {
  DCHECK_GT(header->PayloadSize(), new_size);
  size_t allocation_size = ThreadHeap::AllocationSizeFromSize(new_size);
  DCHECK_GT(header->size(), allocation_size);
  size_t shrink_size = header->size() - allocation_size;
  if (IsObjectAllocatedAtAllocationPoint(header)) {
    current_allocation_point_ -= shrink_size;
    remaining_allocation_size_ += shrink_size;
    SET_MEMORY_INACCESSIBLE(current_allocation_point_, shrink_size);
    header->SetSize(allocation_size);
    return true;
  }
  DCHECK_GE(shrink_size, sizeof(HeapObjectHeader));
  DCHECK_GT(header->GcInfoIndex(), 0u);
  Address shrink_address = header->PayloadEnd() - shrink_size;
  HeapObjectHeader* freed_header = new (NotNull, shrink_address)
      HeapObjectHeader(shrink_size, header->GcInfoIndex());
  // Since only size has been changed, we don't need to update object starts.
  PromptlyFreeObjectInFreeList(freed_header, shrink_size);
#if DCHECK_IS_ON()
  DCHECK_EQ(PageFromObject(reinterpret_cast<Address>(header)),
            FindPageFromAddress(reinterpret_cast<Address>(header)));
#endif
  header->SetSize(allocation_size);

  return false;
}

Address NormalPageArena::AllocateFromFreeList(size_t allocation_size,
                                              size_t gc_info_index) {
  FreeListEntry* entry = free_list_.Allocate(allocation_size);
  if (!entry)
    return nullptr;

  SetAllocationPoint(entry->GetAddress(), entry->size());
  DCHECK(HasCurrentAllocationArea());
  DCHECK_GE(RemainingAllocationSize(), allocation_size);
  return AllocateObject(allocation_size, gc_info_index);
}

Address NormalPageArena::LazySweepPages(size_t allocation_size,
                                        size_t gc_info_index) {
  DCHECK(!HasCurrentAllocationArea());
  Address result = nullptr;
  // First, process unfinalized pages as finalizing a page is faster than
  // sweeping.
  while (BasePage* page = swept_unfinalized_pages_.PopLocked()) {
    swept_pages_.PushLocked(page);
    page->FinalizeSweep(SweepResult::kPageNotEmpty);
    // For NormalPage, stop lazy sweeping once we find a slot to
    // allocate a new object.
    result = AllocateFromFreeList(allocation_size, gc_info_index);
    if (result)
      return result;
  }
  while (BasePage* page = unswept_pages_.PopLocked()) {
    const bool is_empty = SweepUnsweptPage(page);
    if (!is_empty) {
      // For NormalPage, stop lazy sweeping once we find a slot to
      // allocate a new object.
      result = AllocateFromFreeList(allocation_size, gc_info_index);
      if (result)
        return result;
    }
  }
  return result;
}

void NormalPageArena::SetAllocationPoint(Address point, size_t size) {
#if DCHECK_IS_ON()
  if (point) {
    DCHECK(size);
    BasePage* page = PageFromObject(point);
    DCHECK(!page->IsLargeObjectPage());
    DCHECK_LE(size, static_cast<NormalPage*>(page)->PayloadSize());
  }
#endif
  // Free and clear the old linear allocation area.
  if (HasCurrentAllocationArea()) {
    AddToFreeList(CurrentAllocationPoint(), RemainingAllocationSize());
    GetThreadState()->Heap().stats_collector()->DecreaseAllocatedObjectSize(
        RemainingAllocationSize());
  }
  // Set up a new linear allocation area.
  current_allocation_point_ = point;
  remaining_allocation_size_ = size;
  // Update last allocated region in ThreadHeap. This must also be done if the
  // allocation point is set to 0 (before doing GC), so that the last allocated
  // region is automatically reset after GC.
  GetThreadState()->Heap().SetLastAllocatedRegion(point, size);
  if (point) {
    // Only, update allocated size and object start bitmap if the area is
    // actually set up with a non-null address.
    GetThreadState()->Heap().stats_collector()->IncreaseAllocatedObjectSize(
        size);
    // Current allocation point can never be part of the object bitmap start
    // because the area can grow or shrink. Will be added back before a GC when
    // clearing the allocation point.
    NormalPage* page = reinterpret_cast<NormalPage*>(PageFromObject(point));
    page->object_start_bit_map()
        ->ClearBit<HeapObjectHeader::AccessMode::kAtomic>(point);
    // Mark page as containing young objects.
    page->SetAsYoung(true);
  }
}

Address NormalPageArena::OutOfLineAllocate(size_t allocation_size,
                                           size_t gc_info_index) {
  Address result = OutOfLineAllocateImpl(allocation_size, gc_info_index);
  GetThreadState()->Heap().stats_collector()->AllocatedObjectSizeSafepoint();
  return result;
}

Address NormalPageArena::OutOfLineAllocateImpl(size_t allocation_size,
                                               size_t gc_info_index) {
  DCHECK_GT(allocation_size, RemainingAllocationSize());
  DCHECK_GE(allocation_size, kAllocationGranularity);

  // 1. If this allocation is big enough, allocate a large object.
  if (allocation_size >= kLargeObjectSizeThreshold)
    return AllocateLargeObject(allocation_size, gc_info_index);

  // 2. Try to allocate from a free list.
  Address result = AllocateFromFreeList(allocation_size, gc_info_index);
  if (result)
    return result;

  // 3. Reset the allocation point.
  SetAllocationPoint(nullptr, 0);

  // 4. Lazily sweep pages of this heap until we find a freed area for
  // this allocation or we finish sweeping all pages of this heap.
  result = LazySweep(allocation_size, gc_info_index);
  if (result)
    return result;

  // 5. Complete sweeping.
  GetThreadState()->CompleteSweep();

  // 6. Check if we should trigger a GC.
  GetThreadState()->ScheduleGCIfNeeded();

  // 7. Add a new page to this heap.
  AllocatePage();

  // 8. Try to allocate from a free list. This allocation must succeed.
  result = AllocateFromFreeList(allocation_size, gc_info_index);
  CHECK(result);
  return result;
}

LargeObjectArena::LargeObjectArena(ThreadState* state, int index)
    : BaseArena(state, index) {}

Address LargeObjectArena::AllocateLargeObjectPage(size_t allocation_size,
                                                  size_t gc_info_index) {
  // Caller already added space for object header and rounded up to allocation
  // alignment
  DCHECK(!(allocation_size & kAllocationMask));

  // 1. Try to sweep large objects more than allocationSize bytes
  // before allocating a new large object.
  Address result = LazySweep(allocation_size, gc_info_index);
  if (result)
    return result;

  // 2. If we have failed in sweeping allocationSize bytes,
  // we complete sweeping before allocating this large object.
  GetThreadState()->CompleteSweep();

  // 3. Check if we should trigger a GC.
  GetThreadState()->ScheduleGCIfNeeded();

  return DoAllocateLargeObjectPage(allocation_size, gc_info_index);
}

Address LargeObjectArena::DoAllocateLargeObjectPage(size_t allocation_size,
                                                    size_t gc_info_index) {
  size_t large_object_size =
      LargeObjectPage::PageHeaderSize() + allocation_size;
// If ASan is supported we add allocationGranularity bytes to the allocated
// space and poison that to detect overflows
#if defined(ADDRESS_SANITIZER)
  large_object_size += kAllocationGranularity;
#endif

  PageMemory* page_memory = PageMemory::Allocate(
      large_object_size, GetThreadState()->Heap().GetRegionTree());
  Address large_object_address = page_memory->WritableStart();
  Address header_address =
      large_object_address + LargeObjectPage::PageHeaderSize();
#if DCHECK_IS_ON()
  // Verify that the allocated PageMemory is expectedly zeroed.
  for (size_t i = 0; i < large_object_size; ++i)
    DCHECK(!large_object_address[i]);
#endif
  DCHECK_GT(gc_info_index, 0u);
  LargeObjectPage* large_object = new (large_object_address)
      LargeObjectPage(page_memory, this, allocation_size);
  HeapObjectHeader* header = new (NotNull, header_address)
      HeapObjectHeader(kLargeObjectSizeInHeader, gc_info_index);
  Address result = header_address + sizeof(*header);
  DCHECK(!(reinterpret_cast<uintptr_t>(result) & kAllocationMask));

  // Poison the object header and allocationGranularity bytes after the object
  ASAN_POISON_MEMORY_REGION(header, sizeof(*header));
  ASAN_POISON_MEMORY_REGION(large_object->GetAddress() + large_object->size(),
                            kAllocationGranularity);

  swept_pages_.PushLocked(large_object);

  // Update last allocated region in ThreadHeap.
  GetThreadState()->Heap().SetLastAllocatedRegion(large_object->Payload(),
                                                  large_object->PayloadSize());

  // Add all segments of kBlinkPageSize to the bloom filter so that the large
  // object can be kept by derived pointers on stack. An alternative might be to
  // prohibit derived pointers to large objects, but that is dangerous since the
  // compiler is free to optimize on-stack base pointers away.
  for (Address page_begin = RoundToBlinkPageStart(large_object->GetAddress());
       page_begin < large_object->PayloadEnd(); page_begin += kBlinkPageSize) {
    GetThreadState()->Heap().page_bloom_filter()->Add(page_begin);
  }
  GetThreadState()->Heap().stats_collector()->IncreaseAllocatedSpace(
      large_object->size());
  GetThreadState()->Heap().stats_collector()->IncreaseAllocatedObjectSize(
      large_object->PayloadSize());
  // Add page to the list of young pages.
  large_object->SetAsYoung(true);
  SynchronizedStore(large_object);
  return result;
}

void LargeObjectArena::FreeLargeObjectPage(LargeObjectPage* object) {
  ASAN_UNPOISON_MEMORY_REGION(object->Payload(), object->PayloadSize());
  object->ObjectHeader()->Finalize(object->Payload(), object->PayloadSize());
  ThreadHeap& heap = GetThreadState()->Heap();
  heap.stats_collector()->DecreaseAllocatedSpace(object->size());
  heap.page_bloom_filter()->Remove(object->GetAddress());

  // Unpoison the object header and allocationGranularity bytes after the
  // object before freeing.
  ASAN_UNPOISON_MEMORY_REGION(object->ObjectHeader(), sizeof(HeapObjectHeader));
  ASAN_UNPOISON_MEMORY_REGION(object->GetAddress() + object->size(),
                              kAllocationGranularity);

  PageMemory* memory = object->Storage();
  object->~LargeObjectPage();
  delete memory;
}

Address LargeObjectArena::LazySweepPages(size_t allocation_size,
                                         size_t gc_info_index) {
  Address result = nullptr;
  size_t swept_size = 0;
  while (BasePage* page = unswept_pages_.PopLocked()) {
    if (page->Sweep(FinalizeType::kInlined)) {
      swept_size += static_cast<LargeObjectPage*>(page)->ObjectSize();
      page->RemoveFromHeap();
      // For LargeObjectPage, stop lazy sweeping once we have swept
      // more than |allocation_size| bytes.
      if (swept_size >= allocation_size) {
        result = DoAllocateLargeObjectPage(allocation_size, gc_info_index);
        DCHECK(result);
        break;
      }
    } else {
      swept_pages_.PushLocked(page);
      page->MarkAsSwept();
    }
  }
  return result;
}

FreeList::FreeList() : biggest_free_list_index_(0) {
  Clear();
}

void FreeList::Add(Address address, size_t size) {
  DCHECK_LT(size, BlinkPagePayloadSize());
  // The free list entries are only pointer aligned (but when we allocate
  // from them we are 8 byte aligned due to the header size).
  DCHECK(!((reinterpret_cast<uintptr_t>(address) + sizeof(HeapObjectHeader)) &
           kAllocationMask));
  DCHECK(!(size & kAllocationMask));
  DCHECK(!PageFromObject(address)->IsLargeObjectPage());
  ASAN_UNPOISON_MEMORY_REGION(address, size);
  FreeListEntry* entry;
  if (size < sizeof(*entry)) {
    // Create a dummy header with only a size and freelist bit set.
    DCHECK_GE(size, sizeof(HeapObjectHeader));
    // Free list encode the size to mark the lost memory as freelist memory.
    new (NotNull, address)
        HeapObjectHeader(size, kGcInfoIndexForFreeListHeader);
    ASAN_POISON_MEMORY_REGION(address, size);
    // This memory gets lost. Sweeping can reclaim it.
    return;
  }
  entry = new (NotNull, address) FreeListEntry(size);

#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER)
  // The following logic delays reusing free lists for (at least) one GC
  // cycle. This is helpful to detect use-after-free errors that could be caused
  // by lazy sweeping etc.
  size_t allowed_count = 0;
  size_t forbidden_count = 0;
  GetAllowedAndForbiddenCounts(address, size, allowed_count, forbidden_count);
  size_t entry_count = size - sizeof(FreeListEntry);
  if (forbidden_count == entry_count) {
    // If all values in the memory region are reuseForbiddenZapValue,
    // we flip them to reuseAllowedZapValue. This allows the next
    // addToFreeList() to add the memory region to the free list
    // (unless someone concatenates the memory region with another memory
    // region that contains reuseForbiddenZapValue.)
    for (size_t i = sizeof(FreeListEntry); i < size; i++)
      address[i] = kReuseAllowedZapValue;
    ASAN_POISON_MEMORY_REGION(address, size);
    // Don't add the memory region to the free list in this addToFreeList().
    return;
  }
  if (allowed_count != entry_count) {
    // If the memory region mixes reuseForbiddenZapValue and
    // reuseAllowedZapValue, we (conservatively) flip all the values
    // to reuseForbiddenZapValue. These values will be changed to
    // reuseAllowedZapValue in the next addToFreeList().
    for (size_t i = sizeof(FreeListEntry); i < size; i++)
      address[i] = kReuseForbiddenZapValue;
    ASAN_POISON_MEMORY_REGION(address, size);
    // Don't add the memory region to the free list in this addToFreeList().
    return;
  }
// We reach here only when all the values in the memory region are
// reuseAllowedZapValue. In this case, we are allowed to add the memory
// region to the free list and reuse it for another object.
#endif
  ASAN_POISON_MEMORY_REGION(address, size);

  const int index = BucketIndexForSize(size);
  entry->Link(&free_list_heads_[index]);
  if (index > biggest_free_list_index_) {
    biggest_free_list_index_ = index;
  }
  if (!entry->Next()) {
    free_list_tails_[index] = entry;
  }
}

void FreeList::MoveFrom(FreeList* other) {
#if DCHECK_IS_ON()
  const size_t expected_size = FreeListSize() + other->FreeListSize();
#endif

  // Newly created entries get added to the head.
  for (size_t index = 0; index < kBlinkPageSizeLog2; ++index) {
    FreeListEntry* other_tail = other->free_list_tails_[index];
    FreeListEntry*& this_head = this->free_list_heads_[index];
    if (other_tail) {
      other_tail->Append(this_head);
      if (!this_head) {
        this->free_list_tails_[index] = other_tail;
      }
      this_head = other->free_list_heads_[index];
      other->free_list_heads_[index] = nullptr;
      other->free_list_tails_[index] = nullptr;
    }
  }

  biggest_free_list_index_ =
      std::max(biggest_free_list_index_, other->biggest_free_list_index_);
  other->biggest_free_list_index_ = 0;

#if DCHECK_IS_ON()
  DCHECK_EQ(expected_size, FreeListSize());
#endif
  DCHECK(other->IsEmpty());
}

FreeListEntry* FreeList::Allocate(size_t allocation_size) {
  // Try reusing a block from the largest bin. The underlying reasoning
  // being that we want to amortize this slow allocation call by carving
  // off as a large a free block as possible in one go; a block that will
  // service this block and let following allocations be serviced quickly
  // by bump allocation.
  size_t bucket_size = static_cast<size_t>(1) << biggest_free_list_index_;
  int index = biggest_free_list_index_;
  for (; index > 0; --index, bucket_size >>= 1) {
    DCHECK(IsConsistent(index));
    FreeListEntry* entry = free_list_heads_[index];
    if (allocation_size > bucket_size) {
      // Final bucket candidate; check initial entry if it is able
      // to service this allocation. Do not perform a linear scan,
      // as it is considered too costly.
      if (!entry || entry->size() < allocation_size)
        break;
    }
    if (entry) {
      if (!entry->Next()) {
        DCHECK_EQ(entry, free_list_tails_[index]);
        free_list_tails_[index] = nullptr;
      }
      entry->Unlink(&free_list_heads_[index]);
      biggest_free_list_index_ = index;
      return entry;
    }
  }
  biggest_free_list_index_ = index;
  return nullptr;
}

#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER)
NO_SANITIZE_MEMORY
void NOINLINE FreeList::GetAllowedAndForbiddenCounts(Address address,
                                                     size_t size,
                                                     size_t& allowed_count,
                                                     size_t& forbidden_count) {
  for (size_t i = sizeof(FreeListEntry); i < size; i++) {
    if (address[i] == kReuseAllowedZapValue)
      allowed_count++;
    else if (address[i] == kReuseForbiddenZapValue)
      forbidden_count++;
    else
      NOTREACHED();
  }
}

NO_SANITIZE_ADDRESS
NO_SANITIZE_MEMORY
void NOINLINE FreeList::ZapFreedMemory(Address address, size_t size) {
  for (size_t i = 0; i < size; i++) {
    // See the comment in addToFreeList().
    if (address[i] != kReuseAllowedZapValue)
      address[i] = kReuseForbiddenZapValue;
  }
}

void NOINLINE FreeList::CheckFreedMemoryIsZapped(Address address, size_t size) {
  for (size_t i = 0; i < size; i++) {
    DCHECK(address[i] == kReuseAllowedZapValue ||
           address[i] == kReuseForbiddenZapValue);
  }
}
#endif

size_t FreeList::FreeListSize() const {
  size_t free_size = 0;
  for (unsigned i = 0; i < kBlinkPageSizeLog2; ++i) {
    FreeListEntry* entry = free_list_heads_[i];
    while (entry) {
      free_size += entry->size();
      entry = entry->Next();
    }
  }
#if DEBUG_HEAP_FREELIST
  if (free_size) {
    LOG_HEAP_FREELIST_VERBOSE() << "FreeList(" << this << "): " << free_size;
    for (unsigned i = 0; i < kBlinkPageSizeLog2; ++i) {
      FreeListEntry* entry = free_list_heads_[i];
      size_t bucket = 0;
      size_t count = 0;
      while (entry) {
        bucket += entry->size();
        count++;
        entry = entry->Next();
      }
      if (bucket) {
        LOG_HEAP_FREELIST_VERBOSE()
            << "[" << (0x1 << i) << ", " << (0x1 << (i + 1)) << "]: " << bucket
            << " (" << count << ")";
      }
    }
  }
#endif
  return free_size;
}

void FreeList::Clear() {
  biggest_free_list_index_ = 0;
  for (size_t i = 0; i < kBlinkPageSizeLog2; ++i) {
    free_list_heads_[i] = nullptr;
    free_list_tails_[i] = nullptr;
  }
}

bool FreeList::IsEmpty() const {
  if (biggest_free_list_index_)
    return false;
  for (size_t i = 0; i < kBlinkPageSizeLog2; ++i) {
    if (free_list_heads_[i]) {
      DCHECK(free_list_tails_[i]);
      return false;
    }
  }
  return true;
}

int FreeList::BucketIndexForSize(size_t size) {
  DCHECK_GT(size, 0u);
  int index = -1;
  while (size) {
    size >>= 1;
    index++;
  }
  return index;
}

void FreeList::CollectStatistics(
    ThreadState::Statistics::FreeListStatistics* stats) {
  Vector<size_t> bucket_size;
  Vector<size_t> free_count;
  Vector<size_t> free_size;
  for (size_t i = 0; i < kBlinkPageSizeLog2; ++i) {
    size_t entry_count = 0;
    size_t entry_size = 0;
    for (FreeListEntry* entry = free_list_heads_[i]; entry;
         entry = entry->Next()) {
      ++entry_count;
      entry_size += entry->size();
    }
    bucket_size.push_back(1 << i);
    free_count.push_back(entry_count);
    free_size.push_back(entry_size);
  }
  *stats = {std::move(bucket_size), std::move(free_count),
            std::move(free_size)};
}

BasePage::BasePage(PageMemory* storage, BaseArena* arena, PageType page_type)
    : storage_(storage),
      arena_(arena),
      thread_state_(arena->GetThreadState()),
      page_type_(page_type) {
#if DCHECK_IS_ON()
  DCHECK(IsPageHeaderAddress(reinterpret_cast<Address>(this)));
#endif
}

NormalPage::NormalPage(PageMemory* storage, BaseArena* arena)
    : BasePage(storage, arena, PageType::kNormalPage),
      object_start_bit_map_(Payload()) {
#if DCHECK_IS_ON()
  DCHECK(IsPageHeaderAddress(reinterpret_cast<Address>(this)));
#endif  // DCHECK_IS_ON()
}

NormalPage::~NormalPage() {
#if DCHECK_IS_ON()
  DCHECK(IsPageHeaderAddress(reinterpret_cast<Address>(this)));
#endif
}

size_t NormalPage::ObjectPayloadSizeForTesting() {
  size_t object_payload_size = 0;
  Address header_address = Payload();
  DCHECK_NE(header_address, PayloadEnd());
  do {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(header_address);
    if (!header->IsFree()) {
      object_payload_size += header->PayloadSize();
    }
    DCHECK_LT(header->size(), BlinkPagePayloadSize());
    header_address += header->size();
    DCHECK_LE(header_address, PayloadEnd());
  } while (header_address < PayloadEnd());
  return object_payload_size;
}

void NormalPage::RemoveFromHeap() {
  ArenaForNormalPage()->FreePage(this);
}

#if !DCHECK_IS_ON() && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
static void DiscardPages(Address begin, Address end) {
  uintptr_t begin_address =
      base::RoundUpToSystemPage(reinterpret_cast<uintptr_t>(begin));
  uintptr_t end_address =
      base::RoundDownToSystemPage(reinterpret_cast<uintptr_t>(end));
  if (begin_address < end_address) {
    base::DiscardSystemPages(reinterpret_cast<void*>(begin_address),
                             end_address - begin_address);
  }
}
#endif

void NormalPage::ToBeFinalizedObject::Finalize() {
  const size_t size = header->size();
  // This is a fast version of header->PayloadSize().
  const size_t payload_size = size - sizeof(HeapObjectHeader);
  const Address payload = header->Payload();
  // For ASan, unpoison the object before calling the finalizer. The
  // finalized object will be zero-filled and poison'ed afterwards.
  // Given all other unmarked objects are poisoned, ASan will detect
  // an error if the finalizer touches any other on-heap object that
  // die at the same GC cycle.
  ASAN_UNPOISON_MEMORY_REGION(payload, payload_size);

  header->Finalize(payload, payload_size);
  // This memory will be added to the freelist. Maintain the invariant
  // that memory on the freelist is zero filled.
  SET_MEMORY_INACCESSIBLE(reinterpret_cast<Address>(header), size);
}

void NormalPage::FinalizeSweep(SweepResult action) {
  // Call finalizers.
  for (ToBeFinalizedObject& object : to_be_finalized_objects_) {
    object.Finalize();
  }
  to_be_finalized_objects_.clear();
#if BUILDFLAG(BLINK_HEAP_YOUNG_GENERATION)
  // Copy object start bit map.
  DCHECK(cached_object_start_bit_map_);
  object_start_bit_map_ = *cached_object_start_bit_map_;
  cached_object_start_bit_map_.reset();
#endif
  // Merge freelists or unmap the page.
  if (action == SweepResult::kPageNotEmpty) {
    MergeFreeLists();
    MarkAsSwept();
  } else {
    DCHECK(action == SweepResult::kPageEmpty);
    RemoveFromHeap();
  }
}

void NormalPage::AddToFreeList(Address start,
                               size_t size,
                               FinalizeType finalize_type,
                               bool found_finalizer) {
  // If a free allocation block contains an object that is yet to be
  // finalized, push it in a separate freelist to preserve the guarantee
  // that all freelist entries are zeroed out.
  if (found_finalizer && finalize_type == FinalizeType::kDeferred) {
    FutureFreelistEntry entry{start, size};
    unfinalized_freelist_.push_back(std::move(entry));
  } else {
    cached_freelist_.Add(start, size);
#if BUILDFLAG(BLINK_HEAP_YOUNG_GENERATION)
    cached_object_start_bit_map_->SetBit(start);
#else
    object_start_bit_map_.SetBit(start);
#endif
#if !DCHECK_IS_ON() && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
    if (Arena()->GetThreadState()->IsMemoryReducingGC()) {
      DiscardPages(start + sizeof(FreeListEntry), start + size);
    }
#endif
  }
}

void NormalPage::MergeFreeLists() {
  NormalPageArena* arena = ArenaForNormalPage();
  arena->AddToFreeList(&cached_freelist_);
  DCHECK(cached_freelist_.IsEmpty());

  for (const FutureFreelistEntry& entry : unfinalized_freelist_) {
    arena->AddToFreeList(entry.start, entry.size);
#if !DCHECK_IS_ON() && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
    if (Arena()->GetThreadState()->IsMemoryReducingGC()) {
      DiscardPages(entry.start + sizeof(FreeListEntry),
                   entry.start + entry.size);
    }
#endif
  }
  unfinalized_freelist_.clear();
}

bool NormalPage::Sweep(FinalizeType finalize_type) {
  PlatformAwareObjectStartBitmap* bitmap;
#if BUILDFLAG(BLINK_HEAP_YOUNG_GENERATION)
  cached_object_start_bit_map_ =
      std::make_unique<PlatformAwareObjectStartBitmap>(Payload());
  bitmap = cached_object_start_bit_map_.get();
#else
  object_start_bit_map()->Clear();
  bitmap = object_start_bit_map();
#endif
  cached_freelist_.Clear();
  unfinalized_freelist_.clear();
  Address start_of_gap = Payload();
  bool found_finalizer = false;
  for (Address header_address = start_of_gap; header_address < PayloadEnd();) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(header_address);
    const size_t size = header->size();
    DCHECK_GT(size, 0u);
    DCHECK_LT(size, BlinkPagePayloadSize());

    if (header->IsFree<HeapObjectHeader::AccessMode::kAtomic>()) {
      // Zero the memory in the free list header to maintain the
      // invariant that memory on the free list is zero filled.
      // The rest of the memory is already on the free list and is
      // therefore already zero filled.
      SET_MEMORY_INACCESSIBLE(header_address,
                              std::min(size, sizeof(FreeListEntry)));
      CHECK_MEMORY_INACCESSIBLE(header_address, size);
      header_address += size;
      continue;
    }
    if (!header->IsMarked<HeapObjectHeader::AccessMode::kAtomic>()) {
      // The following accesses to the header are safe non-atomically, because
      // we just established the invariant that the object is not marked.
      ToBeFinalizedObject object{header};
      if (finalize_type == FinalizeType::kInlined ||
          !header->HasNonTrivialFinalizer()) {
        // In case the header doesn't have a finalizer, we eagerly call a
        // freehook.
        // TODO(bikineev): It may be unsafe to do this concurrently.
        object.Finalize();
      } else {
        to_be_finalized_objects_.push_back(std::move(object));
        found_finalizer = true;
      }
      header_address += size;
      continue;
    }
    if (start_of_gap != header_address) {
      AddToFreeList(start_of_gap, header_address - start_of_gap, finalize_type,
                    found_finalizer);
      found_finalizer = false;
    }
    bitmap->SetBit(header_address);
#if !BUILDFLAG(BLINK_HEAP_YOUNG_GENERATION)
    header->Unmark<HeapObjectHeader::AccessMode::kAtomic>();
#endif
    header_address += size;
    start_of_gap = header_address;
  }
  // Only add the memory to the free list if the page is not completely empty
  // and we are not at the end of the page. Empty pages are not added to the
  // free list as the pages are removed immediately.
  if (start_of_gap != Payload() && start_of_gap != PayloadEnd()) {
    AddToFreeList(start_of_gap, PayloadEnd() - start_of_gap, finalize_type,
                  found_finalizer);
  }
  return start_of_gap == Payload();
}

void NormalPage::SweepAndCompact(CompactionContext& context) {
  object_start_bit_map()->Clear();
  NormalPage*& current_page = context.current_page_;
  size_t& allocation_point = context.allocation_point_;

  NormalPageArena* page_arena = ArenaForNormalPage();
#if defined(ADDRESS_SANITIZER)
  bool is_vector_arena =
      ThreadHeap::IsVectorArenaIndex(page_arena->ArenaIndex());
#endif
  HeapCompact* compact = page_arena->GetThreadState()->Heap().Compaction();
  for (Address header_address = Payload(); header_address < PayloadEnd();) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(header_address);
    size_t size = header->size();
    DCHECK_GT(size, 0u);
    DCHECK_LT(size, BlinkPagePayloadSize());

    if (header->IsFree()) {
      // Unpoison the freelist entry so that we
      // can compact into it as wanted.
      ASAN_UNPOISON_MEMORY_REGION(header_address, size);
      header_address += size;
      continue;
    }
    // This is a fast version of header->PayloadSize().
    size_t payload_size = size - sizeof(HeapObjectHeader);
    Address payload = header->Payload();
    if (!header->IsMarked()) {
      // For ASan, unpoison the object before calling the finalizer. The
      // finalized object will be zero-filled and poison'ed afterwards.
      // Given all other unmarked objects are poisoned, ASan will detect
      // an error if the finalizer touches any other on-heap object that
      // die at the same GC cycle.
      ASAN_UNPOISON_MEMORY_REGION(header_address, size);
      // Compaction is currently launched only from AtomicPhaseEpilogue, so it's
      // guaranteed to be on the mutator thread - no need to postpone
      // finalization.
      header->Finalize(payload, payload_size);

// As compaction is under way, leave the freed memory accessible
// while compacting the rest of the page. We just zap the payload
// to catch out other finalizers trying to access it.
#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER)
      FreeList::ZapFreedMemory(payload, payload_size);
#endif
      header_address += size;
      continue;
    }
#if !BUILDFLAG(BLINK_HEAP_YOUNG_GENERATION)
    header->Unmark();
#endif
    // Allocate and copy over the live object.
    Address compact_frontier = current_page->Payload() + allocation_point;
    if (compact_frontier + size > current_page->PayloadEnd()) {
      // Can't fit on current allocation page; add remaining onto the
      // freelist and advance to next available page.
      //
      // TODO(sof): be more clever & compact later objects into
      // |currentPage|'s unused slop.
      context.compacted_pages_->Push(current_page);
      size_t free_size = current_page->PayloadSize() - allocation_point;
      if (free_size) {
        SET_MEMORY_INACCESSIBLE(compact_frontier, free_size);
        current_page->ArenaForNormalPage()->AddToFreeList(compact_frontier,
                                                          free_size);
      }

      current_page = static_cast<NormalPage*>(context.available_pages_.Pop());
      allocation_point = 0;
      compact_frontier = current_page->Payload();
    }
    if (LIKELY(compact_frontier != header_address)) {
#if defined(ADDRESS_SANITIZER)
      // Unpoison the header + if it is a vector backing
      // store object, let go of the container annotations.
      // Do that by unpoisoning the payload entirely.
      ASAN_UNPOISON_MEMORY_REGION(header, sizeof(HeapObjectHeader));
      if (is_vector_arena) {
        ASAN_UNPOISON_MEMORY_REGION(payload, payload_size);
      }
#endif
      // Use a non-overlapping copy, if possible.
      if (current_page == this)
        memmove(compact_frontier, header_address, size);
      else
        memcpy(compact_frontier, header_address, size);
      compact->Relocate(payload, compact_frontier + sizeof(HeapObjectHeader));
    }
    current_page->object_start_bit_map()->SetBit(compact_frontier);
    header_address += size;
    allocation_point += size;
    DCHECK(allocation_point <= current_page->PayloadSize());
  }

#if DCHECK_IS_ON() || defined(LEAK_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(MEMORY_SANITIZER)
  // Zap the unused portion, until it is either compacted into or freed.
  if (current_page != this) {
    FreeList::ZapFreedMemory(Payload(), PayloadSize());
  } else {
    FreeList::ZapFreedMemory(Payload() + allocation_point,
                             PayloadSize() - allocation_point);
  }
#endif
}

void NormalPage::MakeConsistentForMutator() {
  object_start_bit_map()->Clear();
  Address start_of_gap = Payload();
  NormalPageArena* normal_arena = ArenaForNormalPage();
  for (Address header_address = Payload(); header_address < PayloadEnd();) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(header_address);
    size_t size = header->size();
    DCHECK_LT(size, BlinkPagePayloadSize());
    if (header->IsFree()) {
      // Zero the memory in the free list header to maintain the
      // invariant that memory on the free list is zero filled.
      // The rest of the memory is already on the free list and is
      // therefore already zero filled.
      SET_MEMORY_INACCESSIBLE(header_address, size < sizeof(FreeListEntry)
                                                  ? size
                                                  : sizeof(FreeListEntry));
      CHECK_MEMORY_INACCESSIBLE(header_address, size);
      header_address += size;
      continue;
    }
    if (start_of_gap != header_address)
      normal_arena->AddToFreeList(start_of_gap, header_address - start_of_gap);
    if (header->IsMarked()) {
      header->Unmark();
    }
    object_start_bit_map()->SetBit(header_address);
    header_address += size;
    start_of_gap = header_address;
    DCHECK_LE(header_address, PayloadEnd());
  }
  if (start_of_gap != PayloadEnd())
    normal_arena->AddToFreeList(start_of_gap, PayloadEnd() - start_of_gap);

  VerifyObjectStartBitmapIsConsistentWithPayload();
}

// This is assumed to be called from the atomic pause, so no concurrency should
// be involved here.
void NormalPage::Unmark() {
  const Address current_allocation_point =
      ArenaForNormalPage()->CurrentAllocationPoint();
  const size_t allocation_area_size =
      ArenaForNormalPage()->RemainingAllocationSize();
  for (Address header_address = Payload(); header_address < PayloadEnd();) {
    // Since unmarking can happen inside IncrementalMarkingStart, the current
    // allocation point can be set and we need to skip over it.
    if (header_address == current_allocation_point && allocation_area_size) {
      header_address += allocation_area_size;
      continue;
    }
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(header_address);
    if (header->IsMarked()) {
      header->Unmark();
    }
    header_address += header->size();
  }
  ClearCardTable();
}

#if defined(ADDRESS_SANITIZER)
void NormalPage::PoisonUnmarkedObjects() {
  for (Address header_address = Payload(); header_address < PayloadEnd();) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(header_address);
    DCHECK_LT(header->size(), BlinkPagePayloadSize());
    // Check if a free list entry first since we cannot call
    // isMarked on a free list entry.
    if (header->IsFree()) {
      header_address += header->size();
      continue;
    }
    if (!header->IsMarked()) {
      ASAN_POISON_MEMORY_REGION(header->Payload(), header->PayloadSize());
    }
    header_address += header->size();
  }
}
#endif

void NormalPage::VerifyObjectStartBitmapIsConsistentWithPayload() {
#if DCHECK_IS_ON()
  HeapObjectHeader* current_header =
      reinterpret_cast<HeapObjectHeader*>(Payload());
  object_start_bit_map()->Iterate([this,
                                   &current_header](Address object_address) {
    const HeapObjectHeader* object_header =
        reinterpret_cast<HeapObjectHeader*>(object_address);
    DCHECK_EQ(object_header, current_header);
    current_header = reinterpret_cast<HeapObjectHeader*>(object_address +
                                                         object_header->size());
    // Skip over allocation area.
    if (reinterpret_cast<Address>(current_header) ==
        ArenaForNormalPage()->CurrentAllocationPoint()) {
      current_header = reinterpret_cast<HeapObjectHeader*>(
          ArenaForNormalPage()->CurrentAllocationPoint() +
          ArenaForNormalPage()->RemainingAllocationSize());
    }
  });
#endif  // DCHECK_IS_ON()
}

void NormalPage::VerifyMarking() {
  DCHECK(!ArenaForNormalPage()->CurrentAllocationPoint());
  MarkingVerifier verifier(ArenaForNormalPage()->GetThreadState());
  for (Address header_address = Payload(); header_address < PayloadEnd();) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(header_address);
    verifier.VerifyObject(header);
    header_address += header->size();
  }
}

void LargeObjectPage::VerifyMarking() {
  MarkingVerifier verifier(Arena()->GetThreadState());
  verifier.VerifyObject(ObjectHeader());
}

HeapObjectHeader* NormalPage::ConservativelyFindHeaderFromAddress(
    ConstAddress address) const {
  if (!ContainedInObjectPayload(address))
    return nullptr;
  if (ArenaForNormalPage()->IsInCurrentAllocationPointRegion(address))
    return nullptr;
  HeapObjectHeader* header = reinterpret_cast<HeapObjectHeader*>(
      object_start_bit_map()->FindHeader(address));
  if (header->IsFree())
    return nullptr;
  DCHECK_LT(0u, header->GcInfoIndex());
  DCHECK_GT(header->PayloadEnd(), address);
  return header;
}

void NormalPage::CollectStatistics(
    ThreadState::Statistics::ArenaStatistics* arena_stats) {
  HeapObjectHeader* header = nullptr;
  size_t live_size = 0;
  for (Address header_address = Payload(); header_address < PayloadEnd();
       header_address += header->size()) {
    header = reinterpret_cast<HeapObjectHeader*>(header_address);
    if (!header->IsFree()) {
      // All non-free objects, dead or alive, are considered as live for the
      // purpose of taking a snapshot.
      live_size += header->size();
      if (!NameClient::HideInternalName()) {
        // Detailed names available.
        uint32_t gc_info_index = header->GcInfoIndex();
        arena_stats->object_stats.type_count[gc_info_index]++;
        arena_stats->object_stats.type_bytes[gc_info_index] += header->size();
        if (arena_stats->object_stats.type_name[gc_info_index].empty()) {
          arena_stats->object_stats.type_name[gc_info_index] = header->Name();
        }
      }
    }
  }
  arena_stats->committed_size_bytes += kBlinkPageSize;
  arena_stats->used_size_bytes += live_size;
  arena_stats->page_stats.emplace_back(
      ThreadState::Statistics::PageStatistics{kBlinkPageSize, live_size});
}

#if DCHECK_IS_ON()
bool NormalPage::Contains(ConstAddress addr) const {
  Address blink_page_start = RoundToBlinkPageStart(GetAddress());
  // Page is at aligned address plus guard page size.
  DCHECK_EQ(blink_page_start, GetAddress() - BlinkGuardPageSize());
  return blink_page_start <= addr && addr < blink_page_start + kBlinkPageSize;
}
#endif

LargeObjectPage::LargeObjectPage(PageMemory* storage,
                                 BaseArena* arena,
                                 size_t object_size)
    : BasePage(storage, arena, PageType::kLargeObjectPage),
      object_size_(object_size)
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
      ,
      is_vector_backing_page_(false)
#endif
{
}

size_t LargeObjectPage::ObjectPayloadSizeForTesting() {
  return PayloadSize();
}

void LargeObjectPage::RemoveFromHeap() {
  static_cast<LargeObjectArena*>(Arena())->FreeLargeObjectPage(this);
}

bool LargeObjectPage::Sweep(FinalizeType) {
  if (!ObjectHeader()->IsMarked()) {
    return true;
  }
#if !BUILDFLAG(BLINK_HEAP_YOUNG_GENERATION)
  ObjectHeader()->Unmark();
#endif
  return false;
}

void LargeObjectPage::Unmark() {
  HeapObjectHeader* header = ObjectHeader();
  if (header->IsMarked()) {
    header->Unmark();
  }
  SetRemembered(false);
}

void LargeObjectPage::MakeConsistentForMutator() {
  Unmark();
}

void LargeObjectPage::FinalizeSweep(SweepResult action) {
  if (action == SweepResult::kPageNotEmpty) {
    MarkAsSwept();
  } else {
    DCHECK(action == SweepResult::kPageEmpty);
    RemoveFromHeap();
  }
}

#if defined(ADDRESS_SANITIZER)
void LargeObjectPage::PoisonUnmarkedObjects() {
  HeapObjectHeader* header = ObjectHeader();
  if (!header->IsMarked()) {
    ASAN_POISON_MEMORY_REGION(header->Payload(), header->PayloadSize());
  }
}
#endif

void LargeObjectPage::CollectStatistics(
    ThreadState::Statistics::ArenaStatistics* arena_stats) {
  HeapObjectHeader* header = ObjectHeader();
  size_t live_size = 0;
  // All non-free objects, dead or alive, are considered as live for the
  // purpose of taking a snapshot.
  live_size += ObjectSize();
  if (!NameClient::HideInternalName()) {
    // Detailed names available.
    uint32_t gc_info_index = header->GcInfoIndex();
    arena_stats->object_stats.type_count[gc_info_index]++;
    arena_stats->object_stats.type_bytes[gc_info_index] += ObjectSize();
  }

  arena_stats->committed_size_bytes += size();
  arena_stats->used_size_bytes += live_size;
  arena_stats->page_stats.emplace_back(
      ThreadState::Statistics::PageStatistics{size(), live_size});
}

#if DCHECK_IS_ON()
bool LargeObjectPage::Contains(ConstAddress object) const {
  return RoundToBlinkPageStart(GetAddress()) <= object &&
         object < RoundToBlinkPageEnd(GetAddress() + size());
}
#endif

}  // namespace blink
