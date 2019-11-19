// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PAGE_MEMORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PAGE_MEMORY_H_

#include "base/atomic_ref_count.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_page.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

class RegionTree;
class RegionTreeNode;

class MemoryRegion {
  USING_FAST_MALLOC(MemoryRegion);

 public:
  MemoryRegion(Address base, size_t size) : base_(base), size_(size) {
    DCHECK_GT(size, 0u);
  }

  bool Contains(Address addr) const {
    return base_ <= addr && addr < (base_ + size_);
  }

  bool Contains(const MemoryRegion& other) const {
    return Contains(other.base_) && Contains(other.base_ + other.size_ - 1);
  }

  void Release();
  WARN_UNUSED_RESULT bool Commit();
  void Decommit();

  Address Base() const { return base_; }
  size_t size() const { return size_; }

 private:
  Address base_;
  size_t size_;
};

// A PageMemoryRegion represents a chunk of reserved virtual address
// space containing a number of blink heap pages. On Windows, reserved
// virtual address space can only be given back to the system as a
// whole. The PageMemoryRegion allows us to do that by keeping track
// of the number of pages using it in order to be able to release all
// of the virtual address space when there are no more pages using it.
class PageMemoryRegion : public MemoryRegion {
 public:
  ~PageMemoryRegion();

  void PageDeleted(Address);

  void MarkPageUsed(Address page) {
    DCHECK(!in_use_[Index(page)]);
    in_use_[Index(page)] = true;
  }

  void MarkPageUnused(Address page) { in_use_[Index(page)] = false; }

  static PageMemoryRegion* AllocateLargePage(size_t size,
                                             RegionTree* region_tree) {
    return Allocate(size, 1, region_tree);
  }

  static PageMemoryRegion* AllocateNormalPages(RegionTree* region_tree) {
    return Allocate(kBlinkPageSize * kBlinkPagesPerRegion, kBlinkPagesPerRegion,
                    region_tree);
  }

  BasePage* PageFromAddress(Address address) {
    DCHECK(Contains(address));
    if (!in_use_[Index(address)])
      return nullptr;
    if (is_large_page_)
      return PageFromObject(Base());
    return PageFromObject(address);
  }

 private:
  PageMemoryRegion(Address base, size_t, unsigned num_pages, RegionTree*);

  unsigned Index(Address address) const {
    DCHECK(Contains(address));
    if (is_large_page_)
      return 0;
    size_t offset = BlinkPageAddress(address) - Base();
    DCHECK_EQ(offset % kBlinkPageSize, 0u);
    return static_cast<unsigned>(offset / kBlinkPageSize);
  }

  static PageMemoryRegion* Allocate(size_t, unsigned num_pages, RegionTree*);

  const bool is_large_page_;
  // A thread owns a page, but not a region. Represent the in-use
  // bitmap such that thread non-interference comes for free.
  bool in_use_[kBlinkPagesPerRegion];
  base::AtomicRefCount num_pages_;
  RegionTree* region_tree_;
};

// A RegionTree is a simple binary search tree of PageMemoryRegions sorted
// by base addresses.
class RegionTree {
  USING_FAST_MALLOC(RegionTree);

 public:
  RegionTree() : root_(nullptr) {}

  void Add(PageMemoryRegion*);
  void Remove(PageMemoryRegion*);
  PageMemoryRegion* Lookup(Address);

 private:
  RegionTreeNode* root_;
};

class RegionTreeNode {
  USING_FAST_MALLOC(RegionTreeNode);

 public:
  explicit RegionTreeNode(PageMemoryRegion* region)
      : region_(region), left_(nullptr), right_(nullptr) {}

  ~RegionTreeNode() {
    delete left_;
    delete right_;
  }

  void AddTo(RegionTreeNode** context);

 private:
  PageMemoryRegion* region_;
  RegionTreeNode* left_;
  RegionTreeNode* right_;

  friend RegionTree;
};

// Representation of the memory used for a Blink heap page.
//
// The representation keeps track of two memory regions:
//
// 1. The virtual memory reserved from the system in order to be able
//    to free all the virtual memory reserved.  Multiple PageMemory
//    instances can share the same reserved memory region and
//    therefore notify the reserved memory region on destruction so
//    that the system memory can be given back when all PageMemory
//    instances for that memory are gone.
//
// 2. The writable memory (a sub-region of the reserved virtual
//    memory region) that is used for the actual heap page payload.
//
// Guard pages are created before and after the writable memory.
class PageMemory {
  USING_FAST_MALLOC(PageMemory);

 public:
  ~PageMemory() {
    __lsan_unregister_root_region(writable_.Base(), writable_.size());
    reserved_->PageDeleted(WritableStart());
  }

  WARN_UNUSED_RESULT bool Commit() {
    reserved_->MarkPageUsed(WritableStart());
    return writable_.Commit();
  }

  void Decommit() {
    reserved_->MarkPageUnused(WritableStart());
    writable_.Decommit();
  }

  void MarkUnused() { reserved_->MarkPageUnused(WritableStart()); }

  PageMemoryRegion* Region() { return reserved_; }

  Address WritableStart() { return writable_.Base(); }

  static PageMemory* SetupPageMemoryInRegion(PageMemoryRegion*,
                                             size_t page_offset,
                                             size_t payload_size);

  // Allocate a virtual address space for one blink page with the
  // following layout:
  //
  //    [ guard os page | ... payload ... | guard os page ]
  //    ^---{ aligned to blink page size }
  //
  // The returned page memory region will be zeroed.
  //
  static PageMemory* Allocate(size_t payload_size, RegionTree*);

 private:
  PageMemory(PageMemoryRegion* reserved, const MemoryRegion& writable);

  PageMemoryRegion* reserved_;
  MemoryRegion writable_;
};

}  // namespace blink

#endif
