// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BYTE_QUEUE_H_
#define MEDIA_BASE_BYTE_QUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/process/memory.h"
#include "media/base/media_export.h"

namespace media {

// Represents a queue of bytes. Data is added to the end of the queue via an
// Push() call and removed via Pop(). The contents of the queue can be observed
// via the Data() method.
//
// This class manages the underlying storage of the queue and tries to minimize
// the number of buffer copies when data is appended and removed.
class MEDIA_EXPORT ByteQueue {
 public:
  ByteQueue();

  ByteQueue(const ByteQueue&) = delete;
  ByteQueue& operator=(const ByteQueue&) = delete;

  ~ByteQueue();

  // Reset the queue to the empty state.
  void Reset();

  // Appends new bytes onto the end of the queue. If allocation failure occurs,
  // then the append of `data` is not done and returns false. Otherwise, returns
  // true.
  [[nodiscard]] bool Push(base::span<const uint8_t> data);

  // Remove |count| bytes from the front of the queue.
  void Pop(int count);

  // Get a read-only span view of the data. This is only valid until the next
  // Push() or Pop() call.
  base::span<const uint8_t> Data() const { return data_; }

 private:
  // Offset from the start of |buffer_| that marks the front of the queue.
  size_t offset_ = 0u;

  base::HeapArray<uint8_t, base::UncheckedFreeDeleter> buffer_;

  // Points to a certain piece of data in the `buffer_`
  base::raw_span<uint8_t> data_;
};

}  // namespace media

#endif  // MEDIA_BASE_BYTE_QUEUE_H_
