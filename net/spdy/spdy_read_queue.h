// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_READ_QUEUE_H_
#define NET_SPDY_SPDY_READ_QUEUE_H_

#include <cstddef>
#include <memory>

#include "base/containers/circular_deque.h"
#include "net/base/net_export.h"

namespace net {

class SpdyBuffer;

// A FIFO queue of incoming data from a SPDY connection. Useful for
// SpdyStream delegates.
class NET_EXPORT_PRIVATE SpdyReadQueue {
 public:
  SpdyReadQueue();

  SpdyReadQueue(const SpdyReadQueue&) = delete;
  SpdyReadQueue& operator=(const SpdyReadQueue&) = delete;

  ~SpdyReadQueue();

  // Returns whether there's anything in the queue.
  bool IsEmpty() const;

  // Returns the total number of bytes in the queue.
  size_t GetTotalSize() const;

  // Enqueues the bytes in |buffer|.
  void Enqueue(std::unique_ptr<SpdyBuffer> buffer);

  // Dequeues up to |len| (which must be positive) bytes into
  // |out|. Returns the number of bytes dequeued.
  size_t Dequeue(char* out, size_t len);

  // Removes all bytes from the queue.
  void Clear();

 private:
  // Class invariant:
  // |total_size_| is the sum of GetRemainingSize() of |queue_|'s elements.
  base::circular_deque<std::unique_ptr<SpdyBuffer>> queue_;
  size_t total_size_ = 0;
};

}  // namespace net

#endif  // NET_SPDY_SPDY_READ_QUEUE_H_
