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
  region_tree_->Add(this);
  for (size_t i = 0; i < kBlinkPagesPerRegion; ++i)
    in_use_[i] = false;
}

PageMemoryRegion::~PageMemoryRegion() {
  if (region_tree_)
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
  OOM_CRASH();
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

PageMemoryRegion* RegionTree::Lookup(Address address) {
  RegionTreeNode* current = root_;
  while (current) {
    Address base = current->region_->Base();
    if (address < base) {
      current = current->left_;
      continue;
    }
    if (address >= base + current->region_->size()) {
      current = current->right_;
      continue;
    }
    DCHECK(current->region_->Contains(address));
    return current->region_;
  }
  return nullptr;
}

void RegionTree::Add(PageMemoryRegion* region) {
  DCHECK(region);
  RegionTreeNode* new_tree = new RegionTreeNode(region);
  new_tree->AddTo(&root_);
}

void RegionTreeNode::AddTo(RegionTreeNode** context) {
  Address base = region_->Base();
  for (RegionTreeNode* current = *context; current; current = *context) {
    DCHECK(!current->region_->Contains(base));
    context =
        (base < current->region_->Base()) ? &current->left_ : &current->right_;
  }
  *context = this;
}

void RegionTree::Remove(PageMemoryRegion* region) {
  DCHECK(region);
  DCHECK(root_);
  Address base = region->Base();
  RegionTreeNode** context = &root_;
  RegionTreeNode* current = root_;
  for (; current; current = *context) {
    if (region == current->region_)
      break;
    context =
        (base < current->region_->Base()) ? &current->left_ : &current->right_;
  }

  // Shutdown via detachMainThread might not have populated the region tree.
  if (!current)
    return;

  *context = nullptr;
  if (current->left_) {
    current->left_->AddTo(context);
    current->left_ = nullptr;
  }
  if (current->right_) {
    current->right_->AddTo(context);
    current->right_ = nullptr;
  }
  delete current;
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
  Address payload_address = region->Base() + page_offset + kBlinkGuardPageSize;
  return new PageMemory(region, MemoryRegion(payload_address, payload_size));
}

PageMemory* PageMemory::Allocate(size_t payload_size, RegionTree* region_tree) {
  DCHECK_GT(payload_size, 0u);

  // Virtual memory allocation routines operate in OS page sizes.
  // Round up the requested size to nearest os page size.
  payload_size = base::RoundUpToSystemPage(payload_size);

  // Overallocate by 2 times OS page size to have space for a
  // guard page at the beginning and end of blink heap page.
  size_t allocation_size = payload_size + 2 * kBlinkGuardPageSize;
  PageMemoryRegion* page_memory_region =
      PageMemoryRegion::AllocateLargePage(allocation_size, region_tree);
  PageMemory* storage =
      SetupPageMemoryInRegion(page_memory_region, 0, payload_size);
  CHECK(storage->Commit());
  return storage;
}

}  // namespace blink
