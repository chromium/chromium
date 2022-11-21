// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_mjpeg_decode_accelerator.h"

#include <stddef.h>
#include <sys/mman.h>
#include <va/va.h>

#include <array>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_util.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/libyuv_image_processor_backend.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

namespace {

void ReportToVAJDAResponseToClientUMA(
    chromeos_camera::MjpegDecodeAccelerator::Error response) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.VAJDA.ResponseToClient", response,
      chromeos_camera::MjpegDecodeAccelerator::Error::MJDA_ERROR_CODE_MAX + 1);
}

chromeos_camera::MjpegDecodeAccelerator::Error VaapiJpegDecodeStatusToError(
    VaapiImageDecodeStatus status) {
  switch (status) {
    case VaapiImageDecodeStatus::kSuccess:
      return chromeos_camera::MjpegDecodeAccelerator::Error::NO_ERRORS;
    case VaapiImageDecodeStatus::kParseFailed:
      return chromeos_camera::MjpegDecodeAccelerator::Error::PARSE_JPEG_FAILED;
    case VaapiImageDecodeStatus::kUnsupportedSubsampling:
      return chromeos_camera::MjpegDecodeAccelerator::Error::UNSUPPORTED_JPEG;
    default:
      return chromeos_camera::MjpegDecodeAccelerator::Error::PLATFORM_FAILURE;
  }
}

bool VerifyDataSize(const VAImage* image) {
  const gfx::Size dimensions(base::strict_cast<int>(image->width),
                             base::strict_cast<int>(image->height));
  size_t min_size = 0;
  if (image->format.fourcc == VA_FOURCC_I420) {
    min_size = VideoFrame::AllocationSize(PIXEL_FORMAT_I420, dimensions);
  } else if (image->format.fourcc == VA_FOURCC_NV12) {
    min_size = VideoFrame::AllocationSize(PIXEL_FORMAT_NV12, dimensions);
  } else if (image->format.fourcc == VA_FOURCC_YUY2 ||
             image->format.fourcc == VA_FOURCC('Y', 'U', 'Y', 'V')) {
    min_size = VideoFrame::AllocationSize(PIXEL_FORMAT_YUY2, dimensions);
  } else {
    return false;
  }
  return base::strict_cast<size_t>(image->data_size) >= min_size;
}

}  // namespace

void VaapiMjpegDecodeAccelerator::NotifyError(int32_t task_id, Error error) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VaapiMjpegDecodeAccelerator::NotifyError,
                       weak_this_factory_.GetWeakPtr(), task_id, error));
    return;
  }
  VLOGF(1) << "Notifying of error " << error;
  // |error| shouldn't be NO_ERRORS because successful decodes should be handled
  // by VideoFrameReady().
  DCHECK_NE(chromeos_camera::MjpegDecodeAccelerator::Error::NO_ERRORS, error);
  ReportToVAJDAResponseToClientUMA(error);
  DCHECK(client_);
  client_->NotifyError(task_id, error);
}

void VaapiMjpegDecodeAccelerator::VideoFrameReady(int32_t task_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  ReportToVAJDAResponseToClientUMA(
      chromeos_camera::MjpegDecodeAccelerator::Error::NO_ERRORS);
  client_->VideoFrameReady(task_id);
}

VaapiMjpegDecodeAccelerator::VaapiMjpegDecodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      io_task_runner_(io_task_runner),
      client_(nullptr),
      decoder_thread_("VaapiMjpegDecoderThread"),
      weak_this_factory_(this) {}

// Some members expect to be destroyed on the |decoder_thread_|.
void VaapiMjpegDecodeAccelerator::CleanUpOnDecoderThread() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  DCHECK(vpp_vaapi_wrapper_->HasOneRef());
  vpp_vaapi_wrapper_.reset();
  decoder_.reset();
  image_processor_.reset();
}

VaapiMjpegDecodeAccelerator::~VaapiMjpegDecodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOGF(2) << "Destroying VaapiMjpegDecodeAccelerator";
  weak_this_factory_.InvalidateWeakPtrs();

  if (decoder_task_runner_) {
    // base::Unretained() is fine here because we control |decoder_task_runner_|
    // lifetime.
    decoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VaapiMjpegDecodeAccelerator::CleanUpOnDecoderThread,
                       base::Unretained(this)));
  }
  decoder_thread_.Stop();
}

