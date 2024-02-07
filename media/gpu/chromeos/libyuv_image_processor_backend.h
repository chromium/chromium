// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_LIBYUV_IMAGE_PROCESSOR_BACKEND_H_
#define MEDIA_GPU_CHROMEOS_LIBYUV_IMAGE_PROCESSOR_BACKEND_H_

#include <memory>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/image_processor_backend.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

class VideoFrameMapper;

// A software image processor which uses libyuv to perform format conversion.
// It expects input FrameResource is mapped into CPU space, and output
// FrameResource is allocated in user space.
class MEDIA_GPU_EXPORT LibYUVImageProcessorBackend
    : public ImageProcessorBackend {
 public:
  // Factory method to create LibYUVImageProcessorBackend to convert video
  // format specified in input_config and output_config. Provided |error_cb|
  // will be posted to the same thread Create() is called if an error occurs
  // after initialization. Returns nullptr if it fails to create
  // LibYUVImageProcessorBackend.
  static std::unique_ptr<ImageProcessorBackend> Create(
      const PortConfig& input_config,
      const PortConfig& output_config,
      OutputMode output_mode,
      ErrorCB error_cb);
  // This is the same as Create() but the caller can specify
  // |backend_task_runner_|.
  // This should be used when LibYUVImageProcessorBackend is used without
  // ImageProcessor.
  static std::unique_ptr<ImageProcessorBackend> CreateWithTaskRunner(
      const PortConfig& input_config,
      const PortConfig& output_config,
      OutputMode output_mode,
      ErrorCB error_cb,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);

  LibYUVImageProcessorBackend(const LibYUVImageProcessorBackend&) = delete;
  LibYUVImageProcessorBackend& operator=(const LibYUVImageProcessorBackend&) =
      delete;

  // ImageProcessorBackend override
  void ProcessFrame(scoped_refptr<FrameResource> input_frame,
                    scoped_refptr<FrameResource> output_frame,
                    FrameResourceReadyCB cb) override;

  bool needs_linear_output_buffers() const override;

  static std::vector<Fourcc> GetSupportedOutputFormats(Fourcc input_format);

  bool supports_incoherent_buffers() const override;

  std::string type() const override;

 private:
  LibYUVImageProcessorBackend(
      std::unique_ptr<VideoFrameMapper> input_frame_mapper,
      std::unique_ptr<VideoFrameMapper> output_frame_mapper,
      scoped_refptr<FrameResource> intermediate_frame,
      scoped_refptr<FrameResource> crop_intermediate_frame,
      const PortConfig& input_config,
      const PortConfig& output_config,
      OutputMode output_mode,
      ErrorCB error_cb,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);
  ~LibYUVImageProcessorBackend() override;

  void NotifyError();

  // Execute Libyuv function for the conversion from |input| to |output|.
  int DoConversion(const FrameResource* const input,
                   FrameResource* const output);

  const gfx::Rect input_visible_rect_;
  const gfx::Rect output_visible_rect_;

  const std::unique_ptr<VideoFrameMapper> input_frame_mapper_;
  const std::unique_ptr<VideoFrameMapper> output_frame_mapper_;

  // A FrameResource for intermediate format conversion when there is no direct
  // conversion method in libyuv, e.g., RGBA -> I420 (pivot) -> NV12.
  scoped_refptr<FrameResource> intermediate_frame_;
  // A frame to be used as a pivot if we need to crop.
  scoped_refptr<FrameResource> crop_intermediate_frame_;
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_LIBYUV_IMAGE_PROCESSOR_BACKEND_H_
