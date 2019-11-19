// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/video_capture_jpeg_decoder_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "components/chromeos_camera/mojo_mjpeg_decode_accelerator.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {

VideoCaptureJpegDecoderImpl::VideoCaptureJpegDecoderImpl(
    MojoMjpegDecodeAcceleratorFactoryCB jpeg_decoder_factory,
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    DecodeDoneCB decode_done_cb,
    base::RepeatingCallback<void(const std::string&)> send_log_message_cb)
    : jpeg_decoder_factory_(std::move(jpeg_decoder_factory)),
      decoder_task_runner_(std::move(decoder_task_runner)),
      decode_done_cb_(std::move(decode_done_cb)),
      send_log_message_cb_(std::move(send_log_message_cb)),
      has_received_decoded_frame_(false),
      next_task_id_(0),
      task_id_(chromeos_camera::MjpegDecodeAccelerator::kInvalidTaskId),
      decoder_status_(INIT_PENDING) {}

VideoCaptureJpegDecoderImpl::~VideoCaptureJpegDecoderImpl() {
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
}

void VideoCaptureJpegDecoderImpl::Initialize() {
  if (!IsVideoCaptureAcceleratedJpegDecodingEnabled()) {
    decoder_status_ = FAILED;
    RecordInitDecodeUMA_Locked();
    return;
  }

  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoCaptureJpegDecoderImpl::FinishInitialization,
                     weak_ptr_factory_.GetWeakPtr()));
}

VideoCaptureJpegDecoder::STATUS VideoCaptureJpegDecoderImpl::GetStatus() const {
  base::AutoLock lock(lock_);
  return decoder_status_;
}

void VideoCaptureJpegDecoderImpl::DecodeCapturedData(
    const uint8_t* data,
    size_t in_buffer_size,
    const media::VideoCaptureFormat& frame_format,
    base::TimeTicks reference_time,
    base::TimeDelta timestamp,
    media::VideoCaptureDevice::Client::Buffer out_buffer) {
  DCHECK(decoder_);

  TRACE_EVENT_ASYNC_BEGIN0("jpeg", "VideoCaptureJpegDecoderImpl decoding",
                           next_task_id_);
  TRACE_EVENT0("jpeg", "VideoCaptureJpegDecoderImpl::DecodeCapturedData");

  // TODO(kcwu): enqueue decode requests in case decoding is not fast enough
  // (say, if decoding time is longer than 16ms for 60fps 4k image)
  {
    base::AutoLock lock(lock_);
    if (IsDecoding_Locked()) {
      DVLOG(1) << "Drop captured frame. Previous jpeg frame is still decoding";
      return;
    }
  }

  // Enlarge input buffer if necessary.
  if (!in_shared_region_.IsValid() || !in_shared_mapping_.IsValid() ||
      in_buffer_size > in_shared_mapping_.size()) {
    // Reserve 2x space to avoid frequent reallocations for initial frames.
    const size_t reserved_size = 2 * in_buffer_size;
    in_shared_region_ = base::UnsafeSharedMemoryRegion::Create(reserved_size);
    if (!in_shared_region_.IsValid()) {
      base::AutoLock lock(lock_);
      decoder_status_ = FAILED;
      LOG(WARNING) << "UnsafeSharedMemoryRegion::Create failed, size="
                   << reserved_size;
      return;
    }
    in_shared_mapping_ = in_shared_region_.Map();
    if (!in_shared_mapping_.IsValid()) {
      base::AutoLock lock(lock_);
      decoder_status_ = FAILED;
      LOG(WARNING) << "UnsafeSharedMemoryRegion::Map failed, size="
                   << reserved_size;
      return;
    }
  }
  memcpy(in_shared_mapping_.memory(), data, in_buffer_size);

  // No need to lock for |task_id_| since IsDecoding_Locked() is false.
  task_id_ = next_task_id_;
  media::BitstreamBuffer in_buffer(task_id_, in_shared_region_.Duplicate(),
                                   in_buffer_size);
  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_task_id_ = (next_task_id_ + 1) & 0x3FFFFFFF;

  // The API of |decoder_| requires us to wrap the |out_buffer| in a VideoFrame.
  const gfx::Size dimensions = frame_format.frame_size;
  base::UnsafeSharedMemoryRegion out_region =
      out_buffer.handle_provider->DuplicateAsUnsafeRegion();
  DCHECK(out_region.IsValid());
  base::WritableSharedMemoryMapping out_mapping = out_region.Map();
  DCHECK(out_mapping.IsValid());
  scoped_refptr<media::VideoFrame> out_frame =
      media::VideoFrame::WrapExternalData(
          media::PIXEL_FORMAT_I420,                       // format
          dimensions,                                     // coded_size
          gfx::Rect(dimensions),                          // visible_rect
          dimensions,                                     // natural_size
          out_mapping.GetMemoryAsSpan<uint8_t>().data(),  // data
          out_mapping.size(),                             // data_size
          timestamp);                                     // timestamp
  if (!out_frame) {
    base::AutoLock lock(lock_);
    decoder_status_ = FAILED;
    LOG(ERROR) << "DecodeCapturedData: WrapExternalSharedMemory failed";
    return;
  }
  out_frame->BackWithOwnedSharedMemory(std::move(out_region),
                                       std::move(out_mapping));

  out_frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                   frame_format.frame_rate);

  out_frame->metadata()->SetTimeTicks(media::VideoFrameMetadata::REFERENCE_TIME,
                                      reference_time);

  media::mojom::VideoFrameInfoPtr out_frame_info =
      media::mojom::VideoFrameInfo::New();
  out_frame_info->timestamp = timestamp;
  out_frame_info->pixel_format = media::PIXEL_FORMAT_I420;
  out_frame_info->coded_size = dimensions;
  out_frame_info->visible_rect = gfx::Rect(dimensions);
  out_frame_info->metadata = out_frame->metadata()->GetInternalValues().Clone();
  out_frame_info->color_space = out_frame->ColorSpace();

  {
    base::AutoLock lock(lock_);
    decode_done_closure_ = base::BindOnce(
        decode_done_cb_, out_buffer.id, out_buffer.frame_feedback_id,
        base::Passed(&out_buffer.access_permission),
        base::Passed(&out_frame_info));
  }

  // base::Unretained is safe because |decoder_| is deleted on
  // |decoder_task_runner_|.
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](chromeos_camera::MjpegDecodeAccelerator* decoder,
             BitstreamBuffer in_buffer, scoped_refptr<VideoFrame> out_frame) {
            decoder->Decode(std::move(in_buffer), std::move(out_frame));
          },
          base::Unretained(decoder_.get()), std::move(in_buffer),
          std::move(out_frame)));
}

