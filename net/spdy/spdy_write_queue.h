// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_WRITE_QUEUE_H_
#define NET_SPDY_SPDY_WRITE_QUEUE_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

// Returns whether this frame type is subject to caps on how many
// frames can be queued at any given time.
NET_EXPORT_PRIVATE bool IsSpdyFrameTypeWriteCapped(
    spdy::SpdyFrameType frame_type);

class SpdyBufferProducer;
class SpdyStream;

// A queue of SpdyBufferProducers to produce frames to write. Ordered
// by priority, and then FIFO.
class NET_EXPORT_PRIVATE SpdyWriteQueue {
 public:
  SpdyWriteQueue();

  SpdyWriteQueue(const SpdyWriteQueue&) = delete;
  SpdyWriteQueue& operator=(const SpdyWriteQueue&) = delete;

  ~SpdyWriteQueue();

  // Returns whether there is anything in the write queue,
  // i.e. whether the next call to Dequeue will return true.
  bool IsEmpty() const;

  // Enqueues the given frame producer of the given type at the given
  // priority associated with the given stream, which may be NULL if
  // the frame producer is not associated with a stream. If |stream|
  // is non-NULL, its priority must be equal to |priority|, and it
  // must remain non-NULL until the write is dequeued or removed.
  void Enqueue(RequestPriority priority,
               spdy::SpdyFrameType frame_type,
               std::unique_ptr<SpdyBufferProducer> frame_producer,
               const base::WeakPtr<SpdyStream>& stream,
               const NetworkTrafficAnnotationTag& traffic_annotation);

  // Dequeues the frame producer with the highest priority that was
  // enqueued the earliest and its associated stream. Returns true and
  // fills in |frame_type|, |frame_producer|, and |stream| if
  // successful -- otherwise, just returns false.
  bool Dequeue(spdy::SpdyFrameType* frame_type,
               std::unique_ptr<SpdyBufferProducer>* frame_producer,
               base::WeakPtr<SpdyStream>* stream,
               MutableNetworkTrafficAnnotationTag* traffic_annotation);

  // Removes all pending writes for the given stream, which must be
  // non-NULL.
  void RemovePendingWritesForStream(SpdyStream* stream);

  // Removes all pending writes for streams after |last_good_stream_id|
  // and streams with no stream id.
  void RemovePendingWritesForStreamsAfter(
      spdy::SpdyStreamId last_good_stream_id);

  // Change priority of all pending writes for the given stream.  Frames will be
  // queued after other writes with |new_priority|.
  void ChangePriorityOfWritesForStream(SpdyStream* stream,
                                       RequestPriority old_priority,
                                       RequestPriority new_priority);

  // Removes all pending writes.
  void Clear();

  // Returns the number of currently queued capped frames including all
  // priorities.
  int num_queued_capped_frames() const { return num_queued_capped_frames_; }

 private:
  // A struct holding a frame producer and its associated stream.
  struct PendingWrite {
    spdy::SpdyFrameType frame_type;
    std::unique_ptr<SpdyBufferProducer> frame_producer;
    base::WeakPtr<SpdyStream> stream;
    MutableNetworkTrafficAnnotationTag traffic_annotation;
    // Whether |stream| was non-NULL when enqueued.
    bool has_stream;

    PendingWrite();
    PendingWrite(spdy::SpdyFrameType frame_type,
                 std::unique_ptr<SpdyBufferProducer> frame_producer,
                 const base::WeakPtr<SpdyStream>& stream,
                 const MutableNetworkTrafficAnnotationTag& traffic_annotation);

    PendingWrite(const PendingWrite&) = delete;
    PendingWrite& operator=(const PendingWrite&) = delete;

    PendingWrite(PendingWrite&& other);
    PendingWrite& operator=(PendingWrite&& other);

    ~PendingWrite();
  };

  bool removing_writes_ = false;

  // Number of currently queued capped frames including all priorities.
  int num_queued_capped_frames_ = 0;

  // The actual write queue, binned by priority.
  base::circular_deque<PendingWrite> queue_[NUM_PRIORITIES];
};

}  // namespace net

#endif  // NET_SPDY_SPDY_WRITE_QUEUE_H_
