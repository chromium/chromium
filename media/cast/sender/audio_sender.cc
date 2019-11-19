// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/audio_sender.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/sender/audio_encoder.h"

namespace media {
namespace cast {

AudioSender::AudioSender(scoped_refptr<CastEnvironment> cast_environment,
                         const FrameSenderConfig& audio_config,
                         const StatusChangeCallback& status_change_cb,
                         CastTransport* const transport_sender)
    : FrameSender(cast_environment,
                  transport_sender,
                  audio_config,
                  NewFixedCongestionControl(audio_config.max_bitrate)),
      samples_in_encoder_(0) {
  if (!audio_config.use_external_encoder) {
    audio_encoder_.reset(new AudioEncoder(
        cast_environment, audio_config.channels, audio_config.rtp_timebase,
        audio_config.max_bitrate, audio_config.codec,
        base::Bind(&AudioSender::OnEncodedAudioFrame, AsWeakPtr(),
                   audio_config.max_bitrate)));
  }

  // AudioEncoder provides no operational status changes during normal use.
  // Post a task now with its initialization result status to allow the client
  // to start sending frames.
  cast_environment_->PostTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(status_change_cb,
                 audio_encoder_ ? audio_encoder_->InitializationResult() :
                     STATUS_INVALID_CONFIGURATION));

  // The number of samples per encoded audio frame depends on the codec and its
  // initialization parameters. Now that we have an encoder, we can calculate
  // the maximum frame rate.
  max_frame_rate_ =
      audio_config.rtp_timebase / audio_encoder_->GetSamplesPerFrame();
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
      RtpTimeDelta::FromTicks(audio_bus->frames()).ToTimeDelta(rtp_timebase());
  if (ShouldDropNextFrame(next_frame_duration))
    return;

  samples_in_encoder_ += audio_bus->frames();

  audio_encoder_->InsertAudio(std::move(audio_bus), recorded_time);
}

base::WeakPtr<AudioSender> AudioSender::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

int AudioSender::GetNumberOfFramesInEncoder() const {
  // Note: It's possible for a partial frame to be in the encoder, but returning
  // the floor() is good enough for the "design limit" check in FrameSender.
  return samples_in_encoder_ / audio_encoder_->GetSamplesPerFrame();
}

base::TimeDelta AudioSender::GetInFlightMediaDuration() const {
  const int samples_in_flight = samples_in_encoder_ +
      GetUnacknowledgedFrameCount() * audio_encoder_->GetSamplesPerFrame();
  return RtpTimeDelta::FromTicks(samples_in_flight).ToTimeDelta(rtp_timebase());
}

void AudioSender::OnEncodedAudioFrame(
    int encoder_bitrate,
    std::unique_ptr<SenderEncodedFrame> encoded_frame,
    int samples_skipped) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  samples_in_encoder_ -= audio_encoder_->GetSamplesPerFrame() + samples_skipped;
  DCHECK_GE(samples_in_encoder_, 0);

  SendEncodedFrame(encoder_bitrate, std::move(encoded_frame));
}

}  // namespace cast
}  // namespace media
