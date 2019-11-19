// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_WITH_POOL_H_
#define MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_WITH_POOL_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// A simple client implementation of ImageProcessor.
// By giving DmabufVideoFramePool, which provides VideoFrame for output, the
// caller can process input frames without managing VideoFrame for output.
class ImageProcessorWithPool {
 public:
  using FrameReadyCB = ImageProcessor::FrameReadyCB;

  // Create ImageProcessorWithPool instance. |num_frames| is the number of
  // frames requested from |frame_pool|.
  static std::unique_ptr<ImageProcessorWithPool> Create(
      std::unique_ptr<ImageProcessor> image_processor,
      DmabufVideoFramePool* const frame_pool,
      size_t num_frames,
      const scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~ImageProcessorWithPool();

  // Processes |frame| by |image_processor_|. The processed output frame will be
  // returned by |ready_cb|, which is called on |task_runner_|.
  void Process(scoped_refptr<VideoFrame> frame, FrameReadyCB ready_cb);

  // Returns true if there are pending frames.
  bool HasPendingFrames() const;

  // Abandons all pending process. After called, no previous frame will be
  // returned by |ready_cb|.
  void Reset();

 private:
  ImageProcessorWithPool(
      std::unique_ptr<ImageProcessor> image_processor,
      DmabufVideoFramePool* const frame_pool,
      const scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Tries to pass pending frames to |image_processor_|.
  void PumpProcessFrames();

  // Called when |image_processor_| finishes processing frames.
  void OnFrameProcessed(FrameReadyCB ready_cb, scoped_refptr<VideoFrame> frame);

  // The image processor for processing frames.
  std::unique_ptr<ImageProcessor> image_processor_;
  // The frame pool to allocate output frames of the image processor.
  // The caller should guarantee the pool alive during the lifetime of this
  // ImageProcessorWithPool instance.
  DmabufVideoFramePool* const frame_pool_;

  // The pending input frames that wait for passing to |image_processor_|.
  base::queue<std::pair<scoped_refptr<VideoFrame>, FrameReadyCB>>
      pending_frames_;
  // Number of frames that are processed in |image_processor_|.
  size_t num_frames_in_ip_ = 0;

  // The main task runner and its checker. All methods should be called on
  // |task_runner_|.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // WeakPtr of this instance and its factory, bound to |task_runner_|.
  base::WeakPtr<ImageProcessorWithPool> weak_this_;
  base::WeakPtrFactory<ImageProcessorWithPool> weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_WITH_POOL_H_
