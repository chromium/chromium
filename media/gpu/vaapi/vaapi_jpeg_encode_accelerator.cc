// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_jpeg_encode_accelerator.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/trace_event/trace_event.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/platform_video_frame_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_jpeg_encoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/parsers/jpeg_parser.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

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

}  // namespace

VaapiJpegEncodeAccelerator::EncodeRequest::EncodeRequest(
    int32_t task_id,
    scoped_refptr<VideoFrame> video_frame,
    base::WritableSharedMemoryMapping exif_mapping,
    base::WritableSharedMemoryMapping output_mapping,
    int quality)
    : task_id(task_id),
      video_frame(std::move(video_frame)),
      exif_mapping(std::move(exif_mapping)),
      output_mapping(std::move(output_mapping)),
      quality(quality) {}

VaapiJpegEncodeAccelerator::EncodeRequest::~EncodeRequest() {}

class VaapiJpegEncodeAccelerator::Encoder {
 public:
  Encoder();

  Encoder(const Encoder&) = delete;
  Encoder& operator=(const Encoder&) = delete;

  ~Encoder();

  void Initialize(
      base::RepeatingCallback<void(int32_t, size_t)> video_frame_ready_cb,
      base::RepeatingCallback<void(int32_t, Status)> notify_error_cb,
      chromeos_camera::JpegEncodeAccelerator::InitCB init_cb);

  // Processes one encode task with DMA-buf.
  void EncodeWithDmaBufTask(scoped_refptr<VideoFrame> input_frame,
                            scoped_refptr<VideoFrame> output_frame,
                            int32_t task_id,
                            int quality,
                            base::WritableSharedMemoryMapping exif_mapping);

  // Processes one encode |request|.
  void EncodeTask(std::unique_ptr<EncodeRequest> request);

 private:
  std::unique_ptr<VaapiJpegEncoder> jpeg_encoder_
      GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<VaapiWrapper> vaapi_wrapper_
      GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<VaapiWrapper> vpp_vaapi_wrapper_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<gpu::GpuMemoryBufferSupport> gpu_memory_buffer_support_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // |cached_output_buffer_| is the last allocated VABuffer during EncodeTask().
  // If the next call to EncodeTask() does not require a buffer bigger than the
  // size of |cached_output_buffer_|, |cached_output_buffer_| will be reused.
  std::unique_ptr<ScopedVABuffer> cached_output_buffer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::RepeatingCallback<void(int32_t, size_t)> video_frame_ready_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingCallback<void(int32_t, Status)> notify_error_cb_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The current VA surface ID used for encoding. Only used for Non-DMA-buf use
  // case.
  VASurfaceID va_surface_id_ GUARDED_BY_CONTEXT(sequence_checker_){
      VA_INVALID_SURFACE};
  // The size of the surface associated with |va_surface_id_|.
  gfx::Size input_size_ GUARDED_BY_CONTEXT(sequence_checker_);
  // The format used to create VAContext. Only used for DMA-buf use case.
  uint32_t va_format_ GUARDED_BY_CONTEXT(sequence_checker_){0};

  SEQUENCE_CHECKER(sequence_checker_);
};

VaapiJpegEncodeAccelerator::Encoder::Encoder() {
  // The constructor is called on |io_task_runner_|.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VaapiJpegEncodeAccelerator::Encoder::~Encoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Destroy ScopedVABuffer before VaapiWrappers are destroyed to ensure
  // VADisplay is valid on ScopedVABuffer's destruction.
  cached_output_buffer_.reset();
}

