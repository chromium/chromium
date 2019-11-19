// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_LIBYUV_IMAGE_PROCESSOR_H_
#define MEDIA_GPU_CHROMEOS_LIBYUV_IMAGE_PROCESSOR_H_

#include <atomic>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoFrameMapper;

// A software image processor which uses libyuv to perform format conversion.
// It expects input VideoFrame is mapped into CPU space, and output VideoFrame
// is allocated in user space.
class MEDIA_GPU_EXPORT LibYUVImageProcessor : public ImageProcessor {
 public:
  // ImageProcessor override
  ~LibYUVImageProcessor() override;
  bool Reset() override;

  // Factory method to create LibYUVImageProcessor to convert video format
  // specified in input_config and output_config. Provided |error_cb| will be
  // posted to the same thread Create() is called if an error occurs after
  // initialization.
  // Returns nullptr if it fails to create LibYUVImageProcessor.
  static std::unique_ptr<LibYUVImageProcessor> Create(
      const ImageProcessor::PortConfig& input_config,
      const ImageProcessor::PortConfig& output_config,
      ImageProcessor::OutputMode output_mode,
      ErrorCB error_cb);

 private:
  LibYUVImageProcessor(const ImageProcessor::PortConfig& input_config,
                       const ImageProcessor::PortConfig& output_config,
                       std::unique_ptr<VideoFrameMapper> video_frame_mapper,
                       ErrorCB error_cb);

  // ImageProcessor override
  bool ProcessInternal(scoped_refptr<VideoFrame> input_frame,
                       scoped_refptr<VideoFrame> output_frame,
                       FrameReadyCB cb) override;

  void ProcessTask(scoped_refptr<VideoFrame> input_frame,
                   scoped_refptr<VideoFrame> output_frame,
                   FrameReadyCB cb);

  void NotifyError();

  // Execute Libyuv function for the conversion from |input| to |output|.
  int DoConversion(const VideoFrame* const input, VideoFrame* const output);

  const gfx::Rect input_visible_rect_;
  const gfx::Rect output_visible_rect_;

  std::unique_ptr<VideoFrameMapper> video_frame_mapper_;

  // A VideoFrame for intermediate format conversion when there is no direct
  // conversion method in libyuv, e.g., RGBA -> I420 (pivot) -> NV12.
  scoped_refptr<VideoFrame> intermediate_frame_;

  // Error callback to the client.
  ErrorCB error_cb_;

  // Thread to process frame format conversion.
  base::Thread process_thread_;

  // CancelableTaskTracker for ProcessTask().
  // Because ProcessTask is posted from |client_task_runner_|'s thread to
  // another sequence, |process_thread_|, it is unsafe to cancel the posted task
  // from |client_task_runner_|'s thread using CancelableCallback and WeakPtr
  // binding. CancelableTaskTracker is designed to deal with this scenario.
  base::CancelableTaskTracker process_task_tracker_;

  // Checker for the thread that creates this LibYUVImageProcessor.
  THREAD_CHECKER(client_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(LibYUVImageProcessor);
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_LIBYUV_IMAGE_PROCESSOR_H_