void VaapiMjpegDecodeAccelerator::InitializeOnDecoderTaskRunner(
    chromeos_camera::MjpegDecodeAccelerator::InitCB init_cb) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  decoder_ = std::make_unique<media::VaapiJpegDecoder>();
  if (!decoder_->Initialize(base::BindRepeating(
          &ReportVaapiErrorToUMA,
          "Media.VaapiMjpegDecodeAccelerator.VAAPIError"))) {
    VLOGF(1) << "Failed initializing |decoder_|";
    std::move(init_cb).Run(false);
    return;
  }

  vpp_vaapi_wrapper_ = VaapiWrapper::Create(
      VaapiWrapper::kVideoProcess, VAProfileNone,
      EncryptionScheme::kUnencrypted,
      base::BindRepeating(&ReportVaapiErrorToUMA,
                          "Media.VaapiMjpegDecodeAccelerator.Vpp.VAAPIError"));
  if (!vpp_vaapi_wrapper_) {
    VLOGF(1) << "Failed initializing VAAPI for VPP";
    std::move(init_cb).Run(false);
    return;
  }

  // Size is irrelevant for a VPP context.
  if (!vpp_vaapi_wrapper_->CreateContext(gfx::Size())) {
    VLOGF(1) << "Failed to create context for VPP";
    std::move(init_cb).Run(false);
    return;
  }

  std::move(init_cb).Run(true);
}

void VaapiMjpegDecodeAccelerator::InitializeOnTaskRunner(
    chromeos_camera::MjpegDecodeAccelerator::Client* client,
    chromeos_camera::MjpegDecodeAccelerator::InitCB init_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_ = client;

  if (!decoder_thread_.Start()) {
    VLOGF(1) << "Failed to start decoding thread.";
    std::move(init_cb).Run(false);
    return;
  }
  decoder_task_runner_ = decoder_thread_.task_runner();

  // base::Unretained() is fine here because we control |decoder_task_runner_|
  // lifetime.
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VaapiMjpegDecodeAccelerator::InitializeOnDecoderTaskRunner,
          base::Unretained(this), std::move(init_cb)));
}

void VaapiMjpegDecodeAccelerator::InitializeAsync(
    chromeos_camera::MjpegDecodeAccelerator::Client* client,
    chromeos_camera::MjpegDecodeAccelerator::InitCB init_cb) {
  VLOGF(2);
  DCHECK(task_runner_->BelongsToCurrentThread());

  // To guarantee that the caller receives an asynchronous call after the
  // return path, we are making use of InitializeOnTaskRunner.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiMjpegDecodeAccelerator::InitializeOnTaskRunner,
                     weak_this_factory_.GetWeakPtr(), client,
                     BindToCurrentLoop(std::move(init_cb))));
}

void VaapiMjpegDecodeAccelerator::CreateImageProcessor(
    const VideoFrame* src_frame,
    const VideoFrame* dst_frame) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  // The fourcc of |src_frame| will be either Fourcc(YUYV) or Fourcc(YU12) based
  // on the implementation of OutputPictureLibYuvOnTaskRunner(). The fourcc of
  // |dst_frame| should have been validated in DecodeImpl().
  const auto src_fourcc = Fourcc::FromVideoPixelFormat(src_frame->format());
  DCHECK(src_fourcc.has_value());
  const auto dst_fourcc = Fourcc::FromVideoPixelFormat(dst_frame->format());
  DCHECK(dst_fourcc.has_value());
  const ImageProcessorBackend::PortConfig input_config(
      *src_fourcc, src_frame->coded_size(), src_frame->layout().planes(),
      src_frame->visible_rect(), {src_frame->storage_type()});
  const ImageProcessorBackend::PortConfig output_config(
      *dst_fourcc, dst_frame->coded_size(), dst_frame->layout().planes(),
      dst_frame->visible_rect(), {dst_frame->storage_type()});
  if (image_processor_ && image_processor_->input_config() == input_config &&
      image_processor_->output_config() == output_config) {
    return;
  }

  // The error callback is posted to the same thread that
  // LibYUVImageProcessorBackend::Create() is called on
  // (i.e., |decoder_thread_|) and we control the lifetime of |decoder_thread_|.
  // Therefore, base::Unretained(this) is safe.
  image_processor_ = LibYUVImageProcessorBackend::Create(
      input_config, output_config, ImageProcessorBackend::OutputMode::IMPORT,
      VIDEO_ROTATION_0,
      base::BindRepeating(&VaapiMjpegDecodeAccelerator::OnImageProcessorError,
                          base::Unretained(this)),
      decoder_task_runner_);
}

