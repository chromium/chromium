// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/page_memory.h"

#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"

namespace blink {

void MemoryRegion::Release() {
  base::FreePages(base_, size_);
}

bool MemoryRegion::Commit() {
  CHECK(base::RecommitSystemPages(base_, size_, base::PageReadWrite));
  return base::TrySetSystemPagesAccess(base_, size_, base::PageReadWrite);
}

void MemoryRegion::Decommit() {
  ASAN_UNPOISON_MEMORY_REGION(base_, size_);
  base::DecommitSystemPages(base_, size_);
  base::SetSystemPagesAccess(base_, size_, base::PageInaccessible);
}

PageMemoryRegion::PageMemoryRegion(Address base,
                                   size_t size,
                                   unsigned num_pages,
                                   RegionTree* region_tree)
    : MemoryRegion(base, size),
      is_large_page_(num_pages == 1),
      num_pages_(num_pages),
      region_tree_(region_tree) {
  DCHECK(region_tree);
  region_tree_->Add(this);
  for (size_t i = 0; i < kBlinkPagesPerRegion; ++i)
    in_use_[i] = false;
}

PageMemoryRegion::~PageMemoryRegion() {
  region_tree_->Remove(this);
  Release();
}

void PageMemoryRegion::PageDeleted(Address page) {
  MarkPageUnused(page);
  if (!num_pages_.Decrement())
    delete this;
}

// TODO(haraken): Like partitionOutOfMemoryWithLotsOfUncommitedPages(),
// we should probably have a way to distinguish physical memory OOM from
// virtual address space OOM.
static NOINLINE void BlinkGCOutOfMemory() {
  // TODO(lizeb): Add the real allocation size here as well.
  OOM_CRASH(0);
}

PageMemoryRegion* PageMemoryRegion::Allocate(size_t size,
                                             unsigned num_pages,
                                             RegionTree* region_tree) {
  // Round size up to the allocation granularity.
  size = base::RoundUpToPageAllocationGranularity(size);
  Address base = static_cast<Address>(
      base::AllocPages(nullptr, size, kBlinkPageSize, base::PageInaccessible,
                       base::PageTag::kBlinkGC));
  if (!base)
    BlinkGCOutOfMemory();
  return new PageMemoryRegion(base, size, num_pages, region_tree);
}

PageMemoryRegion* RegionTree::Lookup(ConstAddress address) {
  auto it = set_.upper_bound(address);
  // This check also covers set_.size() > 0, since for empty vectors it is
  // guaranteed that begin() == end().
  if (it == set_.begin())
    return nullptr;
  auto* result = std::next(it, -1)->second;
  if (address < result->Base() + result->size())
    return result;
  return nullptr;
}

void RegionTree::Add(PageMemoryRegion* region) {
  DCHECK(region);
  auto result = set_.emplace(region->Base(), region);
  DCHECK(result.second);
}

void RegionTree::Remove(PageMemoryRegion* region) {
  DCHECK(region);
  auto size = set_.erase(region->Base());
  DCHECK_EQ(1u, size);
}

PageMemory::PageMemory(PageMemoryRegion* reserved, const MemoryRegion& writable)
    : reserved_(reserved), writable_(writable) {
  DCHECK(reserved->Contains(writable));

  // Register the writable area of the memory as part of the LSan root set.
  // Only the writable area is mapped and can contain C++ objects.  Those
  // C++ objects can contain pointers to objects outside of the heap and
  // should therefore be part of the LSan root set.
  __lsan_register_root_region(writable_.Base(), writable_.size());
}

PageMemory* PageMemory::SetupPageMemoryInRegion(PageMemoryRegion* region,
                                                size_t page_offset,
                                                size_t payload_size) {
  // Setup the payload one guard page into the page memory.
  Address payload_address = region->Base() + page_offset + BlinkGuardPageSize();
  return new PageMemory(region, MemoryRegion(payload_address, payload_size));
}

PageMemory* PageMemory::Allocate(size_t payload_size, RegionTree* region_tree) {
  DCHECK_GT(payload_size, 0u);

  // Virtual memory allocation routines operate in OS page sizes.
  // Round up the requested size to nearest os page size.
  payload_size = base::RoundUpToSystemPage(payload_size);

  // Overallocate by 2 times OS page size to have space for a
  // guard page at the beginning and end of blink heap page.
  size_t allocation_size = payload_size + 2 * BlinkGuardPageSize();
  PageMemoryRegion* page_memory_region =
      PageMemoryRegion::AllocateLargePage(allocation_size, region_tree);
  PageMemory* storage =
      SetupPageMemoryInRegion(page_memory_region, 0, payload_size);
  CHECK(storage->Commit());
  return storage;
}

}  // namespace blink