void VideoCaptureJpegDecoderImpl::VideoFrameReady(int32_t task_id) {
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("jpeg", "VideoCaptureJpegDecoderImpl::VideoFrameReady");
  if (!has_received_decoded_frame_) {
    send_log_message_cb_.Run("Received decoded frame from Gpu Jpeg decoder");
    has_received_decoded_frame_ = true;
  }
  base::AutoLock lock(lock_);

  if (!IsDecoding_Locked()) {
    LOG(ERROR) << "Got decode response while not decoding";
    return;
  }

  if (task_id != task_id_) {
    LOG(ERROR) << "Unexpected task_id " << task_id << ", expected " << task_id_;
    return;
  }
  task_id_ = chromeos_camera::MjpegDecodeAccelerator::kInvalidTaskId;

  std::move(decode_done_closure_).Run();

  TRACE_EVENT_ASYNC_END0("jpeg", "VideoCaptureJpegDecoderImpl decoding",
                         task_id);
}

void VideoCaptureJpegDecoderImpl::NotifyError(
    int32_t task_id,
    chromeos_camera::MjpegDecodeAccelerator::Error error) {
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
  LOG(ERROR) << "Decode error, task_id=" << task_id << ", error=" << error;
  send_log_message_cb_.Run("Gpu Jpeg decoder failed");
  base::AutoLock lock(lock_);
  decode_done_closure_.Reset();
  decoder_status_ = FAILED;
}

void VideoCaptureJpegDecoderImpl::FinishInitialization() {
  TRACE_EVENT0("gpu", "VideoCaptureJpegDecoderImpl::FinishInitialization");
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());

  mojo::PendingRemote<chromeos_camera::mojom::MjpegDecodeAccelerator>
      remote_decoder;
  jpeg_decoder_factory_.Run(remote_decoder.InitWithNewPipeAndPassReceiver());

  base::AutoLock lock(lock_);
  decoder_ = std::make_unique<chromeos_camera::MojoMjpegDecodeAccelerator>(
      decoder_task_runner_, std::move(remote_decoder));

  decoder_->InitializeAsync(
      this,
      base::BindRepeating(&VideoCaptureJpegDecoderImpl::OnInitializationDone,
                          weak_ptr_factory_.GetWeakPtr()));
}

void VideoCaptureJpegDecoderImpl::OnInitializationDone(bool success) {
  TRACE_EVENT0("gpu", "VideoCaptureJpegDecoderImpl::OnInitializationDone");
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock lock(lock_);
  if (!success) {
    decoder_.reset();
    DLOG(ERROR) << "Failed to initialize JPEG decoder";
  }

  decoder_status_ = success ? INIT_PASSED : FAILED;
  RecordInitDecodeUMA_Locked();
}

bool VideoCaptureJpegDecoderImpl::IsDecoding_Locked() const {
  lock_.AssertAcquired();
  return !decode_done_closure_.is_null();
}

void VideoCaptureJpegDecoderImpl::RecordInitDecodeUMA_Locked() {
  UMA_HISTOGRAM_BOOLEAN("Media.VideoCaptureGpuJpegDecoder.InitDecodeSuccess",
                        decoder_status_ == INIT_PASSED);
}

}  // namespace media
