// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/audio_sender.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/encoding/audio_encoder.h"
#include "media/cast/sender/frame_sender.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"

namespace media::cast {
namespace {

// UMA histogram for the percentage of dropped audio frames.
constexpr char kHistogramDroppedFrames[] =
    "CastStreaming.Sender.Audio.PercentDroppedFrames";

// UMA histogram for recording when a frame is dropped.
constexpr char kHistogramFrameDropped[] =
    "CastStreaming.Sender.Audio.FrameDropped";

}  // namespace

AudioSender::AudioSender(scoped_refptr<CastEnvironment> cast_environment,
                         const FrameSenderConfig& audio_config,
                         StatusChangeOnceCallback status_change_cb,
                         std::unique_ptr<openscreen::cast::Sender> sender)
    : cast_environment_(cast_environment),
      rtp_timebase_(audio_config.rtp_timebase),
      frame_sender_(FrameSender::Create(cast_environment,
                                        audio_config,
                                        std::move(sender),
                                        *this)) {
  if (!audio_config.use_hardware_encoder) {
    audio_encoder_ = std::make_unique<AudioEncoder>(
        std::move(cast_environment), audio_config.channels, rtp_timebase_,
        audio_config.max_bitrate, audio_config.audio_codec(),
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

AudioSender::~AudioSender() {
  // Record the number of frames dropped during this session.
  base::UmaHistogramPercentage(kHistogramDroppedFrames,
                               (number_of_frames_dropped_ * 100) /
                                   std::max(1, number_of_frames_inserted_));
}

void AudioSender::InsertAudio(std::unique_ptr<AudioBus> audio_bus,
                              base::TimeTicks recorded_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  CHECK(audio_encoder_);

  number_of_frames_inserted_++;
  const base::TimeDelta next_frame_duration =
      ToTimeDelta(RtpTimeDelta::FromTicks(audio_bus->frames()), rtp_timebase_);
  const CastStreamingFrameDropReason reason =
      frame_sender_->ShouldDropNextFrame(next_frame_duration);
  if (reason != CastStreamingFrameDropReason::kNotDropped) {
    number_of_frames_dropped_++;
    base::UmaHistogramEnumeration(kHistogramFrameDropped, reason);
    TRACE_EVENT_INSTANT2("cast.stream", "Audio Frame Drop (raw frame)",
                         TRACE_EVENT_SCOPE_THREAD, "duration",
                         next_frame_duration, "reason", reason);
    return;
  }

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

int AudioSender::GetEncoderBitrate() const {
  return audio_encoder_->GetBitrate();
}

base::WeakPtr<AudioSender> AudioSender::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

AudioSender::AudioSender() = default;

int AudioSender::GetNumberOfFramesInEncoder() const {
  // Note: It's possible for a partial frame to be in the encoder, but returning
  // the floor() is good enough for the "design limit" check in FrameSenderImpl.
  return samples_in_encoder_ / audio_encoder_->GetSamplesPerFrame();
}

base::TimeDelta AudioSender::GetEncoderBacklogDuration() const {
  return ToTimeDelta(RtpTimeDelta::FromTicks(samples_in_encoder_),
                     rtp_timebase_);
}

void AudioSender::OnEncodedAudioFrame(
    std::unique_ptr<SenderEncodedFrame> encoded_frame,
    int samples_skipped) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  samples_in_encoder_ -= audio_encoder_->GetSamplesPerFrame() + samples_skipped;
  DCHECK_GE(samples_in_encoder_, 0);

  const RtpTimeTicks rtp_timestamp = encoded_frame->rtp_timestamp;
  const CastStreamingFrameDropReason reason =
      frame_sender_->EnqueueFrame(std::move(encoded_frame));
  if (reason != CastStreamingFrameDropReason::kNotDropped) {
    number_of_frames_dropped_++;
    base::UmaHistogramEnumeration(kHistogramFrameDropped, reason);
    TRACE_EVENT_INSTANT2("cast.stream", "Audio Frame Drop (already encoded)",
                         TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp",
                         rtp_timestamp.lower_32_bits(), "reason", reason);
  }
}

}  // namespace media::cast
