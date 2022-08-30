// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_ARENA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_ARENA_H_

#include <memory>
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// A very simple implementation of a memory arena, i.e., a structure for
// making many (small) allocations cheaply and then freeing them all at once.
// This makes allocations somewhat cheaper than using PartitionAlloc or
// Oilpan, and deallocation _much_ cheaper. The downside is, of course,
// that no memory is freed until the arena is gone, and that it's impossible
// to pull out single objects with a larger lifetime.
//
// Arena gets memory blocks from PartitionAlloc, exponentially increasing
// in size. This guarantees amortized O(1) calls to the underlying alloc/free.
//
// Just like malloc/free, destructors are not called unless you do it yourself.
class Arena {
 public:
  template <class T, class... Args>
  T* New(Args&&... args) {
    return new (Alloc(sizeof(T))) T(std::forward<Args>(args)...);
  }

  void* Alloc(size_t bytes) {
    if (static_cast<size_t>(end_ptr_ - current_ptr_) >= bytes) {
      // This is the normal, fast path.
      char* ret = current_ptr_;
      current_ptr_ += bytes;
      return ret;
    }

    // We cannot satisfy the allocation from the current memory block,
    // so we create a new one. The current block (if any) will never
    // be used for allocations again.
    return SlowAlloc(bytes);
  }

 private:
  void* SlowAlloc(size_t bytes);

  // A list of memory blocks fetched from the underlying allocator.
  // These are kept around only so that we can free them when destroyed.
  Vector<std::unique_ptr<char[]>, 4> mem_blocks_;

  // The memory block we are currently allocating from. Will correspond to
  // the last element in mem_blocks_, if any; when allocating, we move
  // current_ptr_ forward to shrink it.
  char* current_ptr_ = nullptr;
  char* end_ptr_ = nullptr;

  size_t next_block_size_ = 4096;
};

template <class T>
struct ArenaDestroy {
  void operator()(T* ptr) const {
    if (ptr) {
      ptr->~T();
    }
  }
};

// A unique_ptr that only calls destructors, instead of deallocating.
template <class T>
using ArenaUniquePtr = std::unique_ptr<T, ArenaDestroy<T>>;

template <class T, bool UseArena>
using MaybeArenaUniquePtr =
    std::conditional_t<UseArena, ArenaUniquePtr<T>, std::unique_ptr<T>>;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_ARENA_H_
