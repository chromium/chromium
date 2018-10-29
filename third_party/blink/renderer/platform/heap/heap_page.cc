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
#include "third_party/blink/renderer/platform/heap/address_cache.h"
#include "third_party/blink/renderer/platform/heap/blink_gc_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/heap/heap_compact.h"
#include "third_party/blink/renderer/platform/heap/heap_stats_collector.h"
#include "third_party/blink/renderer/platform/heap/marking_verifier.h"
#include "third_party/blink/renderer/platform/heap/page_memory.h"
#include "third_party/blink/renderer/platform/heap/page_pool.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/memory_coordinator.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/container_annotations.h"
#include "third_party/blink/renderer/platform/wtf/leak_annotations.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

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

#if DCHECK_IS_ON() && defined(ARCH_CPU_64_BITS)
NO_SANITIZE_ADDRESS
void HeapObjectHeader::ZapMagic() {
  CheckHeader();
  magic_ = kZappedMagic;
}
#endif

void HeapObjectHeader::Finalize(Address object, size_t object_size) {
  HeapAllocHooks::FreeHookIfEnabled(object);
  const GCInfo* gc_info = GCInfoTable::Get().GCInfoFromIndex(GcInfoIndex());
  if (gc_info->HasFinalizer())
    gc_info->finalize_(object);

  ASAN_RETIRE_CONTAINER_ANNOTATION(object, object_size);
}

BaseArena::BaseArena(ThreadState* state, int index)
    : first_page_(nullptr),
      first_unswept_page_(nullptr),
      thread_state_(state),
      index_(index) {}

BaseArena::~BaseArena() {
  DCHECK(!first_page_);
  DCHECK(SweepingCompleted());
}

void BaseArena::RemoveAllPages() {
  ClearFreeLists();

  DCHECK(SweepingCompleted());
  while (first_page_) {
    BasePage* page = first_page_;
    page->Unlink(&first_page_);
    page->RemoveFromHeap();
  }
}

void BaseArena::TakeSnapshot(const String& dump_base_name,
                             ThreadState::GCSnapshotInfo& info) {
  // |dumpBaseName| at this point is "blink_gc/thread_X/heaps/HeapName"
  base::trace_event::MemoryAllocatorDump* allocator_dump =
      BlinkGCMemoryDumpProvider::Instance()
          ->CreateMemoryAllocatorDumpForCurrentGC(dump_base_name);
  size_t page_count = 0;
  BasePage::HeapSnapshotInfo heap_info;
  for (BasePage* page = first_unswept_page_; page; page = page->Next()) {
    String dump_name = dump_base_name +
                       String::Format("/pages/page_%lu",
                                      static_cast<unsigned long>(page_count++));
    base::trace_event::MemoryAllocatorDump* page_dump =
        BlinkGCMemoryDumpProvider::Instance()
            ->CreateMemoryAllocatorDumpForCurrentGC(dump_name);

    page->TakeSnapshot(page_dump, info, heap_info);
  }
  allocator_dump->AddScalar("blink_page_count", "objects", page_count);

  // When taking a full dump (w/ freelist), both the /buckets and /pages
  // report their free size but they are not meant to be added together.
  // Therefore, here we override the free_size of the parent heap to be
  // equal to the free_size of the sum of its heap pages.
  allocator_dump->AddScalar("free_size", "bytes", heap_info.free_size);
  allocator_dump->AddScalar("free_count", "objects", heap_info.free_count);
}

