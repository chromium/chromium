// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of the FencedAllocator class.

#include "gpu/command_buffer/client/fenced_allocator.h"

#include <stdint.h>

#include <algorithm>

#include "gpu/command_buffer/client/cmd_buffer_helper.h"

namespace gpu {

namespace {

// Round down to the largest multiple of kAllocAlignment no greater than |size|.
uint32_t RoundDown(uint32_t size) {
  return size & ~(FencedAllocator::kAllocAlignment - 1);
}

// Round up to the smallest multiple of kAllocAlignment no smaller than |size|.
base::CheckedNumeric<uint32_t> RoundUp(uint32_t size) {
  return (base::CheckedNumeric<uint32_t>(size) +
          (FencedAllocator::kAllocAlignment - 1)) &
         ~(FencedAllocator::kAllocAlignment - 1);
}

}  // namespace

FencedAllocator::FencedAllocator(uint32_t size, CommandBufferHelper* helper)
    : helper_(helper), bytes_in_use_(0) {
  Block block = { FREE, 0, RoundDown(size), kUnusedToken };
  blocks_.push_back(block);
}

FencedAllocator::~FencedAllocator() {
  // All IN_USE blocks should be released at this point. There may still be
  // FREE_PENDING_TOKEN blocks, the assumption is that the underlying memory
  // will not be re-used without higher level synchronization.
  DCHECK_EQ(bytes_in_use_, 0u);
}

// Looks for a non-allocated block that is big enough. Search in the FREE
// blocks first (for direct usage), first-fit, then in the FREE_PENDING_TOKEN
// blocks, waiting for them. The current implementation isn't smart about
// optimizing what to wait for, just looks inside the block in order (first-fit
// as well).
FencedAllocator::Offset FencedAllocator::Alloc(uint32_t size) {
  // size of 0 is not allowed because it would be inconsistent to only sometimes
  // have it succeed. Example: Alloc(SizeOfBuffer), Alloc(0).
  if (size == 0)  {
    return kInvalidOffset;
  }

  // Round up the allocation size to ensure alignment.
  uint32_t aligned_size = 0;
  if (!RoundUp(size).AssignIfValid(&aligned_size)) {
    return kInvalidOffset;
  }

  // Try first to allocate in a free block.
  for (uint32_t i = 0; i < blocks_.size(); ++i) {
    Block &block = blocks_[i];
    if (block.state == FREE && block.size >= aligned_size) {
      return AllocInBlock(i, aligned_size);
    }
  }

  // No free block is available. Look for blocks pending tokens, and wait for
  // them to be re-usable.
  for (uint32_t i = 0; i < blocks_.size(); ++i) {
    if (blocks_[i].state != FREE_PENDING_TOKEN)
      continue;
    i = WaitForTokenAndFreeBlock(i);
    if (blocks_[i].size >= aligned_size)
      return AllocInBlock(i, aligned_size);
  }
  return kInvalidOffset;
}

// Looks for the corresponding block, mark it FREE, and collapse it if
// necessary.
void FencedAllocator::Free(FencedAllocator::Offset offset) {
  BlockIndex index = GetBlockByOffset(offset);
  Block &block = blocks_[index];
  DCHECK_NE(block.state, FREE);
  DCHECK_EQ(block.offset, offset);

  if (block.state == IN_USE)
    bytes_in_use_ -= block.size;

  block.state = FREE;
  CollapseFreeBlock(index);
}

// Looks for the corresponding block, mark it FREE_PENDING_TOKEN.
void FencedAllocator::FreePendingToken(FencedAllocator::Offset offset,
                                       int32_t token) {
  BlockIndex index = GetBlockByOffset(offset);
  Block &block = blocks_[index];
  DCHECK_EQ(block.offset, offset);
  if (block.state == IN_USE)
    bytes_in_use_ -= block.size;
  block.state = FREE_PENDING_TOKEN;
  block.token = token;
}

// Gets the max of the size of the blocks marked as free.
uint32_t FencedAllocator::GetLargestFreeSize() {
  FreeUnused();
  uint32_t max_size = 0;
  for (uint32_t i = 0; i < blocks_.size(); ++i) {
    Block &block = blocks_[i];
    if (block.state == FREE)
      max_size = std::max(max_size, block.size);
  }
  return max_size;
}

// Gets the size of the largest segment of blocks that are either FREE or
// FREE_PENDING_TOKEN.
uint32_t FencedAllocator::GetLargestFreeOrPendingSize() {
  uint32_t max_size = 0;
  uint32_t current_size = 0;
  for (uint32_t i = 0; i < blocks_.size(); ++i) {
    Block &block = blocks_[i];
    if (block.state == IN_USE) {
      max_size = std::max(max_size, current_size);
      current_size = 0;
    } else {
      DCHECK(block.state == FREE || block.state == FREE_PENDING_TOKEN);
      current_size += block.size;
    }
  }
  return std::max(max_size, current_size);
}

// Gets the total size of all blocks marked as free.
uint32_t FencedAllocator::GetFreeSize() {
  FreeUnused();
  uint32_t size = 0;
  for (uint32_t i = 0; i < blocks_.size(); ++i) {
    Block& block = blocks_[i];
    if (block.state == FREE)
      size += block.size;
  }
  return size;
}

// Makes sure that:
// - there is at least one block.
// - there are no contiguous FREE blocks (they should have been collapsed).
// - the successive offsets match the block sizes, and they are in order.
bool FencedAllocator::CheckConsistency() {
  if (blocks_.size() < 1) return false;
  for (uint32_t i = 0; i < blocks_.size() - 1; ++i) {
    Block &current = blocks_[i];
    Block &next = blocks_[i + 1];
    // This test is NOT included in the next one, because offset is unsigned.
    if (next.offset <= current.offset)
      return false;
    if (next.offset != current.offset + current.size)
      return false;
    if (current.state == FREE && next.state == FREE)
      return false;
  }
  return true;
}

// Returns false if all blocks are actually FREE, in which
// case they would be coalesced into one block, true otherwise.
bool FencedAllocator::InUseOrFreePending() {
  return blocks_.size() != 1 || blocks_[0].state != FREE;
}

FencedAllocator::State FencedAllocator::GetBlockStatusForTest(
    Offset offset,
    int32_t* token_if_pending) {
  BlockIndex index = GetBlockByOffset(offset);
  Block& block = blocks_[index];
  if ((block.state == FREE_PENDING_TOKEN) && token_if_pending)
    *token_if_pending = block.token;
  return block.state;
}

// Collapse the block to the next one, then to the previous one. Provided the
// structure is consistent, those are the only blocks eligible for collapse.
FencedAllocator::BlockIndex FencedAllocator::CollapseFreeBlock(
    BlockIndex index) {
  if (index + 1 < blocks_.size()) {
    Block &next = blocks_[index + 1];
    if (next.state == FREE) {
      blocks_[index].size += next.size;
      blocks_.erase(blocks_.begin() + index + 1);
    }
  }
  if (index > 0) {
    Block &prev = blocks_[index - 1];
    if (prev.state == FREE) {
      prev.size += blocks_[index].size;
      blocks_.erase(blocks_.begin() + index);
      --index;
    }
  }
  return index;
}

// Waits for the block's token, then mark the block as free, then collapse it.
FencedAllocator::BlockIndex FencedAllocator::WaitForTokenAndFreeBlock(
    BlockIndex index) {
  Block &block = blocks_[index];
  DCHECK_EQ(block.state, FREE_PENDING_TOKEN);
  helper_->WaitForToken(block.token);
  block.state = FREE;
  return CollapseFreeBlock(index);
}

// Frees any blocks pending a token for which the token has been read.
void FencedAllocator::FreeUnused() {
  helper_->RefreshCachedToken();
  for (uint32_t i = 0; i < blocks_.size();) {
    Block& block = blocks_[i];
    if (block.state == FREE_PENDING_TOKEN &&
        helper_->HasCachedTokenPassed(block.token)) {
      block.state = FREE;
      i = CollapseFreeBlock(i);
    } else {
      ++i;
    }
  }
}

// If the block is exactly the requested size, simply mark it IN_USE, otherwise
// split it and mark the first one (of the requested size) IN_USE.
FencedAllocator::Offset FencedAllocator::AllocInBlock(BlockIndex index,
                                                      uint32_t size) {
  Block &block = blocks_[index];
  DCHECK_GE(block.size, size);
  DCHECK_EQ(block.state, FREE);
  Offset offset = block.offset;
  bytes_in_use_ += size;
  if (block.size == size) {
    block.state = IN_USE;
    return offset;
  }
  Block newblock = { FREE, offset + size, block.size - size, kUnusedToken};
  block.state = IN_USE;
  block.size = size;
  // this is the last thing being done because it may invalidate block;
  blocks_.insert(blocks_.begin() + index + 1, newblock);
  return offset;
}

// The blocks are in offset order, so we can do a binary search.
FencedAllocator::BlockIndex FencedAllocator::GetBlockByOffset(Offset offset) {
  Block templ = { IN_USE, offset, 0, kUnusedToken };
  Container::iterator it = std::lower_bound(blocks_.begin(), blocks_.end(),
                                            templ, OffsetCmp());
  DCHECK(it != blocks_.end());
  return it-blocks_.begin();
}

}  // namespace gpu
