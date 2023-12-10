// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/video_capture_jpeg_decoder_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/typed_macros.h"
#include "components/chromeos_camera/mojo_mjpeg_decode_accelerator.h"
#include "media/base/media_switches.h"
#include "media/capture/video/chromeos/camera_trace_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

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
      decoder_status_(INIT_PENDING),
      next_task_id_(0),
      task_id_(chromeos_camera::MjpegDecodeAccelerator::kInvalidTaskId) {}

VideoCaptureJpegDecoderImpl::~VideoCaptureJpegDecoderImpl() {
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
}

void VideoCaptureJpegDecoderImpl::Initialize() {
  base::AutoLock lock(lock_);
  if (!IsVideoCaptureAcceleratedJpegDecodingEnabled()) {
    decoder_status_ = FAILED;
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

  TRACE_EVENT_BEGIN(
      "jpeg", "VideoCaptureJpegDecoderImpl decoding",
      GetTraceTrack(CameraTraceEvent::kJpegDecoding, next_task_id_));
  TRACE_EVENT("jpeg", "VideoCaptureJpegDecoderImpl::DecodeCapturedData");

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

  const gfx::Size dimensions = frame_format.frame_size;
  if (!VideoFrame::IsValidConfig(PIXEL_FORMAT_I420,
                                 VideoFrame::STORAGE_UNOWNED_MEMORY, dimensions,
                                 gfx::Rect(dimensions), dimensions)) {
    base::AutoLock lock(lock_);
    decoder_status_ = FAILED;
    LOG(ERROR) << "DecodeCapturedData: VideoFrame::IsValidConfig() failed";
    return;
  }

  base::UnsafeSharedMemoryRegion out_region =
      out_buffer.handle_provider->DuplicateAsUnsafeRegion();
  DCHECK(out_region.IsValid());

  media::mojom::VideoFrameInfoPtr out_frame_info =
      media::mojom::VideoFrameInfo::New();
  out_frame_info->timestamp = timestamp;
  out_frame_info->pixel_format = media::PIXEL_FORMAT_I420;
  out_frame_info->coded_size = dimensions;
  out_frame_info->visible_rect = gfx::Rect(dimensions);
  out_frame_info->metadata = VideoFrameMetadata();
  out_frame_info->metadata.frame_rate = frame_format.frame_rate;
  out_frame_info->metadata.reference_time = reference_time;
  out_frame_info->color_space = gfx::ColorSpace();

  {
    base::AutoLock lock(lock_);
    decode_done_closure_ = base::BindOnce(
        decode_done_cb_,
        ReadyFrameInBuffer(out_buffer.id, out_buffer.frame_feedback_id,
                           std::move(out_buffer.access_permission),
                           std::move(out_frame_info)));
  }

  // base::Unretained is safe because |decoder_| is deleted on
  // |decoder_task_runner_|.
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](chromeos_camera::MojoMjpegDecodeAccelerator* decoder,
             BitstreamBuffer in_buffer, VideoPixelFormat format,
             gfx::Size coded_size, base::UnsafeSharedMemoryRegion out_region) {
            decoder->Decode(std::move(in_buffer), format, coded_size,
                            std::move(out_region));
          },
          base::Unretained(decoder_.get()), std::move(in_buffer),
          media::PIXEL_FORMAT_I420, dimensions, std::move(out_region)));
}

void VideoCaptureJpegDecoderImpl::VideoFrameReady(int32_t task_id) {
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT("jpeg", "VideoCaptureJpegDecoderImpl::VideoFrameReady");
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

  TRACE_EVENT_END("jpeg",
                  GetTraceTrack(CameraTraceEvent::kJpegDecoding, task_id));
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
  TRACE_EVENT("gpu", "VideoCaptureJpegDecoderImpl::FinishInitialization");
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());

  mojo::PendingRemote<chromeos_camera::mojom::MjpegDecodeAccelerator>
      remote_decoder;
  jpeg_decoder_factory_.Run(remote_decoder.InitWithNewPipeAndPassReceiver());

  base::AutoLock lock(lock_);
  decoder_ = std::make_unique<chromeos_camera::MojoMjpegDecodeAccelerator>(
      decoder_task_runner_, std::move(remote_decoder));

  decoder_->InitializeAsync(
      this, base::BindOnce(&VideoCaptureJpegDecoderImpl::OnInitializationDone,
                           weak_ptr_factory_.GetWeakPtr()));
}

void VideoCaptureJpegDecoderImpl::OnInitializationDone(bool success) {
  TRACE_EVENT("gpu", "VideoCaptureJpegDecoderImpl::OnInitializationDone");
  DCHECK(decoder_task_runner_->RunsTasksInCurrentSequence());

  base::AutoLock lock(lock_);
  if (!success) {
    decoder_.reset();
    DLOG(ERROR) << "Failed to initialize JPEG decoder";
  }

  decoder_status_ = success ? INIT_PASSED : FAILED;
}

bool VideoCaptureJpegDecoderImpl::IsDecoding_Locked() const {
  lock_.AssertAcquired();
  return !decode_done_closure_.is_null();
}

}  // namespace media