bool VaapiMjpegDecodeAccelerator::OutputPictureLibYuvOnTaskRunner(
    int32_t task_id,
    std::unique_ptr<ScopedVAImage> scoped_image,
    scoped_refptr<VideoFrame> video_frame,
    const gfx::Rect& crop_rect) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  TRACE_EVENT1("jpeg", __func__, "task_id", task_id);

  DCHECK(scoped_image);
  const VAImage* image = scoped_image->image();
  DCHECK(VerifyDataSize(image));
  const gfx::Size src_size(base::strict_cast<int>(image->width),
                           base::strict_cast<int>(image->height));
  DCHECK(gfx::Rect(src_size).Contains(crop_rect));

  // Wrap |image| into VideoFrame.
  std::vector<int32_t> strides(image->num_planes);
  for (uint32_t i = 0; i < image->num_planes; ++i) {
    if (!base::CheckedNumeric<uint32_t>(image->pitches[i])
             .AssignIfValid(&strides[i])) {
      VLOGF(1) << "Invalid VAImage stride " << image->pitches[i]
               << " for plane " << i;
      return false;
    }
  }
  auto* const data = static_cast<uint8_t*>(scoped_image->va_buffer()->data());
  scoped_refptr<VideoFrame> src_frame;
  switch (image->format.fourcc) {
    case VA_FOURCC_YUY2:
    case VA_FOURCC('Y', 'U', 'Y', 'V'): {
      auto layout = VideoFrameLayout::CreateWithStrides(PIXEL_FORMAT_YUY2,
                                                        src_size, strides);
      if (!layout.has_value()) {
        VLOGF(1) << "Failed to create video frame layout";
        return false;
      }
      src_frame = VideoFrame::WrapExternalDataWithLayout(
          *layout, crop_rect, crop_rect.size(), data + image->offsets[0],
          base::strict_cast<size_t>(image->data_size), base::TimeDelta());
      break;
    }
    case VA_FOURCC_I420: {
      auto layout = VideoFrameLayout::CreateWithStrides(PIXEL_FORMAT_I420,
                                                        src_size, strides);
      if (!layout.has_value()) {
        VLOGF(1) << "Failed to create video frame layout";
        return false;
      }
      src_frame = VideoFrame::WrapExternalYuvDataWithLayout(
          *layout, crop_rect, crop_rect.size(), data + image->offsets[0],
          data + image->offsets[1], data + image->offsets[2],
          base::TimeDelta());
      break;
    }
    default:
      VLOGF(1) << "Unsupported VA image format: "
               << FourccToString(image->format.fourcc);
      return false;
  }
  if (!src_frame) {
    VLOGF(1) << "Failed to create video frame";
    return false;
  }

  CreateImageProcessor(src_frame.get(), video_frame.get());
  if (!image_processor_) {
    VLOGF(1) << "Failed to create image processor";
    return false;
  }
  image_processor_->Process(
      std::move(src_frame), std::move(video_frame),
      base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> runner,
             base::OnceClosure cb, scoped_refptr<VideoFrame> frame) {
            runner->PostTask(FROM_HERE, std::move(cb));
          },
          task_runner_,
          base::BindOnce(&VaapiMjpegDecodeAccelerator::VideoFrameReady,
                         weak_this_factory_.GetWeakPtr(), task_id)));
  return true;
}

void VaapiMjpegDecodeAccelerator::OnImageProcessorError() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  VLOGF(1) << "Failed to process frames using the libyuv image processor";
  NotifyError(kInvalidTaskId, PLATFORM_FAILURE);
  image_processor_.reset();
}

