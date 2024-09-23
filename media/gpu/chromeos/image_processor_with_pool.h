// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_WITH_POOL_H_
#define MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_WITH_POOL_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// A simple client implementation of ImageProcessor.
// By giving DmabufVideoFramePool, which provides FrameResource for output, the
// caller can process input frames without managing FrameResource for output.
class ImageProcessorWithPool {
 public:
  using FrameResourceReadyCB = ImageProcessor::FrameResourceReadyCB;

  // Initializes |frame_pool| and creates an ImageProcessorWithPool instance.
  // |num_frames| is the number of frames requested from |frame_pool|.
  // Returns a valid ImageProcessorWithPool instance if successful
  // otherwise returns any given error from the set of CroStatus::Codes.
  static CroStatus::Or<std::unique_ptr<ImageProcessorWithPool>> Create(
      std::unique_ptr<ImageProcessor> image_processor,
      DmabufVideoFramePool* const frame_pool,
      size_t num_frames,
      bool use_protected,
      const scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~ImageProcessorWithPool();

  // Processes |frame| by |image_processor_|. The processed output frame will be
  // returned by |ready_cb|, which is called on |task_runner_|.
  void Process(scoped_refptr<FrameResource> frame,
               FrameResourceReadyCB ready_cb);

  // Returns true if there are pending frames.
  bool HasPendingFrames() const;

  // Abandons all pending process. After called, no previous frame will be
  // returned by |ready_cb|.
  void Reset();

  // Returns true if the image processor supports buffers allocated
  // incoherently. The MTK MDP3 image processor has coherency issues, but the
  // Libyuv image processor benefits greatly from incoherent allocations.
  bool SupportsIncoherentBufs() const {
    return image_processor_ && image_processor_->SupportsIncoherentBufs();
  }

  std::string backend_type() const { return image_processor_->backend_type(); }

 private:
  friend class VideoDecoderPipelineTest;

  ImageProcessorWithPool(
      std::unique_ptr<ImageProcessor> image_processor,
      DmabufVideoFramePool* const frame_pool,
      const scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Tries to pass pending frames to |image_processor_|.
  void PumpProcessFrames();

  // Called when |image_processor_| finishes processing frames.
  void OnFrameProcessed(FrameResourceReadyCB ready_cb,
                        scoped_refptr<FrameResource> frame);

  // The image processor for processing frames.
  std::unique_ptr<ImageProcessor> image_processor_;
  // The frame pool to allocate output frames of the image processor.
  // The caller should guarantee the pool alive during the lifetime of this
  // ImageProcessorWithPool instance.
  //
  // Dangling in VideoDecoderPipelineTest.PickDecoderOutputFormatLinearModifier
  // on chromeos-amd64-generic-rel-gtest
  const raw_ptr<DmabufVideoFramePool, DanglingUntriaged> frame_pool_;

  // The pending input frames that wait for passing to |image_processor_|.
  base::queue<std::pair<scoped_refptr<FrameResource>, FrameResourceReadyCB>>
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
