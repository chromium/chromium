// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_mjpeg_decode_accelerator.h"

#include <stddef.h>
#include <sys/mman.h>
#include <va/va.h>

#include <array>
#include <optional>
#include <utility>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/format_utils.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_util.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/libyuv_image_processor_backend.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
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

class VaapiMjpegDecodeAccelerator::Decoder final {
 public:
  Decoder(base::RepeatingCallback<void(int32_t task_id)> video_frame_ready_cb,
          base::RepeatingCallback<void(int32_t task_id, Error)> error_cb);
  ~Decoder();

  void Initialize(chromeos_camera::MjpegDecodeAccelerator::InitCB init_cb);

  // Processes one decode request.
  void DecodeFromShmTask(int32_t task_id,
                         base::WritableSharedMemoryMapping mapping,
                         scoped_refptr<VideoFrame> dst_frame);
  void DecodeFromDmaBufTask(int32_t task_id,
                            base::ScopedFD src_dmabuf_fd,
                            size_t src_size,
                            off_t src_offset,
                            scoped_refptr<VideoFrame> dst_frame);

 private:
  // Decodes the JPEG in |src_image| into |dst_frame| and notifies the client
  // when finished or when an error occurs.
  void DecodeImpl(int32_t task_id,
                  base::span<const uint8_t> src_image,
                  scoped_refptr<VideoFrame> dst_frame);

  void OnImageProcessorError();
  // Creates |image_processor_| for converting |src_frame| into |dst_frame|.
  void CreateImageProcessor(const VideoFrame* src_frame,
                            const VideoFrame* dst_frame);

  // Puts contents of |surface| within |crop_rect| into given |video_frame|
  // using VA-API Video Processing Pipeline (VPP), and passes the |task_id| of
  // the resulting picture to client for output.
  bool OutputPictureVpp(int32_t task_id,
                        const ScopedVASurface* surface,
                        scoped_refptr<VideoFrame> video_frame,
                        const gfx::Rect& crop_rect);

  // Puts contents of |image| within |crop_rect| into the given |video_frame|
  // using libyuv, and passes the |task_id| of the resulting picture to client
  // for output.
  bool OutputPictureLibYuv(int32_t task_id,
                           std::unique_ptr<ScopedVAImage> image,
                           scoped_refptr<VideoFrame> video_frame,
                           const gfx::Rect& crop_rect);

  const base::RepeatingCallback<void(int32_t task_id)> video_frame_ready_cb_;
  const base::RepeatingCallback<void(int32_t task_id, Error)> error_cb_;

  std::unique_ptr<media::VaapiJpegDecoder> decoder_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);
  // VaapiWrapper for VPP context. This is used to convert decoded data into
  // client buffer.
  scoped_refptr<VaapiWrapper> vpp_vaapi_wrapper_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);
  // Image processor to convert the decoded frame into client buffer when VA-API
  // is not capable.
  std::unique_ptr<ImageProcessorBackend> image_processor_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  SEQUENCE_CHECKER(decoder_sequence_checker_);
};

VaapiMjpegDecodeAccelerator::Decoder::Decoder(
    base::RepeatingCallback<void(int32_t task_id)> video_frame_ready_cb,
    base::RepeatingCallback<void(int32_t task_id, Error)> error_cb)
    : video_frame_ready_cb_(std::move(video_frame_ready_cb)),
      error_cb_(std::move(error_cb)) {
  // Decoder is constructed on |io_task_runner_|.
  DETACH_FROM_SEQUENCE(decoder_sequence_checker_);
}

VaapiMjpegDecodeAccelerator::Decoder::~Decoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
}