void VaapiJpegEncodeAccelerator::Encoder::Initialize(
    base::RepeatingCallback<void(int32_t, size_t)> video_frame_ready_cb,
    base::RepeatingCallback<void(int32_t, Status)> notify_error_cb,
    chromeos_camera::JpegEncodeAccelerator::InitCB init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!VaapiWrapper::IsJpegEncodeSupported()) {
    VLOGF(1) << "Jpeg encoder is not supported.";
    std::move(init_cb).Run(HW_JPEG_ENCODE_NOT_SUPPORTED);
    return;
  }

  vaapi_wrapper_ =
      VaapiWrapper::Create(
          VaapiWrapper::kEncodeConstantBitrate, VAProfileJPEGBaseline,
          EncryptionScheme::kUnencrypted,
          base::BindRepeating(&ReportVaapiErrorToUMA,
                              "Media.VaapiJpegEncodeAccelerator.VAAPIError"))
          .value_or(nullptr);
  if (!vaapi_wrapper_) {
    VLOGF(1) << "Failed initializing VAAPI";
    std::move(init_cb).Run(PLATFORM_FAILURE);
    return;
  }

  vpp_vaapi_wrapper_ =
      VaapiWrapper::Create(
          VaapiWrapper::kVideoProcess, VAProfileNone,
          EncryptionScheme::kUnencrypted,
          base::BindRepeating(
              &ReportVaapiErrorToUMA,
              "Media.VaapiJpegEncodeAccelerator.Vpp.VAAPIError"))
          .value_or(nullptr);
  if (!vpp_vaapi_wrapper_) {
    VLOGF(1) << "Failed initializing VAAPI wrapper for VPP";
    std::move(init_cb).Run(PLATFORM_FAILURE);
    return;
  }

  // Size is irrelevant for a VPP context.
  if (!vpp_vaapi_wrapper_->CreateContext(gfx::Size())) {
    VLOGF(1) << "Failed to create context for VPP";
    std::move(init_cb).Run(PLATFORM_FAILURE);
    return;
  }

  jpeg_encoder_ = std::make_unique<VaapiJpegEncoder>(vaapi_wrapper_);
  gpu_memory_buffer_support_ = std::make_unique<gpu::GpuMemoryBufferSupport>();
  video_frame_ready_cb_ = std::move(video_frame_ready_cb);
  notify_error_cb_ = std::move(notify_error_cb);

  std::move(init_cb).Run(ENCODE_OK);
}