#if DCHECK_IS_ON()
BasePage* BaseArena::FindPageFromAddress(Address address) {
  for (BasePage* page = first_page_; page; page = page->Next()) {
    if (page->Contains(address))
      return page;
  }
  for (BasePage* page = first_unswept_page_; page; page = page->Next()) {
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
  CHECK(SweepingCompleted());

  HeapCompact* heap_compactor = GetThreadState()->Heap().Compaction();
  if (!heap_compactor->IsCompactingArena(ArenaIndex()))
    return;

  BasePage* next_page = first_page_;
  while (next_page) {
    if (!next_page->IsLargeObjectPage())
      heap_compactor->AddCompactingPage(next_page);
    next_page = next_page->Next();
  }
}

void BaseArena::MakeConsistentForMutator() {
  ClearFreeLists();
#if DCHECK_IS_ON()
  DCHECK(IsConsistentForGC());
#endif
  DCHECK(!first_page_);

  // Drop marks from marked objects and rebuild free lists in preparation for
  // resuming the executions of mutators.
  BasePage* previous_page = nullptr;
  for (BasePage *page = first_unswept_page_; page;
       previous_page = page, page = page->Next()) {
    page->MakeConsistentForMutator();
    page->MarkAsSwept();
  }
  if (previous_page) {
    DCHECK(!SweepingCompleted());
    previous_page->next_ = first_page_;
    first_page_ = first_unswept_page_;
    first_unswept_page_ = nullptr;
  }
  DCHECK(SweepingCompleted());

  VerifyObjectStartBitmap();
}

size_t BaseArena::ObjectPayloadSizeForTesting() {
#if DCHECK_IS_ON()
  DCHECK(IsConsistentForGC());
#endif
  // DCHECK(SweepingCompleted());

  size_t object_payload_size = 0;
  for (BasePage* page = first_unswept_page_; page; page = page->Next())
    object_payload_size += page->ObjectPayloadSizeForTesting();
  return object_payload_size;
}

void BaseArena::PrepareForSweep() {
  DCHECK(GetThreadState()->InAtomicMarkingPause());
  DCHECK(SweepingCompleted());

  ClearFreeLists();

  // Verification depends on the allocation point being cleared.
  VerifyObjectStartBitmap();

  for (BasePage* page = first_page_; page; page = page->Next()) {
    page->MarkAsUnswept();
  }

  // Move all pages to a list of unswept pages.
  first_unswept_page_ = first_page_;
  first_page_ = nullptr;
}

#if defined(ADDRESS_SANITIZER)
void BaseArena::PoisonArena() {
  for (BasePage* page = first_unswept_page_; page; page = page->Next())
    page->PoisonUnmarkedObjects();
}
#endif

Address BaseArena::LazySweep(size_t allocation_size, size_t gc_info_index) {
  // If there are no pages to be swept, return immediately.
  if (SweepingCompleted())
    return nullptr;

  CHECK(GetThreadState()->IsSweepingInProgress());

  // lazySweepPages() can be called recursively if finalizers invoked in
  // page->sweep() allocate memory and the allocation triggers
  // lazySweepPages(). This check prevents the sweeping from being executed
  // recursively.
  if (GetThreadState()->SweepForbidden())
    return nullptr;

  Address result = nullptr;
  {
    ThreadHeapStatsCollector::Scope stats_scope(
        GetThreadState()->Heap().stats_collector(),
        ThreadHeapStatsCollector::kLazySweepOnAllocation);
    ThreadState::SweepForbiddenScope sweep_forbidden(GetThreadState());
    ScriptForbiddenScope script_forbidden;
    result = LazySweepPages(allocation_size, gc_info_index);
  }
  return result;
}

void BaseArena::SweepUnsweptPage() {
  BasePage* page = first_unswept_page_;
  const bool is_empty = page->Sweep();
  page->Unlink(&first_unswept_page_);
  if (is_empty) {
    page->RemoveFromHeap();
  } else {
    page->Link(&first_page_);
    page->MarkAsSwept();
  }
}

bool BaseArena::LazySweepWithDeadline(TimeTicks deadline) {
  // It might be heavy to call
  // Platform::current()->monotonicallyIncreasingTimeSeconds() per page (i.e.,
  // 128 KB sweep or one LargeObject sweep), so we check the deadline per 10
  // pages.
  static const int kDeadlineCheckInterval = 10;

  CHECK(GetThreadState()->IsSweepingInProgress());
  DCHECK(GetThreadState()->SweepForbidden());
  DCHECK(ScriptForbiddenScope::IsScriptForbidden());

  NormalPageArena* normal_arena = nullptr;
  if (first_unswept_page_ && !first_unswept_page_->IsLargeObjectPage()) {
    // Mark this NormalPageArena as being lazily swept.
    NormalPage* normal_page =
        reinterpret_cast<NormalPage*>(first_unswept_page_);
    normal_arena = normal_page->ArenaForNormalPage();
    normal_arena->SetIsLazySweeping(true);
  }
  int page_count = 1;
  while (!SweepingCompleted()) {
    SweepUnsweptPage();
    if (page_count % kDeadlineCheckInterval == 0) {
      if (deadline <= CurrentTimeTicks()) {
        // Deadline has come.
        if (normal_arena)
          normal_arena->SetIsLazySweeping(false);
        return SweepingCompleted();
      }
    }
    page_count++;
  }
  if (normal_arena)
    normal_arena->SetIsLazySweeping(false);
  return true;
}

void BaseArena::CompleteSweep() {
  CHECK(GetThreadState()->IsSweepingInProgress());
  DCHECK(GetThreadState()->SweepForbidden());
  DCHECK(ScriptForbiddenScope::IsScriptForbidden());

  // Some phases, e.g. verification, require iterability of a page.
  MakeIterable();

  while (!SweepingCompleted()) {
    SweepUnsweptPage();
  }
}

Address BaseArena::AllocateLargeObject(size_t allocation_size,
                                       size_t gc_info_index) {
  // TODO(sof): should need arise, support eagerly finalized large objects.
  CHECK(ArenaIndex() != BlinkGC::kEagerSweepArenaIndex);
  LargeObjectArena* large_object_arena = static_cast<LargeObjectArena*>(
      GetThreadState()->Heap().Arena(BlinkGC::kLargeObjectArenaIndex));
  Address large_object = large_object_arena->AllocateLargeObjectPage(
      allocation_size, gc_info_index);
  ASAN_MARK_LARGE_VECTOR_CONTAINER(this, large_object);
  return large_object;
}

bool BaseArena::WillObjectBeLazilySwept(BasePage* page,
                                        void* object_pointer) const {
  // If not on the current page being (potentially) lazily swept,
  // |objectPointer| is an unmarked, sweepable object.
  if (page != first_unswept_page_)
    return true;

  DCHECK(!page->IsLargeObjectPage());
  // Check if the arena is currently being lazily swept.
  NormalPage* normal_page = reinterpret_cast<NormalPage*>(page);
  NormalPageArena* normal_arena = normal_page->ArenaForNormalPage();
  if (!normal_arena->IsLazySweeping())
    return true;

  // Rare special case: unmarked object is on the page being lazily swept,
  // and a finalizer for an object on that page calls
  // ThreadHeap::willObjectBeLazilySwept().
  //
  // Need to determine if |objectPointer| represents a live (unmarked) object or
  // an unmarked object that will be lazily swept later. As lazy page sweeping
  // doesn't record a frontier pointer representing how far along it is, the
  // page is scanned from the start, skipping past freed & unmarked regions.
  //
  // If no marked objects are encountered before |objectPointer|, we know that
  // the finalizing object calling willObjectBeLazilySwept() comes later, and
  // |objectPointer| has been deemed to be alive already (=> it won't be swept.)
  //
  // If a marked object is encountered before |objectPointer|, it will
  // not have been lazily swept past already. Hence it represents an unmarked,
  // sweepable object.
  //
  // As willObjectBeLazilySwept() is used rarely and it happening to be
  // used while runnning a finalizer on the page being lazily swept is
  // even rarer, the page scan is considered acceptable and something
  // really wanted -- willObjectBeLazilySwept()'s result can be trusted.
  Address page_end = normal_page->PayloadEnd();
  for (Address header_address = normal_page->Payload();
       header_address < page_end;) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(header_address);
    size_t size = header->size();
    // Scan made it to |objectPointer| without encountering any marked objects.
    //  => lazy sweep will have processed this unmarked, but live, object.
    //  => |object_pointer| will not be lazily swept.
    //
    // Notice that |object_pointer| might be pointer to a GarbageCollectedMixin,
    // hence using |FromPayload| to derive the HeapObjectHeader isn't possible
    // (and use its value to check if |header_address| is equal to it.)
    if (header_address > object_pointer)
      return false;
    if (!header->IsFree() && header->IsMarked()) {
      // There must be a marked object on this page and the one located must
      // have room after it for the unmarked |objectPointer| object.
      DCHECK(header_address + size < page_end);
      return true;
    }
    header_address += size;
  }
  NOTREACHED();
  return true;
}