bool VaapiMjpegDecodeAccelerator::OutputPictureVppOnTaskRunner(
    int32_t task_id,
    const ScopedVASurface* surface,
    scoped_refptr<VideoFrame> video_frame,
    const gfx::Rect& crop_rect) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  DCHECK(surface);
  DCHECK(video_frame);
  DCHECK(gfx::Rect(surface->size()).Contains(crop_rect));

  TRACE_EVENT1("jpeg", __func__, "task_id", task_id);

  scoped_refptr<gfx::NativePixmap> pixmap =
      CreateNativePixmapDmaBuf(video_frame.get());
  if (!pixmap) {
    VLOGF(1) << "Failed to create NativePixmap from VideoFrame";
    return false;
  }

  // Bind a VA surface to |video_frame|.
  scoped_refptr<VASurface> output_surface =
      vpp_vaapi_wrapper_->CreateVASurfaceForPixmap(std::move(pixmap));
  if (!output_surface) {
    VLOGF(1) << "Cannot create VA surface for output buffer";
    return false;
  }

  scoped_refptr<VASurface> src_surface = base::MakeRefCounted<VASurface>(
      surface->id(), surface->size(), surface->format(),
      /*release_cb=*/base::DoNothing());

  // We should call vaSyncSurface() when passing surface between contexts, but
  // on Intel platform, we don't have to call vaSyncSurface() because the
  // underlying drivers handle synchronization between different contexts. See:
  // https://lists.01.org/hyperkitty/list/intel-vaapi-media@lists.01.org/message/YNFLDHHHQM2ZBFPMH7D3U6GLMOELHPFL/
  const bool is_intel_backend =
      VaapiWrapper::GetImplementationType() == VAImplementation::kIntelI965 ||
      VaapiWrapper::GetImplementationType() == VAImplementation::kIntelIHD;
  if (!is_intel_backend && !vpp_vaapi_wrapper_->SyncSurface(surface->id())) {
    VLOGF(1) << "Cannot sync VPP input surface";
    return false;
  }
  if (!vpp_vaapi_wrapper_->BlitSurface(*src_surface, *output_surface,
                                       crop_rect)) {
    VLOGF(1) << "Cannot convert decoded image into output buffer";
    return false;
  }

  // Sync target surface since the buffer is returning to client.
  if (!vpp_vaapi_wrapper_->SyncSurface(output_surface->id())) {
    VLOGF(1) << "Cannot sync VPP output surface";
    return false;
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiMjpegDecodeAccelerator::VideoFrameReady,
                                weak_this_factory_.GetWeakPtr(), task_id));

  return true;
}

void VaapiMjpegDecodeAccelerator::DecodeFromShmTask(
    int32_t task_id,
    base::WritableSharedMemoryMapping mapping,
    scoped_refptr<VideoFrame> dst_frame) {
  DVLOGF(4);
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("jpeg", __func__);

  auto src_image = mapping.GetMemoryAsSpan<uint8_t>();
  DecodeImpl(task_id, src_image, std::move(dst_frame));
}

void VaapiMjpegDecodeAccelerator::DecodeFromDmaBufTask(
    int32_t task_id,
    base::ScopedFD src_dmabuf_fd,
    size_t src_size,
    off_t src_offset,
    scoped_refptr<VideoFrame> dst_frame) {
  DVLOGF(4);
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("jpeg", __func__);

  // The DMA-buf FD should be mapped as read-only since it may only have read
  // permission, e.g. when it comes from camera driver.
  DCHECK(src_dmabuf_fd.is_valid());
  DCHECK_GT(src_size, 0u);
  void* src_addr = mmap(nullptr, src_size, PROT_READ, MAP_SHARED,
                        src_dmabuf_fd.get(), src_offset);
  if (src_addr == MAP_FAILED) {
    VPLOGF(1) << "Failed to map input DMA buffer";
    NotifyError(task_id, UNREADABLE_INPUT);
    return;
  }
  base::span<const uint8_t> src_image =
      base::make_span(static_cast<const uint8_t*>(src_addr), src_size);

  DecodeImpl(task_id, src_image, std::move(dst_frame));

  const int ret = munmap(src_addr, src_size);
  DPCHECK(ret == 0);
}

