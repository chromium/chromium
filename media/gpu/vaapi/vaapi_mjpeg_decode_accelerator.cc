// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_mjpeg_decode_accelerator.h"

#include <stddef.h>
#include <sys/mman.h>
#include <va/va.h>

#include <array>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/files/scoped_file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/process/process_metrics.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/format_utils.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/linux/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace media {

namespace {

// UMA errors that the VaapiMjpegDecodeAccelerator class reports.
enum VAJDAFailure {
  VAAPI_ERROR = 0,
  VAJDA_FAILURES_MAX,
};

static void ReportToVAJDADecoderFailureUMA(VAJDAFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAJDA.DecoderFailure", failure,
                            VAJDA_FAILURES_MAX + 1);
}

static void ReportToVAJDAVppFailureUMA(VAJDAFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAJDA.VppFailure", failure,
                            VAJDA_FAILURES_MAX + 1);
}

static void ReportToVAJDAResponseToClientUMA(
    chromeos_camera::MjpegDecodeAccelerator::Error response) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.VAJDA.ResponseToClient", response,
      chromeos_camera::MjpegDecodeAccelerator::Error::MJDA_ERROR_CODE_MAX + 1);
}

static chromeos_camera::MjpegDecodeAccelerator::Error
VaapiJpegDecodeStatusToError(VaapiImageDecodeStatus status) {
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

static bool VerifyDataSize(const VAImage* image) {
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
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner),
      client_(nullptr),
      decoder_thread_("VaapiMjpegDecoderThread"),
      weak_this_factory_(this) {}

VaapiMjpegDecodeAccelerator::~VaapiMjpegDecodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOGF(2) << "Destroying VaapiMjpegDecodeAccelerator";

  weak_this_factory_.InvalidateWeakPtrs();
  decoder_thread_.Stop();
}

bool VaapiMjpegDecodeAccelerator::Initialize(
    chromeos_camera::MjpegDecodeAccelerator::Client* client) {
  VLOGF(2);
  DCHECK(task_runner_->BelongsToCurrentThread());

  client_ = client;

  if (!decoder_.Initialize(
          base::BindRepeating(&ReportToVAJDADecoderFailureUMA, VAAPI_ERROR))) {
    return false;
  }

  vpp_vaapi_wrapper_ = VaapiWrapper::Create(
      VaapiWrapper::kVideoProcess, VAProfileNone,
      base::BindRepeating(&ReportToVAJDAVppFailureUMA, VAAPI_ERROR));
  if (!vpp_vaapi_wrapper_) {
    VLOGF(1) << "Failed initializing VAAPI for VPP";
    return false;
  }

  // Size is irrelevant for a VPP context.
  if (!vpp_vaapi_wrapper_->CreateContext(gfx::Size())) {
    VLOGF(1) << "Failed to create context for VPP";
    return false;
  }

  gpu_memory_buffer_support_ = std::make_unique<gpu::GpuMemoryBufferSupport>();

  if (!decoder_thread_.Start()) {
    VLOGF(1) << "Failed to start decoding thread.";
    return false;
  }
  decoder_task_runner_ = decoder_thread_.task_runner();

  return true;
}

