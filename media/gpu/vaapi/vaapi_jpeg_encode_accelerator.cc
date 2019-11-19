// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_jpeg_encode_accelerator.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/gpu/linux/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_jpeg_encoder.h"
#include "media/parsers/jpeg_parser.h"

namespace media {

namespace {

// UMA results that the VaapiJpegEncodeAccelerator class reports.
// These values are persisted to logs, and should therefore never be renumbered
// nor reused.
enum VAJEAEncoderResult {
  kSuccess = 0,
  kError,
  kMaxValue = kError,
};

static void ReportToVAJEAEncodeResultUMA(VAJEAEncoderResult result) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAJEA.EncoderResult", result);
}

static void ReportToVAJEAVppFailureUMA(VAJEAEncoderResult result) {
  base::UmaHistogramEnumeration("Media.VAJEA.VppFailure", result);
}
}  // namespace

VaapiJpegEncodeAccelerator::EncodeRequest::EncodeRequest(
    int32_t task_id,
    scoped_refptr<VideoFrame> video_frame,
    std::unique_ptr<UnalignedSharedMemory> exif_shm,
    std::unique_ptr<UnalignedSharedMemory> output_shm,
    int quality)
    : task_id(task_id),
      video_frame(std::move(video_frame)),
      exif_shm(std::move(exif_shm)),
      output_shm(std::move(output_shm)),
      quality(quality) {}

VaapiJpegEncodeAccelerator::EncodeRequest::~EncodeRequest() {}

class VaapiJpegEncodeAccelerator::Encoder {
 public:
  Encoder(scoped_refptr<VaapiWrapper> vaapi_wrapper,
          scoped_refptr<VaapiWrapper> vpp_vaapi_wrapper,
          base::RepeatingCallback<void(int32_t, size_t)> video_frame_ready_cb,
          base::RepeatingCallback<void(int32_t, Status)> notify_error_cb);
  ~Encoder();

  // Processes one encode task with DMA-buf.
  void EncodeWithDmaBufTask(
      scoped_refptr<VideoFrame> input_frame,
      scoped_refptr<VideoFrame> output_frame,
      int32_t task_id,
      int quality,
      std::unique_ptr<WritableUnalignedMapping> exif_mapping);

  // Processes one encode |request|.
  void EncodeTask(std::unique_ptr<EncodeRequest> request);

 private:
  // |cached_output_buffer_id_| is the last allocated VABuffer during
  // EncodeTask() and |cached_output_buffer_size_| is the size of it.
  // If the next call to EncodeTask() does not require a buffer bigger than
  // |cached_output_buffer_size_|, |cached_output_buffer_id_| will be reused.
  size_t cached_output_buffer_size_;
  VABufferID cached_output_buffer_id_;

  std::unique_ptr<VaapiJpegEncoder> jpeg_encoder_;
  scoped_refptr<VaapiWrapper> vaapi_wrapper_;
  scoped_refptr<VaapiWrapper> vpp_vaapi_wrapper_;
  std::unique_ptr<gpu::GpuMemoryBufferSupport> gpu_memory_buffer_support_;

  base::RepeatingCallback<void(int32_t, size_t)> video_frame_ready_cb_;
  base::RepeatingCallback<void(int32_t, Status)> notify_error_cb_;

  // The current VA surface ID used for encoding. Only used for Non-DMA-buf use
  // case.
  VASurfaceID va_surface_id_;
  // The size of the surface associated with |va_surface_id_|.
  gfx::Size input_size_;
  // The format used to create VAContext. Only used for DMA-buf use case.
  uint32_t va_format_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(Encoder);
};

