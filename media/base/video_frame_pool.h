// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_POOL_H_
#define MEDIA_BASE_VIDEO_FRAME_POOL_H_

#include <stddef.h>

#include "media/base/media_export.h"
#include "media/base/video_frame.h"

namespace base {
class TickClock;
}

namespace media {

// Simple VideoFrame pool used to avoid unnecessarily allocating and destroying
// VideoFrame objects. The pool manages the memory for the VideoFrame
// returned by CreateFrame(). When one of these VideoFrames is destroyed,
// the memory is returned to the pool for use by a subsequent CreateFrame()
// call. The memory in the pool is retained for the life of the
// VideoFramePool object. If the parameters passed to CreateFrame() change
// during the life of this object, then the memory used by frames with the old
// parameter values will be purged from the pool.
class MEDIA_EXPORT VideoFramePool {
 public:
  VideoFramePool();
  VideoFramePool(const VideoFramePool&) = delete;
  VideoFramePool& operator=(const VideoFramePool&) = delete;
  ~VideoFramePool();

  // Returns a frame from the pool that matches the specified
  // parameters or creates a new frame if no suitable frame exists in
  // the pool. The pool is drained if no matching frame is found.
  // The buffer for the new frame will be zero initialized.  Reused frames will
  // not be zero initialized.
  scoped_refptr<VideoFrame> CreateFrame(VideoPixelFormat format,
                                        const gfx::Size& coded_size,
                                        const gfx::Rect& visible_rect,
                                        const gfx::Size& natural_size,
                                        base::TimeDelta timestamp);

 protected:
  friend class VideoFramePoolTest;

  // Returns the number of frames in the pool for testing purposes.
  size_t GetPoolSizeForTesting() const;

  // Allows injection of a base::SimpleTestClock for testing.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

 private:
  class PoolImpl;
  scoped_refptr<PoolImpl> pool_;
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_POOL_H_
