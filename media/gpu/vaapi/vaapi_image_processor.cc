// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_processor.h"

#include <stdint.h>

#include <va/va.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/linux/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/native_pixmap.h"

namespace media {

namespace {
// UMA errors that the VaapiImageProcessor class reports.
enum class VaIPFailure {
  kVaapiVppError = 0,
  kMaxValue = kVaapiVppError,
};

void ReportToUMA(base::RepeatingClosure error_cb, VaIPFailure failure) {
  base::UmaHistogramEnumeration("Media.VAIP.VppFailure", failure);
  error_cb.Run();
}

bool IsSupported(uint32_t input_va_fourcc,
                 uint32_t output_va_fourcc,
                 const gfx::Size& input_size,
                 const gfx::Size& output_size) {
  if (!VaapiWrapper::IsVppFormatSupported(input_va_fourcc)) {
    VLOGF(2) << "Unsupported input format: VA_FOURCC_"
             << FourccToString(input_va_fourcc);
    return false;
  }

  if (!VaapiWrapper::IsVppFormatSupported(output_va_fourcc)) {
    VLOGF(2) << "Unsupported output format: VA_FOURCC_"
             << FourccToString(output_va_fourcc);
    return false;
  }

  if (!VaapiWrapper::IsVppResolutionAllowed(input_size)) {
    VLOGF(2) << "Unsupported input size: " << input_size.ToString();
    return false;
  }

  if (!VaapiWrapper::IsVppResolutionAllowed(output_size)) {
    VLOGF(2) << "Unsupported output size: " << output_size.ToString();
    return false;
  }

  return true;
}

void ProcessTask(scoped_refptr<VideoFrame> input_frame,
                 scoped_refptr<VideoFrame> output_frame,
                 ImageProcessor::FrameReadyCB cb,
                 scoped_refptr<VaapiWrapper> vaapi_wrapper) {
  DVLOGF(4);

  auto src_va_surface =
      vaapi_wrapper->CreateVASurfaceForVideoFrame(input_frame.get());
  auto dst_va_surface =
      vaapi_wrapper->CreateVASurfaceForVideoFrame(output_frame.get());
  if (!src_va_surface || !dst_va_surface) {
    // Failed to create VASurface for frames. |cb| isn't executed in the case.
    return;
  }
  // VA-API performs pixel format conversion and scaling without any filters.
  vaapi_wrapper->BlitSurface(std::move(src_va_surface),
                             std::move(dst_va_surface));
  std::move(cb).Run(std::move(output_frame));
}

}  // namespace

// static
std::unique_ptr<VaapiImageProcessor> VaapiImageProcessor::Create(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    const std::vector<ImageProcessor::OutputMode>& preferred_output_modes,
    const base::RepeatingClosure& error_cb) {
// VaapiImageProcessor supports ChromeOS only.
#if !defined(OS_CHROMEOS)
  return nullptr;
#endif

  if (!IsSupported(input_config.fourcc.ToVAFourCC(),
                   output_config.fourcc.ToVAFourCC(), input_config.size,
                   output_config.size)) {
    return nullptr;
  }

  if (!base::Contains(input_config.preferred_storage_types,
                      VideoFrame::STORAGE_DMABUFS) &&
      !base::Contains(input_config.preferred_storage_types,
                      VideoFrame::STORAGE_GPU_MEMORY_BUFFER)) {
    VLOGF(2) << "VaapiImageProcessor supports Dmabuf-backed or GpuMemoryBuffer"
             << " based VideoFrame only for input";
    return nullptr;
  }
  if (!base::Contains(output_config.preferred_storage_types,
                      VideoFrame::STORAGE_DMABUFS) &&
      !base::Contains(output_config.preferred_storage_types,
                      VideoFrame::STORAGE_GPU_MEMORY_BUFFER)) {
    VLOGF(2) << "VaapiImageProcessor supports Dmabuf-backed or GpuMemoryBuffer"
             << " based VideoFrame only for output";
    return nullptr;
  }

  if (!base::Contains(preferred_output_modes, OutputMode::IMPORT)) {
    VLOGF(2) << "VaapiImageProcessor only supports IMPORT mode.";
    return nullptr;
  }

  auto vaapi_wrapper = VaapiWrapper::Create(
      VaapiWrapper::kVideoProcess, VAProfileNone,
      base::BindRepeating(&ReportToUMA, error_cb, VaIPFailure::kVaapiVppError));
  if (!vaapi_wrapper) {
    VLOGF(1) << "Failed to create VaapiWrapper";
    return nullptr;
  }

  // Size is irrelevant for a VPP context.
  if (!vaapi_wrapper->CreateContext(gfx::Size())) {
    VLOGF(1) << "Failed to create context for VPP";
    return nullptr;
  }

  // We should restrict the acceptable PortConfig for input and output both to
  // the one returned by GetPlatformVideoFrameLayout(). However,
  // ImageProcessorFactory interface doesn't provide information about what
  // ImageProcessor will be used for. (e.g. format conversion after decoding and
  // scaling before encoding). Thus we cannot execute
  // GetPlatformVideoFrameLayout() with a proper gfx::BufferUsage.
  // TODO(crbug.com/898423): Adjust layout once ImageProcessor provide the use
  // scenario.
  return base::WrapUnique(new VaapiImageProcessor(input_config, output_config,
                                                  std::move(vaapi_wrapper)));
}

VaapiImageProcessor::VaapiImageProcessor(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    scoped_refptr<VaapiWrapper> vaapi_wrapper)
    : ImageProcessor(input_config, output_config, OutputMode::IMPORT),
      processor_task_runner_(base::CreateSequencedTaskRunner(
          base::TaskTraits{base::ThreadPool()})),
      vaapi_wrapper_(std::move(vaapi_wrapper)) {}

VaapiImageProcessor::~VaapiImageProcessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
}

