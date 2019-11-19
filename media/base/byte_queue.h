// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BYTE_QUEUE_H_
#define MEDIA_BASE_BYTE_QUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "media/base/media_export.h"

namespace media {

// Represents a queue of bytes. Data is added to the end of the queue via an
// Push() call and removed via Pop(). The contents of the queue can be observed
// via the Peek() method.
//
// This class manages the underlying storage of the queue and tries to minimize
// the number of buffer copies when data is appended and removed.
class MEDIA_EXPORT ByteQueue {
 public:
  ByteQueue();
  ~ByteQueue();

  // Reset the queue to the empty state.
  void Reset();

  // Appends new bytes onto the end of the queue.
  void Push(const uint8_t* data, int size);

  // Get a pointer to the front of the queue and the queue size. These values
  // are only valid until the next Push() or Pop() call.
  void Peek(const uint8_t** data, int* size) const;

  // Remove |count| bytes from the front of the queue.
  void Pop(int count);

 private:
  // Default starting size for the queue.
  enum { kDefaultQueueSize = 1024 };

  // Returns a pointer to the front of the queue.
  uint8_t* Front() const;

  // Size of |buffer_|.
  size_t size_ = kDefaultQueueSize;

  // Offset from the start of |buffer_| that marks the front of the queue.
  size_t offset_ = 0u;

  // Number of bytes stored in |buffer_|.
  int used_ = 0;

  std::unique_ptr<uint8_t[]> buffer_;

  DISALLOW_COPY_AND_ASSIGN(ByteQueue);
};

}  // namespace media

#endif  // MEDIA_BASE_BYTE_QUEUE_H_
