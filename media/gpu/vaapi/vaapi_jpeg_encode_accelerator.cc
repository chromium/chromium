// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_jpeg_encode_accelerator.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/gpu/vaapi/vaapi_jpeg_encoder.h"

#define VLOGF(level) VLOG(level) << __func__ << "(): "
#define DVLOGF(level) DVLOG(level) << __func__ << "(): "

namespace media {

namespace {

// JPEG format uses 2 bytes to denote the size of a segment, and the size
// includes the 2 bytes used for specifying it. Therefore, maximum data size
// allowed is: 65535 - 2 = 65533.
constexpr size_t kMaxExifSizeAllowed = 65533;

// UMA results that the VaapiJpegEncodeAccelerator class reports.
// These values are persisted to logs, and should therefore never be renumbered
// nor reused.
enum VAJEAEncoderResult {
  VAAPI_SUCCESS = 0,
  VAAPI_ERROR,
  VAJEA_ENCODER_RESULT_MAX = VAAPI_ERROR,
};

static void ReportToUMA(VAJEAEncoderResult result) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAJEA.EncoderResult", result,
                            VAJEAEncoderResult::VAJEA_ENCODER_RESULT_MAX + 1);
}
}  // namespace

VaapiJpegEncodeAccelerator::EncodeRequest::EncodeRequest(
    int32_t buffer_id,
    scoped_refptr<VideoFrame> video_frame,
    std::unique_ptr<UnalignedSharedMemory> exif_shm,
    std::unique_ptr<UnalignedSharedMemory> output_shm,
    int quality)
    : buffer_id(buffer_id),
      video_frame(std::move(video_frame)),
      exif_shm(std::move(exif_shm)),
      output_shm(std::move(output_shm)),
      quality(quality) {}

VaapiJpegEncodeAccelerator::EncodeRequest::~EncodeRequest() {}

class VaapiJpegEncodeAccelerator::Encoder {
 public:
  Encoder(scoped_refptr<VaapiWrapper> vaapi_wrapper,
          base::RepeatingCallback<void(int32_t, size_t)> video_frame_ready_cb,
          base::RepeatingCallback<void(int32_t, Status)> notify_error_cb);
  ~Encoder();

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

  base::RepeatingCallback<void(int32_t, size_t)> video_frame_ready_cb_;
  base::RepeatingCallback<void(int32_t, Status)> notify_error_cb_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(Encoder);
};

