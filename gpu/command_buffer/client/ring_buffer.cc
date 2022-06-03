// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of the RingBuffer class.

#include "gpu/command_buffer/client/ring_buffer.h"

#include <stdint.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"

namespace gpu {

RingBuffer::RingBuffer(uint32_t alignment,
                       Offset base_offset,
                       uint32_t size,
                       CommandBufferHelper* helper,
                       void* base)
    : helper_(helper),
      base_offset_(base_offset),
      size_(size),
      alignment_(alignment),
      base_(static_cast<int8_t*>(base) - base_offset) {}

RingBuffer::~RingBuffer() {
  DCHECK_EQ(num_used_blocks_, 0u);
  for (const auto& block : blocks_)
    DCHECK(block.state != IN_USE);
}

void RingBuffer::FreeOldestBlock() {
  DCHECK(!blocks_.empty()) << "no free blocks";
  Block& block = blocks_.front();
  DCHECK(block.state != IN_USE)
      << "attempt to allocate more than maximum memory";
  if (block.state == FREE_PENDING_TOKEN) {
    helper_->WaitForToken(block.token);
  }
  in_use_offset_ += block.size;
  if (in_use_offset_ == size_) {
    in_use_offset_ = 0;
  }
  // If they match then the entire buffer is free.
  if (in_use_offset_ == free_offset_) {
    in_use_offset_ = 0;
    free_offset_ = 0;
  }
  blocks_.pop_front();
}

void* RingBuffer::Alloc(uint32_t size) {
  DCHECK_LE(size, size_) << "attempt to allocate more than maximum memory";
  // Similarly to malloc, an allocation of 0 allocates at least 1 byte, to
  // return different pointers every time.
  if (size == 0) size = 1;
  // Allocate rounded to alignment size so that the offsets are always
  // memory-aligned.
  size = RoundToAlignment(size);
  DCHECK_LE(size, size_)
      << "attempt to allocate more than maximum memory after rounding";

  // Wait until there is enough room.
  while (size > GetLargestFreeSizeNoWaitingInternal()) {
    FreeOldestBlock();
  }

  if (size + free_offset_ > size_) {
    // Add padding to fill space before wrapping around
    blocks_.push_back(Block(free_offset_, size_ - free_offset_, PADDING));
    free_offset_ = 0;
  }

  Offset offset = free_offset_;
  blocks_.push_back(Block(offset, size, IN_USE));
  num_used_blocks_++;

  free_offset_ += size;
  if (free_offset_ == size_) {
    free_offset_ = 0;
  }

  return GetPointer(offset + base_offset_);
}

void RingBuffer::FreePendingToken(void* pointer, uint32_t token) {
  Offset offset = GetOffset(pointer);
  offset -= base_offset_;
  DCHECK(!blocks_.empty()) << "no allocations to free";
  for (Container::reverse_iterator it = blocks_.rbegin();
        it != blocks_.rend();
        ++it) {
    Block& block = *it;
    if (block.offset == offset) {
      DCHECK(block.state == IN_USE)
          << "block that corresponds to offset already freed";
      block.token = token;
      block.state = FREE_PENDING_TOKEN;
      num_used_blocks_--;
      return;
    }
  }

  NOTREACHED() << "attempt to free non-existant block";
}

void RingBuffer::DiscardBlock(void* pointer) {
  Offset offset = GetOffset(pointer);
  offset -= base_offset_;
  DCHECK(!blocks_.empty()) << "no allocations to discard";
  for (Container::reverse_iterator it = blocks_.rbegin();
        it != blocks_.rend();
        ++it) {
    Block& block = *it;
    if (block.offset == offset) {
      DCHECK(block.state != PADDING)
          << "block that corresponds to offset already discarded";
      if (block.state == IN_USE)
        num_used_blocks_--;
      block.state = PADDING;

      // Remove block if it were in the back along with any extra padding.
      while (!blocks_.empty() && blocks_.back().state == PADDING) {
        free_offset_= blocks_.back().offset;
        blocks_.pop_back();
      }

      // Remove blocks if it were in the front along with extra padding.
      while (!blocks_.empty() && blocks_.front().state == PADDING) {
        blocks_.pop_front();
        if (blocks_.empty())
          break;

        in_use_offset_ = blocks_.front().offset;
      }

      // In the special case when there are no blocks, we should be reset it.
      if (blocks_.empty()) {
        in_use_offset_ = 0;
        free_offset_ = 0;
      }
      return;
    }
  }
  NOTREACHED() << "attempt to discard non-existant block";
}

uint32_t RingBuffer::GetLargestFreeSizeNoWaiting() {
  uint32_t size = GetLargestFreeSizeNoWaitingInternal();
  DCHECK_EQ(size, RoundToAlignment(size));
  return size;
}

uint32_t RingBuffer::GetLargestFreeSizeNoWaitingInternal() {
  while (!blocks_.empty()) {
    Block& block = blocks_.front();
    if (!helper_->HasTokenPassed(block.token) || block.state == IN_USE) break;
    FreeOldestBlock();
  }
  if (free_offset_ == in_use_offset_) {
    if (blocks_.empty()) {
      // The entire buffer is free.
      DCHECK_EQ(free_offset_, 0u);
      return size_;
    } else {
      // The entire buffer is in use.
      return 0;
    }
  } else if (free_offset_ > in_use_offset_) {
    // It's free from free_offset_ to size_ and from 0 to in_use_offset_
    return std::max(size_ - free_offset_, in_use_offset_);
  } else {
    // It's free from free_offset_ -> in_use_offset_;
    return in_use_offset_ - free_offset_;
  }
}

uint32_t RingBuffer::GetTotalFreeSizeNoWaiting() {
  uint32_t largest_free_size = GetLargestFreeSizeNoWaitingInternal();
  if (free_offset_ > in_use_offset_) {
    // It's free from free_offset_ to size_ and from 0 to in_use_offset_.
    return size_ - free_offset_ + in_use_offset_;
  } else {
    return largest_free_size;
  }
}

void RingBuffer::ShrinkLastBlock(uint32_t new_size) {
  if (blocks_.empty())
    return;
  auto& block = blocks_.back();
  DCHECK_LT(new_size, block.size);
  DCHECK_EQ(block.state, IN_USE);

  // Can't shrink to size 0, see comments in Alloc.
  new_size = std::max(new_size, 1u);

  // Allocate rounded to alignment size so that the offsets are always
  // memory-aligned.
  new_size = RoundToAlignment(new_size);
  if (new_size == block.size)
    return;
  free_offset_ = block.offset + new_size;
  block.size = new_size;
}

}  // namespace gpu
