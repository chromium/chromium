// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_sender_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/base/video_frame.h"
#include "media/cast/common/video_frame_factory.h"

namespace media {
namespace cast {

// The LocalVideoFrameInput class posts all incoming video frames to the main
// cast thread for processing.
class LocalVideoFrameInput final : public VideoFrameInput {
 public:
  LocalVideoFrameInput(scoped_refptr<CastEnvironment> cast_environment,
                       base::WeakPtr<VideoSender> video_sender)
      : cast_environment_(cast_environment),
        video_sender_(video_sender),
        video_frame_factory_(
            video_sender.get() ?
                video_sender->CreateVideoFrameFactory().release() : nullptr) {}

  LocalVideoFrameInput(const LocalVideoFrameInput&) = delete;
  LocalVideoFrameInput& operator=(const LocalVideoFrameInput&) = delete;

  void InsertRawVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                           base::TimeTicks capture_time) final {
    cast_environment_->PostTask(
        CastEnvironment::MAIN, FROM_HERE,
        base::BindOnce(&VideoSender::InsertRawVideoFrame, video_sender_,
                       std::move(video_frame), capture_time));
  }

  scoped_refptr<VideoFrame> MaybeCreateOptimizedFrame(
      const gfx::Size& frame_size,
      base::TimeDelta timestamp) final {
    return video_frame_factory_ ?
        video_frame_factory_->MaybeCreateFrame(frame_size, timestamp) : nullptr;
  }

  bool CanCreateOptimizedFrames() const final {
    return video_frame_factory_.get() != nullptr;
  }

 protected:
  ~LocalVideoFrameInput() final = default;

 private:
  friend class base::RefCountedThreadSafe<LocalVideoFrameInput>;

  const scoped_refptr<CastEnvironment> cast_environment_;
  const base::WeakPtr<VideoSender> video_sender_;
  const std::unique_ptr<VideoFrameFactory> video_frame_factory_;
};

// The LocalAudioFrameInput class posts all incoming audio frames to the main
// cast thread for processing. Therefore frames can be inserted from any thread.
class LocalAudioFrameInput final : public AudioFrameInput {
 public:
  LocalAudioFrameInput(scoped_refptr<CastEnvironment> cast_environment,
                       base::WeakPtr<AudioSender> audio_sender)
      : cast_environment_(cast_environment), audio_sender_(audio_sender) {}

  LocalAudioFrameInput(const LocalAudioFrameInput&) = delete;
  LocalAudioFrameInput& operator=(const LocalAudioFrameInput&) = delete;

  void InsertAudio(std::unique_ptr<AudioBus> audio_bus,
                   const base::TimeTicks& recorded_time) final {
    cast_environment_->PostTask(
        CastEnvironment::MAIN, FROM_HERE,
        base::BindOnce(&AudioSender::InsertAudio, audio_sender_,
                       std::move(audio_bus), recorded_time));
  }

 protected:
  ~LocalAudioFrameInput() final = default;

 private:
  friend class base::RefCountedThreadSafe<LocalAudioFrameInput>;

  scoped_refptr<CastEnvironment> cast_environment_;
  base::WeakPtr<AudioSender> audio_sender_;
};

std::unique_ptr<CastSender> CastSender::Create(
    scoped_refptr<CastEnvironment> cast_environment,
    CastTransport* const transport_sender) {
  CHECK(cast_environment.get());
  return std::unique_ptr<CastSender>(
      new CastSenderImpl(cast_environment, transport_sender));
}

CastSenderImpl::CastSenderImpl(scoped_refptr<CastEnvironment> cast_environment,
                               CastTransport* const transport_sender)
    : cast_environment_(cast_environment), transport_sender_(transport_sender) {
  CHECK(cast_environment.get());
}

void CastSenderImpl::InitializeAudio(
    const FrameSenderConfig& audio_config,
    StatusChangeOnceCallback status_change_cb) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  CHECK(audio_config.use_hardware_encoder ||
        cast_environment_->HasAudioThread());

  VLOG(1) << "CastSenderImpl@" << this << "::InitializeAudio()";

  audio_sender_ = std::make_unique<AudioSender>(
      cast_environment_, audio_config,
      base::BindOnce(&CastSenderImpl::OnAudioStatusChange,
                     weak_factory_.GetWeakPtr(), std::move(status_change_cb)),
      transport_sender_);
  if (video_sender_) {
    DCHECK(audio_sender_->GetTargetPlayoutDelay() ==
           video_sender_->GetTargetPlayoutDelay());
  }
}

void CastSenderImpl::InitializeVideo(
    const FrameSenderConfig& video_config,
    std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider,
    const StatusChangeCallback& status_change_cb,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  VLOG(1) << "CastSenderImpl@" << this << "::InitializeVideo()";

  // No feedback callback, since it's ignored for CastSender.
  video_sender_ = std::make_unique<VideoSender>(
      cast_environment_, video_config,
      base::BindRepeating(&CastSenderImpl::OnVideoStatusChange,
                          weak_factory_.GetWeakPtr(), status_change_cb),
      create_vea_cb, transport_sender_, std::move(metrics_provider),
      base::BindRepeating(&CastSenderImpl::SetTargetPlayoutDelay,
                          weak_factory_.GetWeakPtr()),
      media::VideoCaptureFeedbackCB());
  if (audio_sender_) {
    DCHECK(audio_sender_->GetTargetPlayoutDelay() ==
           video_sender_->GetTargetPlayoutDelay());
  }
}

CastSenderImpl::~CastSenderImpl() {
  VLOG(1) << "CastSenderImpl@" << this << "::~CastSenderImpl()";
}

scoped_refptr<AudioFrameInput> CastSenderImpl::audio_frame_input() {
  return audio_frame_input_;
}

scoped_refptr<VideoFrameInput> CastSenderImpl::video_frame_input() {
  return video_frame_input_;
}

void CastSenderImpl::SetTargetPlayoutDelay(
    base::TimeDelta new_target_playout_delay) {
  VLOG(1) << "CastSenderImpl@" << this << "::SetTargetPlayoutDelay("
          << new_target_playout_delay.InMilliseconds() << " ms)";
  if (audio_sender_) {
    audio_sender_->SetTargetPlayoutDelay(new_target_playout_delay);
  }
  if (video_sender_) {
    video_sender_->SetTargetPlayoutDelay(new_target_playout_delay);
  }
}

void CastSenderImpl::OnAudioStatusChange(
    StatusChangeOnceCallback status_change_cb,
    OperationalStatus status) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (status == STATUS_INITIALIZED && !audio_frame_input_) {
    audio_frame_input_ =
        new LocalAudioFrameInput(cast_environment_, audio_sender_->AsWeakPtr());
  }
  std::move(status_change_cb).Run(status);
}

void CastSenderImpl::OnVideoStatusChange(
    const StatusChangeCallback& status_change_cb,
    OperationalStatus status) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (status == STATUS_INITIALIZED && !video_frame_input_) {
    video_frame_input_ =
        new LocalVideoFrameInput(cast_environment_, video_sender_->AsWeakPtr());
  }
  status_change_cb.Run(status);
}

}  // namespace cast
}  // namespace media