void VaapiMjpegDecodeAccelerator::Decoder::Initialize(
    chromeos_camera::MjpegDecodeAccelerator::InitCB init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  decoder_ = std::make_unique<media::VaapiJpegDecoder>();
  if (!decoder_->Initialize(base::BindRepeating(
          &ReportVaapiErrorToUMA,
          "Media.VaapiMjpegDecodeAccelerator.VAAPIError"))) {
    VLOGF(1) << "Failed initializing |decoder_|";
    std::move(init_cb).Run(false);
    return;
  }

  vpp_vaapi_wrapper_ =
      VaapiWrapper::Create(
          VaapiWrapper::kVideoProcess, VAProfileNone,
          EncryptionScheme::kUnencrypted,
          base::BindRepeating(
              &ReportVaapiErrorToUMA,
              "Media.VaapiMjpegDecodeAccelerator.Vpp.VAAPIError"))
          .value_or(nullptr);
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

void VaapiMjpegDecodeAccelerator::Decoder::DecodeFromShmTask(
    int32_t task_id,
    base::WritableSharedMemoryMapping mapping,
    scoped_refptr<VideoFrame> dst_frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  TRACE_EVENT0("jpeg", __func__);

  auto src_image = mapping.GetMemoryAsSpan<uint8_t>();
  DecodeImpl(task_id, src_image, std::move(dst_frame));
}

void VaapiMjpegDecodeAccelerator::Decoder::DecodeFromDmaBufTask(
    int32_t task_id,
    base::ScopedFD src_dmabuf_fd,
    size_t src_size,
    off_t src_offset,
    scoped_refptr<VideoFrame> dst_frame) {
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  TRACE_EVENT0("jpeg", __func__);

  // The DMA-buf FD should be mapped as read-only since it may only have read
  // permission, e.g. when it comes from camera driver.
  DCHECK(src_dmabuf_fd.is_valid());
  DCHECK_GT(src_size, 0u);
  void* src_addr = mmap(nullptr, src_size, PROT_READ, MAP_SHARED,
                        src_dmabuf_fd.get(), src_offset);
  if (src_addr == MAP_FAILED) {
    VPLOGF(1) << "Failed to map input DMA buffer";
    error_cb_.Run(task_id, UNREADABLE_INPUT);
    return;
  }
  base::span<const uint8_t> src_image =
      base::make_span(static_cast<const uint8_t*>(src_addr), src_size);

  DecodeImpl(task_id, src_image, std::move(dst_frame));

  const int ret = munmap(src_addr, src_size);
  DPCHECK(ret == 0);
}

void VaapiMjpegDecodeAccelerator::Decoder::DecodeImpl(
    int32_t task_id,
    base::span<const uint8_t> src_image,
    scoped_refptr<VideoFrame> dst_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  VaapiImageDecodeStatus status = decoder_->Decode(src_image);
  if (status != VaapiImageDecodeStatus::kSuccess) {
    VLOGF(1) << "Failed to decode JPEG image";
    error_cb_.Run(task_id, VaapiJpegDecodeStatusToError(status));
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
    error_cb_.Run(task_id, INVALID_ARGUMENT);
    return;
  }

  // Note that |surface->size()| is the visible size of the JPEG image. The
  // underlying VASurface size (coded size) can be larger because of alignments.
  if (surface->size().width() < dst_frame->visible_rect().width() ||
      surface->size().height() < dst_frame->visible_rect().height()) {
    VLOGF(1) << "Invalid JPEG image and video frame sizes: "
             << surface->size().ToString() << ", "
             << dst_frame->visible_rect().size().ToString();
    error_cb_.Run(task_id, INVALID_ARGUMENT);
    return;
  }

  // For DMA-buf backed |dst_frame|, we will import it as a VA surface and use
  // VPP to convert the decoded |surface| into it, if the formats and sizes are
  // supported.
  const auto dst_frame_fourcc =
      Fourcc::FromVideoPixelFormat(dst_frame->format());
  if (!dst_frame_fourcc) {
    VLOGF(1) << "Unsupported video frame format: " << dst_frame->format();
    error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }

  const auto dst_frame_va_fourcc = dst_frame_fourcc->ToVAFourCC();
  if (!dst_frame_va_fourcc) {
    VLOGF(1) << "Unsupported video frame format: " << dst_frame->format();
    error_cb_.Run(task_id, PLATFORM_FAILURE);
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
    error_cb_.Run(task_id, PLATFORM_FAILURE);
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
    if (!OutputPictureVpp(task_id, surface, std::move(dst_frame), crop_rect)) {
      VLOGF(1) << "Output picture using VPP failed";
      error_cb_.Run(task_id, PLATFORM_FAILURE);
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
    error_cb_.Run(task_id, VaapiJpegDecodeStatusToError(status));
    return;
  }
  DCHECK_EQ(image->image()->width, surface->size().width());
  DCHECK_EQ(image->image()->height, surface->size().height());
  if (!OutputPictureLibYuv(task_id, std::move(image), std::move(dst_frame),
                           crop_rect)) {
    VLOGF(1) << "Output picture using libyuv failed";
    error_cb_.Run(task_id, PLATFORM_FAILURE);
  }
}

void VaapiMjpegDecodeAccelerator::Decoder::OnImageProcessorError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
  VLOGF(1) << "Failed to process frames using the libyuv image processor";
  error_cb_.Run(kInvalidTaskId, PLATFORM_FAILURE);
  image_processor_.reset();
}

void VaapiMjpegDecodeAccelerator::Decoder::CreateImageProcessor(
    const VideoFrame* src_frame,
    const VideoFrame* dst_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

  // The fourcc of |src_frame| will be either Fourcc(YUYV) or Fourcc(YU12) based
  // on the implementation of OutputPictureLibYuvOnTaskRunner(). The fourcc of
  // |dst_frame| should have been validated in DecodeImpl().
  const auto src_fourcc = Fourcc::FromVideoPixelFormat(src_frame->format());
  DCHECK(src_fourcc.has_value());
  const auto dst_fourcc = Fourcc::FromVideoPixelFormat(dst_frame->format());
  DCHECK(dst_fourcc.has_value());
  const ImageProcessorBackend::PortConfig input_config(
      *src_fourcc, src_frame->coded_size(), src_frame->layout().planes(),
      src_frame->visible_rect(), src_frame->storage_type());
  const ImageProcessorBackend::PortConfig output_config(
      *dst_fourcc, dst_frame->coded_size(), dst_frame->layout().planes(),
      dst_frame->visible_rect(), dst_frame->storage_type());
  if (image_processor_ && image_processor_->input_config() == input_config &&
      image_processor_->output_config() == output_config) {
    return;
  }

  // The error callback is posted to the same thread that
  // LibYUVImageProcessorBackend::Create() is called on
  // (i.e., |decoder_thread_|) and we control the lifetime of |decoder_thread_|.
  // Therefore, base::Unretained(this) is safe.
  image_processor_ = LibYUVImageProcessorBackend::CreateWithTaskRunner(
      input_config, output_config, ImageProcessorBackend::OutputMode::IMPORT,
      base::BindRepeating(
          &VaapiMjpegDecodeAccelerator::Decoder::OnImageProcessorError,
          base::Unretained(this)),
      base::SequencedTaskRunner::GetCurrentDefault());
}

bool VaapiMjpegDecodeAccelerator::Decoder::OutputPictureLibYuv(
    int32_t task_id,
    std::unique_ptr<ScopedVAImage> scoped_image,
    scoped_refptr<VideoFrame> video_frame,
    const gfx::Rect& crop_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);

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
          [](base::RepeatingCallback<void(int32_t task_id)>
                 video_frame_ready_cb,
             int32_t task_id, scoped_refptr<VideoFrame> frame) {
            video_frame_ready_cb.Run(task_id);
          },
          video_frame_ready_cb_, task_id));
  return true;
}

