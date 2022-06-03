// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_FRAME_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_FRAME_QUEUE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

// Implements a thread-safe circular queue.
template <typename NativeFrameType>
class FrameQueue
    : public WTF::ThreadSafeRefCounted<FrameQueue<NativeFrameType>> {
 public:
  explicit FrameQueue(wtf_size_t max_size)
      : max_size_(std::max(1u, max_size)) {}

  Mutex& GetMutex() { return lock_; }

  absl::optional<NativeFrameType> Push(NativeFrameType frame) {
    MutexLocker locker_(lock_);
    return PushLocked(std::move(frame));
  }

  absl::optional<NativeFrameType> PushLocked(NativeFrameType frame) {
    lock_.AssertAcquired();
    absl::optional<NativeFrameType> ret;
    if (queue_.size() == max_size_)
      ret = queue_.TakeFirst();
    queue_.push_back(std::move(frame));
    return ret;
  }

  absl::optional<NativeFrameType> Pop() {
    MutexLocker locker_(lock_);
    return PopLocked();
  }

  absl::optional<NativeFrameType> PopLocked() {
    lock_.AssertAcquired();
    if (queue_.empty())
      return absl::nullopt;
    return queue_.TakeFirst();
  }

  absl::optional<NativeFrameType> PeekLocked() {
    lock_.AssertAcquired();
    if (queue_.empty())
      return absl::nullopt;
    return queue_.front();
  }

  bool IsEmpty() {
    MutexLocker locker_(lock_);
    return IsEmptyLocked();
  }

  bool IsEmptyLocked() {
    lock_.AssertAcquired();
    return queue_.empty();
  }

  wtf_size_t MaxSize() const { return max_size_; }

  void Clear() {
    MutexLocker locker_(lock_);
    queue_.clear();
  }

 private:
  Mutex lock_;
  Deque<NativeFrameType> queue_ GUARDED_BY(lock_);
  const wtf_size_t max_size_;
};

// Wrapper that allows sharing a single FrameQueue reference across multiple
// threads.
template <typename NativeFrameType>
class FrameQueueHandle {
 public:
  explicit FrameQueueHandle(
      scoped_refptr<FrameQueue<NativeFrameType>> frame_queue)
      : queue_(std::move(frame_queue)) {
    DCHECK(queue_);
  }
  FrameQueueHandle(const FrameQueueHandle&) = delete;
  FrameQueueHandle& operator=(const FrameQueueHandle&) = delete;

  ~FrameQueueHandle() { Invalidate(); }

  // Returns a copy of |queue_|, which should be checked for nullity and
  // re-used throughout the scope of a function call, instead of calling
  // Queue() multiple times.  Otherwise the queue could be destroyed
  // between calls.
  scoped_refptr<FrameQueue<NativeFrameType>> Queue() const {
    MutexLocker locker(mutex_);
    return queue_;
  }

  // Releases the internal FrameQueue reference.
  void Invalidate() {
    MutexLocker locker(mutex_);
    queue_.reset();
  }

 private:
  mutable Mutex mutex_;
  scoped_refptr<FrameQueue<NativeFrameType>> queue_ GUARDED_BY(mutex_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_STREAM_TEST_UTILS_H_