bool VaapiMjpegDecodeAccelerator::OutputPictureLibYuvOnTaskRunner(
    std::unique_ptr<ScopedVAImage> scoped_image,
    int32_t input_buffer_id,
    scoped_refptr<VideoFrame> video_frame) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  TRACE_EVENT1("jpeg", __func__, "input_buffer_id", input_buffer_id);

  DCHECK(scoped_image);
  const VAImage* image = scoped_image->image();

  // For camera captures, we assume that the visible size is the same as the
  // coded size.
  DCHECK_EQ(video_frame->visible_rect().size(), video_frame->coded_size());
  DCHECK_EQ(0, video_frame->visible_rect().x());
  DCHECK_EQ(0, video_frame->visible_rect().y());
  DCHECK(decoder_.GetScopedVASurface());
  const gfx::Size visible_size(base::strict_cast<int>(image->width),
                               base::strict_cast<int>(image->height));
  if (visible_size != video_frame->visible_rect().size()) {
    VLOGF(1) << "The decoded visible size is not the same as the video frame's";
    return false;
  }

  // The decoded image size is aligned up to JPEG MCU size, so it may be larger
  // than |video_frame|'s visible size.
  if (base::strict_cast<int>(image->width) < visible_size.width() ||
      base::strict_cast<int>(image->height) < visible_size.height()) {
    VLOGF(1) << "Decoded image size is smaller than output frame size";
    return false;
  }
  DCHECK(VerifyDataSize(image));

  // Extract source pointers and strides.
  auto* const mem =
      static_cast<const uint8_t*>(scoped_image->va_buffer()->data());
  std::array<const uint8_t*, VideoFrame::kMaxPlanes> src_ptrs{};
  std::array<int, VideoFrame::kMaxPlanes> src_strides{};
  for (uint32_t i = 0; i < image->num_planes; i++) {
    src_ptrs[i] = mem + image->offsets[i];
    if (!base::CheckedNumeric<uint32_t>(image->pitches[i])
             .AssignIfValid(&src_strides[i])) {
      VLOGF(1) << "Can't extract the strides";
      return false;
    }
  }

  // Extract destination pointers and strides.
  std::array<uint8_t*, VideoFrame::kMaxPlanes> dst_ptrs{};
  std::array<int, VideoFrame::kMaxPlanes> dst_strides{};
  base::ScopedClosureRunner buffer_unmapper;
  if (video_frame->HasDmaBufs()) {
    // Dmabuf-backed frame needs to be mapped for SW access.
    DCHECK(gpu_memory_buffer_support_);
    base::Optional<gfx::BufferFormat> gfx_format =
        VideoPixelFormatToGfxBufferFormat(video_frame->format());
    if (!gfx_format) {
      VLOGF(1) << "Unsupported format: " << video_frame->format();
      return false;
    }
    auto gmb_handle = CreateGpuMemoryBufferHandle(video_frame.get());
    DCHECK(!gmb_handle.is_null());
    std::unique_ptr<gpu::GpuMemoryBufferImpl> gmb =
        gpu_memory_buffer_support_->CreateGpuMemoryBufferImplFromHandle(
            std::move(gmb_handle), video_frame->coded_size(), *gfx_format,
            gfx::BufferUsage::SCANOUT_CPU_READ_WRITE, base::DoNothing());
    if (!gmb) {
      VLOGF(1) << "Failed to create GPU memory buffer";
      return false;
    }
    if (!gmb->Map()) {
      VLOGF(1) << "Failed to map GPU memory buffer";
      return false;
    }
    for (size_t i = 0; i < video_frame->layout().num_planes(); i++) {
      dst_ptrs[i] = static_cast<uint8_t*>(gmb->memory(i));
      dst_strides[i] = gmb->stride(i);
    }
    buffer_unmapper.ReplaceClosure(
        base::BindOnce(&gpu::GpuMemoryBufferImpl::Unmap, std::move(gmb)));
  } else {
    DCHECK(video_frame->IsMappable());
    for (size_t i = 0; i < video_frame->layout().num_planes(); i++) {
      dst_ptrs[i] = video_frame->visible_data(i);
      dst_strides[i] = video_frame->stride(i);
    }
  }

  switch (image->format.fourcc) {
    case VA_FOURCC_I420:
      DCHECK_EQ(image->num_planes, 3u);
      switch (video_frame->format()) {
        case PIXEL_FORMAT_I420:
          DCHECK_EQ(video_frame->layout().num_planes(), 3u);
          if (libyuv::I420Copy(src_ptrs[0], src_strides[0], src_ptrs[1],
                               src_strides[1], src_ptrs[2], src_strides[2],
                               dst_ptrs[0], dst_strides[0], dst_ptrs[1],
                               dst_strides[1], dst_ptrs[2], dst_strides[2],
                               visible_size.width(), visible_size.height())) {
            VLOGF(1) << "I420Copy failed";
            return false;
          }
          break;
        case PIXEL_FORMAT_NV12:
          DCHECK_EQ(video_frame->layout().num_planes(), 2u);
          if (libyuv::I420ToNV12(src_ptrs[0], src_strides[0], src_ptrs[1],
                                 src_strides[1], src_ptrs[2], src_strides[2],
                                 dst_ptrs[0], dst_strides[0], dst_ptrs[1],
                                 dst_strides[1], visible_size.width(),
                                 visible_size.height())) {
            VLOGF(1) << "I420ToNV12 failed";
            return false;
          }
          break;
        default:
          VLOGF(1) << "Can't convert image from I420 to "
                   << video_frame->format();
          return false;
      }
      break;
    case VA_FOURCC_YUY2:
    case VA_FOURCC('Y', 'U', 'Y', 'V'):
      DCHECK_EQ(image->num_planes, 1u);
      switch (video_frame->format()) {
        case PIXEL_FORMAT_I420:
          DCHECK_EQ(video_frame->layout().num_planes(), 3u);
          if (libyuv::YUY2ToI420(src_ptrs[0], src_strides[0], dst_ptrs[0],
                                 dst_strides[0], dst_ptrs[1], dst_strides[1],
                                 dst_ptrs[2], dst_strides[2],
                                 visible_size.width(), visible_size.height())) {
            VLOGF(1) << "YUY2ToI420 failed";
            return false;
          }
          break;
        case PIXEL_FORMAT_NV12:
          DCHECK_EQ(video_frame->layout().num_planes(), 2u);
          if (libyuv::YUY2ToNV12(src_ptrs[0], src_strides[0], dst_ptrs[0],
                                 dst_strides[0], dst_ptrs[1], dst_strides[1],
                                 visible_size.width(), visible_size.height())) {
            VLOGF(1) << "YUY2ToNV12 failed";
            return false;
          }
          break;
        default:
          VLOGF(1) << "Can't convert image from YUYV to "
                   << video_frame->format();
          return false;
      }
      break;
    default:
      VLOGF(1) << "Can't convert image from "
               << FourccToString(image->format.fourcc) << " to "
               << video_frame->format();
      return false;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiMjpegDecodeAccelerator::VideoFrameReady,
                     weak_this_factory_.GetWeakPtr(), input_buffer_id));

  return true;
}