bool VaapiMjpegDecodeAccelerator::Decoder::OutputPictureVpp(
    int32_t task_id,
    const ScopedVASurface* surface,
    scoped_refptr<VideoFrame> video_frame,
    const gfx::Rect& crop_rect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(decoder_sequence_checker_);
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
  const std::unique_ptr<ScopedVASurface> output_surface =
      vpp_vaapi_wrapper_->CreateVASurfaceForPixmap(std::move(pixmap));
  if (!output_surface) {
    VLOGF(1) << "Cannot create VA surface for output buffer";
    return false;
  }

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
  if (!vpp_vaapi_wrapper_->BlitSurface(surface->id(), surface->size(),
                                       output_surface->id(),
                                       output_surface->size(), crop_rect)) {
    VLOGF(1) << "Cannot convert decoded image into output buffer";
    return false;
  }

  // Sync target surface since the buffer is returning to client.
  if (!vpp_vaapi_wrapper_->SyncSurface(output_surface->id())) {
    VLOGF(1) << "Cannot sync VPP output surface";
    return false;
  }

  video_frame_ready_cb_.Run(task_id);
  return true;
}

void VaapiMjpegDecodeAccelerator::NotifyError(int32_t task_id, Error error) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  VLOGF(1) << "Notifying of error " << error;
  // |error| shouldn't be NO_ERRORS because successful decodes should be handled
  // by VideoFrameReady().
  DCHECK_NE(chromeos_camera::MjpegDecodeAccelerator::Error::NO_ERRORS, error);
  ReportToVAJDAResponseToClientUMA(error);
  DCHECK(client_);
  client_->NotifyError(task_id, error);
}

