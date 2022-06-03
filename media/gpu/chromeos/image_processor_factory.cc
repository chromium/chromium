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

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_image_processor_backend.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif  // BUILDFLAG(USE_VAAPI)

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_image_processor_backend.h"
#include "media/gpu/v4l2/v4l2_vda_helpers.h"
#endif  // BUILDFLAG(USE_V4L2_CODEC)

namespace media {

namespace {

#if BUILDFLAG(USE_VAAPI)
std::unique_ptr<ImageProcessor> CreateVaapiImageProcessorWithInputCandidates(
    const std::vector<std::pair<Fourcc, gfx::Size>>& input_candidates,
    const gfx::Rect& input_visible_rect,
    const gfx::Size& output_size,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    ImageProcessorFactory::PickFormatCB out_format_picker,
    ImageProcessor::ErrorCB error_cb) {
  std::vector<Fourcc> vpp_supported_formats =
      VaapiWrapper::GetVppSupportedFormats();
  absl::optional<std::pair<Fourcc, gfx::Size>> chosen_input_candidate;
  for (const auto& input_candidate : input_candidates) {
    if (base::Contains(vpp_supported_formats, input_candidate.first) &&
        VaapiWrapper::IsVppResolutionAllowed(input_candidate.second)) {
      chosen_input_candidate = input_candidate;
      break;
    }
  }
  if (!chosen_input_candidate)
    return nullptr;

  // Note that we pick the first input candidate as the preferred output format.
  // The reason is that in practice, the VaapiVideoDecoder will make
  // |input_candidates| either {NV12} or {P010} depending on the bitdepth. So
  // choosing the first (and only) element will keep the bitdepth of the frame
  // which is needed to display HDR content.
  auto chosen_output_format =
      out_format_picker.Run(/*candidates=*/vpp_supported_formats,
                            /*preferred_fourcc=*/input_candidates[0].first);
  if (!chosen_output_format)
    return nullptr;

  // Note: the VaapiImageProcessorBackend doesn't use the ColorPlaneLayouts in
  // the PortConfigs, so we just pass an empty list of plane layouts.
  ImageProcessor::PortConfig input_config(
      /*fourcc=*/chosen_input_candidate->first,
      /*size=*/chosen_input_candidate->second, /*planes=*/{},
      input_visible_rect, {VideoFrame::STORAGE_GPU_MEMORY_BUFFER});
  ImageProcessor::PortConfig output_config(
      /*fourcc=*/*chosen_output_format, /*size=*/output_size, /*planes=*/{},
      /*visible_rect=*/gfx::Rect(output_size),
      {VideoFrame::STORAGE_GPU_MEMORY_BUFFER});
  return ImageProcessor::Create(
      base::BindRepeating(&VaapiImageProcessorBackend::Create), input_config,
      output_config, {ImageProcessor::OutputMode::IMPORT}, VIDEO_ROTATION_0,
      std::move(error_cb), std::move(client_task_runner));
}
#endif  // BUILDFLAG(USE_VAAPI)

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

  const auto output_fourcc = out_format_picker.Run(
      /*candidates=*/supported_fourccs, /*preferred_fourcc=*/absl::nullopt);
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
  create_funcs.push_back(
      base::BindRepeating(&VaapiImageProcessorBackend::Create));
#elif BUILDFLAG(USE_V4L2_CODEC)
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
    const gfx::Rect& input_visible_rect,
    const gfx::Size& output_size,
    size_t num_buffers,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    PickFormatCB out_format_picker,
    ImageProcessor::ErrorCB error_cb) {
#if BUILDFLAG(USE_VAAPI)
  auto processor = CreateVaapiImageProcessorWithInputCandidates(
      input_candidates, input_visible_rect, output_size, client_task_runner,
      out_format_picker, error_cb);
  if (processor)
    return processor;
#elif BUILDFLAG(USE_V4L2_CODEC)
  // TODO(andrescj): we need to pass the |input_visible_rect| along for the V4L2
  // ImageProcessor.
  auto processor = CreateV4L2ImageProcessorWithInputCandidates(
      input_candidates, output_size, num_buffers, client_task_runner,
      out_format_picker, error_cb);
  if (processor)
    return processor;
#endif  // BUILDFLAG(USE_V4L2_CODEC)

  // TODO(crbug.com/1004727): Implement LibYUVImageProcessorBackend. When doing
  // so, we must keep in mind that it might not be desirable to fallback to
  // libyuv if the hardware image processor fails (e.g., in the case of
  // protected content).
  return nullptr;
}

}  // namespace media
