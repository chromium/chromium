// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_config.h"

namespace media {
namespace cast {

VideoCodecParams::VideoCodecParams()
    : max_qp(kDefaultMaxQp),
      min_qp(kDefaultMinQp),
      max_cpu_saver_qp(kDefaultMaxCpuSaverQp),
      max_number_of_video_buffers_used(kDefaultNumberOfVideoBuffers),
      number_of_encode_threads(1) {}

VideoCodecParams::VideoCodecParams(const VideoCodecParams& other) = default;

VideoCodecParams::~VideoCodecParams() = default;

FrameSenderConfig::FrameSenderConfig()
    : sender_ssrc(0),
      receiver_ssrc(0),
      // In production, these values are overridden by the mirror settings
      // and potentially the mirroring session parameters, however we provide
      // a reasonable default here for some use cases, such as tests.
      // All three delays are set to the same value due to adaptive latency
      // being disabled in Chrome. This will be fixed as part of the migration
      // to libcast.
      min_playout_delay(kDefaultTargetPlayoutDelay),
      max_playout_delay(kDefaultTargetPlayoutDelay),
      animated_playout_delay(min_playout_delay),
      rtp_payload_type(RtpPayloadType::UNKNOWN),
      use_external_encoder(false),
      rtp_timebase(0),
      channels(0),
      max_bitrate(0),
      min_bitrate(0),
      start_bitrate(0),
      max_frame_rate(kDefaultMaxFrameRate),
      codec(CODEC_UNKNOWN) {}

FrameSenderConfig::FrameSenderConfig(const FrameSenderConfig& other) = default;

FrameSenderConfig::~FrameSenderConfig() = default;

FrameReceiverConfig::FrameReceiverConfig()
    : receiver_ssrc(0),
      sender_ssrc(0),
      rtp_max_delay_ms(kDefaultTargetPlayoutDelay.InMilliseconds()),
      rtp_payload_type(RtpPayloadType::UNKNOWN),
      rtp_timebase(0),
      channels(0),
      target_frame_rate(0),
      codec(CODEC_UNKNOWN) {}

FrameReceiverConfig::FrameReceiverConfig(const FrameReceiverConfig& other) =
    default;

FrameReceiverConfig::~FrameReceiverConfig() = default;

}  // namespace cast
}  // namespace media