bool VaapiMjpegDecodeAccelerator::OutputPictureVppOnTaskRunner(
    const ScopedVASurface* surface,
    int32_t input_buffer_id,
    scoped_refptr<VideoFrame> video_frame) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  DCHECK(surface);

  TRACE_EVENT1("jpeg", __func__, "input_buffer_id", input_buffer_id);

  // Bind a VA surface to |video_frame|.
  scoped_refptr<VASurface> output_surface =
      vpp_vaapi_wrapper_->CreateVASurfaceForVideoFrame(video_frame.get());
  if (!output_surface) {
    VLOGF(1) << "Cannot create VA surface for output buffer";
    return false;
  }

  // Use VPP to blit the visible size region within |surface| into
  // |output_surface|. BlitSurface() does scaling not cropping when source and
  // destination sizes don't match, so we manipulate the sizes of surfaces to
  // effectively do the cropping.
  const gfx::Size& blit_size = video_frame->visible_rect().size();
  if (surface->size().width() < blit_size.width() ||
      surface->size().height() < blit_size.height()) {
    VLOGF(1) << "Decoded surface size is smaller than target size";
    return false;
  }
  scoped_refptr<VASurface> src_surface = base::MakeRefCounted<VASurface>(
      surface->id(), blit_size, surface->format(),
      base::DoNothing() /* release_cb */);
  scoped_refptr<VASurface> dst_surface = base::MakeRefCounted<VASurface>(
      output_surface->id(), blit_size, output_surface->format(),
      base::DoNothing() /* release_cb */);

  // We should call vaSyncSurface() when passing surface between contexts. See:
  // https://lists.01.org/pipermail/intel-vaapi-media/2019-June/000131.html
  if (!vpp_vaapi_wrapper_->SyncSurface(src_surface->id())) {
    VLOGF(1) << "Cannot sync VPP input surface";
    return false;
  }
  if (!vpp_vaapi_wrapper_->BlitSurface(src_surface, dst_surface)) {
    VLOGF(1) << "Cannot convert decoded image into output buffer";
    return false;
  }

  // Sync target surface since the buffer is returning to client.
  if (!vpp_vaapi_wrapper_->SyncSurface(dst_surface->id())) {
    VLOGF(1) << "Cannot sync VPP output surface";
    return false;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiMjpegDecodeAccelerator::VideoFrameReady,
                     weak_this_factory_.GetWeakPtr(), input_buffer_id));

  return true;
}