VaapiJpegEncodeAccelerator::Encoder::Encoder(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    scoped_refptr<VaapiWrapper> vpp_vaapi_wrapper,
    base::RepeatingCallback<void(int32_t, size_t)> video_frame_ready_cb,
    base::RepeatingCallback<void(int32_t, Status)> notify_error_cb)
    : cached_output_buffer_size_(0),
      jpeg_encoder_(new VaapiJpegEncoder(vaapi_wrapper)),
      vaapi_wrapper_(std::move(vaapi_wrapper)),
      vpp_vaapi_wrapper_(std::move(vpp_vaapi_wrapper)),
      gpu_memory_buffer_support_(new gpu::GpuMemoryBufferSupport()),
      video_frame_ready_cb_(std::move(video_frame_ready_cb)),
      notify_error_cb_(std::move(notify_error_cb)),
      va_surface_id_(VA_INVALID_SURFACE),
      input_size_(gfx::Size()),
      va_format_(0) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VaapiJpegEncodeAccelerator::Encoder::~Encoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VaapiJpegEncodeAccelerator::Encoder::EncodeWithDmaBufTask(
    scoped_refptr<VideoFrame> input_frame,
    scoped_refptr<VideoFrame> output_frame,
    int32_t task_id,
    int quality,
    std::unique_ptr<WritableUnalignedMapping> exif_mapping) {
  DVLOGF(4);
  TRACE_EVENT0("jpeg", "EncodeWithDmaBufTask");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  gfx::Size input_size = input_frame->coded_size();
  gfx::BufferFormat buffer_format = gfx::BufferFormat::YUV_420_BIPLANAR;
  uint32_t va_format = VaapiWrapper::BufferFormatToVARTFormat(buffer_format);
  bool context_changed = input_size != input_size_ || va_format != va_format_;
  if (context_changed) {
    vaapi_wrapper_->DestroyContextAndSurfaces(
        std::vector<VASurfaceID>({va_surface_id_}));
    va_surface_id_ = VA_INVALID_SURFACE;
    va_format_ = 0;
    input_size_ = gfx::Size();

    std::vector<VASurfaceID> va_surfaces;
    if (!vaapi_wrapper_->CreateContextAndSurfaces(
            va_format, input_size, VaapiWrapper::SurfaceUsageHint::kGeneric, 1,
            &va_surfaces)) {
      VLOGF(1) << "Failed to create VA surface";
      notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
      return;
    }
    va_surface_id_ = va_surfaces[0];
    va_format_ = va_format;
    input_size_ = input_size;
  }

  // We need to explicitly blit the bound input surface here to make sure the
  // input we sent to VAAPI encoder is in tiled NV12 format since implicit
  // tiling logic is not contained in every driver.
  auto input_surface =
      vpp_vaapi_wrapper_->CreateVASurfaceForVideoFrame(input_frame.get());
  if (!input_surface) {
    VLOGF(1) << "Failed to create input va surface";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }
  auto blit_surface =
      base::MakeRefCounted<VASurface>(va_surface_id_, input_size, va_format,
                                      base::DoNothing() /* release_cb */);
  if (!vpp_vaapi_wrapper_->BlitSurface(input_surface, blit_surface)) {
    VLOGF(1) << "Failed to blit surfaces";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }
  // We should call vaSyncSurface() when passing surface between contexts. See:
  // https://lists.01.org/pipermail/intel-vaapi-media/2019-June/000131.html
  // Sync |blit_surface| since it it passing to the JPEG encoding context.
  if (!vpp_vaapi_wrapper_->SyncSurface(blit_surface->id())) {
    VLOGF(1) << "Cannot sync VPP output surface";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }

  // Create output buffer for encoding result.
  size_t max_coded_buffer_size =
      VaapiJpegEncoder::GetMaxCodedBufferSize(input_size);
  if (context_changed || max_coded_buffer_size > cached_output_buffer_size_) {
    vaapi_wrapper_->DestroyVABuffers();
    cached_output_buffer_size_ = 0;

    VABufferID output_buffer_id;
    if (!vaapi_wrapper_->CreateVABuffer(max_coded_buffer_size,
                                        &output_buffer_id)) {
      VLOGF(1) << "Failed to create VA buffer for encoding output";
      notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
      return;
    }
    cached_output_buffer_size_ = max_coded_buffer_size;
    cached_output_buffer_id_ = output_buffer_id;
  }

  // Prepare exif.
  const uint8_t* exif_buffer;
  size_t exif_buffer_size = 0;
  if (exif_mapping) {
    exif_buffer = static_cast<const uint8_t*>(exif_mapping->memory());
    exif_buffer_size = exif_mapping->size();
  } else {
    exif_buffer = nullptr;
  }
  // When the exif buffer contains a thumbnail, the VAAPI encoder would
  // generate a corrupted JPEG. We can work around the problem by supplying an
  // all-zero buffer with the same size and fill in the real exif buffer after
  // encoding.
  // TODO(shenghao): Remove this mechanism after b/79840013 is fixed.
  std::vector<uint8_t> exif_buffer_dummy(exif_buffer_size, 0);
  size_t exif_offset = 0;

  if (!jpeg_encoder_->Encode(input_size, exif_buffer_dummy.data(),
                             exif_buffer_size, quality, blit_surface->id(),
                             cached_output_buffer_id_, &exif_offset)) {
    VLOGF(1) << "Encode JPEG failed";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }

  // Create gmb buffer from output VideoFrame. Since the JPEG VideoFrame's coded
  // size is the 2D image size, we should use (buffer_size, 1) as the R8 gmb's
  // size, where buffer_size can be obtained from the first plane's size.
  auto output_gmb_handle = CreateGpuMemoryBufferHandle(output_frame.get());
  DCHECK(!output_gmb_handle.is_null());

  // In this case, we use the R_8 buffer with height == 1 to represent a data
  // container. As a result, we use plane.stride as size of the data here since
  // plane.size might be larger due to height alignment.
  const gfx::Size output_gmb_buffer_size(
      base::checked_cast<int32_t>(output_frame->layout().planes()[0].stride),
      1);
  auto output_gmb_buffer =
      gpu_memory_buffer_support_->CreateGpuMemoryBufferImplFromHandle(
          std::move(output_gmb_handle), output_gmb_buffer_size,
          gfx::BufferFormat::R_8, gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE,
          base::DoNothing());
  if (output_gmb_buffer == nullptr) {
    VLOGF(1) << "Failed to create GpuMemoryBufferImpl from handle";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }

  bool isMapped = output_gmb_buffer->Map();
  if (!isMapped) {
    VLOGF(1) << "Map the output gmb buffer failed";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }

  // Get the encoded output. DownloadFromVABuffer() is a blocking call. It
  // would wait until encoding is finished.
  uint8_t* output_memory = static_cast<uint8_t*>(output_gmb_buffer->memory(0));
  size_t encoded_size = 0;
  // Since the format of |output_gmb_buffer| is gfx::BufferFormat::R_8, we can
  // use its area as the maximum bytes we need to download to avoid buffer
  // overflow.
  if (!vaapi_wrapper_->DownloadFromVABuffer(
          cached_output_buffer_id_, blit_surface->id(),
          static_cast<uint8_t*>(output_memory),
          output_gmb_buffer->GetSize().GetArea(), &encoded_size)) {
    VLOGF(1) << "Failed to retrieve output image from VA coded buffer";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);

    output_gmb_buffer->Unmap();
    return;
  }

  // Copy the real exif buffer into preserved space.
  memcpy(static_cast<uint8_t*>(output_memory) + exif_offset, exif_buffer,
         exif_buffer_size);

  output_gmb_buffer->Unmap();
  video_frame_ready_cb_.Run(task_id, encoded_size);
}

