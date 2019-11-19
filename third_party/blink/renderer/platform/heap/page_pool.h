// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PAGE_POOL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PAGE_POOL_H_

#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PageMemory;

// Once pages have been used for one type of thread heap they will never be
// reused for another type of thread heap.  Instead of unmapping, we add the
// pages to a pool of pages to be reused later by a thread heap of the same
// type. This is done as a security feature to avoid type confusion.  The
// heaps are type segregated by having separate thread arenas for different
// types of objects.  Holding on to pages ensures that the same virtual address
// space cannot be used for objects of another type than the type contained
// in this page to begin with.
class PagePool {
  USING_FAST_MALLOC(PagePool);

 public:
  PagePool();
  ~PagePool();
  void Add(int, PageMemory*);
  PageMemory* Take(int);

 private:
  class PoolEntry {
    USING_FAST_MALLOC(PoolEntry);

   public:
    PoolEntry(PageMemory* data, PoolEntry* next) : data(data), next(next) {}

    PageMemory* data;
    PoolEntry* next;
  };

  PoolEntry* pool_[BlinkGC::kNumberOfArenas];
};

}  // namespace blink

#endif
