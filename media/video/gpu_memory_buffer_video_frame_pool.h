// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
#define MEDIA_VIDEO_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "media/base/video_frame.h"

namespace base {
class TickClock;
}

namespace media {
class GpuVideoAcceleratorFactories;

// Interface to a pool of MappableSI/textures/images that can be used to
// transform software VideoFrames to VideoFrames backed by native textures.
// The resources used by the VideoFrame created by the pool will be
// automatically put back into the pool once the frame is destroyed.
// The pool recycles resources to a void unnecessarily allocating and
// destroying textures, images and MappableSI that could result
// in a round trip to the browser/GPU process.
//
// NOTE: While destroying the pool will abort any uncompleted copies, it will
// not immediately invalidate outstanding video frames. GPU memory buffers will
// be kept alive by video frames indirectly referencing them. Video frames
// themselves are ref-counted and will be released when they are no longer
// needed, potentially after the pool is destroyed.
class MEDIA_EXPORT GpuMemoryBufferVideoFramePool {
 public:
  GpuMemoryBufferVideoFramePool();
  GpuMemoryBufferVideoFramePool(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      GpuVideoAcceleratorFactories* gpu_factories);

  GpuMemoryBufferVideoFramePool(const GpuMemoryBufferVideoFramePool&) = delete;
  GpuMemoryBufferVideoFramePool& operator=(
      const GpuMemoryBufferVideoFramePool&) = delete;

  virtual ~GpuMemoryBufferVideoFramePool();

  // Callback used by MaybeCreateHardwareFrame to deliver a new VideoFrame
  // after it has been copied to MappableSI.
  using FrameReadyCB = base::OnceCallback<void(scoped_refptr<VideoFrame>)>;

  // Calls |cb| with a new VideoFrame containing only mailbox to native
  // resource. The content of the new object is copied from the
  // software-allocated |video_frame|.
  //
  // If it's not possible to create a new hardware VideoFrame, |video_frame|
  // itself will passed to |cb|.
  virtual void MaybeCreateHardwareFrame(scoped_refptr<VideoFrame> video_frame,
                                        FrameReadyCB frame_ready_cb);

  // Aborts any pending copies. Previously provided |frame_ready_cb| callbacks
  // may still be called if the copy has already started.
  virtual void Abort();

  // Allows injection of a base::SimpleTestClock for testing.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  // This is currently used to suppress some tests when MappableSI is
  // enabled.
  bool IsMappableSIEnabledForTesting() const;

 private:
  class PoolImpl;
  scoped_refptr<PoolImpl> pool_impl_;
};

}  // namespace media

#endif  // MEDIA_VIDEO_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