VaapiJpegEncodeAccelerator::Encoder::Encoder(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    base::RepeatingCallback<void(int32_t, size_t)> video_frame_ready_cb,
    base::RepeatingCallback<void(int32_t, Status)> notify_error_cb)
    : cached_output_buffer_size_(0),
      jpeg_encoder_(new VaapiJpegEncoder(vaapi_wrapper)),
      vaapi_wrapper_(std::move(vaapi_wrapper)),
      video_frame_ready_cb_(std::move(video_frame_ready_cb)),
      notify_error_cb_(std::move(notify_error_cb)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VaapiJpegEncodeAccelerator::Encoder::~Encoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VaapiJpegEncodeAccelerator::Encoder::EncodeTask(
    std::unique_ptr<EncodeRequest> request) {
  DVLOGF(4);
  TRACE_EVENT0("jpeg", "EncodeTask");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int buffer_id = request->buffer_id;
  gfx::Size input_size = request->video_frame->coded_size();
  std::vector<VASurfaceID> va_surfaces;
  if (!vaapi_wrapper_->CreateSurfaces(VA_RT_FORMAT_YUV420, input_size, 1,
                                      &va_surfaces)) {
    VLOGF(1) << "Failed to create VA surface";
    notify_error_cb_.Run(buffer_id, PLATFORM_FAILURE);
    return;
  }
  VASurfaceID va_surface_id = va_surfaces[0];

  if (!vaapi_wrapper_->UploadVideoFrameToSurface(request->video_frame,
                                                 va_surface_id)) {
    VLOGF(1) << "Failed to upload video frame to VA surface";
    notify_error_cb_.Run(buffer_id, PLATFORM_FAILURE);
    return;
  }

  // Create output buffer for encoding result.
  size_t max_coded_buffer_size =
      VaapiJpegEncoder::GetMaxCodedBufferSize(input_size);
  if (max_coded_buffer_size > cached_output_buffer_size_) {
    vaapi_wrapper_->DestroyCodedBuffers();
    cached_output_buffer_size_ = 0;

    VABufferID output_buffer_id;
    if (!vaapi_wrapper_->CreateCodedBuffer(max_coded_buffer_size,
                                           &output_buffer_id)) {
      VLOGF(1) << "Failed to create VA buffer for encoding output";
      notify_error_cb_.Run(buffer_id, PLATFORM_FAILURE);
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
                             exif_buffer_size, request->quality, va_surface_id,
                             cached_output_buffer_id_, &exif_offset)) {
    VLOGF(1) << "Encode JPEG failed";
    notify_error_cb_.Run(buffer_id, PLATFORM_FAILURE);
    return;
  }

  // Get the encoded output. DownloadFromCodedBuffer() is a blocking call. It
  // would wait until encoding is finished.
  size_t encoded_size = 0;
  if (!vaapi_wrapper_->DownloadFromCodedBuffer(
          cached_output_buffer_id_, va_surface_id,
          static_cast<uint8_t*>(request->output_shm->memory()),
          request->output_shm->size(), &encoded_size)) {
    VLOGF(1) << "Failed to retrieve output image from VA coded buffer";
    notify_error_cb_.Run(buffer_id, PLATFORM_FAILURE);
  }

  // Copy the real exif buffer into preserved space.
  memcpy(static_cast<uint8_t*>(request->output_shm->memory()) + exif_offset,
         exif_buffer, exif_buffer_size);

  video_frame_ready_cb_.Run(buffer_id, encoded_size);
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

void VaapiJpegEncodeAccelerator::NotifyError(int32_t buffer_id, Status status) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOGF(1) << "output_buffer_id=" << buffer_id << ", status=" << status;
  DCHECK(client_);
  client_->NotifyError(buffer_id, status);
}

void VaapiJpegEncodeAccelerator::VideoFrameReady(int32_t buffer_id,
                                                 size_t encoded_picture_size) {
  DVLOGF(4) << "output_buffer_id=" << buffer_id
            << ", size=" << encoded_picture_size;
  DCHECK(task_runner_->BelongsToCurrentThread());
  ReportToUMA(VAJEAEncoderResult::VAAPI_SUCCESS);

  client_->VideoFrameReady(buffer_id, encoded_picture_size);
}

JpegEncodeAccelerator::Status VaapiJpegEncodeAccelerator::Initialize(
    JpegEncodeAccelerator::Client* client) {
  VLOGF(2);
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!VaapiWrapper::IsJpegEncodeSupported()) {
    VLOGF(1) << "Jpeg encoder is not supported.";
    return HW_JPEG_ENCODE_NOT_SUPPORTED;
  }

  client_ = client;
  scoped_refptr<VaapiWrapper> vaapi_wrapper = VaapiWrapper::Create(
      VaapiWrapper::kEncode, VAProfileJPEGBaseline,
      base::Bind(&ReportToUMA, VAJEAEncoderResult::VAAPI_ERROR));

  if (!vaapi_wrapper) {
    VLOGF(1) << "Failed initializing VAAPI";
    return PLATFORM_FAILURE;
  }

  encoder_task_runner_ = base::CreateSingleThreadTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  if (!encoder_task_runner_) {
    VLOGF(1) << "Failed to create encoder task runner.";
    return THREAD_CREATION_FAILED;
  }

  encoder_ = std::make_unique<Encoder>(
      std::move(vaapi_wrapper),
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
                                        const BitstreamBuffer* exif_buffer,
                                        const BitstreamBuffer& output_buffer) {
  DVLOGF(4);
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  int32_t buffer_id = output_buffer.id();
  TRACE_EVENT1("jpeg", "Encode", "output_buffer_id", buffer_id);

  // TODO(shenghao): support other YUV formats.
  if (video_frame->format() != VideoPixelFormat::PIXEL_FORMAT_I420) {
    VLOGF(1) << "Unsupported input format: " << video_frame->format();
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                  weak_this_, buffer_id, INVALID_ARGUMENT));
    return;
  }

  std::unique_ptr<UnalignedSharedMemory> exif_shm;
  if (exif_buffer) {
    // |exif_shm| will take ownership of the |exif_buffer->handle()|.
    exif_shm = std::make_unique<UnalignedSharedMemory>(
        exif_buffer->handle(), exif_buffer->size(), true);
    if (!exif_shm->MapAt(exif_buffer->offset(), exif_buffer->size())) {
      VLOGF(1) << "Failed to map exif buffer";
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                    weak_this_, buffer_id, PLATFORM_FAILURE));
      return;
    }
    if (exif_shm->size() > kMaxExifSizeAllowed) {
      VLOGF(1) << "Exif buffer too big: " << exif_shm->size();
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&VaapiJpegEncodeAccelerator::NotifyError,
                                    weak_this_, buffer_id, INVALID_ARGUMENT));
      return;
    }
  }

  // |output_shm| will take ownership of the |output_buffer.handle()|.
  auto output_shm = std::make_unique<UnalignedSharedMemory>(
      output_buffer.handle(), output_buffer.size(), false);
  if (!output_shm->MapAt(output_buffer.offset(), output_buffer.size())) {
    VLOGF(1) << "Failed to map output buffer";
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VaapiJpegEncodeAccelerator::NotifyError, weak_this_,
                   buffer_id, INACCESSIBLE_OUTPUT_BUFFER));
    return;
  }

  auto request = std::make_unique<EncodeRequest>(
      buffer_id, std::move(video_frame), std::move(exif_shm),
      std::move(output_shm), quality);

  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VaapiJpegEncodeAccelerator::Encoder::EncodeTask,
                 base::Unretained(encoder_.get()), base::Passed(&request)));
}

}  // namespace media
