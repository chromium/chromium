// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file contains the definition of the FencedAllocator class.

#ifndef GPU_COMMAND_BUFFER_CLIENT_FENCED_ALLOCATOR_H_
#define GPU_COMMAND_BUFFER_CLIENT_FENCED_ALLOCATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "gpu/gpu_export.h"

namespace gpu {
class CommandBufferHelper;

// FencedAllocator provides a mechanism to manage allocations within a fixed
// block of memory (storing the book-keeping externally). Furthermore this
// class allows to free data "pending" the passage of a command buffer token,
// that is, the memory won't be reused until the command buffer has processed
// that token.
//
// NOTE: Although this class is intended to be used in the command buffer
// environment which is multi-process, this class isn't "thread safe", because
// it isn't meant to be shared across modules. It is thread-compatible though
// (see http://www.corp.google.com/eng/doc/cpp_primer.html#thread_safety).
class GPU_EXPORT FencedAllocator {
 public:
  typedef uint32_t Offset;
  // Invalid offset, returned by Alloc in case of failure.
  enum : Offset { kInvalidOffset = 0xffffffffU };

  // Allocation alignment, must be a power of two.
  enum : uint32_t { kAllocAlignment = 16 };

  // Status of a block of memory, for book-keeping.
  enum State { IN_USE, FREE, FREE_PENDING_TOKEN };

  FencedAllocator() = delete;

  // Creates a FencedAllocator. Note that the size of the buffer is passed, but
  // not its base address: everything is handled as offsets into the buffer.
  FencedAllocator(uint32_t size, CommandBufferHelper* helper);

  FencedAllocator(const FencedAllocator&) = delete;
  FencedAllocator& operator=(const FencedAllocator&) = delete;

  ~FencedAllocator();

  // Allocates a block of memory. If the buffer is out of directly available
  // memory, this function may wait until memory that was freed "pending a
  // token" can be re-used.
  //
  // Parameters:
  //   size: the size of the memory block to allocate.
  //
  // Returns:
  //   the offset of the allocated memory block, or kInvalidOffset if out of
  //   memory.
  Offset Alloc(uint32_t size);

  // Frees a block of memory.
  //
  // Parameters:
  //   offset: the offset of the memory block to free.
  void Free(Offset offset);

  // Frees a block of memory, pending the passage of a token. That memory won't
  // be re-allocated until the token has passed through the command stream.
  //
  // Parameters:
  //   offset: the offset of the memory block to free.
  //   token: the token value to wait for before re-using the memory.
  void FreePendingToken(Offset offset, int32_t token);

  // Frees any blocks pending a token for which the token has been read.
  void FreeUnused();

  // Gets the size of the largest free block that is available without waiting.
  uint32_t GetLargestFreeSize();

  // Gets the size of the largest free block that can be allocated if the
  // caller can wait. Allocating a block of this size will succeed, but may
  // block.
  uint32_t GetLargestFreeOrPendingSize();

  // Gets the total size of all free blocks that are available without waiting.
  uint32_t GetFreeSize();

  // Checks for consistency inside the book-keeping structures. Used for
  // testing.
  bool CheckConsistency();

  // True if any memory is allocated.
  bool InUseOrFreePending();

  // Return bytes of memory that is IN_USE
  uint32_t bytes_in_use() const { return bytes_in_use_; }

  // Gets the status of a block, as well as the corresponding token if
  // FREE_PENDING_TOKEN.
  State GetBlockStatusForTest(Offset offset, int32_t* token_if_pending);

 private:
  // Book-keeping sturcture that describes a block of memory.
  struct Block {
    State state;
    Offset offset;
    uint32_t size;
    int32_t token;  // token to wait for in the FREE_PENDING_TOKEN case.
  };

  // Comparison functor for memory block sorting.
  class OffsetCmp {
   public:
    bool operator() (const Block &left, const Block &right) {
      return left.offset < right.offset;
    }
  };

  typedef std::vector<Block> Container;
  typedef uint32_t BlockIndex;

  static const int32_t kUnusedToken = 0;

  // Gets the index of a memory block, given its offset.
  BlockIndex GetBlockByOffset(Offset offset);

  // Collapse a free block with its neighbours if they are free. Returns the
  // index of the collapsed block.
  // NOTE: this will invalidate block indices.
  BlockIndex CollapseFreeBlock(BlockIndex index);

  // Waits for a FREE_PENDING_TOKEN block to be usable, and free it. Returns
  // the new index of that block (since it may have been collapsed).
  // NOTE: this will invalidate block indices.
  BlockIndex WaitForTokenAndFreeBlock(BlockIndex index);

