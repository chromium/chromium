// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// A class to Manage a growing transfer buffer.

#include "gpu/command_buffer/client/transfer_buffer.h"

#include <stddef.h>
#include <stdint.h>
#include <climits>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"

namespace gpu {

TransferBuffer::TransferBuffer(CommandBufferHelper* helper)
    : helper_(helper),
      result_size_(0),
      default_buffer_size_(0),
      min_buffer_size_(0),
      max_buffer_size_(0),
      alignment_(0),
      buffer_id_(-1),
      result_buffer_(nullptr),
      result_shm_offset_(0),
      usable_(true) {}

TransferBuffer::~TransferBuffer() {
  Free();
}

base::UnguessableToken TransferBuffer::shared_memory_guid() const {
  if (!HaveBuffer())
    return base::UnguessableToken();
  if (!buffer_->backing())
    return base::UnguessableToken();
  return buffer_->backing()->GetGUID();
}

bool TransferBuffer::Initialize(unsigned int default_buffer_size,
                                unsigned int result_size,
                                unsigned int min_buffer_size,
                                unsigned int max_buffer_size,
                                unsigned int alignment) {
  result_size_ = result_size;
  alignment_ = alignment;
  default_buffer_size_ = base::bits::AlignUp(default_buffer_size, alignment);
  min_buffer_size_ = base::bits::AlignUp(min_buffer_size, alignment);
  max_buffer_size_ = base::bits::AlignUp(max_buffer_size, alignment);
  ReallocateRingBuffer(default_buffer_size_ - result_size);
  return HaveBuffer();
}

void TransferBuffer::Free() {
  DCHECK(!outstanding_result_pointer_);
  if (HaveBuffer()) {
    TRACE_EVENT0("gpu", "TransferBuffer::Free");
    helper_->OrderingBarrier();
    helper_->command_buffer()->DestroyTransferBuffer(buffer_id_);
    if (!HaveBuffer()) {
      // The above may call this function reentrantly. If the buffer was
      // already freed, then our work is done.
      return;
    }
    buffer_id_ = -1;
    result_shm_offset_ = 0;
    DCHECK_EQ(ring_buffer_->NumUsedBlocks(), 0u);
    previous_ring_buffers_.push_back(std::move(ring_buffer_));
    last_allocated_size_ = 0;
    high_water_mark_ = GetPreviousRingBufferUsedBytes();
    bytes_since_last_shrink_ = 0;
    result_buffer_ = nullptr;
    buffer_ = nullptr;
  }
}

bool TransferBuffer::HaveBuffer() const {
  DCHECK(buffer_id_ == -1 || buffer_.get());
  return buffer_id_ != -1;
}

RingBuffer::Offset TransferBuffer::GetOffset(void* pointer) const {
  return ring_buffer_->GetOffset(pointer);
}

void TransferBuffer::DiscardBlock(void* p) {
  ring_buffer_->DiscardBlock(p);
}

void TransferBuffer::FreePendingToken(void* p, unsigned int token) {
  ring_buffer_->FreePendingToken(p, token);
}

unsigned int TransferBuffer::GetSize() const {
  return HaveBuffer() ? ring_buffer_->GetLargestFreeOrPendingSize() : 0;
}

unsigned int TransferBuffer::GetFreeSize() const {
  return HaveBuffer() ? ring_buffer_->GetLargestFreeSizeNoWaiting() : 0;
}

unsigned int TransferBuffer::GetFragmentedFreeSize() const {
  return HaveBuffer() ? ring_buffer_->GetTotalFreeSizeNoWaiting() : 0;
}

void TransferBuffer::ShrinkLastBlock(unsigned int new_size) {
  ring_buffer_->ShrinkLastBlock(new_size);
}

unsigned int TransferBuffer::GetMaxSize() const {
  return max_buffer_size_ - result_size_;
}

void TransferBuffer::AllocateRingBuffer(unsigned int size) {
  for (;size >= min_buffer_size_; size /= 2) {
    int32_t id = -1;
    scoped_refptr<gpu::Buffer> buffer =
        helper_->command_buffer()->CreateTransferBuffer(size, &id, alignment_);
    if (id != -1) {
      last_allocated_size_ = size;
      DCHECK(buffer.get());
      buffer_ = buffer;
      ring_buffer_ = std::make_unique<RingBuffer>(
          alignment_, result_size_, buffer_->size() - result_size_, helper_,
          static_cast<char*>(buffer_->memory()) + result_size_);
      buffer_id_ = id;
      result_buffer_ = buffer_->memory();
      result_shm_offset_ = 0;
      bytes_since_last_shrink_ = 0;
      return;
    }
    // we failed so don't try larger than this.
    max_buffer_size_ = base::bits::AlignUp(size / 2, alignment_);
  }
  usable_ = false;
}

static unsigned int ComputePOTSize(unsigned int dimension) {
  // Avoid shifting by more than the size of an unsigned int - 1, because that's
  // undefined behavior.
  return (dimension == 0)
             ? 0
             : 1 << std::min(static_cast<int>(sizeof(dimension) * CHAR_BIT - 1),
                             base::bits::Log2Ceiling(dimension));
}

void TransferBuffer::ReallocateRingBuffer(unsigned int size, bool shrink) {
  // We should never attempt to shrink the buffer if someone has a result
  // pointer that hasn't been released.
  DCHECK(!shrink || !outstanding_result_pointer_);
  // What size buffer would we ask for if we needed a new one?
  unsigned int needed_buffer_size = ComputePOTSize(size + result_size_);
  DCHECK_EQ(needed_buffer_size % alignment_, 0u)
      << "Buffer size is not a multiple of alignment_";
  needed_buffer_size = std::max(needed_buffer_size, min_buffer_size_);
  if (!HaveBuffer())
    needed_buffer_size = std::max(needed_buffer_size, default_buffer_size_);
  needed_buffer_size = std::min(needed_buffer_size, max_buffer_size_);

  unsigned int current_size = HaveBuffer() ? buffer_->size() : 0;
  if (current_size == needed_buffer_size)
    return;

  if (usable_ && (shrink || needed_buffer_size > current_size)) {
    // We should never attempt to reallocate the buffer if someone has a result
    // pointer that hasn't been released. This would cause a use-after-free.
    DCHECK(!outstanding_result_pointer_);
    if (HaveBuffer()) {
      Free();
    }
    AllocateRingBuffer(needed_buffer_size);
  }
}

unsigned int TransferBuffer::GetPreviousRingBufferUsedBytes() {
  while (!previous_ring_buffers_.empty() &&
         previous_ring_buffers_.front()->GetUsedSize() == 0) {
    previous_ring_buffers_.pop_front();
  }
  unsigned int total = 0;
  for (auto& buffer : previous_ring_buffers_) {
    total += buffer->GetUsedSize();
  }
  return total;
}

void TransferBuffer::ShrinkOrExpandRingBufferIfNecessary(
    unsigned int size_to_allocate) {
  // We should never attempt to shrink the buffer if someone has a result
  // pointer that hasn't been released.
  DCHECK(!outstanding_result_pointer_);
  // Don't resize the buffer while blocks are in use to avoid throwing away
  // live allocations.
  if (HaveBuffer() && ring_buffer_->NumUsedBlocks() > 0)
    return;

  unsigned int available_size = GetFreeSize();
  high_water_mark_ =
      std::max(high_water_mark_, last_allocated_size_ - available_size +
                                     size_to_allocate +
                                     GetPreviousRingBufferUsedBytes());
  if (size_to_allocate > available_size) {
    // Try to expand the ring buffer.
    ReallocateRingBuffer(high_water_mark_);
  } else if (bytes_since_last_shrink_ > high_water_mark_ * kShrinkThreshold) {
    // The intent of the above check is to limit the frequency of buffer shrink
    // attempts. Unfortunately if an application uploads a large amount of data
    // once and from then on uploads only a small amount per frame, it will be a
    // very long time before we attempt to shrink (or forever, if no data is
    // uploaded).
    // TODO(jdarpinian): Change this heuristic to be based on frame number
    // instead, and consider shrinking at the end of each frame (for clients
    // that have a notion of frames).
    bytes_since_last_shrink_ = 0;
    ReallocateRingBuffer(high_water_mark_ + high_water_mark_ / 4,
                         true /* shrink */);
    high_water_mark_ = size_to_allocate + GetPreviousRingBufferUsedBytes();
  }
}

void* TransferBuffer::AllocUpTo(
    unsigned int size, unsigned int* size_allocated) {
  DCHECK(size_allocated);

  ShrinkOrExpandRingBufferIfNecessary(size);

  if (!HaveBuffer()) {
    return nullptr;
  }

  unsigned int max_size = ring_buffer_->GetLargestFreeOrPendingSize();
  *size_allocated = std::min(max_size, size);
  bytes_since_last_shrink_ += *size_allocated;
  return ring_buffer_->Alloc(*size_allocated);
}

void* TransferBuffer::Alloc(unsigned int size) {
  ShrinkOrExpandRingBufferIfNecessary(size);

  if (!HaveBuffer()) {
    return nullptr;
  }

  unsigned int max_size = ring_buffer_->GetLargestFreeOrPendingSize();
  if (size > max_size) {
    return nullptr;
  }
  bytes_since_last_shrink_ += size;
  return ring_buffer_->Alloc(size);
}

void* TransferBuffer::AcquireResultBuffer() {
  // There should never be two result pointers active at the same time. The
  // previous pointer should always be released first. ScopedResultPtr helps
  // ensure this invariant.
  DCHECK(!outstanding_result_pointer_);
  ReallocateRingBuffer(result_size_);
#if DCHECK_IS_ON()
  outstanding_result_pointer_ = true;
#endif
  return result_buffer_;
}

void TransferBuffer::ReleaseResultBuffer() {
  DCHECK(outstanding_result_pointer_);
#if DCHECK_IS_ON()
  outstanding_result_pointer_ = false;
#endif
}

int TransferBuffer::GetResultOffset() {
  DCHECK(outstanding_result_pointer_);
  return result_shm_offset_;
}

int TransferBuffer::GetShmId() {
  ReallocateRingBuffer(result_size_);
  return buffer_id_;
}

unsigned int TransferBuffer::GetCurrentMaxAllocationWithoutRealloc() const {
  return HaveBuffer() ? ring_buffer_->GetLargestFreeOrPendingSize() : 0;
}

ScopedTransferBufferPtr::ScopedTransferBufferPtr(
    ScopedTransferBufferPtr&& other)
    : buffer_(other.buffer_),
      size_(other.size_),
      helper_(other.helper_),
      transfer_buffer_(other.transfer_buffer_) {
  other.buffer_ = nullptr;
  other.size_ = 0u;
}

void ScopedTransferBufferPtr::Release() {
  if (buffer_) {
    transfer_buffer_->FreePendingToken(buffer_, helper_->InsertToken());
    buffer_ = nullptr;
    size_ = 0;
  }
}

void ScopedTransferBufferPtr::Discard() {
  if (buffer_) {
    transfer_buffer_->DiscardBlock(buffer_);
    buffer_ = nullptr;
    size_ = 0;
  }
}

void ScopedTransferBufferPtr::Reset(unsigned int new_size) {
  Release();
  // NOTE: we allocate buffers of size 0 so that HaveBuffer will be true, so
  // that address will return a pointer just like malloc, and so that GetShmId
  // will be valid. That has the side effect that we'll insert a token on free.
  // We could add code skip the token for a zero size buffer but it doesn't seem
  // worth the complication.
  buffer_ = transfer_buffer_->AllocUpTo(new_size, &size_);
}

void ScopedTransferBufferPtr::Shrink(unsigned int new_size) {
  if (!transfer_buffer_->HaveBuffer() || new_size >= size_)
    return;
  transfer_buffer_->ShrinkLastBlock(new_size);
  size_ = new_size;
}

bool ScopedTransferBufferPtr::BelongsToBuffer(uint8_t* memory) const {
  if (!buffer_)
    return false;
  uint8_t* start = static_cast<uint8_t*>(buffer_.get());
  uint8_t* end = start + size_;
  return memory >= start && memory <= end;
}

}  // namespace gpu