void VaapiJpegEncodeAccelerator::Encoder::EncodeTask(
    std::unique_ptr<EncodeRequest> request) {
  DVLOGF(4);
  TRACE_EVENT0("jpeg", "EncodeTask");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int task_id = request->task_id;
  gfx::Size input_size = request->video_frame->coded_size();

  // Recreate VASurface if the video frame's size changed.
  bool context_changed =
      input_size != input_size_ || va_surface_id_ == VA_INVALID_SURFACE;
  if (context_changed) {
    vaapi_wrapper_->DestroyContextAndSurfaces(
        std::vector<VASurfaceID>({va_surface_id_}));
    va_surface_id_ = VA_INVALID_SURFACE;
    input_size_ = gfx::Size();

    std::vector<VASurfaceID> va_surfaces;
    if (!vaapi_wrapper_->CreateContextAndSurfaces(
            VA_RT_FORMAT_YUV420, input_size,
            VaapiWrapper::SurfaceUsageHint::kGeneric, 1, &va_surfaces)) {
      VLOGF(1) << "Failed to create VA surface";
      notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
      return;
    }
    va_surface_id_ = va_surfaces[0];
    input_size_ = input_size;
  }

  if (!vaapi_wrapper_->UploadVideoFrameToSurface(*request->video_frame,
                                                 va_surface_id_)) {
    VLOGF(1) << "Failed to upload video frame to VA surface";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }

  // Create output buffer for encoding result.
  size_t max_coded_buffer_size =
      VaapiJpegEncoder::GetMaxCodedBufferSize(input_size);
  if (max_coded_buffer_size > cached_output_buffer_size_ || context_changed) {
    vaapi_wrapper_->DestroyVABuffers();
    cached_output_buffer_size_ = 0;

    VABufferID output_buffer_id;
    if (!vaapi_wrapper_->CreateVABuffer(max_coded_buffer_size,
                                        &output_buffer_id)) {
      VLOGF(1) << "Failed to create VA buffer for encoding output";
      notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
      return;
    }
    cached_output_buffer_size_ = max_coded_buffer_size;
    cached_output_buffer_id_ = output_buffer_id;
  }

  uint8_t* exif_buffer = nullptr;
  size_t exif_buffer_size = 0;
  if (request->exif_shm) {
    exif_buffer = static_cast<uint8_t*>(request->exif_shm->memory());
    exif_buffer_size = request->exif_shm->size();
  }

  // When the exif buffer contains a thumbnail, the VAAPI encoder would
  // generate a corrupted JPEG. We can work around the problem by supplying an
  // all-zero buffer with the same size and fill in the real exif buffer after
  // encoding.
  // TODO(shenghao): Remove this mechanism after b/79840013 is fixed.
  std::vector<uint8_t> exif_buffer_dummy(exif_buffer_size, 0);
  size_t exif_offset = 0;
  if (!jpeg_encoder_->Encode(input_size, exif_buffer_dummy.data(),
                             exif_buffer_size, request->quality, va_surface_id_,
                             cached_output_buffer_id_, &exif_offset)) {
    VLOGF(1) << "Encode JPEG failed";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }

  // Get the encoded output. DownloadFromVABuffer() is a blocking call. It
  // would wait until encoding is finished.
  size_t encoded_size = 0;
  if (!vaapi_wrapper_->DownloadFromVABuffer(
          cached_output_buffer_id_, va_surface_id_,
          static_cast<uint8_t*>(request->output_shm->memory()),
          request->output_shm->size(), &encoded_size)) {
    VLOGF(1) << "Failed to retrieve output image from VA coded buffer";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }

  // Copy the real exif buffer into preserved space.
  memcpy(static_cast<uint8_t*>(request->output_shm->memory()) + exif_offset,
         exif_buffer, exif_buffer_size);

  video_frame_ready_cb_.Run(task_id, encoded_size);
}

