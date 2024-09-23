// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_DMABUF_VIDEO_FRAME_POOL_H_
#define MEDIA_GPU_CHROMEOS_DMABUF_VIDEO_FRAME_POOL_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/status.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/chromeos_status.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/gpu_buffer_layout.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class FrameResource;

// Forward declare for use in AsPlatformVideoFramePool.
class PlatformVideoFramePool;

// Interface for allocating and managing DMA-buf frames. The client should
// set a task runner first via set_parent_task_runner(), and guarantee that
// Initialize(), GetFrame(), GetGpuBufferLayout() and the destructor are
// executed on this task runner. Note: other public methods might be called at
// different thread. The implementation must be thread-safe.
class MEDIA_GPU_EXPORT DmabufVideoFramePool {
 public:
  using CreateFrameCB =
      base::RepeatingCallback<CroStatus::Or<scoped_refptr<FrameResource>>(
          VideoPixelFormat,
          const gfx::Size&,
          const gfx::Rect&,
          const gfx::Size&,
          bool,
          bool,
          bool,
          base::TimeDelta)>;

  DmabufVideoFramePool();
  virtual ~DmabufVideoFramePool();

  // Setter method of |parent_task_runner_|. GetFrame() and destructor method
  // should be called at |parent_task_runner_|.
  // This method must be called only once before any GetFrame() is called.
  virtual void set_parent_task_runner(
      scoped_refptr<base::SequencedTaskRunner> parent_task_runner);

  // Allows downcasting to an implementation of DmabufVideoFramePool safely
  // since it has custom behavior that VaapiVideoDecoder needs to take
  // advantage of.
  virtual PlatformVideoFramePool* AsPlatformVideoFramePool();

  // Sets the parameters of allocating frames and the maximum number of frames
  // which can be allocated.
  // Returns a valid GpuBufferLayout if the initialization is successful,
  // otherwise returns any given error from the set of CroStatus::Codes.
  virtual CroStatus::Or<GpuBufferLayout> Initialize(
      const Fourcc& fourcc,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      size_t max_num_frames,
      bool use_protected,
      bool use_linear_buffers = false) = 0;

  // Returns a frame from the pool with the layout that is returned by the
  // previous Initialize() method and zero timestamp. Returns nullptr if the
  // pool is exhausted.
  virtual scoped_refptr<FrameResource> GetFrame() = 0;

  // Returns the storage type of frames that GetFrame() returns.
  virtual VideoFrame::StorageType GetFrameStorageType() const = 0;

  // Checks whether the pool is exhausted. This happens when the pool reached
  // its maximum size and all frames are in use. Calling GetFrame() when the
  // pool is exhausted will return a nullptr.
  virtual bool IsExhausted() = 0;

  // Set the callback for notifying when the pool is no longer exhausted. The
  // callback will be called on |parent_task_runner_|. Note: if there is a
  // pending callback when calling NotifyWhenFrameAvailable(), the old callback
  // would be dropped immediately.
  virtual void NotifyWhenFrameAvailable(base::OnceClosure cb) = 0;

  // Invoke to cause the pool to release all the frames it has allocated before
  // which will cause new ones to be allocated. This method must be called on
  // |parent_task_runner_| because it may invalidate weak ptrs.
  virtual void ReleaseAllFrames() = 0;

  // Detailed information of the allocated GpuBufferLayout. Only valid after a
  // successful Initialize() call, otherwise returns std::nullopt.
  virtual std::optional<GpuBufferLayout> GetGpuBufferLayout() = 0;

  // Returns true if and only if the pool is a mock pool used for testing.
  virtual bool IsFakeVideoFramePool();

 protected:
  scoped_refptr<base::SequencedTaskRunner> parent_task_runner_;
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_DMABUF_VIDEO_FRAME_POOL_H_
