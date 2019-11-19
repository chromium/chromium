// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/gc_info.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/bits.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

constexpr size_t kEntrySize = sizeof(GCInfo*);

// Allocation and resizing are built around the following invariants.
static_assert(base::bits::IsPowerOfTwo(kEntrySize),
              "GCInfoTable entries size must be power of "
              "two");
static_assert(
    0 == base::kPageAllocationGranularity % base::kSystemPageSize,
    "System page size must be a multiple of page page allocation granularity");

constexpr size_t ComputeInitialTableLimit() {
  // (Light) experimentation suggests that Blink doesn't need more than this
  // while handling content on popular web properties.
  constexpr size_t kInitialWantedLimit = 512;

  // Different OSes have different page sizes, so we have to choose the minimum
  // of memory wanted and OS page size.
  constexpr size_t memory_wanted = kInitialWantedLimit * kEntrySize;
  return base::RoundUpToPageAllocationGranularity(memory_wanted) / kEntrySize;
}

constexpr size_t MaxTableSize() {
  constexpr size_t kMaxTableSize = base::RoundUpToPageAllocationGranularity(
      GCInfoTable::kMaxIndex * kEntrySize);
  return kMaxTableSize;
}

}  // namespace

GCInfoTable* GCInfoTable::global_table_ = nullptr;
constexpr uint32_t GCInfoTable::kMaxIndex;

void GCInfoTable::CreateGlobalTable() {
  DEFINE_STATIC_LOCAL(GCInfoTable, table, ());
  global_table_ = &table;
}

uint32_t GCInfoTable::EnsureGCInfoIndex(
    const GCInfo* gc_info,
    std::atomic<uint32_t>* gc_info_index_slot) {
  DCHECK(gc_info);
  DCHECK(gc_info_index_slot);

  // Ensuring a new index involves current index adjustment as well
  // as potentially resizing the table, both operations that require
  // a lock.
  MutexLocker locker(table_mutex_);

  // If more than one thread ends up allocating a slot for
  // the same GCInfo, have later threads reuse the slot
  // allocated by the first.
  uint32_t gc_info_index = gc_info_index_slot->load(std::memory_order_acquire);
  if (gc_info_index)
    return gc_info_index;

  gc_info_index = ++current_index_;
  CHECK(gc_info_index < GCInfoTable::kMaxIndex);
  if (current_index_ >= limit_)
    Resize();

  table_[gc_info_index] = gc_info;
  gc_info_index_slot->store(gc_info_index, std::memory_order_release);
  return gc_info_index;
}

void GCInfoTable::Resize() {
  const size_t new_limit = (limit_) ? 2 * limit_ : ComputeInitialTableLimit();
  const size_t old_committed_size = limit_ * kEntrySize;
  const size_t new_committed_size = new_limit * kEntrySize;
  CHECK(table_);
  CHECK_EQ(0u, new_committed_size % base::kPageAllocationGranularity);
  CHECK_GE(MaxTableSize(), limit_ * kEntrySize);

  // Recommitting and zapping assumes byte-addressable storage.
  uint8_t* const current_table_end =
      reinterpret_cast<uint8_t*>(table_) + old_committed_size;
  const size_t table_size_delta = new_committed_size - old_committed_size;

  // Commit the new size and allow read/write.
  // TODO(ajwong): SetSystemPagesAccess should be part of RecommitSystemPages to
  // avoid having two calls here.
  base::SetSystemPagesAccess(current_table_end, table_size_delta,
                             base::PageReadWrite);
  bool ok = base::RecommitSystemPages(current_table_end, table_size_delta,
                                      base::PageReadWrite);
  CHECK(ok);

#if DCHECK_IS_ON()
  // Check that newly-committed memory is zero-initialized.
  for (size_t i = 0; i < (table_size_delta / sizeof(uintptr_t)); ++i) {
    DCHECK(!reinterpret_cast<uintptr_t*>(current_table_end)[i]);
  }
#endif  // DCHECK_IS_ON()

  limit_ = static_cast<uint32_t>(new_limit);
}

GCInfoTable::GCInfoTable() {
  CHECK(!table_);
  table_ = reinterpret_cast<GCInfo const**>(base::AllocPages(
      nullptr, MaxTableSize(), base::kPageAllocationGranularity,
      base::PageInaccessible, base::PageTag::kBlinkGC));
  CHECK(table_);
  Resize();
}

#if DCHECK_IS_ON()
void AssertObjectHasGCInfo(const void* payload, size_t gc_info_index) {
  HeapObjectHeader::CheckFromPayload(payload);
#if !defined(COMPONENT_BUILD)
  // On component builds we cannot compare the gcInfos as they are statically
  // defined in each of the components and hence will not match.
  DCHECK_EQ(HeapObjectHeader::FromPayload(payload)->GcInfoIndex(),
            gc_info_index);
#endif
}
#endif

}  // namespace blink