VaapiJpegEncodeAccelerator::VaapiJpegEncodeAccelerator(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(std::move(io_task_runner)),
      weak_this_factory_(this) {
  VLOGF(2);
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VaapiJpegEncodeAccelerator::~VaapiJpegEncodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOGF(2) << "Destroying VaapiJpegEncodeAccelerator";

  weak_this_factory_.InvalidateWeakPtrs();
  if (encoder_task_runner_) {
    encoder_task_runner_->DeleteSoon(FROM_HERE, std::move(encoder_));
  }
}

void VaapiJpegEncodeAccelerator::NotifyError(int32_t task_id, Status status) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOGF(1) << "task_id=" << task_id << ", status=" << status;
  DCHECK(client_);
  client_->NotifyError(task_id, status);
}

void VaapiJpegEncodeAccelerator::VideoFrameReady(int32_t task_id,
                                                 size_t encoded_picture_size) {
  DVLOGF(4) << "task_id=" << task_id << ", size=" << encoded_picture_size;
  DCHECK(task_runner_->BelongsToCurrentThread());
  ReportToVAJEAEncodeResultUMA(VAJEAEncoderResult::kSuccess);

  client_->VideoFrameReady(task_id, encoded_picture_size);
}

chromeos_camera::JpegEncodeAccelerator::Status
VaapiJpegEncodeAccelerator::Initialize(
    chromeos_camera::JpegEncodeAccelerator::Client* client) {
  VLOGF(2);
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!VaapiWrapper::IsJpegEncodeSupported()) {
    VLOGF(1) << "Jpeg encoder is not supported.";
    return HW_JPEG_ENCODE_NOT_SUPPORTED;
  }

  client_ = client;
  scoped_refptr<VaapiWrapper> vaapi_wrapper =
      VaapiWrapper::Create(VaapiWrapper::kEncode, VAProfileJPEGBaseline,
                           base::BindRepeating(&ReportToVAJEAEncodeResultUMA,
                                               VAJEAEncoderResult::kError));

  if (!vaapi_wrapper) {
    VLOGF(1) << "Failed initializing VAAPI";
    return PLATFORM_FAILURE;
  }

  scoped_refptr<VaapiWrapper> vpp_vaapi_wrapper =
      VaapiWrapper::Create(VaapiWrapper::kVideoProcess, VAProfileNone,
                           base::BindRepeating(&ReportToVAJEAVppFailureUMA,
                                               VAJEAEncoderResult::kError));
  if (!vpp_vaapi_wrapper) {
    VLOGF(1) << "Failed initializing VAAPI wrapper for VPP";
    return PLATFORM_FAILURE;
  }

  // Size is irrelevant for a VPP context.
  if (!vpp_vaapi_wrapper->CreateContext(gfx::Size())) {
    VLOGF(1) << "Failed to create context for VPP";
    return PLATFORM_FAILURE;
  }

  encoder_task_runner_ =
      base::CreateSingleThreadTaskRunner({base::ThreadPool(), base::MayBlock(),
                                          base::TaskPriority::USER_BLOCKING});
  if (!encoder_task_runner_) {
    VLOGF(1) << "Failed to create encoder task runner.";
    return THREAD_CREATION_FAILED;
  }

  encoder_ = std::make_unique<Encoder>(
      std::move(vaapi_wrapper), std::move(vpp_vaapi_wrapper),
      BindToCurrentLoop(base::BindRepeating(
          &VaapiJpegEncodeAccelerator::VideoFrameReady, weak_this_)),
      BindToCurrentLoop(base::BindRepeating(
          &VaapiJpegEncodeAccelerator::NotifyError, weak_this_)));

  return ENCODE_OK;
}

