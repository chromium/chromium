// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_BUFFER_QUEUE_H_
#define MEDIA_BASE_DECODER_BUFFER_QUEUE_H_

#include <stddef.h>

#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

class DecoderBuffer;

// Maintains a queue of DecoderBuffers in increasing timestamp order.
//
// Individual buffer durations are ignored when calculating the duration of the
// queue i.e., the queue must have at least 2 in-order buffers to calculate
// duration.
//
// Not thread safe: access must be externally synchronized.
class MEDIA_EXPORT DecoderBufferQueue {
 public:
  DecoderBufferQueue();

  DecoderBufferQueue(const DecoderBufferQueue&) = delete;
  DecoderBufferQueue& operator=(const DecoderBufferQueue&) = delete;

  ~DecoderBufferQueue();

  // Push |buffer| to the end of the queue. If |buffer| is queued out of order
  // it will be excluded from duration calculations.
  //
  // It is invalid to push an end-of-stream |buffer|.
  void Push(scoped_refptr<DecoderBuffer> buffer);

  // Pops a DecoderBuffer from the front of the queue.
  //
  // It is invalid to call Pop() on an empty queue.
  scoped_refptr<DecoderBuffer> Pop();

  // Removes all queued buffers.
  void Clear();

  // Returns true if this queue is empty.
  bool IsEmpty();

  // Returns the duration of encoded data stored in this queue as measured by
  // the timestamps of the earliest and latest buffers, ignoring out of order
  // buffers.
  //
  // Returns zero if the queue is empty.
  base::TimeDelta Duration();

  // Returns the total memory occupied by this class and the buffers it holds,
  // including the bookkeeping data and buffered data. For simplicity, the
  // bookkeeping data of this class itself isn't included as it's relatively
  // small compared to the other data.
  size_t memory_usage_in_bytes() const { return memory_usage_in_bytes_; }

  // Returns the number of buffers in the queue.
  size_t queue_size() const { return queue_.size(); }

 private:
  using Queue = base::circular_deque<scoped_refptr<DecoderBuffer>>;
  Queue queue_;

  // A subset of |queue_| that contains buffers that are in strictly
  // increasing timestamp order. Used to calculate Duration() while ignoring
  // out-of-order buffers.
  Queue in_order_queue_;

  base::TimeDelta earliest_valid_timestamp_;

  // Total memory usage in bytes for buffers in the queue.
  size_t memory_usage_in_bytes_ = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_QUEUE_H_