  // Allocates a block of memory inside a given block, splitting it in two
  // (unless that block is of the exact requested size).
  // NOTE: this will invalidate block indices.
  // Returns the offset of the allocated block (NOTE: this is different from
  // the other functions that return a block index).
  Offset AllocInBlock(BlockIndex index, uint32_t size);

  raw_ptr<CommandBufferHelper> helper_;
  Container blocks_;
  uint32_t bytes_in_use_;
};

// This class functions just like FencedAllocator, but its API uses pointers
// instead of offsets.
class FencedAllocatorWrapper {
 public:
  FencedAllocatorWrapper() = delete;

  FencedAllocatorWrapper(uint32_t size, CommandBufferHelper* helper, void* base)
      : allocator_(size, helper), base_(base) {}

  FencedAllocatorWrapper(const FencedAllocatorWrapper&) = delete;
  FencedAllocatorWrapper& operator=(const FencedAllocatorWrapper&) = delete;

  // Allocates a block of memory. If the buffer is out of directly available
  // memory, this function may wait until memory that was freed "pending a
  // token" can be re-used.
  //
  // Parameters:
  //   size: the size of the memory block to allocate.
  //
  // Returns:
  //   the pointer to the allocated memory block, or NULL if out of
  //   memory.
  void* Alloc(uint32_t size) {
    FencedAllocator::Offset offset = allocator_.Alloc(size);
    return GetPointer(offset);
  }

  // Allocates a block of memory. If the buffer is out of directly available
  // memory, this function may wait until memory that was freed "pending a
  // token" can be re-used.
  // This is a type-safe version of Alloc, returning a typed pointer.
  //
  // Parameters:
  //   count: the number of elements to allocate.
  //
  // Returns:
  //   the pointer to the allocated memory block, or NULL if out of
  //   memory.
  template <typename T>
  T* AllocTyped(uint32_t count) {
    return static_cast<T *>(Alloc(count * sizeof(T)));
  }

  // Frees a block of memory.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  void Free(void *pointer) {
    DCHECK(pointer);
    allocator_.Free(GetOffset(pointer));
  }

  // Frees a block of memory, pending the passage of a token. That memory won't
  // be re-allocated until the token has passed through the command stream.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  //   token: the token value to wait for before re-using the memory.
  void FreePendingToken(void* pointer, int32_t token) {
    DCHECK(pointer);
    allocator_.FreePendingToken(GetOffset(pointer), token);
  }

  // Frees any blocks pending a token for which the token has been read.
  void FreeUnused() {
    allocator_.FreeUnused();
  }

  // Gets a pointer to a memory block given the base memory and the offset.
  // It translates FencedAllocator::kInvalidOffset to nullptr.
  void *GetPointer(FencedAllocator::Offset offset) {
    return (offset == FencedAllocator::kInvalidOffset)
               ? nullptr
               : static_cast<char*>(base_) + offset;
  }

  // Gets the offset to a memory block given the base memory and the address.
  // It translates nullptr to FencedAllocator::kInvalidOffset.
  FencedAllocator::Offset GetOffset(void *pointer) {
    return pointer ?
        static_cast<FencedAllocator::Offset>(
            static_cast<char*>(pointer) - static_cast<char*>(base_)) :
        FencedAllocator::kInvalidOffset;
  }

  // Gets the size of the largest free block that is available without waiting.
  uint32_t GetLargestFreeSize() { return allocator_.GetLargestFreeSize(); }

  // Gets the size of the largest free block that can be allocated if the
  // caller can wait.
  uint32_t GetLargestFreeOrPendingSize() {
    return allocator_.GetLargestFreeOrPendingSize();
  }

  // Gets the total size of all free blocks.
  uint32_t GetFreeSize() { return allocator_.GetFreeSize(); }

  // Checks for consistency inside the book-keeping structures. Used for
  // testing.
  bool CheckConsistency() {
    return allocator_.CheckConsistency();
  }

  // True if any memory is allocated.
  bool InUseOrFreePending() { return allocator_.InUseOrFreePending(); }

  FencedAllocator &allocator() { return allocator_; }

  uint32_t bytes_in_use() const { return allocator_.bytes_in_use(); }

  FencedAllocator::State GetPointerStatusForTest(void* pointer,
                                                 int32_t* token_if_pending) {
    return allocator_.GetBlockStatusForTest(GetOffset(pointer),
                                            token_if_pending);
  }

 private:
  FencedAllocator allocator_;
  raw_ptr<void> base_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_FENCED_ALLOCATOR_H_