void VaapiMjpegDecodeAccelerator::VideoFrameReady(int32_t task_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ReportToVAJDAResponseToClientUMA(
      chromeos_camera::MjpegDecodeAccelerator::Error::NO_ERRORS);
  client_->VideoFrameReady(task_id);
}

VaapiMjpegDecodeAccelerator::VaapiMjpegDecodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : io_task_runner_(io_task_runner),
      weak_this_factory_(this) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
}

VaapiMjpegDecodeAccelerator::~VaapiMjpegDecodeAccelerator() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  VLOGF(2) << "Destroying VaapiMjpegDecodeAccelerator";
  weak_this_factory_.InvalidateWeakPtrs();

  if (decoder_task_runner_) {
    decoder_task_runner_->DeleteSoon(FROM_HERE, std::move(decoder_));
  }
}

void VaapiMjpegDecodeAccelerator::InitializeAsync(
    chromeos_camera::MjpegDecodeAccelerator::Client* client,
    chromeos_camera::MjpegDecodeAccelerator::InitCB init_cb) {
  VLOGF(2);
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  client_ = client;
  decoder_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE});
  DCHECK(decoder_task_runner_);

  auto video_frame_ready_cb = base::BindPostTask(
      io_task_runner_,
      base::BindRepeating(&VaapiMjpegDecodeAccelerator::VideoFrameReady,
                          weak_this_factory_.GetWeakPtr()));
  auto error_cb = base::BindPostTask(
      io_task_runner_,
      base::BindRepeating(&VaapiMjpegDecodeAccelerator::NotifyError,
                          weak_this_factory_.GetWeakPtr()));
  decoder_ = std::make_unique<Decoder>(std::move(video_frame_ready_cb),
                                       std::move(error_cb));
  // base::Unretained(decoder_.get()) is safe because |decoder_| is posted
  // to |decoder_task_runner_| with DeleteSoon() in destructor.
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiMjpegDecodeAccelerator::Decoder::Initialize,
                     base::Unretained(decoder_.get()),
                     base::BindPostTaskToCurrentDefault(std::move(init_cb))));
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

  // base::Unretained(decoder_.get()) is safe because |decoder_| is posted
  // to |decoder_task_runner_| with DeleteSoon() in destructor.
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiMjpegDecodeAccelerator::Decoder::DecodeFromShmTask,
                     base::Unretained(decoder_.get()), bitstream_buffer.id(),
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

  // base::Unretained(decoder_.get()) is safe because |decoder_| is posted
  // to |decoder_task_runner_| with DeleteSoon() in destructor.
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VaapiMjpegDecodeAccelerator::Decoder::DecodeFromDmaBufTask,
          base::Unretained(decoder_.get()), task_id, std::move(src_dmabuf_fd),
          src_size, src_offset, std::move(dst_frame)));
}

bool VaapiMjpegDecodeAccelerator::IsSupported() {
  return VaapiWrapper::IsDecodeSupported(VAProfileJPEGBaseline);
}

}  // namespace media
