// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/image_processor_factory.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/video_types.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/chromeos/libyuv_image_processor_backend.h"
#include "media/gpu/macros.h"

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_image_processor_backend.h"
#include "media/gpu/v4l2/v4l2_vda_helpers.h"
#endif  // BUILDFLAG(USE_V4L2_CODEC)

namespace media {

namespace {

#if BUILDFLAG(USE_V4L2_CODEC)
std::unique_ptr<ImageProcessor> CreateV4L2ImageProcessorWithInputCandidates(
    const std::vector<std::pair<Fourcc, gfx::Size>>& input_candidates,
    const gfx::Size& visible_size,
    size_t num_buffers,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    ImageProcessorFactory::PickFormatCB out_format_picker,
    ImageProcessor::ErrorCB error_cb) {
  // Pick a renderable output format, and try each available input format.
  // TODO(akahuang): let |out_format_picker| return a list of supported output
  // formats, and try all combination of input/output format, if any platform
  // fails to create ImageProcessor via current approach.
  const std::vector<uint32_t> supported_output_formats =
      V4L2ImageProcessorBackend::GetSupportedOutputFormats();
  std::vector<Fourcc> supported_fourccs;
  for (const auto& format : supported_output_formats) {
    const auto fourcc = Fourcc::FromV4L2PixFmt(format);
    if (fourcc.has_value())
      supported_fourccs.push_back(*fourcc);
  }

  const auto output_fourcc = out_format_picker.Run(supported_fourccs);
  if (!output_fourcc)
    return nullptr;

  const auto supported_input_pixfmts =
      V4L2ImageProcessorBackend::GetSupportedInputFormats();
  for (const auto& input_candidate : input_candidates) {
    const Fourcc input_fourcc = input_candidate.first;
    const gfx::Size& input_size = input_candidate.second;

    if (!base::Contains(supported_input_pixfmts, input_fourcc.ToV4L2PixFmt()))
      continue;

    // Try to get an image size as close as possible to the final size.
    gfx::Size output_size = visible_size;
    size_t num_planes = 0;
    if (!V4L2ImageProcessorBackend::TryOutputFormat(
            input_fourcc.ToV4L2PixFmt(), output_fourcc->ToV4L2PixFmt(),
            input_size, &output_size, &num_planes)) {
      VLOGF(2) << "Failed to get output size and plane count of IP";
      continue;
    }

    return v4l2_vda_helpers::CreateImageProcessor(
        input_fourcc, *output_fourcc, input_size, output_size, visible_size,
        VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER, num_buffers,
        V4L2Device::Create(), ImageProcessor::OutputMode::IMPORT,
        std::move(client_task_runner), std::move(error_cb));
  }
  return nullptr;
}
#endif  // BUILDFLAG(USE_V4L2_CODEC)

}  // namespace

// static
std::unique_ptr<ImageProcessor> ImageProcessorFactory::Create(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    const std::vector<ImageProcessor::OutputMode>& preferred_output_modes,
    size_t num_buffers,
    VideoRotation relative_rotation,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    ImageProcessor::ErrorCB error_cb) {
  std::vector<ImageProcessor::CreateBackendCB> create_funcs;
#if BUILDFLAG(USE_VAAPI)
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(USE_VAAPI)
#if BUILDFLAG(USE_V4L2_CODEC)
  create_funcs.push_back(base::BindRepeating(
      &V4L2ImageProcessorBackend::Create, V4L2Device::Create(), num_buffers));
#endif  // BUILDFLAG(USE_V4L2_CODEC)
  create_funcs.push_back(
      base::BindRepeating(&LibYUVImageProcessorBackend::Create));

  std::unique_ptr<ImageProcessor> image_processor;
  for (auto& create_func : create_funcs) {
    image_processor =
        ImageProcessor::Create(std::move(create_func), input_config,
                               output_config, preferred_output_modes,
                               relative_rotation, error_cb, client_task_runner);
    if (image_processor)
      return image_processor;
  }
  return nullptr;
}

// static
std::unique_ptr<ImageProcessor>
ImageProcessorFactory::CreateWithInputCandidates(
    const std::vector<std::pair<Fourcc, gfx::Size>>& input_candidates,
    const gfx::Size& visible_size,
    size_t num_buffers,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    PickFormatCB out_format_picker,
    ImageProcessor::ErrorCB error_cb) {
#if BUILDFLAG(USE_V4L2_CODEC)
  auto processor = CreateV4L2ImageProcessorWithInputCandidates(
      input_candidates, visible_size, num_buffers, client_task_runner,
      out_format_picker, error_cb);
  if (processor)
    return processor;
#endif  // BUILDFLAG(USE_V4L2_CODEC)

  // TODO(crbug.com/1004727): Implement LibYUVImageProcessorBackend and
  // VaapiImageProcessorBackend.
  return nullptr;
}

}  // namespace media
