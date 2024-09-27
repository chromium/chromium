// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_encoder_wrapper.h"

#include "base/containers/contains.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/base/video_frame.h"
#include "media/media_buildflags.h"
#include "media/video/alpha_video_encoder_wrapper.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

MediaRecorderEncoderWrapper::EncodeTask::EncodeTask(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks capture_timestamp,
    bool request_keyframe)
    : frame(std::move(frame)),
      capture_timestamp(capture_timestamp),
      request_keyframe(request_keyframe) {}

MediaRecorderEncoderWrapper::EncodeTask::~EncodeTask() = default;

MediaRecorderEncoderWrapper::VideoParamsAndTimestamp::VideoParamsAndTimestamp(
    const media::Muxer::VideoParameters& params,
    base::TimeTicks timestamp)
    : params(params), timestamp(timestamp) {}

MediaRecorderEncoderWrapper::VideoParamsAndTimestamp::
    ~VideoParamsAndTimestamp() = default;

MediaRecorderEncoderWrapper::MediaRecorderEncoderWrapper(
    scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
    media::VideoCodecProfile profile,
    uint32_t bits_per_second,
    bool is_screencast,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    CreateEncoderCB create_encoder_cb,
    VideoTrackRecorder::OnEncodedVideoCB on_encoded_video_cb,
    OnErrorCB on_error_cb)
    : Encoder(std::move(encoding_task_runner),
              on_encoded_video_cb,
              bits_per_second),
      gpu_factories_(gpu_factories),
      profile_(profile),
      codec_(media::VideoCodecProfileToVideoCodec(profile_)),
      create_encoder_cb_(create_encoder_cb),
      on_error_cb_(std::move(on_error_cb)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  CHECK(create_encoder_cb_);
  CHECK(on_error_cb_);
  constexpr media::VideoCodec kSupportedCodecs[] = {
      media::VideoCodec::kH264,
      media::VideoCodec::kVP8,
      media::VideoCodec::kVP9,
      media::VideoCodec::kAV1,
  };
  CHECK(base::Contains(kSupportedCodecs, codec_));
  options_.latency_mode = media::VideoEncoder::LatencyMode::Quality;
  options_.bitrate = media::Bitrate::VariableBitrate(
      bits_per_second, base::ClampMul(bits_per_second, 2u).RawValue());
  options_.content_hint = is_screencast
                              ? media::VideoEncoder::ContentHint::Screen
                              : media::VideoEncoder::ContentHint::Camera;
  if (codec_ == media::VideoCodec::kH264) {
    options_.avc.produce_annexb = true;
  }
}

MediaRecorderEncoderWrapper::~MediaRecorderEncoderWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool MediaRecorderEncoderWrapper::CanEncodeAlphaChannel() const {
  // Alpha encoding is supported only with VP8 and VP9 software encoders.
  return !gpu_factories_ && (codec_ == media::VideoCodec::kVP8 ||
                             codec_ == media::VideoCodec::kVP9);
}

bool MediaRecorderEncoderWrapper::IsScreenContentEncodingForTesting() const {
  return options_.content_hint.has_value() &&
         *options_.content_hint == media::VideoEncoder::ContentHint::Screen;
}

void MediaRecorderEncoderWrapper::EnterErrorState(
    const media::EncoderStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kInError) {
    CHECK(!on_error_cb_);
    return;
  }

  metrics_provider_->SetError(status);
  state_ = State::kInError;
  pending_encode_tasks_ = {};
  params_in_encode_ = {};
  CHECK(on_error_cb_);
  std::move(on_error_cb_).Run();
}

void MediaRecorderEncoderWrapper::Reconfigure(const gfx::Size& frame_size,
                                              bool encode_alpha) {
  TRACE_EVENT2(
      "media", "MediaRecorderEncoderWrapper::ReconfigureForNewResolution",
      "frame_size", frame_size.ToString(), "encode_alpha", encode_alpha);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(encoder_);
  CHECK_NE(state_, State::kInError);
  state_ = State::kInitializing;
  encoder_->Flush(
      WTF::BindOnce(&MediaRecorderEncoderWrapper::CreateAndInitialize,
                    weak_factory_.GetWeakPtr(), frame_size, encode_alpha));
}

