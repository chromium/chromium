// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_pool.h"

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"

namespace media {

class VideoFramePool::PoolImpl
    : public base::RefCountedThreadSafe<VideoFramePool::PoolImpl> {
 public:
  PoolImpl();
  PoolImpl(const PoolImpl&) = delete;
  PoolImpl& operator=(const PoolImpl&) = delete;

  // See VideoFramePool::CreateFrame() for usage. Attempts to keep |frames_| in
  // LRU order by always pulling from the back of |frames_|.
  scoped_refptr<VideoFrame> CreateFrame(VideoPixelFormat format,
                                        const gfx::Size& coded_size,
                                        const gfx::Rect& visible_rect,
                                        const gfx::Size& natural_size,
                                        base::TimeDelta timestamp);

  // Shuts down the frame pool and releases all frames in |frames_|.
  // Once this is called frames will no longer be inserted back into
  // |frames_|.
  void Shutdown();

  size_t get_pool_size_for_testing() {
    base::AutoLock auto_lock(lock_);
    return frames_.size();
  }

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 private:
  friend class base::RefCountedThreadSafe<VideoFramePool::PoolImpl>;
  ~PoolImpl();

  // Called when the frame wrapper gets destroyed. |frame| is the actual frame
  // that was wrapped and is placed in |frames_| by this function so it can be
  // reused. This will attempt to expire frames that haven't been used in some
  // time. It relies on |frames_| being in LRU order with the front being the
  // least recently used entry.
  void FrameReleased(scoped_refptr<VideoFrame> frame);

  base::Lock lock_;
  bool is_shutdown_ GUARDED_BY(lock_) = false;

  struct FrameEntry {
    base::TimeTicks last_use_time;
    scoped_refptr<VideoFrame> frame;
  };

  base::circular_deque<FrameEntry> frames_ GUARDED_BY(lock_);

  // |tick_clock_| is always a DefaultTickClock outside of testing.
  raw_ptr<const base::TickClock> tick_clock_;
};

VideoFramePool::PoolImpl::PoolImpl()
    : tick_clock_(base::DefaultTickClock::GetInstance()) {}

VideoFramePool::PoolImpl::~PoolImpl() {
  DCHECK(is_shutdown_);
}

scoped_refptr<VideoFrame> VideoFramePool::PoolImpl::CreateFrame(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!is_shutdown_);

  scoped_refptr<VideoFrame> frame;
  while (!frames_.empty()) {
    scoped_refptr<VideoFrame> pool_frame = std::move(frames_.back().frame);
    frames_.pop_back();

    if (pool_frame->IsSameAllocation(format, coded_size, visible_rect,
                                     natural_size)) {
      frame = pool_frame;
      frame->set_timestamp(timestamp);
      frame->clear_metadata();
      break;
    }
  }

  if (!frame) {
    frame = VideoFrame::CreateZeroInitializedFrame(
        format, coded_size, visible_rect, natural_size, timestamp);
    // This can happen if the arguments are not valid.
    if (!frame) {
      LOG(ERROR) << "Failed to create a video frame";
      return nullptr;
    }
  }

  scoped_refptr<VideoFrame> wrapped_frame = VideoFrame::WrapVideoFrame(
      frame, frame->format(), frame->visible_rect(), frame->natural_size());
  wrapped_frame->AddDestructionObserver(base::BindOnce(
      &VideoFramePool::PoolImpl::FrameReleased, this, std::move(frame)));
  return wrapped_frame;
}

void VideoFramePool::PoolImpl::Shutdown() {
  base::AutoLock auto_lock(lock_);
  is_shutdown_ = true;
  frames_.clear();
}

void VideoFramePool::PoolImpl::FrameReleased(scoped_refptr<VideoFrame> frame) {
  base::AutoLock auto_lock(lock_);
  if (is_shutdown_)
    return;

  const base::TimeTicks now = tick_clock_->NowTicks();
  frames_.push_back({now, std::move(frame)});

  // After this loop, |stale_index| is the index of the oldest non-stale frame.
  // Such an index must exist because |frame| is never stale.
  int stale_index = -1;
  constexpr base::TimeDelta kStaleFrameLimit = base::Seconds(10);
  while (now - frames_[++stale_index].last_use_time > kStaleFrameLimit) {
    // Last frame should never be included since we just added it.
    DCHECK_LE(static_cast<size_t>(stale_index), frames_.size());
  }

  if (stale_index)
    frames_.erase(frames_.begin(), frames_.begin() + stale_index);
}

VideoFramePool::VideoFramePool() : pool_(new PoolImpl()) {}

VideoFramePool::~VideoFramePool() {
  pool_->Shutdown();
}

scoped_refptr<VideoFrame> VideoFramePool::CreateFrame(
    VideoPixelFormat format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  return pool_->CreateFrame(format, coded_size, visible_rect, natural_size,
                            timestamp);
}

size_t VideoFramePool::GetPoolSizeForTesting() const {
  return pool_->get_pool_size_for_testing();
}

void VideoFramePool::SetTickClockForTesting(const base::TickClock* tick_clock) {
  pool_->set_tick_clock_for_testing(tick_clock);
}

}  // namespace media
