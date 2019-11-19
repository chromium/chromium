// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/cast_receiver_impl.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "media/cast/net/rtcp/rtcp_utility.h"
#include "media/cast/receiver/audio_decoder.h"
#include "media/cast/receiver/video_decoder.h"

namespace media {
namespace cast {

std::unique_ptr<CastReceiver> CastReceiver::Create(
    scoped_refptr<CastEnvironment> cast_environment,
    const FrameReceiverConfig& audio_config,
    const FrameReceiverConfig& video_config,
    CastTransport* const transport) {
  return std::unique_ptr<CastReceiver>(new CastReceiverImpl(
      cast_environment, audio_config, video_config, transport));
}

CastReceiverImpl::CastReceiverImpl(
    scoped_refptr<CastEnvironment> cast_environment,
    const FrameReceiverConfig& audio_config,
    const FrameReceiverConfig& video_config,
    CastTransport* const transport)
    : cast_environment_(cast_environment),
      audio_receiver_(cast_environment, audio_config, AUDIO_EVENT, transport),
      video_receiver_(cast_environment, video_config, VIDEO_EVENT, transport),
      ssrc_of_audio_sender_(audio_config.sender_ssrc),
      ssrc_of_video_sender_(video_config.sender_ssrc),
      num_audio_channels_(audio_config.channels),
      audio_sampling_rate_(audio_config.rtp_timebase),
      audio_codec_(audio_config.codec),
      video_codec_(video_config.codec) {}

CastReceiverImpl::~CastReceiverImpl() = default;

void CastReceiverImpl::ReceivePacket(std::unique_ptr<Packet> packet) {
  const uint8_t* const data = &packet->front();
  const size_t length = packet->size();

  uint32_t ssrc_of_sender;
  if (IsRtcpPacket(data, length)) {
    ssrc_of_sender = GetSsrcOfSender(data, length);
  } else if (!RtpParser::ParseSsrc(data, length, &ssrc_of_sender)) {
    VLOG(1) << "Invalid RTP packet.";
    return;
  }

  base::WeakPtr<FrameReceiver> target;
  if (ssrc_of_sender == ssrc_of_video_sender_) {
    target = video_receiver_.AsWeakPtr();
  } else if (ssrc_of_sender == ssrc_of_audio_sender_) {
    target = audio_receiver_.AsWeakPtr();
  } else {
    VLOG(1) << "Dropping packet with a non matching sender SSRC: "
            << ssrc_of_sender;
    return;
  }
  cast_environment_->PostTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(base::IgnoreResult(&FrameReceiver::ProcessPacket),
                 target,
                 base::Passed(&packet)));
}

void CastReceiverImpl::RequestDecodedAudioFrame(
    const AudioFrameDecodedCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!callback.is_null());
  audio_receiver_.RequestEncodedFrame(base::Bind(
      &CastReceiverImpl::DecodeEncodedAudioFrame,
      // Note: Use of Unretained is safe since this Closure is guaranteed to be
      // invoked or discarded by |audio_receiver_| before destruction of |this|.
      base::Unretained(this),
      callback));
}

void CastReceiverImpl::RequestEncodedAudioFrame(
    const ReceiveEncodedFrameCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  audio_receiver_.RequestEncodedFrame(callback);
}

void CastReceiverImpl::RequestDecodedVideoFrame(
    const VideoFrameDecodedCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!callback.is_null());
  video_receiver_.RequestEncodedFrame(base::Bind(
      &CastReceiverImpl::DecodeEncodedVideoFrame,
      // Note: Use of Unretained is safe since this Closure is guaranteed to be
      // invoked or discarded by |video_receiver_| before destruction of |this|.
      base::Unretained(this),
      callback));
}

void CastReceiverImpl::RequestEncodedVideoFrame(
    const ReceiveEncodedFrameCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  video_receiver_.RequestEncodedFrame(callback);
}