void MediaRecorderEncoderWrapper::CreateAndInitialize(
    const gfx::Size& frame_size,
    bool encode_alpha,
    media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT1("media", "MediaRecorderEncoderWrapper::CreateAndInitialize",
               "frame_size", frame_size.ToString());
  if (!status.is_ok()) {
    // CreateAndInitialize() is called (1) in VideoFrameBuffer() for the first
    // encoder creation with status=kOk and as Flush done callback. The status
    // can be non kOk only if it is invoked as flush callback.
    DLOG(ERROR) << "Flush() failed: " << status.message();
    EnterErrorState(status);
    return;
  }
  CHECK_NE(state_, State::kInError);
  CHECK(!encoder_ || state_ == State::kInitializing)
      << ", unexpected status: " << static_cast<int>(state_);
  state_ = State::kInitializing;
  options_.frame_size = frame_size;
  encode_alpha_ = encode_alpha;

  if (encode_alpha_) {
    CHECK(CanEncodeAlphaChannel());
    auto yuv_encoder = create_encoder_cb_.Run(gpu_factories_);
    auto alpha_encoder = create_encoder_cb_.Run(gpu_factories_);
    CHECK(yuv_encoder && alpha_encoder);
    encoder_ = std::make_unique<media::AlphaVideoEncoderWrapper>(
        std::move(yuv_encoder), std::move(alpha_encoder));
  } else {
    encoder_ = create_encoder_cb_.Run(gpu_factories_);
  }
  CHECK(encoder_);

  // MediaRecorderEncoderWrapper doesn't require an encoder to post a callback
  // because a given |on_encoded_video_cb_| already hops a thread.
  encoder_->DisablePostedCallbacks();
  metrics_provider_->Initialize(profile_, options_.frame_size,
                                /*is_hardware_encoder=*/gpu_factories_);
  encoder_->Initialize(
      profile_, options_,
      /*info_cb=*/base::DoNothing(),
      WTF::BindRepeating(&MediaRecorderEncoderWrapper::OutputEncodeData,
                         weak_factory_.GetWeakPtr()),
      WTF::BindOnce(&MediaRecorderEncoderWrapper::InitializeDone,
                    weak_factory_.GetWeakPtr()));
}

void MediaRecorderEncoderWrapper::InitializeDone(media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media", "MediaRecorderEncoderWrapper::InitizalizeDone");
  if (!status.is_ok()) {
    DLOG(ERROR) << "Initialize() failed: " << status.message();
    EnterErrorState(status);
    return;
  }
  CHECK_NE(state_, State::kInError);

  state_ = State::kEncoding;
  EncodePendingTasks();
}

void MediaRecorderEncoderWrapper::EncodeFrame(
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks capture_timestamp,
    bool request_keyframe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media", "MediaRecorderEncoderWrapper::EncodeFrame");
  if (state_ == State::kInError) {
    CHECK(!on_error_cb_);
    return;
  }
  pending_encode_tasks_.emplace_back(std::move(frame), capture_timestamp,
                                     request_keyframe);
  if (state_ == State::kEncoding) {
    EncodePendingTasks();
  }
}

void MediaRecorderEncoderWrapper::EncodePendingTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (state_ == State::kEncoding && !pending_encode_tasks_.empty()) {
    auto& task = pending_encode_tasks_.front();
    const gfx::Size& frame_size = task.frame->visible_rect().size();
    CHECK(media::IsOpaque(task.frame->format()) ||
          task.frame->format() == media::PIXEL_FORMAT_I420A);
    const bool need_alpha_encode =
        task.frame->format() == media::PIXEL_FORMAT_I420A;

    // When a frame size is different from the current frame size (or first
    // Encode() call), encoder needs to be re-created because
    // media::VideoEncoder don't support all resolution change cases.
    // If |encoder_| exists, we first Flush() to not drop frames being encoded.
    if (frame_size != options_.frame_size ||
        encode_alpha_ != need_alpha_encode) {
      if (encoder_) {
        Reconfigure(frame_size, need_alpha_encode);
      } else {
        // Only first Encode() call.
        CreateAndInitialize(frame_size, need_alpha_encode,
                            media::EncoderStatus::Codes::kOk);
      }
      return;
    }
    params_in_encode_.emplace_back(media::Muxer::VideoParameters(*task.frame),
                                   task.capture_timestamp);
    bool request_keyframe = task.request_keyframe;
    auto frame = std::move(task.frame);
    pending_encode_tasks_.pop_front();
    // Encode() calls EncodeDone() and OutputEncodeData() within a call because
    // we DisablePostedCallbacks(). Therefore, |params_in_encode_| and
    // |pending_encode_tasks_| must be changed before calling Encode().
    encoder_->Encode(std::move(frame),
                     media::VideoEncoder::EncodeOptions(request_keyframe),
                     WTF::BindOnce(&MediaRecorderEncoderWrapper::EncodeDone,
                                   weak_factory_.GetWeakPtr()));
  }
}

void MediaRecorderEncoderWrapper::EncodeDone(media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!status.is_ok()) {
    DLOG(ERROR) << "EncodeDone() failed: " << status.message();
    EnterErrorState(status);
    return;
  }
}

void MediaRecorderEncoderWrapper::OutputEncodeData(
    media::VideoEncoderOutput output,
    std::optional<media::VideoEncoder::CodecDescription> description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media", "MediaRecorderEncoderWrapper::OutputEncodeData");
  if (state_ == State::kInError) {
    CHECK(!on_error_cb_);
    return;
  }

  metrics_provider_->IncrementEncodedFrameCount();

  // TODO(crbug.com/1330919): Check OutputEncodeData() in the same order as
  // Encode().
  CHECK(!params_in_encode_.empty());
  auto [video_params, capture_timestamp] = std::move(params_in_encode_.front());
  params_in_encode_.pop_front();
  video_params.codec = codec_;

  auto buffer = media::DecoderBuffer::FromArray(std::move(output.data));
  if (encode_alpha_) {
    buffer->WritableSideData().alpha_data.assign(output.alpha_data.begin(),
                                                 output.alpha_data.end());
  }
  buffer->set_is_key_frame(output.key_frame);

  on_encoded_video_cb_.Run(video_params, std::move(buffer),
                           std::move(description), capture_timestamp);
}

}  // namespace blink