NormalPageArena::NormalPageArena(ThreadState* state, int index)
    : BaseArena(state, index),
      current_allocation_point_(nullptr),
      remaining_allocation_size_(0),
      last_remaining_allocation_size_(0),
      promptly_freed_size_(0),
      is_lazy_sweeping_(false) {
  ClearFreeLists();
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
  BasePage* page = first_page_;
  while (page) {
    size += page->size();
    page = page->Next();
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
  context.compacted_pages_ = &first_page_;

  while (!SweepingCompleted()) {
    BasePage* page = first_unswept_page_;
    // Large objects do not belong to this arena.
    DCHECK(!page->IsLargeObjectPage());
    NormalPage* normal_page = static_cast<NormalPage*>(page);
    normal_page->Unlink(&first_unswept_page_);
    normal_page->MarkAsSwept();
    // If not the first page, add |normalPage| onto the available pages chain.
    if (!context.current_page_)
      context.current_page_ = normal_page;
    else
      normal_page->Link(&context.available_pages_);
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
    context.current_page_->Link(&context.available_pages_);
  } else {
    NormalPage* current_page = context.current_page_;
    current_page->Link(&first_page_);
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
  BasePage* available_pages = context.available_pages_;
#if DEBUG_HEAP_COMPACTION
  std::stringstream stream;
#endif
  while (available_pages) {
    size_t page_size = available_pages->size();
#if DEBUG_HEAP_COMPACTION
    if (!freed_page_count)
      stream << "Releasing:";
    stream << " [" << available_pages << ", " << (available_pages + page_size)
           << "]";
#endif
    freed_size += page_size;
    freed_page_count++;
    BasePage* next_page;
    available_pages->Unlink(&next_page);
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
    NormalPage* unused_page = reinterpret_cast<NormalPage*>(available_pages);
    memset(unused_page->Payload(), 0, unused_page->PayloadSize());
#endif
    available_pages->RemoveFromHeap();
    available_pages = static_cast<NormalPage*>(next_page);
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
  for (NormalPage* page = static_cast<NormalPage*>(first_page_); page;
       page = static_cast<NormalPage*>(page->Next()))
    page->VerifyObjectStartBitmapIsConsistentWithPayload();
#endif  // DCHECK_IS_ON()
}

void NormalPageArena::VerifyMarking() {
#if DCHECK_IS_ON()
  // We cannot rely on other marking phases to clear the allocation area as
  // for incremental marking the application is running between steps and
  // might set up a new area.
  SetAllocationPoint(nullptr, 0);
  for (NormalPage* page = static_cast<NormalPage*>(first_page_); page;
       page = static_cast<NormalPage*>(page->Next()))
    page->VerifyMarking();
#endif  // DCHECK_IS_ON()
}

#if DCHECK_IS_ON()
bool NormalPageArena::IsConsistentForGC() {
  // A thread heap is consistent for sweeping if none of the pages to be swept
  // contain a freelist block or the current allocation point.
  for (size_t i = 0; i < kBlinkPageSizeLog2; ++i) {
    for (FreeListEntry* free_list_entry = free_list_.free_lists_[i];
         free_list_entry; free_list_entry = free_list_entry->Next()) {
      if (PagesToBeSweptContains(free_list_entry->GetAddress()))
        return false;
    }
  }
  if (HasCurrentAllocationArea()) {
    if (PagesToBeSweptContains(CurrentAllocationPoint()))
      return false;
  }
  return true;
}

bool NormalPageArena::PagesToBeSweptContains(Address address) {
  for (BasePage* page = first_unswept_page_; page; page = page->Next()) {
    if (page->Contains(address))
      return true;
  }
  return false;
}
#endif

void NormalPageArena::TakeFreelistSnapshot(const String& dump_name) {
  if (free_list_.TakeSnapshot(dump_name)) {
    base::trace_event::MemoryAllocatorDump* buckets_dump =
        BlinkGCMemoryDumpProvider::Instance()
            ->CreateMemoryAllocatorDumpForCurrentGC(dump_name + "/buckets");
    base::trace_event::MemoryAllocatorDump* pages_dump =
        BlinkGCMemoryDumpProvider::Instance()
            ->CreateMemoryAllocatorDumpForCurrentGC(dump_name + "/pages");
    BlinkGCMemoryDumpProvider::Instance()
        ->CurrentProcessMemoryDump()
        ->AddOwnershipEdge(pages_dump->guid(), buckets_dump->guid());
  }
}

void NormalPageArena::AllocatePage() {
  GetThreadState()->Heap().address_cache()->MarkDirty();
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
  page->Link(&first_page_);

  GetThreadState()->Heap().IncreaseAllocatedSpace(page->size());
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
}

void NormalPageArena::FreePage(NormalPage* page) {
  GetThreadState()->Heap().DecreaseAllocatedSpace(page->size());

  PageMemory* memory = page->Storage();
  page->~NormalPage();
  GetThreadState()->Heap().GetFreePagePool()->Add(ArenaIndex(), memory);
}

ObjectStartBitmap::ObjectStartBitmap(Address offset) : offset_(offset) {
  Clear();
}

void ObjectStartBitmap::Clear() {
  memset(&object_start_bit_map_, 0, kReservedForBitmap);
}

void NormalPageArena::PromptlyFreeObject(HeapObjectHeader* header) {
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
      SetRemainingAllocationSize(remaining_allocation_size_ + size);
      SET_MEMORY_INACCESSIBLE(address, size);
      // Memory that is part of the allocation point is not allowed to be part
      // of the object start bit map.
      reinterpret_cast<NormalPage*>(PageFromObject(header))
          ->object_start_bit_map()
          ->ClearBit(address);
      return;
    }
    // The object may be on a page that has not been swept yet and requires
    // manual unmarking.
    if (header->IsMarked())
      header->Unmark();
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
  GetThreadState()->Heap().DecreaseAllocatedObjectSize(size);
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
    SetRemainingAllocationSize(remaining_allocation_size_ - expand_size);
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
    SetRemainingAllocationSize(remaining_allocation_size_ + shrink_size);
    SET_MEMORY_INACCESSIBLE(current_allocation_point_, shrink_size);
    header->SetSize(allocation_size);
    return true;
  }
  DCHECK_GE(shrink_size, sizeof(HeapObjectHeader));
  DCHECK_GT(header->GcInfoIndex(), 0u);
  Address shrink_address = header->PayloadEnd() - shrink_size;
  HeapObjectHeader* freed_header =
      new (NotNull, shrink_address) HeapObjectHeader(
          shrink_size, header->GcInfoIndex(), HeapObjectHeader::kNormalPage);
  PromptlyFreeObjectInFreeList(freed_header, shrink_size);
#if DCHECK_IS_ON()
  DCHECK_EQ(PageFromObject(reinterpret_cast<Address>(header)),
            FindPageFromAddress(reinterpret_cast<Address>(header)));
#endif
  header->SetSize(allocation_size);

  return false;
}

Address NormalPageArena::LazySweepPages(size_t allocation_size,
                                        size_t gc_info_index) {
  DCHECK(!HasCurrentAllocationArea());
  base::AutoReset<bool> is_lazy_sweeping(&is_lazy_sweeping_, true);
  Address result = nullptr;
  while (!SweepingCompleted()) {
    BasePage* page = first_unswept_page_;
    const bool is_empty = page->Sweep();
    page->Unlink(&first_unswept_page_);
    if (is_empty) {
      page->RemoveFromHeap();
    } else {
      page->Link(&first_page_);
      page->MarkAsSwept();
      // For NormalPage, stop lazy sweeping once we find a slot to
      // allocate a new object.
      result = AllocateFromFreeList(allocation_size, gc_info_index);
      if (result)
        break;
    }
  }
  return result;
}

void NormalPageArena::SetRemainingAllocationSize(
    size_t new_remaining_allocation_size) {
  remaining_allocation_size_ = new_remaining_allocation_size;

  // Sync recorded allocated-object size:
  //  - if previous alloc checkpoint is larger, allocation size has increased.
  //  - if smaller, a net reduction in size since last call to
  //  updateRemainingAllocationSize().
  if (last_remaining_allocation_size_ > remaining_allocation_size_) {
    GetThreadState()->Heap().IncreaseAllocatedObjectSize(
        last_remaining_allocation_size_ - remaining_allocation_size_);
  } else if (last_remaining_allocation_size_ != remaining_allocation_size_) {
    GetThreadState()->Heap().DecreaseAllocatedObjectSize(
        remaining_allocation_size_ - last_remaining_allocation_size_);
  }
  last_remaining_allocation_size_ = remaining_allocation_size_;
}

void NormalPageArena::UpdateRemainingAllocationSize() {
  if (last_remaining_allocation_size_ > RemainingAllocationSize()) {
    GetThreadState()->Heap().IncreaseAllocatedObjectSize(
        last_remaining_allocation_size_ - RemainingAllocationSize());
    last_remaining_allocation_size_ = RemainingAllocationSize();
  }
  DCHECK_EQ(last_remaining_allocation_size_, RemainingAllocationSize());
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
  if (HasCurrentAllocationArea()) {
    AddToFreeList(CurrentAllocationPoint(), RemainingAllocationSize());
  }
  UpdateRemainingAllocationSize();
  current_allocation_point_ = point;
  last_remaining_allocation_size_ = remaining_allocation_size_ = size;
  if (point) {
    // Current allocation point can never be part of the object bitmap start
    // because the area can grow or shrink. Will be added back before a GC when
    // clearing the allocation point.
    NormalPage* page = reinterpret_cast<NormalPage*>(PageFromObject(point));
    page->object_start_bit_map()->ClearBit(point);
  }
}

Address NormalPageArena::OutOfLineAllocate(size_t allocation_size,
                                           size_t gc_info_index) {
  DCHECK_GT(allocation_size, RemainingAllocationSize());
  DCHECK_GE(allocation_size, kAllocationGranularity);

  // 1. If this allocation is big enough, allocate a large object.
  if (allocation_size >= kLargeObjectSizeThreshold)
    return AllocateLargeObject(allocation_size, gc_info_index);

  // 2. Try to allocate from a free list.
  UpdateRemainingAllocationSize();
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

Address NormalPageArena::AllocateFromFreeList(size_t allocation_size,
                                              size_t gc_info_index) {
  // Try reusing a block from the largest bin. The underlying reasoning
  // being that we want to amortize this slow allocation call by carving
  // off as a large a free block as possible in one go; a block that will
  // service this block and let following allocations be serviced quickly
  // by bump allocation.
  size_t bucket_size = static_cast<size_t>(1)
                       << free_list_.biggest_free_list_index_;
  int index = free_list_.biggest_free_list_index_;
  for (; index > 0; --index, bucket_size >>= 1) {
    FreeListEntry* entry = free_list_.free_lists_[index];
    if (allocation_size > bucket_size) {
      // Final bucket candidate; check initial entry if it is able
      // to service this allocation. Do not perform a linear scan,
      // as it is considered too costly.
      if (!entry || entry->size() < allocation_size)
        break;
    }
    if (entry) {
      entry->Unlink(&free_list_.free_lists_[index]);
      SetAllocationPoint(entry->GetAddress(), entry->size());
      DCHECK(HasCurrentAllocationArea());
      DCHECK_GE(RemainingAllocationSize(), allocation_size);
      free_list_.biggest_free_list_index_ = index;
      return AllocateObject(allocation_size, gc_info_index);
    }
  }
  free_list_.biggest_free_list_index_ = index;
  return nullptr;
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

  GetThreadState()->Heap().address_cache()->MarkDirty();
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
  HeapObjectHeader* header = new (NotNull, header_address) HeapObjectHeader(
      kLargeObjectSizeInHeader, gc_info_index, HeapObjectHeader::kLargePage);
  Address result = header_address + sizeof(*header);
  DCHECK(!(reinterpret_cast<uintptr_t>(result) & kAllocationMask));

  // Poison the object header and allocationGranularity bytes after the object
  ASAN_POISON_MEMORY_REGION(header, sizeof(*header));
  ASAN_POISON_MEMORY_REGION(large_object->GetAddress() + large_object->size(),
                            kAllocationGranularity);

  large_object->Link(&first_page_);

  GetThreadState()->Heap().IncreaseAllocatedSpace(large_object->size());
  GetThreadState()->Heap().IncreaseAllocatedObjectSize(
      large_object->PayloadSize());
  return result;
}

void LargeObjectArena::FreeLargeObjectPage(LargeObjectPage* object) {
  ASAN_UNPOISON_MEMORY_REGION(object->Payload(), object->PayloadSize());
  object->ObjectHeader()->Finalize(object->Payload(), object->PayloadSize());
  GetThreadState()->Heap().DecreaseAllocatedSpace(object->size());

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
  while (!SweepingCompleted()) {
    BasePage* page = first_unswept_page_;
    const bool is_empty = page->Sweep();
    page->Unlink(&first_unswept_page_);
    if (is_empty) {
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
      page->Link(&first_page_);
      page->MarkAsSwept();
    }
  }
  return result;
}

FreeList::FreeList() : biggest_free_list_index_(0) {}

void FreeList::AddToFreeList(Address address, size_t size) {
  DCHECK_LT(size, BlinkPagePayloadSize());
  // The free list entries are only pointer aligned (but when we allocate
  // from them we are 8 byte aligned due to the header size).
  DCHECK(!((reinterpret_cast<uintptr_t>(address) + sizeof(HeapObjectHeader)) &
           kAllocationMask));
  DCHECK(!(size & kAllocationMask));
  ASAN_UNPOISON_MEMORY_REGION(address, size);
  FreeListEntry* entry;
  if (size < sizeof(*entry)) {
    // Create a dummy header with only a size and freelist bit set.
    DCHECK_GE(size, sizeof(HeapObjectHeader));
    // Free list encode the size to mark the lost memory as freelist memory.
    new (NotNull, address) HeapObjectHeader(size, kGcInfoIndexForFreeListHeader,
                                            HeapObjectHeader::kNormalPage);

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

  int index = BucketIndexForSize(size);
  entry->Link(&free_lists_[index]);
  if (index > biggest_free_list_index_)
    biggest_free_list_index_ = index;
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
    FreeListEntry* entry = free_lists_[i];
    while (entry) {
      free_size += entry->size();
      entry = entry->Next();
    }
  }
#if DEBUG_HEAP_FREELIST
  if (free_size) {
    LOG_HEAP_FREELIST_VERBOSE() << "FreeList(" << this << "): " << free_size;
    for (unsigned i = 0; i < kBlinkPageSizeLog2; ++i) {
      FreeListEntry* entry = free_lists_[i];
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
  for (size_t i = 0; i < kBlinkPageSizeLog2; ++i)
    free_lists_[i] = nullptr;
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

bool FreeList::TakeSnapshot(const String& dump_base_name) {
  bool did_dump_bucket_stats = false;
  for (size_t i = 0; i < kBlinkPageSizeLog2; ++i) {
    size_t entry_count = 0;
    size_t free_size = 0;
    for (FreeListEntry* entry = free_lists_[i]; entry; entry = entry->Next()) {
      ++entry_count;
      free_size += entry->size();
    }

    String dump_name =
        dump_base_name + String::Format("/buckets/bucket_%lu",
                                        static_cast<unsigned long>(1 << i));
    base::trace_event::MemoryAllocatorDump* bucket_dump =
        BlinkGCMemoryDumpProvider::Instance()
            ->CreateMemoryAllocatorDumpForCurrentGC(dump_name);
    bucket_dump->AddScalar("free_count", "objects", entry_count);
    bucket_dump->AddScalar("free_size", "bytes", free_size);
    did_dump_bucket_stats = true;
  }
  return did_dump_bucket_stats;
}

BasePage::BasePage(PageMemory* storage, BaseArena* arena)
    : magic_(GetMagic()),
      storage_(storage),
      arena_(arena),
      next_(nullptr),
      swept_(true) {
#if DCHECK_IS_ON()
  DCHECK(IsPageHeaderAddress(reinterpret_cast<Address>(this)));
#endif
}

NormalPage::NormalPage(PageMemory* storage, BaseArena* arena)
    : BasePage(storage, arena), object_start_bit_map_(Payload()) {
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

bool NormalPage::Sweep() {
  object_start_bit_map()->Clear();
  size_t marked_object_size = 0;
  Address start_of_gap = Payload();
  NormalPageArena* page_arena = ArenaForNormalPage();
  for (Address header_address = start_of_gap; header_address < PayloadEnd();) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(header_address);
    size_t size = header->size();
    DCHECK_GT(size, 0u);
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
    if (!header->IsMarked()) {
      // This is a fast version of header->PayloadSize().
      size_t payload_size = size - sizeof(HeapObjectHeader);
      Address payload = header->Payload();
      // For ASan, unpoison the object before calling the finalizer. The
      // finalized object will be zero-filled and poison'ed afterwards.
      // Given all other unmarked objects are poisoned, ASan will detect
      // an error if the finalizer touches any other on-heap object that
      // die at the same GC cycle.
      ASAN_UNPOISON_MEMORY_REGION(payload, payload_size);
      header->Finalize(payload, payload_size);
      // This memory will be added to the freelist. Maintain the invariant
      // that memory on the freelist is zero filled.
      SET_MEMORY_INACCESSIBLE(header_address, size);
      header_address += size;
      continue;
    }
    if (start_of_gap != header_address) {
      page_arena->AddToFreeList(start_of_gap, header_address - start_of_gap);
#if !DCHECK_IS_ON() && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
      // Discarding pages increases page faults and may regress performance.
      // So we enable this only on low-RAM devices.
      if (MemoryCoordinator::IsLowEndDevice())
        DiscardPages(start_of_gap + sizeof(FreeListEntry), header_address);
#endif
    }
    object_start_bit_map()->SetBit(header_address);
    header->Unmark();
    header_address += size;
    marked_object_size += size;
    start_of_gap = header_address;
  }
  // Only add the memory to the free list if the page is not completely empty
  // and we are not at the end of the page. Empty pages are not added to the
  // free list as the pages are removed immediately.
  if (start_of_gap != Payload() && start_of_gap != PayloadEnd()) {
    page_arena->AddToFreeList(start_of_gap, PayloadEnd() - start_of_gap);
#if !DCHECK_IS_ON() && !defined(LEAK_SANITIZER) && !defined(ADDRESS_SANITIZER)
    if (MemoryCoordinator::IsLowEndDevice())
      DiscardPages(start_of_gap + sizeof(FreeListEntry), PayloadEnd());
#endif
  }

  if (marked_object_size) {
    page_arena->GetThreadState()->Heap().IncreaseMarkedObjectSize(
        marked_object_size);
  }

  VerifyObjectStartBitmapIsConsistentWithPayload();
  return start_of_gap == Payload();
}

void NormalPage::SweepAndCompact(CompactionContext& context) {
  object_start_bit_map()->Clear();
  NormalPage*& current_page = context.current_page_;
  size_t& allocation_point = context.allocation_point_;

  size_t marked_object_size = 0;
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
    header->Unmark();
    // Allocate and copy over the live object.
    Address compact_frontier = current_page->Payload() + allocation_point;
    if (compact_frontier + size > current_page->PayloadEnd()) {
      // Can't fit on current allocation page; add remaining onto the
      // freelist and advance to next available page.
      //
      // TODO(sof): be more clever & compact later objects into
      // |currentPage|'s unused slop.
      current_page->Link(context.compacted_pages_);
      size_t free_size = current_page->PayloadSize() - allocation_point;
      if (free_size) {
        SET_MEMORY_INACCESSIBLE(compact_frontier, free_size);
        current_page->ArenaForNormalPage()->AddToFreeList(compact_frontier,
                                                          free_size);
      }

      BasePage* next_available_page;
      context.available_pages_->Unlink(&next_available_page);
      current_page = reinterpret_cast<NormalPage*>(context.available_pages_);
      context.available_pages_ = next_available_page;
      allocation_point = 0;
      compact_frontier = current_page->Payload();
    }
    if (LIKELY(compact_frontier != header_address)) {
#if defined(ADDRESS_SANITIZER)
      // Unpoison the header + if it is a vector backing
      // store object, let go of the container annotations.
      // Do that by unpoisoning the payload entirely.
      ASAN_UNPOISON_MEMORY_REGION(header, sizeof(HeapObjectHeader));
      if (is_vector_arena)
        ASAN_UNPOISON_MEMORY_REGION(payload, payload_size);
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
    marked_object_size += size;
    allocation_point += size;
    DCHECK(allocation_point <= current_page->PayloadSize());
  }
  if (marked_object_size) {
    page_arena->GetThreadState()->Heap().IncreaseMarkedObjectSize(
        marked_object_size);
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
  size_t marked_object_size = 0;
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
      marked_object_size += size;
    }
    object_start_bit_map()->SetBit(header_address);
    header_address += size;
    start_of_gap = header_address;
    DCHECK_LE(header_address, PayloadEnd());
  }
  if (start_of_gap != PayloadEnd())
    normal_arena->AddToFreeList(start_of_gap, PayloadEnd() - start_of_gap);

  if (marked_object_size) {
    ArenaForNormalPage()->GetThreadState()->Heap().IncreaseMarkedObjectSize(
        marked_object_size);
  }

  VerifyObjectStartBitmapIsConsistentWithPayload();
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
    if (!header->IsMarked())
      ASAN_POISON_MEMORY_REGION(header->Payload(), header->PayloadSize());
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
    DCHECK(object_header->IsValidOrZapped());
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
  DCHECK(!ArenaForNormalPage()
              ->GetThreadState()
              ->Heap()
              .GetStackFrameDepth()
              .IsSafeToRecurse());
  DCHECK(!ArenaForNormalPage()->CurrentAllocationPoint());
  MarkingVerifier verifier(ArenaForNormalPage()->GetThreadState());
  for (Address header_address = Payload(); header_address < PayloadEnd();) {
    HeapObjectHeader* header =
        reinterpret_cast<HeapObjectHeader*>(header_address);
    verifier.VerifyObject(header);
    header_address += header->size();
  }
}

Address ObjectStartBitmap::FindHeader(
    Address address_maybe_pointing_to_the_middle_of_object) {
  size_t object_offset =
      address_maybe_pointing_to_the_middle_of_object - offset_;
  size_t object_start_number = object_offset / kAllocationGranularity;
  size_t cell_index = object_start_number / kCellSize;
#if DCHECK_IS_ON()
  const size_t bitmap_size = kReservedForBitmap;
  DCHECK_LT(cell_index, bitmap_size);
#endif
  size_t bit = object_start_number & kCellMask;
  uint8_t byte = object_start_bit_map_[cell_index] & ((1 << (bit + 1)) - 1);
  while (!byte) {
    DCHECK_LT(0u, cell_index);
    byte = object_start_bit_map_[--cell_index];
  }
  int leading_zeroes = base::bits::CountLeadingZeroBits(byte);
  object_start_number =
      (cell_index * kCellSize) + (kCellSize - 1) - leading_zeroes;
  object_offset = object_start_number * kAllocationGranularity;
  return object_offset + offset_;
}

HeapObjectHeader* NormalPage::FindHeaderFromAddress(Address address) {
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

void NormalPage::TakeSnapshot(base::trace_event::MemoryAllocatorDump* page_dump,
                              ThreadState::GCSnapshotInfo& info,
                              HeapSnapshotInfo& heap_info) {
  HeapObjectHeader* header = nullptr;
  size_t live_count = 0;
  size_t dead_count = 0;
  size_t free_count = 0;
  size_t live_size = 0;
  size_t dead_size = 0;
  size_t free_size = 0;
  for (Address header_address = Payload(); header_address < PayloadEnd();
       header_address += header->size()) {
    header = reinterpret_cast<HeapObjectHeader*>(header_address);
    if (header->IsFree()) {
      free_count++;
      free_size += header->size();
    } else if (header->IsMarked()) {
      live_count++;
      live_size += header->size();

      uint32_t gc_info_index = header->GcInfoIndex();
      info.live_count[gc_info_index]++;
      info.live_size[gc_info_index] += header->size();
    } else {
      dead_count++;
      dead_size += header->size();

      uint32_t gc_info_index = header->GcInfoIndex();
      info.dead_count[gc_info_index]++;
      info.dead_size[gc_info_index] += header->size();
    }
  }

  page_dump->AddScalar("live_count", "objects", live_count);
  page_dump->AddScalar("dead_count", "objects", dead_count);
  page_dump->AddScalar("free_count", "objects", free_count);
  page_dump->AddScalar("live_size", "bytes", live_size);
  page_dump->AddScalar("dead_size", "bytes", dead_size);
  page_dump->AddScalar("free_size", "bytes", free_size);
  heap_info.free_size += free_size;
  heap_info.free_count += free_count;
}

#if DCHECK_IS_ON()
bool NormalPage::Contains(Address addr) {
  Address blink_page_start = RoundToBlinkPageStart(GetAddress());
  // Page is at aligned address plus guard page size.
  DCHECK_EQ(blink_page_start, GetAddress() - kBlinkGuardPageSize);
  return blink_page_start <= addr && addr < blink_page_start + kBlinkPageSize;
}
#endif

LargeObjectPage::LargeObjectPage(PageMemory* storage,
                                 BaseArena* arena,
                                 size_t object_size)
    : BasePage(storage, arena),
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

bool LargeObjectPage::Sweep() {
  if (!ObjectHeader()->IsMarked()) {
    return true;
  }
  ObjectHeader()->Unmark();
  Arena()->GetThreadState()->Heap().IncreaseMarkedObjectSize(size());
  return false;
}

void LargeObjectPage::MakeConsistentForMutator() {
  HeapObjectHeader* header = ObjectHeader();
  if (header->IsMarked()) {
    header->Unmark();
    Arena()->GetThreadState()->Heap().IncreaseMarkedObjectSize(size());
  }
}

#if defined(ADDRESS_SANITIZER)
void LargeObjectPage::PoisonUnmarkedObjects() {
  HeapObjectHeader* header = ObjectHeader();
  if (!header->IsMarked())
    ASAN_POISON_MEMORY_REGION(header->Payload(), header->PayloadSize());
}
#endif

void LargeObjectPage::TakeSnapshot(
    base::trace_event::MemoryAllocatorDump* page_dump,
    ThreadState::GCSnapshotInfo& info,
    HeapSnapshotInfo&) {
  size_t live_size = 0;
  size_t dead_size = 0;
  size_t live_count = 0;
  size_t dead_count = 0;
  HeapObjectHeader* header = ObjectHeader();
  uint32_t gc_info_index = header->GcInfoIndex();
  size_t payload_size = header->PayloadSize();
  if (header->IsMarked()) {
    live_count = 1;
    live_size += payload_size;
    info.live_count[gc_info_index]++;
    info.live_size[gc_info_index] += payload_size;
  } else {
    dead_count = 1;
    dead_size += payload_size;
    info.dead_count[gc_info_index]++;
    info.dead_size[gc_info_index] += payload_size;
  }

  page_dump->AddScalar("live_count", "objects", live_count);
  page_dump->AddScalar("dead_count", "objects", dead_count);
  page_dump->AddScalar("live_size", "bytes", live_size);
  page_dump->AddScalar("dead_size", "bytes", dead_size);
}

#if DCHECK_IS_ON()
bool LargeObjectPage::Contains(Address object) {
  return RoundToBlinkPageStart(GetAddress()) <= object &&
         object < RoundToBlinkPageEnd(GetAddress() + size());
}
#endif

ALWAYS_INLINE uint32_t RotateLeft16(uint32_t x) {
#if defined(COMPILER_MSVC)
  return _lrotr(x, 16);
#else
  // http://blog.regehr.org/archives/1063
  return (x << 16) | (x >> (-16 & 31));
#endif
}

uint32_t ComputeRandomMagic() {
// Ignore C4319: It is OK to 0-extend into the high-order bits of the uintptr_t
// on 64-bit, in this case.
#if defined(COMPILER_MSVC)
#pragma warning(push)
#pragma warning(disable : 4319)
#endif

  // Get an ASLR'd address from one of our own DLLs/.sos, and then another from
  // a system DLL/.so:

  const uint32_t random1 =
      ~(RotateLeft16(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(
          base::trace_event::MemoryAllocatorDump::kNameSize))));

#if defined(OS_WIN)
  uintptr_t random2 = reinterpret_cast<uintptr_t>(::ReadFile);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  uintptr_t random2 = reinterpret_cast<uintptr_t>(::read);
#else
#error platform not supported
#endif

#if defined(ARCH_CPU_64_BITS)
  static_assert(sizeof(uintptr_t) == sizeof(uint64_t),
                "uintptr_t is not uint64_t");
  // Shift in some high-order bits.
  random2 = random2 >> 16;
#elif defined(ARCH_CPU_32_BITS)
  // Although we don't use heap metadata canaries on 32-bit due to memory
  // pressure, keep this code around just in case we do, someday.
  static_assert(sizeof(uintptr_t) == sizeof(uint32_t),
                "uintptr_t is not uint32_t");
#else
#error architecture not supported
#endif

  random2 = ~(RotateLeft16(static_cast<uint32_t>(random2)));

  // Combine the 2 values:
  const uint32_t random = (random1 & 0x0000FFFFUL) |
                          (static_cast<uint32_t>(random2) & 0xFFFF0000UL);

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

  return random;
}

#if defined(ARCH_CPU_64_BITS)
// Returns a random magic value.
uint32_t HeapObjectHeader::GetMagic() {
  static const uint32_t magic = ComputeRandomMagic() ^ 0x6e0b6ead;
  return magic;
}
#endif  // defined(ARCH_CPU_64_BITS)

uint32_t BasePage::GetMagic() {
  static const uint32_t magic = ComputeRandomMagic() ^ 0xba5e4a9e;
  return magic;
}

}  // namespace blink
