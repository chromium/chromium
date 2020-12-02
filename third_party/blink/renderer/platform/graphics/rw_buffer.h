// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_RW_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_RW_BUFFER_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkStreamAsset;

namespace blink {

class ROBuffer;

/**
 * Accumulates bytes of memory that are "appended" to it, growing internal
 * storage as needed. The growth is done such that at any time in the writer's
 * thread, an ROBuffer or StreamAsset can be snapped off (and safely passed to
 * another thread). The ROBuffer/StreamAsset snapshot can see the previously
 * stored bytes, but will be unaware of any future writes.
 */
class PLATFORM_EXPORT RWBuffer {
 public:
  struct BufferHead;
  struct BufferBlock;

  explicit RWBuffer(size_t initialCapacity = 0);
  ~RWBuffer();

  RWBuffer& operator=(const RWBuffer&) = delete;
  RWBuffer(const RWBuffer&) = delete;

  size_t size() const { return total_used_; }

  /**
   * Append |length| bytes from |buffer|.
   *
   * If the caller knows in advance how much more data they are going to
   * append, they can pass a |reserve| hint (representing the number of upcoming
   * bytes *in addition* to the current append), to minimize the number of
   * internal allocations.
   */
  void Append(const void* buffer, size_t length, size_t reserve = 0);

  sk_sp<ROBuffer> MakeROBufferSnapshot() const;

  std::unique_ptr<SkStreamAsset> MakeStreamSnapshot() const;

  void Validate() const;

 private:
  BufferHead* head_ = nullptr;
  BufferBlock* tail_ = nullptr;
  size_t total_used_ = 0;
};

/**
 * Contains a read-only, thread-sharable block of memory. To access the memory,
 * the caller must instantiate a local iterator, as the memory is stored in 1 or
 * more contiguous blocks.
 */
class PLATFORM_EXPORT ROBuffer : public SkRefCnt {
 public:
  /**
   * Return the logical length of the data owned/shared by this buffer. It may
   * be stored in multiple contiguous blocks, accessible via the iterator.
   */
  size_t size() const { return available_; }

  class PLATFORM_EXPORT Iter {
   public:
    explicit Iter(const ROBuffer*);
    explicit Iter(const sk_sp<ROBuffer>&);

    void Reset(const ROBuffer*);

    /**
     * Return the current continuous block of memory, or nullptr if the
     * iterator is exhausted
     */
    const void* data() const;

    /**
     * Returns the number of bytes in the current contiguous block of memory,
     * or 0 if the iterator is exhausted.
     */
    size_t size() const;

    /**
     * Advance to the next contiguous block of memory, returning true if there
     * is another block, or false if the iterator is exhausted.
     */
    bool Next();

   private:
    const RWBuffer::BufferBlock* block_;
    size_t remaining_;
    const ROBuffer* buffer_;
  };

 private:
  ROBuffer(const RWBuffer::BufferHead* head,
           size_t available,
           const RWBuffer::BufferBlock* tail);
  ~ROBuffer() override;

  const RWBuffer::BufferHead* head_;
  const size_t available_;
  const RWBuffer::BufferBlock* tail_;

  friend class RWBuffer;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_RW_BUFFER_H_
