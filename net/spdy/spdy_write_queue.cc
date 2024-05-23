// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/spdy/spdy_write_queue.h"

#include <cstddef>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/circular_deque.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "net/spdy/spdy_buffer.h"
#include "net/spdy/spdy_buffer_producer.h"
#include "net/spdy/spdy_stream.h"

namespace net {

bool IsSpdyFrameTypeWriteCapped(spdy::SpdyFrameType frame_type) {
  return frame_type == spdy::SpdyFrameType::RST_STREAM ||
         frame_type == spdy::SpdyFrameType::SETTINGS ||
         frame_type == spdy::SpdyFrameType::WINDOW_UPDATE ||
         frame_type == spdy::SpdyFrameType::PING ||
         frame_type == spdy::SpdyFrameType::GOAWAY;
}

SpdyWriteQueue::PendingWrite::PendingWrite() = default;

SpdyWriteQueue::PendingWrite::PendingWrite(
    spdy::SpdyFrameType frame_type,
    std::unique_ptr<SpdyBufferProducer> frame_producer,
    const base::WeakPtr<SpdyStream>& stream,
    const MutableNetworkTrafficAnnotationTag& traffic_annotation)
    : frame_type(frame_type),
      frame_producer(std::move(frame_producer)),
      stream(stream),
      traffic_annotation(traffic_annotation),
      has_stream(stream.get() != nullptr) {}

SpdyWriteQueue::PendingWrite::~PendingWrite() = default;

SpdyWriteQueue::PendingWrite::PendingWrite(PendingWrite&& other) = default;
SpdyWriteQueue::PendingWrite& SpdyWriteQueue::PendingWrite::operator=(
    PendingWrite&& other) = default;

SpdyWriteQueue::SpdyWriteQueue() = default;

SpdyWriteQueue::~SpdyWriteQueue() {
  DCHECK_GE(num_queued_capped_frames_, 0);
  Clear();
}

bool SpdyWriteQueue::IsEmpty() const {
  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; i++) {
    if (!queue_[i].empty())
      return false;
  }
  return true;
}

void SpdyWriteQueue::Enqueue(
    RequestPriority priority,
    spdy::SpdyFrameType frame_type,
    std::unique_ptr<SpdyBufferProducer> frame_producer,
    const base::WeakPtr<SpdyStream>& stream,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  CHECK(!removing_writes_);
  CHECK_GE(priority, MINIMUM_PRIORITY);
  CHECK_LE(priority, MAXIMUM_PRIORITY);
  if (stream.get())
    DCHECK_EQ(stream->priority(), priority);
  queue_[priority].push_back(
      {frame_type, std::move(frame_producer), stream,
       MutableNetworkTrafficAnnotationTag(traffic_annotation)});
  if (IsSpdyFrameTypeWriteCapped(frame_type)) {
    DCHECK_GE(num_queued_capped_frames_, 0);
    num_queued_capped_frames_++;
  }
}

bool SpdyWriteQueue::Dequeue(
    spdy::SpdyFrameType* frame_type,
    std::unique_ptr<SpdyBufferProducer>* frame_producer,
    base::WeakPtr<SpdyStream>* stream,
    MutableNetworkTrafficAnnotationTag* traffic_annotation) {
  CHECK(!removing_writes_);
  for (int i = MAXIMUM_PRIORITY; i >= MINIMUM_PRIORITY; --i) {
    if (!queue_[i].empty()) {
      PendingWrite pending_write = std::move(queue_[i].front());
      queue_[i].pop_front();
      *frame_type = pending_write.frame_type;
      *frame_producer = std::move(pending_write.frame_producer);
      *stream = pending_write.stream;
      *traffic_annotation = pending_write.traffic_annotation;
      if (pending_write.has_stream)
        DCHECK(stream->get());
      if (IsSpdyFrameTypeWriteCapped(*frame_type)) {
        num_queued_capped_frames_--;
        DCHECK_GE(num_queued_capped_frames_, 0);
      }
      return true;
    }
  }
  return false;
}