void VaapiMjpegDecodeAccelerator::DecodeImpl(
    int32_t task_id,
    base::span<const uint8_t> src_image,
    scoped_refptr<VideoFrame> dst_frame) {
  VaapiImageDecodeStatus status = decoder_->Decode(src_image);
  if (status != VaapiImageDecodeStatus::kSuccess) {
    VLOGF(1) << "Failed to decode JPEG image";
    NotifyError(task_id, VaapiJpegDecodeStatusToError(status));
    return;
  }
  const ScopedVASurface* surface = decoder_->GetScopedVASurface();
  DCHECK(surface);
  DCHECK(surface->IsValid());

  // For camera captures, we assume that the visible size is the same as the
  // coded size.
  if (dst_frame->visible_rect().size() != dst_frame->coded_size() ||
      dst_frame->visible_rect().x() != 0 ||
      dst_frame->visible_rect().y() != 0) {
    VLOGF(1)
        << "The video frame visible size should be the same as the coded size";
    NotifyError(task_id, INVALID_ARGUMENT);
    return;
  }

  // Note that |surface->size()| is the visible size of the JPEG image. The
  // underlying VASurface size (coded size) can be larger because of alignments.
  if (surface->size().width() < dst_frame->visible_rect().width() ||
      surface->size().height() < dst_frame->visible_rect().height()) {
    VLOGF(1) << "Invalid JPEG image and video frame sizes: "
             << surface->size().ToString() << ", "
             << dst_frame->visible_rect().size().ToString();
    NotifyError(task_id, INVALID_ARGUMENT);
    return;
  }

  // For DMA-buf backed |dst_frame|, we will import it as a VA surface and use
  // VPP to convert the decoded |surface| into it, if the formats and sizes are
  // supported.
  const auto dst_frame_fourcc =
      Fourcc::FromVideoPixelFormat(dst_frame->format());
  if (!dst_frame_fourcc) {
    VLOGF(1) << "Unsupported video frame format: " << dst_frame->format();
    NotifyError(task_id, PLATFORM_FAILURE);
    return;
  }

  const auto dst_frame_va_fourcc = dst_frame_fourcc->ToVAFourCC();
  if (!dst_frame_va_fourcc) {
    VLOGF(1) << "Unsupported video frame format: " << dst_frame->format();
    NotifyError(task_id, PLATFORM_FAILURE);
    return;
  }

  // Crop and scale the decoded image into |dst_frame|.
  // The VPP is known to have some problems with odd-sized buffers, so we
  // request a crop rectangle whose dimensions are aligned to 2.
  const gfx::Rect crop_rect = CropSizeForScalingToTarget(
      surface->size(), dst_frame->visible_rect().size(), /*alignment=*/2u);
  if (crop_rect.IsEmpty()) {
    VLOGF(1) << "Failed to calculate crop rectangle for "
             << surface->size().ToString() << " to "
             << dst_frame->visible_rect().size().ToString();
    NotifyError(task_id, PLATFORM_FAILURE);
    return;
  }

  // TODO(kamesan): move HasDmaBufs() to DCHECK when we deprecate
  // shared-memory-backed video frame.
  // Check all the sizes involved until we figure out the definition of min/max
  // resolutions in the VPP profile (b/195312242).
  if (dst_frame->HasDmaBufs() &&
      VaapiWrapper::IsVppResolutionAllowed(surface->size()) &&
      VaapiWrapper::IsVppResolutionAllowed(crop_rect.size()) &&
      VaapiWrapper::IsVppResolutionAllowed(dst_frame->visible_rect().size()) &&
      VaapiWrapper::IsVppSupportedForJpegDecodedSurfaceToFourCC(
          surface->format(), *dst_frame_va_fourcc)) {
    if (!OutputPictureVppOnTaskRunner(task_id, surface, std::move(dst_frame),
                                      crop_rect)) {
      VLOGF(1) << "Output picture using VPP failed";
      NotifyError(task_id, PLATFORM_FAILURE);
    }
    return;
  }

  // Fallback to do conversion by libyuv. This happens when:
  // 1. |dst_frame| is backed by shared memory.
  // 2. VPP doesn't support the format conversion. This is intended for AMD
  //    VAAPI driver whose VPP only supports converting decoded 4:2:0 JPEGs.
  std::unique_ptr<ScopedVAImage> image =
      decoder_->GetImage(*dst_frame_va_fourcc, &status);
  if (status != VaapiImageDecodeStatus::kSuccess) {
    NotifyError(task_id, VaapiJpegDecodeStatusToError(status));
    return;
  }
  DCHECK_EQ(image->image()->width, surface->size().width());
  DCHECK_EQ(image->image()->height, surface->size().height());
  if (!OutputPictureLibYuvOnTaskRunner(task_id, std::move(image),
                                       std::move(dst_frame), crop_rect)) {
    VLOGF(1) << "Output picture using libyuv failed";
    NotifyError(task_id, PLATFORM_FAILURE);
  }
}

