// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_config.h"

namespace media::cast {

VideoCodecParams::VideoCodecParams() = default;
VideoCodecParams::VideoCodecParams(VideoCodec codec) : codec(codec) {}
VideoCodecParams::VideoCodecParams(const VideoCodecParams& other) = default;
VideoCodecParams::VideoCodecParams(VideoCodecParams&& other) = default;
VideoCodecParams& VideoCodecParams::operator=(const VideoCodecParams& other) =
    default;
VideoCodecParams& VideoCodecParams::operator=(VideoCodecParams&& other) =
    default;
VideoCodecParams::~VideoCodecParams() = default;

FrameSenderConfig::FrameSenderConfig() = default;
FrameSenderConfig::FrameSenderConfig(
    uint32_t sender_ssrc,
    uint32_t receiver_ssrc,
    base::TimeDelta min_playout_delay,
    base::TimeDelta max_playout_delay,
    RtpPayloadType rtp_payload_type,
    bool use_hardware_encoder,
    int rtp_timebase,
    int channels,
    int max_bitrate,
    int min_bitrate,
    int start_bitrate,
    double max_frame_rate,
    std::string aes_key,
    std::string aes_iv_mask,
    std::optional<VideoCodecParams> video_codec_params,
    std::optional<AudioCodecParams> audio_codec_params)
    : sender_ssrc(sender_ssrc),
      receiver_ssrc(receiver_ssrc),
      min_playout_delay(min_playout_delay),
      max_playout_delay(max_playout_delay),
      rtp_payload_type(rtp_payload_type),
      use_hardware_encoder(use_hardware_encoder),
      rtp_timebase(rtp_timebase),
      channels(channels),
      max_bitrate(max_bitrate),
      min_bitrate(min_bitrate),
      start_bitrate(start_bitrate),
      max_frame_rate(max_frame_rate),
      aes_key(aes_key),
      aes_iv_mask(aes_iv_mask),
      video_codec_params(video_codec_params),
      audio_codec_params(audio_codec_params) {
  CHECK(video_codec_params || audio_codec_params);
}

FrameSenderConfig::FrameSenderConfig(const FrameSenderConfig& other) = default;
FrameSenderConfig::FrameSenderConfig(FrameSenderConfig&& other) = default;
FrameSenderConfig& FrameSenderConfig::operator=(
    const FrameSenderConfig& other) = default;
FrameSenderConfig& FrameSenderConfig::operator=(FrameSenderConfig&& other) =
    default;
FrameSenderConfig::~FrameSenderConfig() = default;

}  // namespace media::cast