void VaapiJpegEncodeAccelerator::Encoder::EncodeWithDmaBufTask(
    scoped_refptr<VideoFrame> input_frame,
    scoped_refptr<VideoFrame> output_frame,
    int32_t task_id,
    int quality,
    base::WritableSharedMemoryMapping exif_mapping) {
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
            va_format, input_size, {VaapiWrapper::SurfaceUsageHint::kGeneric},
            1, &va_surfaces)) {
      VLOGF(1) << "Failed to create VA surface";
      notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
      return;
    }
    va_surface_id_ = va_surfaces[0];
    va_format_ = va_format;
    input_size_ = input_size;
  }
  DCHECK(input_frame);
  scoped_refptr<gfx::NativePixmap> pixmap =
      CreateNativePixmapDmaBuf(input_frame.get());
  if (!pixmap) {
    VLOGF(1) << "Failed to create NativePixmap from VideoFrame";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }

  // We need to explicitly blit the bound input surface here to make sure the
  // input we sent to VAAPI encoder is in tiled NV12 format since implicit
  // tiling logic is not contained in every driver.
  auto input_surface =
      vpp_vaapi_wrapper_->CreateVASurfaceForPixmap(std::move(pixmap));

  if (!input_surface) {
    VLOGF(1) << "Failed to create input va surface";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }
  if (!vpp_vaapi_wrapper_->BlitSurface(input_surface->id(),
                                       input_surface->size(), va_surface_id_,
                                       input_size)) {
    VLOGF(1) << "Failed to blit surfaces";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }
  // We should call vaSyncSurface() when passing surface between contexts. See:
  // https://lists.01.org/pipermail/intel-vaapi-media/2019-June/000131.html
  // Sync |va_surface_id_| since it it passing to the JPEG encoding context.
  if (!vpp_vaapi_wrapper_->SyncSurface(va_surface_id_)) {
    VLOGF(1) << "Cannot sync VPP output surface";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }

  // Create output buffer for encoding result.
  size_t max_coded_buffer_size =
      VaapiJpegEncoder::GetMaxCodedBufferSize(input_size);
  if (context_changed || !cached_output_buffer_ ||
      cached_output_buffer_->size() < max_coded_buffer_size) {
    cached_output_buffer_.reset();

    auto output_buffer = vaapi_wrapper_->CreateVABuffer(VAEncCodedBufferType,
                                                        max_coded_buffer_size);
    if (!output_buffer) {
      VLOGF(1) << "Failed to create VA buffer for encoding output";
      notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
      return;
    }
    cached_output_buffer_ = std::move(output_buffer);
  }

  // Prepare exif.
  const uint8_t* exif_buffer = nullptr;
  size_t exif_buffer_size = 0;
  if (exif_mapping.IsValid()) {
    exif_buffer = exif_mapping.GetMemoryAs<uint8_t>();
    exif_buffer_size = exif_mapping.size();
  }

  if (!jpeg_encoder_->Encode(input_size, /*exif_buffer=*/nullptr,
                             /*exif_buffer_size=*/0u, quality, va_surface_id_,
                             cached_output_buffer_->id(),
                             /*exif_offset=*/nullptr)) {
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

  const bool is_mapped = output_gmb_buffer->Map();
  if (!is_mapped) {
    VLOGF(1) << "Map the output gmb buffer failed";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }
  absl::Cleanup output_gmb_buffer_unmapper = [&output_gmb_buffer] {
    output_gmb_buffer->Unmap();
  };

  // Get the encoded output. DownloadFromVABuffer() is a blocking call. It
  // would wait until encoding is finished.
  uint8_t* output_memory = static_cast<uint8_t*>(output_gmb_buffer->memory(0));
  size_t encoded_size = 0;
  // Since the format of |output_gmb_buffer| is gfx::BufferFormat::R_8, we can
  // use its area as the maximum bytes we need to download to avoid buffer
  // overflow.
  // Since we didn't supply EXIF data to the JPEG encoder, it creates a default
  // APP0 segment in the header. We will download the result to an offset and
  // replace the APP0 segment by APP1 including EXIF data:
  //      SOI + APP0 (2 + 2 + 14 bytes) + other data
  //   -> SOI + APP1 (2 + 2 + |exif_buffer_size| bytes) + other data
  // Note that |exif_buffer_size| >= 14 since EXIF + TIFF headers are 14 bytes,
  // and <= (2^16-1)-2 since APP1 data size is stored in 2 bytes.
  // TODO(b/171369066, b/171340559): Remove this workaround when Intel iHD
  // driver has fixed the EXIF handling.
  constexpr size_t kApp0DataSize = 14;
  constexpr size_t kMaxExifSize = ((1u << 16) - 1) - 2;
  if (exif_buffer_size > 0 &&
      (exif_buffer_size < kApp0DataSize || exif_buffer_size > kMaxExifSize)) {
    VLOGF(1) << "Unexpected EXIF data size (" << exif_buffer_size << ")";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }
  const size_t output_offset =
      exif_buffer_size > 0 ? exif_buffer_size - kApp0DataSize : 0;
  const size_t output_size =
      base::checked_cast<size_t>(output_gmb_buffer->GetSize().GetArea());
  if (output_offset >= output_size) {
    VLOGF(1) << "Output buffer size (" << output_size << ") is too small";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }
  uint8_t* frame_content = output_memory + output_offset;
  const size_t max_frame_size = output_size - output_offset;
  if (!vaapi_wrapper_->DownloadFromVABuffer(cached_output_buffer_->id(),
                                            va_surface_id_, frame_content,
                                            max_frame_size, &encoded_size)) {
    VLOGF(1) << "Failed to retrieve output image from VA coded buffer";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }
  CHECK_LE(encoded_size, max_frame_size);

  if (exif_buffer_size > 0) {
    // Check the output header is 2+2+14 bytes APP0 as expected.
    constexpr uint8_t kJpegSoiAndApp0Header[] = {
        0xFF, JPEG_SOI, 0xFF, JPEG_APP0, 0x00, 0x10,
    };
    if (encoded_size < std::size(kJpegSoiAndApp0Header)) {
      VLOGF(1) << "Unexpected JPEG data size received from encoder";
      notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
      return;
    }
    for (size_t i = 0; i < std::size(kJpegSoiAndApp0Header); ++i) {
      if (frame_content[i] != kJpegSoiAndApp0Header[i]) {
        VLOGF(1) << "Unexpected JPEG header received from encoder";
        notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
        return;
      }
    }
    // Copy the EXIF data into preserved space.
    const uint8_t jpeg_soi_and_app1_header[] = {
        0xFF,
        JPEG_SOI,
        0xFF,
        JPEG_APP1,
        static_cast<uint8_t>((exif_buffer_size + 2) / 256),
        static_cast<uint8_t>((exif_buffer_size + 2) % 256),
    };
    CHECK_GE(output_size, std::size(jpeg_soi_and_app1_header));
    if (exif_buffer_size > output_size - std::size(jpeg_soi_and_app1_header)) {
      VLOGF(1) << "Insufficient buffer size reserved for JPEG APP1 data";
      notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
      return;
    }
    memcpy(output_memory, jpeg_soi_and_app1_header,
           std::size(jpeg_soi_and_app1_header));
    memcpy(output_memory + std::size(jpeg_soi_and_app1_header), exif_buffer,
           exif_buffer_size);
    encoded_size += output_offset;
  }

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
            {VaapiWrapper::SurfaceUsageHint::kGeneric}, 1, &va_surfaces)) {
      VLOGF(1) << "Failed to create VA surface";
      notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
      return;
    }
    va_surface_id_ = va_surfaces[0];
    input_size_ = input_size;
  }

  if (!vaapi_wrapper_->UploadVideoFrameToSurface(*request->video_frame,
                                                 va_surface_id_, input_size_)) {
    VLOGF(1) << "Failed to upload video frame to VA surface";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }

  // Create output buffer for encoding result.
  size_t max_coded_buffer_size =
      VaapiJpegEncoder::GetMaxCodedBufferSize(input_size);
  if (context_changed || !cached_output_buffer_ ||
      cached_output_buffer_->size() < max_coded_buffer_size) {
    cached_output_buffer_.reset();

    auto output_buffer = vaapi_wrapper_->CreateVABuffer(VAEncCodedBufferType,
                                                        max_coded_buffer_size);
    if (!output_buffer) {
      VLOGF(1) << "Failed to create VA buffer for encoding output";
      notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
      return;
    }
    cached_output_buffer_ = std::move(output_buffer);
  }

  uint8_t* exif_buffer = nullptr;
  size_t exif_buffer_size = 0;
  if (request->exif_mapping.IsValid()) {
    exif_buffer = request->exif_mapping.GetMemoryAs<uint8_t>();
    exif_buffer_size = request->exif_mapping.size();
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
                             cached_output_buffer_->id(), &exif_offset)) {
    VLOGF(1) << "Encode JPEG failed";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }

  // Get the encoded output. DownloadFromVABuffer() is a blocking call. It
  // would wait until encoding is finished.
  size_t encoded_size = 0;
  if (!vaapi_wrapper_->DownloadFromVABuffer(
          cached_output_buffer_->id(), va_surface_id_,
          request->output_mapping.GetMemoryAs<uint8_t>(),
          request->output_mapping.size(), &encoded_size)) {
    VLOGF(1) << "Failed to retrieve output image from VA coded buffer";
    notify_error_cb_.Run(task_id, PLATFORM_FAILURE);
    return;
  }

  // Copy the real exif buffer into preserved space.
  memcpy(request->output_mapping.GetMemoryAs<uint8_t>() + exif_offset,
         exif_buffer, exif_buffer_size);

  video_frame_ready_cb_.Run(task_id, encoded_size);
}