void CastReceiverImpl::DecodeEncodedAudioFrame(
    const AudioFrameDecodedCallback& callback,
    std::unique_ptr<EncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!encoded_frame) {
    callback.Run(base::WrapUnique<AudioBus>(NULL), base::TimeTicks(), false);
    return;
  }

  if (!audio_decoder_) {
    audio_decoder_.reset(new AudioDecoder(cast_environment_,
                                          num_audio_channels_,
                                          audio_sampling_rate_,
                                          audio_codec_));
  }
  const FrameId frame_id = encoded_frame->frame_id;
  const RtpTimeTicks rtp_timestamp = encoded_frame->rtp_timestamp;
  const base::TimeTicks playout_time = encoded_frame->reference_time;
  audio_decoder_->DecodeFrame(
      std::move(encoded_frame),
      base::Bind(&CastReceiverImpl::EmitDecodedAudioFrame, cast_environment_,
                 callback, frame_id, rtp_timestamp, playout_time));
}

void CastReceiverImpl::DecodeEncodedVideoFrame(
    const VideoFrameDecodedCallback& callback,
    std::unique_ptr<EncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!encoded_frame) {
    callback.Run(base::WrapRefCounted<VideoFrame>(NULL), base::TimeTicks(),
                 false);
    return;
  }

  // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
  TRACE_EVENT_INSTANT1("cast_perf_test", "PullEncodedVideoFrame",
                       TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp",
                       encoded_frame->rtp_timestamp.lower_32_bits());

  if (!video_decoder_)
    video_decoder_.reset(new VideoDecoder(cast_environment_, video_codec_));
  const FrameId frame_id = encoded_frame->frame_id;
  const RtpTimeTicks rtp_timestamp = encoded_frame->rtp_timestamp;
  const base::TimeTicks playout_time = encoded_frame->reference_time;
  video_decoder_->DecodeFrame(
      std::move(encoded_frame),
      base::Bind(&CastReceiverImpl::EmitDecodedVideoFrame, cast_environment_,
                 callback, frame_id, rtp_timestamp, playout_time));
}

// static
void CastReceiverImpl::EmitDecodedAudioFrame(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const AudioFrameDecodedCallback& callback,
    FrameId frame_id,
    RtpTimeTicks rtp_timestamp,
    const base::TimeTicks& playout_time,
    std::unique_ptr<AudioBus> audio_bus,
    bool is_continuous) {
  DCHECK(cast_environment->CurrentlyOn(CastEnvironment::MAIN));

  if (audio_bus.get()) {
    // TODO(miu): This is reporting incorrect timestamp and delay.
    // http://crbug.com/547251
    std::unique_ptr<FrameEvent> playout_event(new FrameEvent());
    playout_event->timestamp = cast_environment->Clock()->NowTicks();
    playout_event->type = FRAME_PLAYOUT;
    playout_event->media_type = AUDIO_EVENT;
    playout_event->rtp_timestamp = rtp_timestamp;
    playout_event->frame_id = frame_id;
    playout_event->delay_delta = playout_time - playout_event->timestamp;
    cast_environment->logger()->DispatchFrameEvent(std::move(playout_event));
  }

  callback.Run(std::move(audio_bus), playout_time, is_continuous);
}

// static
void CastReceiverImpl::EmitDecodedVideoFrame(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const VideoFrameDecodedCallback& callback,
    FrameId frame_id,
    RtpTimeTicks rtp_timestamp,
    const base::TimeTicks& playout_time,
    scoped_refptr<VideoFrame> video_frame,
    bool is_continuous) {
  DCHECK(cast_environment->CurrentlyOn(CastEnvironment::MAIN));

  if (video_frame) {
    // TODO(miu): This is reporting incorrect timestamp and delay.
    // http://crbug.com/547251
    std::unique_ptr<FrameEvent> playout_event(new FrameEvent());
    playout_event->timestamp = cast_environment->Clock()->NowTicks();
    playout_event->type = FRAME_PLAYOUT;
    playout_event->media_type = VIDEO_EVENT;
    playout_event->rtp_timestamp = rtp_timestamp;
    playout_event->frame_id = frame_id;
    playout_event->delay_delta = playout_time - playout_event->timestamp;
    cast_environment->logger()->DispatchFrameEvent(std::move(playout_event));

    // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
    TRACE_EVENT_INSTANT2("cast_perf_test", "VideoFrameDecoded",
                         TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp",
                         rtp_timestamp.lower_32_bits(), "playout_time",
                         (playout_time - base::TimeTicks()).InMicroseconds());
  }

  callback.Run(std::move(video_frame), playout_time, is_continuous);
}

}  // namespace cast
}  // namespace media
