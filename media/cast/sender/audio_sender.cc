// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/audio_sender.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/encoding/audio_encoder.h"
#include "media/cast/net/cast_transport_config.h"

namespace media::cast {

AudioSender::AudioSender(scoped_refptr<CastEnvironment> cast_environment,
                         const FrameSenderConfig& audio_config,
                         StatusChangeOnceCallback status_change_cb,
                         CastTransport* const transport_sender)
    : cast_environment_(cast_environment),
      rtp_timebase_(audio_config.rtp_timebase),
      frame_sender_(FrameSender::Create(cast_environment,
                                        audio_config,
                                        transport_sender,
                                        this)) {
  if (!audio_config.use_external_encoder) {
    audio_encoder_ = std::make_unique<AudioEncoder>(
        std::move(cast_environment), audio_config.channels, rtp_timebase_,
        audio_config.max_bitrate, audio_config.codec,
        base::BindRepeating(&AudioSender::OnEncodedAudioFrame, AsWeakPtr()));
  }

  // AudioEncoder provides no operational status changes during normal use.
  // Post a task now with its initialization result status to allow the client
  // to start sending frames.
  cast_environment_->PostTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::BindOnce(std::move(status_change_cb),
                     audio_encoder_ ? audio_encoder_->InitializationResult()
                                    : STATUS_INVALID_CONFIGURATION));

  // The number of samples per encoded audio frame depends on the codec and its
  // initialization parameters. Now that we have an encoder, we can calculate
  // the maximum frame rate.
  frame_sender_->SetMaxFrameRate(rtp_timebase_ /
                                 audio_encoder_->GetSamplesPerFrame());
}

AudioSender::~AudioSender() = default;

void AudioSender::InsertAudio(std::unique_ptr<AudioBus> audio_bus,
                              const base::TimeTicks& recorded_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  if (!audio_encoder_) {
    NOTREACHED();
    return;
  }

  const base::TimeDelta next_frame_duration =
      RtpTimeDelta::FromTicks(audio_bus->frames()).ToTimeDelta(rtp_timebase_);
  if (frame_sender_->ShouldDropNextFrame(next_frame_duration))
    return;

  samples_in_encoder_ += audio_bus->frames();

  audio_encoder_->InsertAudio(std::move(audio_bus), recorded_time);
}

void AudioSender::SetTargetPlayoutDelay(
    base::TimeDelta new_target_playout_delay) {
  frame_sender_->SetTargetPlayoutDelay(new_target_playout_delay);
}

base::TimeDelta AudioSender::GetTargetPlayoutDelay() const {
  return frame_sender_->GetTargetPlayoutDelay();
}

base::WeakPtr<AudioSender> AudioSender::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

int AudioSender::GetNumberOfFramesInEncoder() const {
  // Note: It's possible for a partial frame to be in the encoder, but returning
  // the floor() is good enough for the "design limit" check in FrameSenderImpl.
  return samples_in_encoder_ / audio_encoder_->GetSamplesPerFrame();
}

base::TimeDelta AudioSender::GetEncoderBacklogDuration() const {
  return RtpTimeDelta::FromTicks(samples_in_encoder_)
      .ToTimeDelta(rtp_timebase_);
}

void AudioSender::OnEncodedAudioFrame(
    std::unique_ptr<SenderEncodedFrame> encoded_frame,
    int samples_skipped) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  samples_in_encoder_ -= audio_encoder_->GetSamplesPerFrame() + samples_skipped;
  DCHECK_GE(samples_in_encoder_, 0);
  frame_sender_->EnqueueFrame(std::move(encoded_frame));
}

}  // namespace media::cast
