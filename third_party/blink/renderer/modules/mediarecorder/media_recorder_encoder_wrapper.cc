// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/media_recorder_encoder_wrapper.h"

#include "base/numerics/safe_conversions.h"
#include "media/base/video_frame.h"
#include "media/media_buildflags.h"
#include "third_party/blink/renderer/modules/mediarecorder/buildflags.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

#if BUILDFLAG(ENABLE_LIBAOM)
#include "media/video/av1_video_encoder.h"
#endif

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
    CreateEncoderCB create_encoder_cb,
    VideoTrackRecorder::OnEncodedVideoCB on_encoded_video_cb,
    OnErrorCB on_error_cb)
    : Encoder(std::move(encoding_task_runner),
              on_encoded_video_cb,
              bits_per_second),
      profile_(profile),
      codec_(media::VideoCodecProfileToVideoCodec(profile_)),
      create_encoder_cb_(create_encoder_cb),
      on_error_cb_(std::move(on_error_cb)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  CHECK(on_error_cb_);
  CHECK_EQ(codec_, media::VideoCodec::kAV1);
  options_.bitrate = media::Bitrate::VariableBitrate(
      bits_per_second, base::ClampMul(bits_per_second, 2u).RawValue());
}

MediaRecorderEncoderWrapper::~MediaRecorderEncoderWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool MediaRecorderEncoderWrapper::CanEncodeAlphaChannel() const {
  // TODO(crbug.com/1424974): This should query media::VideoEncoder.
  return false;
}

void MediaRecorderEncoderWrapper::EnterErrorState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ == State::kInError) {
    CHECK(!on_error_cb_);
    return;
  }

  state_ = State::kInError;
  pending_encode_tasks_ = {};
  params_in_encode_ = {};
  CHECK(on_error_cb_);
  std::move(on_error_cb_).Run();
}

void MediaRecorderEncoderWrapper::ReconfigureForNewResolution(
    const gfx::Size& frame_size) {
  TRACE_EVENT1("media",
               "MediaRecorderEncoderWrapper::ReconfigureForNewResolution",
               "frame_size", frame_size.ToString());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(encoder_);
  CHECK_NE(state_, State::kInError);
  state_ = State::kInitializing;
  encoder_->Flush(
      WTF::BindOnce(&MediaRecorderEncoderWrapper::CreateAndInitialize,
                    weak_factory_.GetWeakPtr(), frame_size));
}

void MediaRecorderEncoderWrapper::CreateAndInitialize(
    const gfx::Size& frame_size,
    media::EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT1("media", "MediaRecorderEncoderWrapper::CreateAndInitialize",
               "frame_size", frame_size.ToString());
  if (!status.is_ok()) {
    // CreateAndInitialize() is called (1) in VideoFrameBuffer() for the first
    // encoder creation with status=kOk and as Flush done callback. The status
    // can be non kOk only if it is invoked as flush callback.
    DLOG(ERROR) << "Flush() failed: " << status.message();
    EnterErrorState();
    return;
  }
  CHECK_NE(state_, State::kInError);
  CHECK(!encoder_ || state_ == State::kInitializing)
      << ", unexpected status: " << static_cast<int>(state_);
  state_ = State::kInitializing;
  options_.frame_size = frame_size;

  encoder_ = create_encoder_cb_.Run();
  CHECK(encoder_);

  // MediaRecorderEncoderWrapper doesn't require an encoder to post a callback
  // because a given |on_encoded_video_cb_| already hops a thread.
  encoder_->DisablePostedCallbacks();
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
    EnterErrorState();
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
  CHECK_EQ(state_, State::kEncoding)
      << ", unexpected status: " << static_cast<int>(state_);

  while (!pending_encode_tasks_.empty()) {
    auto& task = pending_encode_tasks_.front();
    const gfx::Size& frame_size = task.frame->visible_rect().size();
    // When a frame size is different from the current frame size (or first
    // Encode() call), encoder needs to be re-created because
    // media::VideoEncoder don't support all resolution change cases.
    // If |encoder_| exists, we first Flush() to not drop frames being encoded.
    if (frame_size != options_.frame_size) {
      if (encoder_) {
        ReconfigureForNewResolution(frame_size);
      } else {
        // Only first Encode() call.
        CreateAndInitialize(frame_size, media::EncoderStatus::Codes::kOk);
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
    EnterErrorState();
    return;
  }
}

void MediaRecorderEncoderWrapper::OutputEncodeData(
    media::VideoEncoderOutput output,
    absl::optional<media::VideoEncoder::CodecDescription> /*description*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("media", "MediaRecorderEncoderWrapper::OutputEncodeData");
  if (state_ == State::kInError) {
    CHECK(!on_error_cb_);
    return;
  }

  // TODO(crbug.com/1330919): Check OutputEncodeData() in the same order as
  // Encode().
  CHECK(!params_in_encode_.empty());
  auto [video_params, capture_timestamp] = std::move(params_in_encode_.front());
  params_in_encode_.pop_front();
  video_params.codec = codec_;
  on_encoded_video_cb_.Run(
      video_params,
      std::string(reinterpret_cast<const char*>(output.data.get()),
                  output.size),
      /*encoded_alpha=*/std::string(), capture_timestamp, output.key_frame);
}

}  // namespace blink
