// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_processor_backend.h"

#include <stdint.h>
#include <va/va.h>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/format_utils.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/native_pixmap.h"

namespace media {

namespace {

bool IsSupported(const ImageProcessorBackend::PortConfig& config) {
  if (!config.fourcc.ToVAFourCC())
    return false;
  const uint32_t va_fourcc = *config.fourcc.ToVAFourCC();
  if (!VaapiWrapper::IsVppFormatSupported(va_fourcc)) {
    VLOGF(2) << "Unsupported format: VA_FOURCC_" << FourccToString(va_fourcc);
    return false;
  }
  if (!VaapiWrapper::IsVppResolutionAllowed(config.size)) {
    VLOGF(2) << "Unsupported size: " << config.size.ToString();
    return false;
  }
  const gfx::Size& visible_size = config.visible_rect.size();
  if (!VaapiWrapper::IsVppResolutionAllowed(visible_size)) {
    VLOGF(2) << "Unsupported visible size: " << visible_size.ToString();
    return false;
  }
  if (!gfx::Rect(config.size).Contains(config.visible_rect)) {
    VLOGF(2) << "The frame size (" << config.size.ToString()
             << ") does not contain the visible rect ("
             << config.visible_rect.ToString() << ")";
    return false;
  }
  return true;
}

}  // namespace

// static
std::unique_ptr<ImageProcessorBackend> VaapiImageProcessorBackend::Create(
    const PortConfig& input_config,
    const PortConfig& output_config,
    OutputMode output_mode,
    ErrorCB error_cb) {
  DCHECK_EQ(output_mode, OutputMode::IMPORT)
      << "Only OutputMode::IMPORT supported";
  if (!IsSupported(input_config) || !IsSupported(output_config))
    return nullptr;

  if (input_config.storage_type != VideoFrame::STORAGE_GPU_MEMORY_BUFFER &&
      input_config.storage_type != VideoFrame::STORAGE_DMABUFS) {
    VLOGF(2) << "VaapiImageProcessorBackend supports GpuMemoryBuffer or DMABuf "
                "based FrameResource only for input";
    return nullptr;
  }
  if (output_config.storage_type != VideoFrame::STORAGE_GPU_MEMORY_BUFFER &&
      output_config.storage_type != VideoFrame::STORAGE_DMABUFS) {
    VLOGF(2) << "VaapiImageProcessorBackend supports GpuMemoryBuffer or DMABuf "
                "based FrameResource only for output";
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
  return base::WrapUnique<ImageProcessorBackend>(new VaapiImageProcessorBackend(
      input_config, output_config, OutputMode::IMPORT, std::move(error_cb)));
}

VaapiImageProcessorBackend::VaapiImageProcessorBackend(
    const PortConfig& input_config,
    const PortConfig& output_config,
    OutputMode output_mode,
    ErrorCB error_cb)
    : ImageProcessorBackend(input_config,
                            output_config,
                            output_mode,
                            std::move(error_cb),
                            base::ThreadPool::CreateSequencedTaskRunner(
                                {base::TaskPriority::USER_VISIBLE})) {}

VaapiImageProcessorBackend::~VaapiImageProcessorBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  DCHECK(vaapi_wrapper_ || allocated_va_surfaces_.IsEmpty());
  if (vaapi_wrapper_) {
    // To clear |allocated_va_surfaces_|, we have to first DestroyContext().
    vaapi_wrapper_->DestroyContext();
    allocated_va_surfaces_.Clear();
  }
}

std::string VaapiImageProcessorBackend::type() const {
  return "VaapiImageProcessor";
}

const ScopedVASurface* VaapiImageProcessorBackend::GetOrCreateSurfaceForFrame(
    const FrameResource& frame,
    bool use_protected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  const auto shared_memory_id = frame.GetSharedMemoryId().id;
  const auto* surface = allocated_va_surfaces_.Lookup(shared_memory_id);
  if (surface) {
    CHECK_EQ(frame.coded_size(), surface->size());
    const auto buffer_format =
        VideoPixelFormatToGfxBufferFormat(frame.format());
    CHECK(buffer_format.has_value());
    const unsigned int format =
        VaapiWrapper::BufferFormatToVARTFormat(*buffer_format);
    CHECK_NE(format, 0u);
    CHECK_EQ(format, surface->format());
    return surface;
  }

  DCHECK(vaapi_wrapper_);
  auto va_surface =
      vaapi_wrapper_->CreateVASurfaceForFrameResource(frame, use_protected);
  if (!va_surface) {
    VLOGF(1) << "Failed to create VASurface from frame";
    return nullptr;
  }

  allocated_va_surfaces_.AddWithID(std::move(va_surface), shared_memory_id);
  return allocated_va_surfaces_.Lookup(shared_memory_id);
}

void VaapiImageProcessorBackend::ProcessFrame(
    scoped_refptr<FrameResource> input_frame,
    scoped_refptr<FrameResource> output_frame,
    FrameResourceReadyCB cb) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  TRACE_EVENT2("media", "VaapiImageProcessorBackend::Process", "input_frame",
               input_frame->AsHumanReadableString(), "output_frame",
               output_frame->AsHumanReadableString());

