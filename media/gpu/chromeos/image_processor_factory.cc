// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/image_processor_factory.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/media_switches.h"
#include "media/base/video_types.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/chromeos/gl_image_processor_backend.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/gpu/chromeos/libyuv_image_processor_backend.h"
#include "media/gpu/macros.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_image_processor_backend.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#elif BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_image_processor_backend.h"
#include "media/gpu/v4l2/v4l2_vda_helpers.h"
#endif

namespace media {

namespace {

using PixelLayoutCandidate = ImageProcessor::PixelLayoutCandidate;

#if BUILDFLAG(USE_VAAPI)
std::unique_ptr<ImageProcessor> CreateVaapiImageProcessorWithInputCandidates(
    const std::vector<PixelLayoutCandidate>& input_candidates,
    const gfx::Rect& input_visible_rect,
    const gfx::Size& output_size,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    ImageProcessorFactory::PickFormatCB out_format_picker,
    ImageProcessor::ErrorCB error_cb) {
  std::vector<Fourcc> vpp_supported_formats =
      VaapiWrapper::GetVppSupportedFormats();
  absl::optional<PixelLayoutCandidate> chosen_input_candidate;
  for (const auto& input_candidate : input_candidates) {
    if (base::Contains(vpp_supported_formats, input_candidate.fourcc) &&
        VaapiWrapper::IsVppResolutionAllowed(input_candidate.size)) {
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
  auto chosen_output_format = out_format_picker.Run(
      /*candidates=*/vpp_supported_formats, input_candidates[0].fourcc);
  if (!chosen_output_format)
    return nullptr;

  // Note: the VaapiImageProcessorBackend doesn't use the ColorPlaneLayouts in
  // the PortConfigs, so we just pass an empty list of plane layouts.
  ImageProcessor::PortConfig input_config(
      chosen_input_candidate->fourcc, chosen_input_candidate->size,
      /*planes=*/{}, input_visible_rect,
      {VideoFrame::STORAGE_GPU_MEMORY_BUFFER});
  ImageProcessor::PortConfig output_config(
      /*fourcc=*/*chosen_output_format, /*size=*/output_size, /*planes=*/{},
      /*visible_rect=*/gfx::Rect(output_size),
      {VideoFrame::STORAGE_GPU_MEMORY_BUFFER});
  return ImageProcessor::Create(
      base::BindRepeating(&VaapiImageProcessorBackend::Create), input_config,
      output_config, ImageProcessor::OutputMode::IMPORT, VIDEO_ROTATION_0,
      std::move(error_cb), std::move(client_task_runner));
}

#elif BUILDFLAG(USE_V4L2_CODEC)

std::unique_ptr<ImageProcessor> CreateV4L2ImageProcessorWithInputCandidates(
    const std::vector<PixelLayoutCandidate>& input_candidates,
    const gfx::Rect& visible_rect,
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
  if (!output_fourcc) {
#if DCHECK_IS_ON()
    std::string output_fourccs_string;
    for (const auto fourcc : supported_fourccs) {
      output_fourccs_string += fourcc.ToString();
      output_fourccs_string += " ";
    }
    DVLOGF(1) << "None of " << output_fourccs_string << "formats is supported.";
#endif
    return nullptr;
  }

  const auto supported_input_pixfmts =
      V4L2ImageProcessorBackend::GetSupportedInputFormats();
  for (const auto& input_candidate : input_candidates) {
    const Fourcc input_fourcc = input_candidate.fourcc;
    const gfx::Size& input_size = input_candidate.size;

    if (!base::Contains(supported_input_pixfmts, input_fourcc.ToV4L2PixFmt()))
      continue;

    // Ideally the ImageProcessor would be able to scale and crop |input_size|
    // to the |visible_rect| area of |output_size|. TryOutputFormat() below is
    // called to verify that a given combination of fourcc values and input/
    // output sizes are indeed supported (the driver can potentially return a
    // different supported |output_size|). Some Image Processors(e.g. MTK8183)
    // are not able to crop/scale correctly -- but TryOutputFormat()  doesn't
    // return a "corrected" |output_size|. To avoid troubles (and, in general,
    // low performance), we set |output_size| to be equal to |input_size|; the
    // |visible_rect| will carry the information of exactly what part of the
    // video frame contains valid pixels, and the media/compositor pipeline
    // will take care of it.
    gfx::Size output_size = input_size;
    size_t num_planes = 0;
    if (!V4L2ImageProcessorBackend::TryOutputFormat(
            input_fourcc.ToV4L2PixFmt(), output_fourcc->ToV4L2PixFmt(),
            input_size, &output_size, &num_planes)) {
      VLOGF(2) << "Failed to get output size and plane count of IP";
      continue;
    }
    // This is very restrictive because it assumes the IP has the same alignment
    // criteria as the video decoder that will produce the input video frames.
    // In practice, this applies to all Image Processors, i.e. Mediatek devices.
    DCHECK_EQ(input_size, output_size);
    // |visible_rect| applies equally to both |input_size| and |output_size|.
    DCHECK(gfx::Rect(output_size).Contains(visible_rect));

    return v4l2_vda_helpers::CreateImageProcessor(
        input_fourcc, *output_fourcc, input_size, output_size, visible_rect,
        VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER, num_buffers,
        V4L2Device::Create(), ImageProcessor::OutputMode::IMPORT,
        std::move(client_task_runner), std::move(error_cb));
  }
  return nullptr;
}

std::unique_ptr<ImageProcessor> CreateLibYUVImageProcessorWithInputCandidates(
    const std::vector<PixelLayoutCandidate>& input_candidates,
    const gfx::Rect& input_visible_rect,
    const gfx::Size& output_size,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    ImageProcessorFactory::PickFormatCB out_format_picker,
    ImageProcessor::ErrorCB error_cb) {
  if (input_candidates.size() != 1)
    return nullptr;

  if (input_candidates[0].fourcc != Fourcc(Fourcc::MM21) &&
      input_candidates[0].fourcc != Fourcc(Fourcc::MT2T)) {
    return nullptr;
  }

  std::vector<Fourcc> supported_output_formats =
      LibYUVImageProcessorBackend::GetSupportedOutputFormats(
          input_candidates[0].fourcc);
  auto output_format =
      out_format_picker.Run(supported_output_formats, absl::nullopt);

  if (!output_format)
    return nullptr;

  ImageProcessor::PortConfig input_config(
      input_candidates[0].fourcc, input_candidates[0].size, /*planes=*/{},
      input_visible_rect, {VideoFrame::STORAGE_DMABUFS});
  ImageProcessor::PortConfig output_config(
      *output_format, output_size, /*planes=*/{}, gfx::Rect(output_size),
      {VideoFrame::STORAGE_GPU_MEMORY_BUFFER});
  return ImageProcessor::Create(
      base::BindRepeating(&LibYUVImageProcessorBackend::Create), input_config,
      output_config, ImageProcessor::OutputMode::IMPORT, VIDEO_ROTATION_0,
      std::move(error_cb), std::move(client_task_runner));
}

#if defined(ARCH_CPU_ARM_FAMILY)
std::unique_ptr<ImageProcessor> CreateGLImageProcessorWithInputCandidates(
    const std::vector<PixelLayoutCandidate>& input_candidates,
    const gfx::Rect& input_visible_rect,
    const gfx::Size& output_size,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    ImageProcessorFactory::PickFormatCB out_format_picker,
    ImageProcessor::ErrorCB error_cb) {
  if (input_candidates.size() != 1)
    return nullptr;

  if (input_candidates[0].fourcc != Fourcc(Fourcc::MM21))
    return nullptr;

  ImageProcessor::PortConfig input_config(
      Fourcc(Fourcc::MM21), input_candidates[0].size, /*planes=*/{},
      input_visible_rect, {VideoFrame::STORAGE_DMABUFS});
  ImageProcessor::PortConfig output_config(
      Fourcc(Fourcc::NV12), output_size, /*planes=*/{}, gfx::Rect(output_size),
      {VideoFrame::STORAGE_GPU_MEMORY_BUFFER});

  if (!GLImageProcessorBackend::IsSupported(input_config, output_config,
                                            VIDEO_ROTATION_0)) {
    return nullptr;
  }

  return ImageProcessor::Create(
      base::BindRepeating(&GLImageProcessorBackend::Create), input_config,
      output_config, ImageProcessor::OutputMode::IMPORT, VIDEO_ROTATION_0,
      std::move(error_cb), std::move(client_task_runner));
}
#endif  // defined(ARCH_CPU_ARM_FAMILY)
#endif

}  // namespace

// static
std::unique_ptr<ImageProcessor> ImageProcessorFactory::Create(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    ImageProcessor::OutputMode output_mode,
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
#endif
  create_funcs.push_back(
      base::BindRepeating(&LibYUVImageProcessorBackend::Create));

  std::unique_ptr<ImageProcessor> image_processor;
  for (auto& create_func : create_funcs) {
    image_processor = ImageProcessor::Create(
        std::move(create_func), input_config, output_config, output_mode,
        relative_rotation, error_cb, client_task_runner);
    if (image_processor)
      return image_processor;
  }
  return nullptr;
}

// static
std::unique_ptr<ImageProcessor>
ImageProcessorFactory::CreateWithInputCandidates(
    const std::vector<PixelLayoutCandidate>& input_candidates,
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
#if defined(ARCH_CPU_ARM_FAMILY)
  if (base::FeatureList::IsEnabled(media::kPreferGLImageProcessor)) {
    auto processor = CreateGLImageProcessorWithInputCandidates(
        input_candidates, input_visible_rect, output_size, client_task_runner,
        out_format_picker, error_cb);
    if (processor)
      return processor;
  }
#endif  // defined(ARCH_CPU_ARM_FAMILY)

    auto processor = CreateLibYUVImageProcessorWithInputCandidates(
        input_candidates, input_visible_rect, output_size, client_task_runner,
        out_format_picker, error_cb);
    if (processor)
      return processor;

    processor = CreateV4L2ImageProcessorWithInputCandidates(
        input_candidates, input_visible_rect, num_buffers, client_task_runner,
        out_format_picker, error_cb);
    if (processor)
      return processor;

#endif

  // TODO(crbug.com/1004727): Implement LibYUVImageProcessorBackend. When doing
  // so, we must keep in mind that it might not be desirable to fallback to
  // libyuv if the hardware image processor fails (e.g., in the case of
  // protected content).
  return nullptr;
}

#if BUILDFLAG(USE_V4L2_CODEC)
std::unique_ptr<ImageProcessor>
ImageProcessorFactory::CreateLibYUVImageProcessorWithInputCandidatesForTesting(
    const std::vector<ImageProcessor::PixelLayoutCandidate>& input_candidates,
    const gfx::Rect& input_visible_rect,
    const gfx::Size& output_size,
    size_t num_buffers,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    PickFormatCB out_format_picker,
    ImageProcessor::ErrorCB error_cb) {
  return CreateLibYUVImageProcessorWithInputCandidates(
      input_candidates, input_visible_rect, output_size, client_task_runner,
      out_format_picker, error_cb);
}

std::unique_ptr<ImageProcessor>
ImageProcessorFactory::CreateGLImageProcessorWithInputCandidatesForTesting(
    const std::vector<ImageProcessor::PixelLayoutCandidate>& input_candidates,
    const gfx::Rect& input_visible_rect,
    const gfx::Size& output_size,
    size_t num_buffers,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    PickFormatCB out_format_picker,
    ImageProcessor::ErrorCB error_cb) {
  return CreateGLImageProcessorWithInputCandidates(
      input_candidates, input_visible_rect, output_size, client_task_runner,
      out_format_picker, error_cb);
}
#endif

}  // namespace media