void VaapiMjpegDecodeAccelerator::DecodeFromShmTask(
    int32_t task_id,
    std::unique_ptr<UnalignedSharedMemory> shm,
    scoped_refptr<VideoFrame> dst_frame) {
  DVLOGF(4);
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("jpeg", __func__);

  auto src_image =
      base::make_span(static_cast<const uint8_t*>(shm->memory()), shm->size());
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
  // TODO(andrescj): validate that the video frame's visible size is the same as
  // the parsed JPEG's visible size when it is returned from Decode(), and
  // remove the size checks in OutputPicture*().
  VaapiImageDecodeStatus status = decoder_.Decode(src_image);
  if (status != VaapiImageDecodeStatus::kSuccess) {
    NotifyError(task_id, VaapiJpegDecodeStatusToError(status));
    return;
  }
  const ScopedVASurface* surface = decoder_.GetScopedVASurface();
  DCHECK(surface);
  DCHECK(surface->IsValid());

  // For DMA-buf backed |dst_frame|, we will import it as a VA surface and use
  // VPP to convert the decoded |surface| into it, if the formats and sizes are
  // supported.
  const uint32_t video_frame_va_fourcc =
      Fourcc::FromVideoPixelFormat(dst_frame->format()).ToVAFourCC();
  if (video_frame_va_fourcc == kInvalidVaFourcc) {
    VLOGF(1) << "Unsupported video frame format: " << dst_frame->format();
    NotifyError(task_id, PLATFORM_FAILURE);
    return;
  }
  // TODO(kamesan): move HasDmaBufs() to DCHECK when we deprecate
  // shared-memory-backed video frame.
  if (dst_frame->HasDmaBufs() &&
      VaapiWrapper::IsVppResolutionAllowed(surface->size()) &&
      VaapiWrapper::IsVppSupportedForJpegDecodedSurfaceToFourCC(
          surface->format(), video_frame_va_fourcc)) {
    if (!OutputPictureVppOnTaskRunner(surface, task_id, std::move(dst_frame))) {
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
      decoder_.GetImage(video_frame_va_fourcc, &status);
  if (status != VaapiImageDecodeStatus::kSuccess) {
    NotifyError(task_id, VaapiJpegDecodeStatusToError(status));
    return;
  }
  if (!OutputPictureLibYuvOnTaskRunner(std::move(image), task_id,
                                       std::move(dst_frame))) {
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

  // UnalignedSharedMemory will take over the |bitstream_buffer.handle()|.
  auto shm = std::make_unique<UnalignedSharedMemory>(
      bitstream_buffer.TakeRegion(), bitstream_buffer.size(),
      false /* read_only */);

  if (!shm->MapAt(bitstream_buffer.offset(), bitstream_buffer.size())) {
    VLOGF(1) << "Failed to map input buffer";
    NotifyError(bitstream_buffer.id(), UNREADABLE_INPUT);
    return;
  }

  // It's safe to use base::Unretained(this) because |decoder_task_runner_| runs
  // tasks on |decoder_thread_| which is stopped in the destructor of |this|.
  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiMjpegDecodeAccelerator::DecodeFromShmTask,
                                base::Unretained(this), bitstream_buffer.id(),
                                std::move(shm), std::move(video_frame)));
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