  if (!vaapi_wrapper_) {
    // Note that EncryptionScheme::kUnencrypted is fine even for the use case
    // where the VPP is needed for processing protected content after decoding.
    // That's because when calling VaapiWrapper::BlitSurface(), we re-use the
    // protected session ID created at decoding time.
    auto vaapi_wrapper =
        VaapiWrapper::Create(
            VaapiWrapper::kVideoProcess, VAProfileNone,
            EncryptionScheme::kUnencrypted,
            base::BindRepeating(&ReportVaapiErrorToUMA,
                                "Media.VaapiImageProcessorBackend.VAAPIError"))
            .value_or(nullptr);
    if (!vaapi_wrapper) {
      VLOGF(1) << "Failed to create VaapiWrapper";
      error_cb_.Run();
      return;
    }

    // Size is irrelevant for a VPP context.
    if (!vaapi_wrapper->CreateContext(gfx::Size())) {
      VLOGF(1) << "Failed to create context for VPP";
      error_cb_.Run();
      return;
    }

    vaapi_wrapper_ = std::move(vaapi_wrapper);
  }

  bool use_protected = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  VAProtectedSessionID va_protected_session_id = VA_INVALID_ID;
  if (input_frame->metadata().hw_va_protected_session_id.has_value()) {
    static_assert(
        std::is_same<decltype(input_frame->metadata()
                                  .hw_va_protected_session_id)::value_type,
                     VAProtectedSessionID>::value,
        "The optional type of VideoFrameMetadata::hw_va_protected_session_id "
        "is "
        "not VAProtectedSessionID");
    va_protected_session_id =
        input_frame->metadata().hw_va_protected_session_id.value();
    use_protected = va_protected_session_id != VA_INVALID_ID;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (needs_context_ && !vaapi_wrapper_->CreateContext(gfx::Size())) {
    VLOGF(1) << "Failed to create context for VPP";
    error_cb_.Run();
    return;
  }
  needs_context_ = false;

  DCHECK(input_frame);
  DCHECK(output_frame);
  const ScopedVASurface* src_va_surface =
      GetOrCreateSurfaceForFrame(*input_frame, use_protected);
  if (!src_va_surface) {
    error_cb_.Run();
    return;
  }
  const ScopedVASurface* dst_va_surface =
      GetOrCreateSurfaceForFrame(*output_frame, use_protected);
  if (!dst_va_surface) {
    error_cb_.Run();
    return;
  }

  // VA-API performs pixel format conversion and scaling without any filters.
  if (!vaapi_wrapper_->BlitSurface(src_va_surface->id(), src_va_surface->size(),
                                   dst_va_surface->id(), dst_va_surface->size(),
                                   input_frame->visible_rect(),
                                   output_frame->visible_rect()
#if BUILDFLAG(IS_CHROMEOS_ASH)
                                       ,
                                   va_protected_session_id
#endif
                                   )) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (use_protected &&
        vaapi_wrapper_->IsProtectedSessionDead(va_protected_session_id)) {
      DCHECK_NE(va_protected_session_id, VA_INVALID_ID);

      // If the VPP failed because the protected session is dead, we should
      // still output the frame. That's because we don't want to put the
      // VideoDecoderPipeline into an unusable error state: the
      // VaapiVideoDecoder can recover from a dead protected session later and
      // the compositor should not try to render the frame we output here
      // anyway.
      output_frame->set_timestamp(input_frame->timestamp());
      output_frame->set_color_space(input_frame->ColorSpace());
      std::move(cb).Run(std::move(output_frame));
      return;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    error_cb_.Run();
    return;
  }

  output_frame->set_timestamp(input_frame->timestamp());
  output_frame->set_color_space(input_frame->ColorSpace());

  std::move(cb).Run(std::move(output_frame));
}

void VaapiImageProcessorBackend::Reset() {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  DCHECK(vaapi_wrapper_ || allocated_va_surfaces_.IsEmpty());
  if (vaapi_wrapper_) {
    // To clear |allocated_va_surfaces_|, we have to first DestroyContext().
    vaapi_wrapper_->DestroyContext();
    allocated_va_surfaces_.Clear();
  }
  needs_context_ = true;
}

}  // namespace media