size_t VaapiJpegEncodeAccelerator::GetMaxCodedBufferSize(
    const gfx::Size& picture_size) {
  return VaapiJpegEncoder::GetMaxCodedBufferSize(picture_size);
}

void VaapiJpegEncodeAccelerator::Encode(scoped_refptr<VideoFrame> video_frame,
                                        int quality,
                                        BitstreamBuffer* exif_buffer,
                                        BitstreamBuffer output_buffer) {
  DVLOGF(4);
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  int32_t task_id = output_buffer.id();
  TRACE_EVENT1("jpeg", "Encode", "task_id", task_id);

  // TODO(shenghao): support other YUV formats.
  if (video_frame->format() != VideoPixelFormat::PIXEL_FORMAT_I420) {
    VLOGF(1) << "Unsupported input format: " << video_frame->format();
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                  weak_this_, task_id, INVALID_ARGUMENT));
    return;
  }

  std::unique_ptr<UnalignedSharedMemory> exif_shm;
  if (exif_buffer) {
    // |exif_shm| will take ownership of the |exif_buffer->region()|.
    exif_shm = std::make_unique<UnalignedSharedMemory>(
        exif_buffer->TakeRegion(), exif_buffer->size(), false);
    if (!exif_shm->MapAt(exif_buffer->offset(), exif_buffer->size())) {
      VLOGF(1) << "Failed to map exif buffer";
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                    weak_this_, task_id, PLATFORM_FAILURE));
      return;
    }
    if (exif_shm->size() > kMaxMarkerSizeAllowed) {
      VLOGF(1) << "Exif buffer too big: " << exif_shm->size();
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                    weak_this_, task_id, INVALID_ARGUMENT));
      return;
    }
  }

  // |output_shm| will take ownership of the |output_buffer.handle()|.
  auto output_shm = std::make_unique<UnalignedSharedMemory>(
      output_buffer.TakeRegion(), output_buffer.size(), false);
  if (!output_shm->MapAt(output_buffer.offset(), output_buffer.size())) {
    VLOGF(1) << "Failed to map output buffer";
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError, weak_this_,
                       task_id, INACCESSIBLE_OUTPUT_BUFFER));
    return;
  }

  auto request = std::make_unique<EncodeRequest>(
      task_id, std::move(video_frame), std::move(exif_shm),
      std::move(output_shm), quality);

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiJpegEncodeAccelerator::Encoder::EncodeTask,
                     base::Unretained(encoder_.get()), std::move(request)));
}