bool VaapiImageProcessor::ProcessInternal(
    scoped_refptr<VideoFrame> input_frame,
    scoped_refptr<VideoFrame> output_frame,
    FrameReadyCB cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  DCHECK(input_frame);
  DCHECK(output_frame);

  const Fourcc input_frame_fourcc =
      Fourcc::FromVideoPixelFormat(input_frame->layout().format());
  if (input_frame_fourcc != input_config_.fourcc) {
    VLOGF(1) << "Invalid input_frame format=" << input_frame_fourcc.ToString()
             << ", expected=" << input_config_.fourcc.ToString();
    return false;
  }

  if (input_frame->layout().coded_size() != input_config_.size) {
    VLOGF(1) << "Invalid input_frame size="
             << input_frame->layout().coded_size().ToString()
             << ", expected=" << input_config_.size.ToString();
    return false;
  }

  const Fourcc output_frame_fourcc =
      Fourcc::FromVideoPixelFormat(output_frame->layout().format());
  if (output_frame_fourcc != output_config_.fourcc) {
    VLOGF(1) << "Invalid output_frame format=" << output_frame_fourcc.ToString()
             << ", expected=" << output_config_.fourcc.ToString();
    return false;
  }

  if (output_frame->layout().coded_size() != output_config_.size) {
    VLOGF(1) << "Invalid output_frame size="
             << output_frame->layout().coded_size().ToString()
             << ", expected=" << output_config_.size.ToString();
    return false;
  }

  if (input_frame->storage_type() != input_config_.storage_type()) {
    VLOGF(1) << "Invalid input_frame->storage_type="
             << input_frame->storage_type()
             << ", input_storage_type=" << input_config_.storage_type();
    return false;
  }
  if (output_frame->storage_type() != output_config_.storage_type()) {
    VLOGF(1) << "Invalid output_frame->storage_type="
             << output_frame->storage_type()
             << ", expected=" << output_config_.storage_type();
    return false;
  }

  process_task_tracker_.PostTask(
      processor_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ProcessTask, std::move(input_frame),
                     std::move(output_frame), std::move(cb), vaapi_wrapper_));
  return true;
}

bool VaapiImageProcessor::Reset() {
  VLOGF(2);
  DCHECK_CALLED_ON_VALID_SEQUENCE(client_sequence_checker_);
  process_task_tracker_.TryCancelAll();
  return true;
}
}  // namespace media
