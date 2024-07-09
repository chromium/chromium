// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/rw_buffer.h"

#include "base/atomic_ref_count.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

#include <algorithm>
#include <atomic>
#include <new>

namespace blink {

namespace {

// Force small chunks to be a page's worth
static const size_t kMinAllocSize = 4096;

}  // namespace

struct RWBuffer::BufferBlock {
  raw_ptr<RWBuffer::BufferBlock> next_;  // updated by the writer
  size_t used_;                  // updated by the writer
  const size_t capacity_;

  explicit BufferBlock(size_t capacity)
      : next_(nullptr), used_(0), capacity_(capacity) {}

  const void* startData() const { return this + 1; }

  size_t avail() const { return capacity_ - used_; }
  void* avail_data() {
    return reinterpret_cast<char*>(const_cast<void*>(startData())) + used_;
  }

  static RWBuffer::BufferBlock* Alloc(size_t length) {
    size_t capacity = LengthToCapacity(length);
    void* buffer =
        WTF::Partitions::BufferMalloc(sizeof(RWBuffer::BufferBlock) + capacity,
                                      "blink::RWBuffer::BufferBlock");
    return new (buffer) RWBuffer::BufferBlock(capacity);
  }

  // Return number of bytes actually appended. Important that we always
  // completely fill this block before spilling into the next, since the reader
  // uses capacity_ to know how many bytes it can read.
  size_t Append(const void* src, size_t length) {
    Validate();
    size_t amount = std::min(avail(), length);
    memcpy(avail_data(), src, amount);
    used_ += amount;
    Validate();
    return amount;
  }

  // Do not call in the reader thread, since the writer may be updating used_.
  // (The assertion is still true, but TSAN still may complain about its
  // raciness.)
  void Validate() const {
    DCHECK_GT(capacity_, 0u);
    DCHECK_LE(used_, capacity_);
  }

 private:
  static size_t LengthToCapacity(size_t length) {
    const size_t min_size = kMinAllocSize - sizeof(RWBuffer::BufferBlock);
    return std::max(length, min_size);
  }
};

struct RWBuffer::BufferHead {
  mutable base::AtomicRefCount ref_count_;
  RWBuffer::BufferBlock block_;

  explicit BufferHead(size_t capacity) : ref_count_(1), block_(capacity) {}

  static size_t LengthToCapacity(size_t length) {
    const size_t min_size = kMinAllocSize - sizeof(RWBuffer::BufferHead);
    return std::max(length, min_size);
  }

  static RWBuffer::BufferHead* Alloc(size_t length) {
    size_t capacity = LengthToCapacity(length);
    size_t size = sizeof(RWBuffer::BufferHead) + capacity;
    void* buffer =
        WTF::Partitions::BufferMalloc(size, "blink::RWBuffer::BufferHead");
    return new (buffer) RWBuffer::BufferHead(capacity);
  }

  void ref() const {
    auto old_ref_count = ref_count_.Increment();
    DCHECK_GT(old_ref_count, 0);
  }

  void unref() const {
    // A release here acts in place of all releases we "should" have been doing
    // in ref().
    if (!ref_count_.Decrement()) {
      // Like unique(), the acquire is only needed on success.
      RWBuffer::BufferBlock* block = block_.next_;

      // `buffer_` has a `raw_ptr` that needs to be destroyed to
      // properly lower the refcount.
      block_.~BufferBlock();
      WTF::Partitions::BufferFree(
          reinterpret_cast<void*>(const_cast<RWBuffer::BufferHead*>(this)));
      while (block) {
        RWBuffer::BufferBlock* next = block->next_;
        block->~BufferBlock();
        WTF::Partitions::BufferFree(block);
        block = next;
      }
    }
  }