void VaapiJpegEncodeAccelerator::EncodeWithDmaBuf(
    scoped_refptr<VideoFrame> input_frame,
    scoped_refptr<VideoFrame> output_frame,
    int quality,
    int32_t task_id,
    BitstreamBuffer* exif_buffer) {
  DVLOGF(4);
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("jpeg", "Encode", "task_id", task_id);

  // TODO(wtlee): Supports other formats.
  if (input_frame->format() != VideoPixelFormat::PIXEL_FORMAT_NV12) {
    VLOGF(1) << "Unsupported input format: " << input_frame->format();
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                  weak_this_, task_id, INVALID_ARGUMENT));
    return;
  }
  if (output_frame->format() != VideoPixelFormat::PIXEL_FORMAT_MJPEG) {
    VLOGF(1) << "Unsupported output format: " << output_frame->format();
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                  weak_this_, task_id, INVALID_ARGUMENT));
    return;
  }

  std::unique_ptr<WritableUnalignedMapping> exif_mapping;
  if (exif_buffer) {
    // |exif_mapping| will take ownership of the |exif_buffer->region()|.
    exif_mapping = std::make_unique<WritableUnalignedMapping>(
        base::UnsafeSharedMemoryRegion::Deserialize(exif_buffer->TakeRegion()),
        exif_buffer->size(), exif_buffer->offset());
    if (!exif_mapping->IsValid()) {
      LOG(ERROR) << "Failed to map exif buffer";
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                    weak_this_, task_id, PLATFORM_FAILURE));
      return;
    }
    if (exif_mapping->size() > kMaxMarkerSizeAllowed) {
      LOG(ERROR) << "Exif buffer too big: " << exif_mapping->size();
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                    weak_this_, task_id, INVALID_ARGUMENT));
      return;
    }
  }

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiJpegEncodeAccelerator::Encoder::EncodeWithDmaBufTask,
                     base::Unretained(encoder_.get()), input_frame,
                     output_frame, task_id, quality, std::move(exif_mapping)));
}

}  // namespace media
