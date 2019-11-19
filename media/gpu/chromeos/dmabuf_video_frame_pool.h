// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_DMABUF_VIDEO_FRAME_POOL_H_
#define MEDIA_GPU_CHROMEOS_DMABUF_VIDEO_FRAME_POOL_H_

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/gpu_buffer_layout.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Interface for allocating and managing DMA-buf VideoFrame. The client should
// set a task runner first, and guarantee both GetFrame() and the destructor are
// executed on this task runner.
// Note: other public methods might be called at different thread. The
// implementation must be thread-safe.
class MEDIA_GPU_EXPORT DmabufVideoFramePool {
 public:
  using DmabufId = const std::vector<base::ScopedFD>*;

  // Get the identifier of Dmabuf-backed |frame|. Calling this method with the
  // frames backed by the same Dmabuf should return the same result.
  static DmabufId GetDmabufId(const VideoFrame& frame);

  DmabufVideoFramePool();
  virtual ~DmabufVideoFramePool();

  // Setter method of |parent_task_runner_|. GetFrame() and destructor method
  // should be called at |parent_task_runner_|.
  // This method must be called only once before any GetFrame() is called.
  virtual void set_parent_task_runner(
      scoped_refptr<base::SequencedTaskRunner> parent_task_runner);

  // Sets the parameters of allocating frames and the maximum number of frames
  // which can be allocated. Returns a valid GpuBufferLayout if VideoFrame
  // will be created by GetFrame().
  virtual base::Optional<GpuBufferLayout> RequestFrames(
      const Fourcc& fourcc,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      size_t max_num_frames) = 0;

  // Returns a frame from the pool with the parameters assigned by
  // SetFrameFormat() and zero timestamp. Returns nullptr if the pool is
  // exhausted.
  virtual scoped_refptr<VideoFrame> GetFrame() = 0;

  // Checks whether the pool is exhausted. This happens when the pool reached
  // its maximum size and all frames are in use. Calling GetFrame() when the
  // pool is exhausted will return a nullptr.
  virtual bool IsExhausted() = 0;

  // Set the callback for notifying when the pool is no longer exhausted. The
  // callback will be called on |parent_task_runner_|. Note: if there is a
  // pending callback when calling NotifyWhenFrameAvailable(), the old callback
  // would be dropped immediately.
  virtual void NotifyWhenFrameAvailable(base::OnceClosure cb) = 0;

 protected:
  scoped_refptr<base::SequencedTaskRunner> parent_task_runner_;
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_DMABUF_VIDEO_FRAME_POOL_H_