void VaapiMjpegDecodeAccelerator::Decode(
    BitstreamBuffer bitstream_buffer,
    scoped_refptr<VideoFrame> video_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("jpeg", __func__, "input_id", bitstream_buffer.id());

  DVLOGF(4) << "Mapping new input buffer id: " << bitstream_buffer.id()
            << " size: " << bitstream_buffer.size();

  if (bitstream_buffer.id() < 0) {
    VLOGF(1) << "Invalid bitstream_buffer, id: " << bitstream_buffer.id();
    NotifyError(bitstream_buffer.id(), INVALID_ARGUMENT);
    return;
  }

  // Validate output video frame.
  if (!video_frame->IsMappable() && !video_frame->HasDmaBufs()) {
    VLOGF(1) << "Unsupported output frame storage type";
    NotifyError(bitstream_buffer.id(), INVALID_ARGUMENT);
    return;
  }
  if ((video_frame->visible_rect().width() & 1) ||
      (video_frame->visible_rect().height() & 1)) {
    VLOGF(1) << "Video frame visible size has odd dimension";
    NotifyError(bitstream_buffer.id(), PLATFORM_FAILURE);
    return;
  }

  auto region = bitstream_buffer.TakeRegion();
  auto mapping =
      region.MapAt(bitstream_buffer.offset(), bitstream_buffer.size());
  if (!mapping.IsValid()) {
    VLOGF(1) << "Failed to map input buffer";
    NotifyError(bitstream_buffer.id(), UNREADABLE_INPUT);
    return;
  }

  // It's safe to use base::Unretained(this) because |decoder_task_runner_| runs
  // tasks on |decoder_thread_| which is stopped in the destructor of |this|.
  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiMjpegDecodeAccelerator::DecodeFromShmTask,
                                base::Unretained(this), bitstream_buffer.id(),
                                std::move(mapping), std::move(video_frame)));
}

void VaapiMjpegDecodeAccelerator::Decode(int32_t task_id,
                                         base::ScopedFD src_dmabuf_fd,
                                         size_t src_size,
                                         off_t src_offset,
                                         scoped_refptr<VideoFrame> dst_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("jpeg", __func__, "task_id", task_id);

  if (task_id < 0) {
    VLOGF(1) << "Invalid task id: " << task_id;
    NotifyError(task_id, INVALID_ARGUMENT);
    return;
  }

  // Validate input arguments.
  if (!src_dmabuf_fd.is_valid()) {
    VLOGF(1) << "Invalid input buffer FD";
    NotifyError(task_id, INVALID_ARGUMENT);
    return;
  }
  if (src_size == 0) {
    VLOGF(1) << "Input buffer size is zero";
    NotifyError(task_id, INVALID_ARGUMENT);
    return;
  }
  const size_t page_size = base::GetPageSize();
  if (src_offset < 0 || src_offset % page_size != 0) {
    VLOGF(1) << "Input buffer offset (" << src_offset
             << ") should be non-negative and aligned to page size ("
             << page_size << ")";
    NotifyError(task_id, INVALID_ARGUMENT);
    return;
  }

  // Validate output video frame.
  if (!dst_frame->IsMappable() && !dst_frame->HasDmaBufs()) {
    VLOGF(1) << "Unsupported output frame storage type";
    NotifyError(task_id, INVALID_ARGUMENT);
    return;
  }
  if ((dst_frame->visible_rect().width() & 1) ||
      (dst_frame->visible_rect().height() & 1)) {
    VLOGF(1) << "Output frame visible size has odd dimension";
    NotifyError(task_id, PLATFORM_FAILURE);
    return;
  }

  // It's safe to use base::Unretained(this) because |decoder_task_runner_| runs
  // tasks on |decoder_thread_| which is stopped in the destructor of |this|.
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiMjpegDecodeAccelerator::DecodeFromDmaBufTask,
                     base::Unretained(this), task_id, std::move(src_dmabuf_fd),
                     src_size, src_offset, std::move(dst_frame)));
}

bool VaapiMjpegDecodeAccelerator::IsSupported() {
  return VaapiWrapper::IsDecodeSupported(VAProfileJPEGBaseline);
}

}  // namespace media
