// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_IMAGE_PROCESSOR_BACKEND_H_
#define MEDIA_GPU_VAAPI_VAAPI_IMAGE_PROCESSOR_BACKEND_H_

#include <memory>

#include "base/macros.h"
#include "media/gpu/chromeos/image_processor_backend.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

class VaapiWrapper;

// ImageProcessor that is hardware accelerated with VA-API. This ImageProcessor
// supports only dma-buf and GpuMemoryBuffer VideoFrames for both input and
// output.
class VaapiImageProcessorBackend : public ImageProcessorBackend {
 public:
  VaapiImageProcessorBackend(const VaapiImageProcessorBackend&) = delete;
  VaapiImageProcessorBackend& operator=(const VaapiImageProcessorBackend&) =
      delete;

  // Factory method to create a VaapiImageProcessorBackend for processing frames
  // as specified by |input_config| and |output_config|. The provided |error_cb|
  // will be posted to the same thread that executes Create() if an error occurs
  // after initialization.
  // Returns nullptr if it fails to create a VaapiImageProcessorBackend.
  static std::unique_ptr<ImageProcessorBackend> Create(
      const PortConfig& input_config,
      const PortConfig& output_config,
      const std::vector<OutputMode>& preferred_output_modes,
      VideoRotation relative_rotation,
      ErrorCB error_cb,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);

  // ImageProcessor implementation.
  void Process(scoped_refptr<VideoFrame> input_frame,
               scoped_refptr<VideoFrame> output_frame,
               FrameReadyCB cb) override;

 private:
  VaapiImageProcessorBackend(
      scoped_refptr<VaapiWrapper> vaapi_wrapper,
      const PortConfig& input_config,
      const PortConfig& output_config,
      OutputMode output_mode,
      VideoRotation relative_rotation,
      ErrorCB error_cb,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);
  ~VaapiImageProcessorBackend() override;

  const scoped_refptr<VaapiWrapper> vaapi_wrapper_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_IMAGE_PROCESSOR_BACKEND_H_
