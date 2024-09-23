// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file contains the definition of the RingBuffer class.

#ifndef GPU_COMMAND_BUFFER_CLIENT_RING_BUFFER_H_
#define GPU_COMMAND_BUFFER_CLIENT_RING_BUFFER_H_

#include <stdint.h>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "gpu/gpu_export.h"

namespace gpu {
class CommandBufferHelper;

// RingBuffer manages a piece of memory as a ring buffer. Memory is allocated
// with Alloc and then a is freed pending a token with FreePendingToken.  Old
// allocations must not be kept past new allocations.
class GPU_EXPORT RingBuffer {
 public:
  typedef uint32_t Offset;

  RingBuffer() = delete;

  // Creates a RingBuffer.
  // Parameters:
  //   alignment: Alignment for allocations.
  //   base_offset: The offset of the start of the buffer.
  //   size: The size of the buffer in bytes.
  //   helper: A CommandBufferHelper for dealing with tokens.
  //   base: The physical address that corresponds to base_offset.
  RingBuffer(uint32_t alignment,
             Offset base_offset,
             uint32_t size,
             CommandBufferHelper* helper,
             void* base);

  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;

  ~RingBuffer();

  // Allocates a block of memory. If the buffer is out of directly available
  // memory, this function may wait until memory that was freed "pending a
  // token" can be re-used.  The safest pattern of allocation is to only have
  // one used allocation at once.  Allocating while NumUsedBlocks > 0 can
  // lead to deadlock if the entire buffer is exhausted.  In this case, it is
  // recommended to only Alloc smaller than GetFreeSizeNoWaiting.
  //
  // Parameters:
  //   size: the size of the memory block to allocate.
  //
  // Returns:
  //   the pointer to the allocated memory block.
  void* Alloc(uint32_t size);

  // Frees a block of memory, pending the passage of a token. That memory won't
  // be re-allocated until the token has passed through the command stream.
  // If a block is freed out of order, that hole will be counted as used
  // in the Get*FreeSize* functions below.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  //   token: the token value to wait for before re-using the memory.
  void FreePendingToken(void* pointer, uint32_t token);

  // Discards a block within the ring buffer.
  //
  // Parameters:
  //   pointer: the pointer to the memory block to free.
  void DiscardBlock(void* pointer);

  // Gets the size of the largest free block that is available without waiting.
  uint32_t GetLargestFreeSizeNoWaiting();

  // Gets the total size of all free blocks that are available without waiting.
  uint32_t GetTotalFreeSizeNoWaiting();

  // Gets the size of the largest free block that can be allocated if the
  // caller can wait. Allocating a block of this size will succeed, but may
  // block.
  uint32_t GetLargestFreeOrPendingSize() {
    // If size_ is not a multiple of alignment_, then trying to allocate it will
    // cause us to try to allocate more than we actually can due to rounding up.
    // So, round down here.
    return size_ - size_ % alignment_;
  }

  // Total size minus usable size.
  uint32_t GetUsedSize() { return size_ - GetLargestFreeSizeNoWaiting(); }

  uint32_t NumUsedBlocks() const { return num_used_blocks_; }

  // Gets a pointer to a memory block given the base memory and the offset.
  void* GetPointer(RingBuffer::Offset offset) const {
    return static_cast<int8_t*>(base_) + offset;
  }

  // Gets the offset to a memory block given the base memory and the address.
  RingBuffer::Offset GetOffset(void* pointer) const {
    return static_cast<int8_t*>(pointer) - static_cast<int8_t*>(base_);
  }

  // Rounds the given size to the alignment in use.
  uint32_t RoundToAlignment(uint32_t size) {
    return (size + alignment_ - 1) & ~(alignment_ - 1);
  }

  // Shrinks the last block.  new_size must be smaller than the current size
  // and the block must still be in use in order to shrink.
  void ShrinkLastBlock(uint32_t new_size);

 private:
  enum State {
    IN_USE,
    PADDING,
    FREE_PENDING_TOKEN
  };
  // Book-keeping sturcture that describes a block of memory.
  struct Block {
    Block(Offset _offset, uint32_t _size, State _state)
        : offset(_offset), size(_size), token(0), state(_state) {}
    Offset offset;
    uint32_t size;
    uint32_t token;  // token to wait for.
    State state;
  };

  using Container = base::circular_deque<Block>;
  using BlockIndex = uint32_t;

  void FreeOldestBlock();
  uint32_t GetLargestFreeSizeNoWaitingInternal();

  raw_ptr<CommandBufferHelper> helper_;

  // Used blocks are added to the end, blocks are freed from the beginning.
  Container blocks_;

  // The base offset of the ring buffer.
  Offset base_offset_;

  // The size of the ring buffer.
  Offset size_;

  // Offset of first free byte.
  Offset free_offset_ = 0;

  // Offset of first used byte.
  // Range between in_use_mark and free_mark is in use.
  Offset in_use_offset_ = 0;

  // Alignment for allocations.
  uint32_t alignment_;

  // Number of blocks in |blocks_| that are in the IN_USE state.
  uint32_t num_used_blocks_ = 0;

  // The physical address that corresponds to base_offset.
  raw_ptr<void, AcrossTasksDanglingUntriaged> base_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_RING_BUFFER_H_
