// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/impl/page_pool.h"

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/impl/page_memory.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

PagePool::PagePool() {
  for (int i = 0; i < BlinkGC::kNumberOfArenas; ++i) {
    pool_[i] = nullptr;
  }
}

PagePool::~PagePool() {
  for (int index = 0; index < BlinkGC::kNumberOfArenas; ++index) {
    while (PoolEntry* entry = pool_[index]) {
      pool_[index] = entry->next;
      PageMemory* memory = entry->data;
      DCHECK(memory);
      delete memory;
      delete entry;
    }
  }
}

void PagePool::Add(int index, PageMemory* memory) {
  // When adding a page to the pool we decommit it to ensure it is unused
  // while in the pool.  This also allows the physical memory, backing the
  // page, to be given back to the OS.
  memory->Decommit();
  PoolEntry* entry = new PoolEntry(memory, pool_[index]);
  pool_[index] = entry;
}

PageMemory* PagePool::Take(int index) {
  if (PoolEntry* entry = pool_[index]) {
    pool_[index] = entry->next;
    PageMemory* memory = entry->data;
    DCHECK(memory);
    delete entry;
    memory->Commit();
    return memory;
  }
  return nullptr;
}

}  // namespace blink