VaapiJpegEncodeAccelerator::VaapiJpegEncodeAccelerator(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner)), weak_this_factory_(this) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  VLOGF(2);
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VaapiJpegEncodeAccelerator::~VaapiJpegEncodeAccelerator() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  VLOGF(2) << "Destroying VaapiJpegEncodeAccelerator";

  weak_this_factory_.InvalidateWeakPtrs();

  if (encoder_task_runner_) {
    encoder_task_runner_->DeleteSoon(FROM_HERE, std::move(encoder_));
  }
}

void VaapiJpegEncodeAccelerator::NotifyError(int32_t task_id, Status status) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  VLOGF(1) << "task_id=" << task_id << ", status=" << status;
  DCHECK(client_);
  client_->NotifyError(task_id, status);
}

void VaapiJpegEncodeAccelerator::VideoFrameReady(int32_t task_id,
                                                 size_t encoded_picture_size) {
  DVLOGF(4) << "task_id=" << task_id << ", size=" << encoded_picture_size;
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  client_->VideoFrameReady(task_id, encoded_picture_size);
}

void VaapiJpegEncodeAccelerator::InitializeAsync(
    chromeos_camera::JpegEncodeAccelerator::Client* client,
    chromeos_camera::JpegEncodeAccelerator::InitCB init_cb) {
  VLOGF(2);
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  client_ = client;

  encoder_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT});
  DCHECK(encoder_task_runner_);

  encoder_ = std::make_unique<Encoder>();

  // base::Unretained(encoder_) is safe because |encoder_| is passed to
  // and destroyed on |encoder_task_runner_| in destructor. Thus |encoder_| is
  // outlive any task that has been posted by |this|.
  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VaapiJpegEncodeAccelerator::Encoder::Initialize,
          base::Unretained(encoder_.get()),
          BindPostTask(
              io_task_runner_,
              base::BindRepeating(&VaapiJpegEncodeAccelerator::VideoFrameReady,
                                  weak_this_)),
          BindPostTask(
              io_task_runner_,
              base::BindRepeating(&VaapiJpegEncodeAccelerator::NotifyError,
                                  weak_this_)),
          base::BindPostTaskToCurrentDefault(std::move(init_cb))));
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
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                  weak_this_, task_id, INVALID_ARGUMENT));
    return;
  }

  base::WritableSharedMemoryMapping exif_mapping;
  if (exif_buffer) {
    base::UnsafeSharedMemoryRegion exif_region = exif_buffer->TakeRegion();
    exif_mapping =
        exif_region.MapAt(exif_buffer->offset(), exif_buffer->size());
    if (!exif_mapping.IsValid()) {
      VLOGF(1) << "Failed to map exif buffer";
      io_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                    weak_this_, task_id, PLATFORM_FAILURE));
      return;
    }
    if (exif_mapping.size() > kMaxMarkerSizeAllowed) {
      VLOGF(1) << "Exif buffer too big: " << exif_mapping.size();
      io_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                    weak_this_, task_id, INVALID_ARGUMENT));
      return;
    }
  }

  base::UnsafeSharedMemoryRegion output_region = output_buffer.TakeRegion();
  base::WritableSharedMemoryMapping output_mapping =
      output_region.MapAt(output_buffer.offset(), output_buffer.size());
  if (!output_mapping.IsValid()) {
    VLOGF(1) << "Failed to map output buffer";
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError, weak_this_,
                       task_id, INACCESSIBLE_OUTPUT_BUFFER));
    return;
  }

  auto request = std::make_unique<EncodeRequest>(
      task_id, std::move(video_frame), std::move(exif_mapping),
      std::move(output_mapping), quality);

  // base::Unretained(encoder_) is safe because |encoder_| is passed to
  // and destroyed on |encoder_task_runner_| in destructor. Thus |encoder_| is
  // outlive any task that has been posted by |this|.
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
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                  weak_this_, task_id, INVALID_ARGUMENT));
    return;
  }
  if (output_frame->format() != VideoPixelFormat::PIXEL_FORMAT_MJPEG) {
    VLOGF(1) << "Unsupported output format: " << output_frame->format();
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                  weak_this_, task_id, INVALID_ARGUMENT));
    return;
  }

  base::WritableSharedMemoryMapping exif_mapping;
  if (exif_buffer) {
    base::UnsafeSharedMemoryRegion exif_region = exif_buffer->TakeRegion();
    exif_mapping =
        exif_region.MapAt(exif_buffer->offset(), exif_buffer->size());
    if (!exif_mapping.IsValid()) {
      LOG(ERROR) << "Failed to map exif buffer";
      io_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                    weak_this_, task_id, PLATFORM_FAILURE));
      return;
    }
    if (exif_mapping.size() > kMaxMarkerSizeAllowed) {
      LOG(ERROR) << "Exif buffer too big: " << exif_mapping.size();
      io_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                    weak_this_, task_id, INVALID_ARGUMENT));
      return;
    }
  }

  // base::Unretained(encoder_) is safe because |encoder_| is passed to
  // and destroyed on |encoder_task_runner_| in destructor. Thus |encoder_| is
  // outlive any task that has been posted by |this|.
  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiJpegEncodeAccelerator::Encoder::EncodeWithDmaBufTask,
                     base::Unretained(encoder_.get()), input_frame,
                     output_frame, task_id, quality, std::move(exif_mapping)));
}

}  // namespace media