  void Validate(size_t minUsed,
                const RWBuffer::BufferBlock* tail = nullptr) const {
#if DCHECK_IS_ON()
    DCHECK(!ref_count_.IsZero());
    size_t totalUsed = 0;
    const RWBuffer::BufferBlock* block = &block_;
    const RWBuffer::BufferBlock* lastBlock = block;
    while (block) {
      block->Validate();
      totalUsed += block->used_;
      lastBlock = block;
      block = block->next_;
    }
    DCHECK(minUsed <= totalUsed);
    if (tail) {
      DCHECK(tail == lastBlock);
    }
#endif
  }
};

size_t RWBuffer::ROIter::size() const {
  if (!block_) {
    return 0;
  }

  return std::min(block_->capacity_, remaining_);
}

RWBuffer::ROIter::ROIter(RWBuffer* rw_buffer, size_t available)
    : rw_buffer_(rw_buffer), remaining_(available) {
  DCHECK(rw_buffer_);
  block_ = &rw_buffer_->head_->block_;
}

const void* RWBuffer::ROIter::data() const {
  return remaining_ ? block_->startData() : nullptr;
}

bool RWBuffer::ROIter::Next() {
  if (remaining_) {
    size_t current_size = size();
    DCHECK_LE(current_size, remaining_);
    remaining_ -= current_size;
    if (remaining_ == 0) {
      block_ = nullptr;
    } else {
      block_ = block_->next_;
      DCHECK(block_);
    }
  }
  return remaining_ != 0;
}

bool RWBuffer::ROIter::HasNext() const {
  return block_ && block_->next_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// The reader can only access block.capacity_ (which never changes), and cannot
// access block.used_, which may be updated by the writer.
//
ROBuffer::ROBuffer(const RWBuffer::BufferHead* head,
                   size_t available,
                   const RWBuffer::BufferBlock* tail)
    : head_(head), available_(available), tail_(tail) {
  if (head) {
    head_->ref();
    DCHECK_GT(available, 0u);
    head->Validate(available, tail);
  } else {
    DCHECK_EQ(0u, available);
    DCHECK(!tail);
  }
}

ROBuffer::~ROBuffer() {
  if (head_) {
    tail_ = nullptr;
    head_.ExtractAsDangling()->unref();
  }
}

ROBuffer::Iter::Iter(const ROBuffer* buffer) {
  Reset(buffer);
}

ROBuffer::Iter::Iter(const scoped_refptr<ROBuffer>& buffer) {
  Reset(buffer.get());
}

void ROBuffer::Iter::Reset(const ROBuffer* buffer) {
  buffer_ = buffer;
  if (buffer && buffer->head_) {
    block_ = &buffer->head_->block_;
    remaining_ = buffer->available_;
  } else {
    block_ = nullptr;
    remaining_ = 0;
  }
}

const void* ROBuffer::Iter::data() const {
  return remaining_ ? block_->startData() : nullptr;
}

size_t ROBuffer::Iter::size() const {
  if (!block_) {
    return 0;
  }
  return std::min(block_->capacity_, remaining_);
}

bool ROBuffer::Iter::Next() {
  if (remaining_) {
    remaining_ -= size();
    if (buffer_->tail_ == block_) {
      // There are more blocks, but buffer_ does not know about them.
      DCHECK_EQ(0u, remaining_);
      block_ = nullptr;
    } else {
      block_ = block_->next_;
    }
  }
  return remaining_ != 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

RWBuffer::RWBuffer(size_t initial_capacity) {
  if (initial_capacity) {
    head_ = RWBuffer::BufferHead::Alloc(initial_capacity);
    tail_ = &head_->block_;
  }
}

RWBuffer::RWBuffer(base::OnceCallback<size_t(void*, size_t)> writer,
                   size_t initial_capacity) {
  if (initial_capacity) {
    head_ = RWBuffer::BufferHead::Alloc(initial_capacity);
    tail_ = &head_->block_;
  }

  size_t written = std::move(writer).Run(const_cast<void*>(tail_->startData()),
                                         initial_capacity);
  total_used_ += written;
  tail_->used_ += written;

  Validate();
}

RWBuffer::~RWBuffer() {
  Validate();
  if (head_) {
    tail_ = nullptr;
    head_.ExtractAsDangling()->unref();
  }
}

// It is important that we always completely fill the current block before
// spilling over to the next, since our reader will be using capacity_ (min'd
// against its total available) to know how many bytes to read from a given
// block.
//
void RWBuffer::Append(const void* src, size_t length, size_t reserve) {
  Validate();
  if (0 == length) {
    return;
  }

  total_used_ += length;

  if (!head_) {
    head_ = RWBuffer::BufferHead::Alloc(length + reserve);
    tail_ = &head_->block_;
  }

  size_t written = tail_->Append(src, length);
  DCHECK(written <= length);
  src = static_cast<const char*>(src) + written;
  length -= written;

  if (length) {
    auto* block = RWBuffer::BufferBlock::Alloc(length + reserve);
    tail_->next_ = block;
    tail_ = block;
    written = tail_->Append(src, length);
    DCHECK(written == length);
  }
  Validate();
}

scoped_refptr<ROBuffer> RWBuffer::MakeROBufferSnapshot() const {
  return AdoptRef(new ROBuffer(head_, total_used_, tail_));
}

bool RWBuffer::HasNoSnapshots() const {
  // Trivially, there are no other references to the underlying buffer, because
  // there is no underlying buffer.
  if (!head_) {
    return true;
  }

  return head_->ref_count_.IsOne();
}

void RWBuffer::Validate() const {
#if DCHECK_IS_ON()
  if (head_) {
    head_->Validate(total_used_, tail_);
  } else {
    DCHECK(!tail_);
    DCHECK_EQ(0u, total_used_);
  }
#endif
}

}  // namespace blink