void SpdyWriteQueue::RemovePendingWritesForStream(SpdyStream* stream) {
  CHECK(!removing_writes_);
  removing_writes_ = true;
  RequestPriority priority = stream->priority();
  CHECK_GE(priority, MINIMUM_PRIORITY);
  CHECK_LE(priority, MAXIMUM_PRIORITY);

#if DCHECK_IS_ON()
  // |stream| should not have pending writes in a queue not matching
  // its priority.
  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; ++i) {
    if (priority == i)
      continue;
    for (auto it = queue_[i].begin(); it != queue_[i].end(); ++it)
      DCHECK_NE(it->stream.get(), stream);
  }
#endif

  // Defer deletion until queue iteration is complete, as
  // SpdyBuffer::~SpdyBuffer() can result in callbacks into SpdyWriteQueue.
  std::vector<std::unique_ptr<SpdyBufferProducer>> erased_buffer_producers;
  base::circular_deque<PendingWrite>& queue = queue_[priority];
  for (auto it = queue.begin(); it != queue.end();) {
    if (it->stream.get() == stream) {
      if (IsSpdyFrameTypeWriteCapped(it->frame_type)) {
        num_queued_capped_frames_--;
        DCHECK_GE(num_queued_capped_frames_, 0);
      }
      erased_buffer_producers.push_back(std::move(it->frame_producer));
      it = queue.erase(it);
    } else {
      ++it;
    }
  }
  removing_writes_ = false;

  // Iteration on |queue| is completed.  Now |erased_buffer_producers| goes out
  // of scope, SpdyBufferProducers are destroyed.
}

void SpdyWriteQueue::RemovePendingWritesForStreamsAfter(
    spdy::SpdyStreamId last_good_stream_id) {
  CHECK(!removing_writes_);
  removing_writes_ = true;

  // Defer deletion until queue iteration is complete, as
  // SpdyBuffer::~SpdyBuffer() can result in callbacks into SpdyWriteQueue.
  std::vector<std::unique_ptr<SpdyBufferProducer>> erased_buffer_producers;
  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; ++i) {
    base::circular_deque<PendingWrite>& queue = queue_[i];
    for (auto it = queue.begin(); it != queue.end();) {
      if (it->stream.get() && (it->stream->stream_id() > last_good_stream_id ||
                               it->stream->stream_id() == 0)) {
        if (IsSpdyFrameTypeWriteCapped(it->frame_type)) {
          num_queued_capped_frames_--;
          DCHECK_GE(num_queued_capped_frames_, 0);
        }
        erased_buffer_producers.push_back(std::move(it->frame_producer));
        it = queue.erase(it);
      } else {
        ++it;
      }
    }
  }
  removing_writes_ = false;

  // Iteration on each |queue| is completed.  Now |erased_buffer_producers| goes
  // out of scope, SpdyBufferProducers are destroyed.
}

void SpdyWriteQueue::ChangePriorityOfWritesForStream(
    SpdyStream* stream,
    RequestPriority old_priority,
    RequestPriority new_priority) {
  CHECK(!removing_writes_);
  DCHECK(stream);

#if DCHECK_IS_ON()
  // |stream| should not have pending writes in a queue not matching
  // |old_priority|.
  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; ++i) {
    if (i == old_priority)
      continue;
    for (auto it = queue_[i].begin(); it != queue_[i].end(); ++it)
      DCHECK_NE(it->stream.get(), stream);
  }
#endif

  base::circular_deque<PendingWrite>& old_queue = queue_[old_priority];
  base::circular_deque<PendingWrite>& new_queue = queue_[new_priority];
  for (auto it = old_queue.begin(); it != old_queue.end();) {
    if (it->stream.get() == stream) {
      new_queue.push_back(std::move(*it));
      it = old_queue.erase(it);
    } else {
      ++it;
    }
  }
}

void SpdyWriteQueue::Clear() {
  CHECK(!removing_writes_);
  removing_writes_ = true;
  std::vector<std::unique_ptr<SpdyBufferProducer>> erased_buffer_producers;

  for (int i = MINIMUM_PRIORITY; i <= MAXIMUM_PRIORITY; ++i) {
    for (auto& pending_write : queue_[i]) {
      erased_buffer_producers.push_back(
          std::move(pending_write.frame_producer));
    }
    queue_[i].clear();
  }
  removing_writes_ = false;
  num_queued_capped_frames_ = 0;
}

}  // namespace net
