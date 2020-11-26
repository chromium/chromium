// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/impl/gc_info.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/bits.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

constexpr size_t kEntrySize = sizeof(GCInfo*);

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t ComputeInitialTableLimit() {
  // (Light) experimentation suggests that Blink doesn't need more than this
  // while handling content on popular web properties.
  constexpr size_t kInitialWantedLimit = 512;

  // Different OSes have different page sizes, so we have to choose the minimum
  // of memory wanted and OS page size.
  constexpr size_t memory_wanted = kInitialWantedLimit * kEntrySize;
  return base::RoundUpToPageAllocationGranularity(memory_wanted) / kEntrySize;
}

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t MaxTableSize() {
  return base::RoundUpToPageAllocationGranularity(GCInfoTable::kMaxIndex *
                                                  kEntrySize);
}

}  // namespace

GCInfoTable* GCInfoTable::global_table_ = nullptr;
constexpr GCInfoIndex GCInfoTable::kMaxIndex;
constexpr GCInfoIndex GCInfoTable::kMinIndex;

void GCInfoTable::CreateGlobalTable() {
  // Allocation and resizing are built around the following invariants.
  static_assert(base::bits::IsPowerOfTwo(kEntrySize),
                "GCInfoTable entries size must be power of "
                "two");

#if defined(PAGE_ALLOCATOR_CONSTANTS_ARE_CONSTEXPR)
#define STATIC_ASSERT_OR_CHECK(condition, message) \
  static_assert(condition, message)
#else
#define STATIC_ASSERT_OR_CHECK(condition, message) \
  do {                                             \
    CHECK(condition) << (message);                 \
  } while (false)
#endif

  STATIC_ASSERT_OR_CHECK(
      0 == base::PageAllocationGranularity() % base::SystemPageSize(),
      "System page size must be a multiple of page allocation granularity");

#undef STATIC_ASSERT_OR_CHECK

  DEFINE_STATIC_LOCAL(GCInfoTable, table, ());
  global_table_ = &table;
}

GCInfoIndex GCInfoTable::EnsureGCInfoIndex(
    const GCInfo* gc_info,
    std::atomic<GCInfoIndex>* gc_info_index_slot) {
  DCHECK(gc_info);
  DCHECK(gc_info_index_slot);

  // Ensuring a new index involves current index adjustment as well as
  // potentially resizing the table. For simplicity we use a lock.
  MutexLocker locker(table_mutex_);

  // If more than one thread ends up allocating a slot for the same GCInfo, have
  // later threads reuse the slot allocated by the first.
  GCInfoIndex gc_info_index =
      gc_info_index_slot->load(std::memory_order_relaxed);
  if (gc_info_index)
    return gc_info_index;

  if (current_index_ == limit_)
    Resize();

  gc_info_index = current_index_++;
  CHECK_LT(gc_info_index, GCInfoTable::kMaxIndex);

  table_[gc_info_index] = gc_info;
  gc_info_index_slot->store(gc_info_index, std::memory_order_release);
  return gc_info_index;
}

void GCInfoTable::Resize() {
  const GCInfoIndex new_limit =
      (limit_) ? 2 * limit_ : ComputeInitialTableLimit();
  CHECK_GT(new_limit, limit_);
  const size_t old_committed_size = limit_ * kEntrySize;
  const size_t new_committed_size = new_limit * kEntrySize;
  CHECK(table_);
  CHECK_EQ(0u, new_committed_size % base::PageAllocationGranularity());
  CHECK_GE(MaxTableSize(), limit_ * kEntrySize);

  // Recommitting and zapping assumes byte-addressable storage.
  uint8_t* const current_table_end =
      reinterpret_cast<uint8_t*>(table_) + old_committed_size;
  const size_t table_size_delta = new_committed_size - old_committed_size;

  // Commit the new size and allow read/write.
  base::RecommitSystemPages(current_table_end, table_size_delta,
                            base::PageReadWrite, base::PageUpdatePermissions);

#if DCHECK_IS_ON()
  // Check that newly-committed memory is zero-initialized.
  for (size_t i = 0; i < (table_size_delta / sizeof(uintptr_t)); ++i) {
    DCHECK(!reinterpret_cast<uintptr_t*>(current_table_end)[i]);
  }
#endif  // DCHECK_IS_ON()

  limit_ = new_limit;
}

GCInfoTable::GCInfoTable() {
  table_ = reinterpret_cast<GCInfo const**>(base::AllocPages(
      nullptr, MaxTableSize(), base::PageAllocationGranularity(),
      base::PageInaccessible, base::PageTag::kBlinkGC));
  CHECK(table_);
  Resize();
}

}  // namespace blink
