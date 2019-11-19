// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_sender_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "media/base/video_frame.h"
#include "media/cast/sender/video_frame_factory.h"

namespace media {
namespace cast {

// The LocalVideoFrameInput class posts all incoming video frames to the main
// cast thread for processing.
class LocalVideoFrameInput : public VideoFrameInput {
 public:
  LocalVideoFrameInput(scoped_refptr<CastEnvironment> cast_environment,
                       base::WeakPtr<VideoSender> video_sender)
      : cast_environment_(cast_environment),
        video_sender_(video_sender),
        video_frame_factory_(
            video_sender.get() ?
                video_sender->CreateVideoFrameFactory().release() : nullptr) {}

  void InsertRawVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                           base::TimeTicks capture_time) final {
    cast_environment_->PostTask(
        CastEnvironment::MAIN, FROM_HERE,
        base::BindRepeating(&VideoSender::InsertRawVideoFrame, video_sender_,
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

  DISALLOW_COPY_AND_ASSIGN(LocalVideoFrameInput);
};

// The LocalAudioFrameInput class posts all incoming audio frames to the main
// cast thread for processing. Therefore frames can be inserted from any thread.
class LocalAudioFrameInput : public AudioFrameInput {
 public:
  LocalAudioFrameInput(scoped_refptr<CastEnvironment> cast_environment,
                       base::WeakPtr<AudioSender> audio_sender)
      : cast_environment_(cast_environment), audio_sender_(audio_sender) {}

  void InsertAudio(std::unique_ptr<AudioBus> audio_bus,
                   const base::TimeTicks& recorded_time) final {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&AudioSender::InsertAudio,
                                           audio_sender_,
                                           base::Passed(&audio_bus),
                                           recorded_time));
  }

 protected:
  ~LocalAudioFrameInput() final = default;

 private:
  friend class base::RefCountedThreadSafe<LocalAudioFrameInput>;

  scoped_refptr<CastEnvironment> cast_environment_;
  base::WeakPtr<AudioSender> audio_sender_;

  DISALLOW_COPY_AND_ASSIGN(LocalAudioFrameInput);
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
    const StatusChangeCallback& status_change_cb) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  CHECK(audio_config.use_external_encoder ||
        cast_environment_->HasAudioThread());

  VLOG(1) << "CastSenderImpl@" << this << "::InitializeAudio()";

  audio_sender_.reset(
      new AudioSender(cast_environment_,
                      audio_config,
                      base::Bind(&CastSenderImpl::OnAudioStatusChange,
                                 weak_factory_.GetWeakPtr(),
                                 status_change_cb),
                      transport_sender_));
  if (video_sender_) {
    DCHECK(audio_sender_->GetTargetPlayoutDelay() ==
           video_sender_->GetTargetPlayoutDelay());
  }
}

void CastSenderImpl::InitializeVideo(
    const FrameSenderConfig& video_config,
    const StatusChangeCallback& status_change_cb,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const CreateVideoEncodeMemoryCallback& create_video_encode_mem_cb) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  VLOG(1) << "CastSenderImpl@" << this << "::InitializeVideo()";

  video_sender_.reset(new VideoSender(
      cast_environment_,
      video_config,
      base::Bind(&CastSenderImpl::OnVideoStatusChange,
                 weak_factory_.GetWeakPtr(),
                 status_change_cb),
      create_vea_cb,
      create_video_encode_mem_cb,
      transport_sender_,
      base::Bind(&CastSenderImpl::SetTargetPlayoutDelay,
                 weak_factory_.GetWeakPtr())));
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
    const StatusChangeCallback& status_change_cb,
    OperationalStatus status) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (status == STATUS_INITIALIZED && !audio_frame_input_) {
    audio_frame_input_ =
        new LocalAudioFrameInput(cast_environment_, audio_sender_->AsWeakPtr());
  }
  status_change_cb.Run(status);
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
