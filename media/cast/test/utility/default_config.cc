// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/default_config.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "media/cast/cast_config.h"
#include "media/cast/constants.h"
#include "media/cast/net/cast_transport_config.h"

namespace media {
namespace cast {

FrameReceiverConfig GetDefaultAudioReceiverConfig() {
  FrameReceiverConfig config;
  config.receiver_ssrc = 2;
  config.sender_ssrc = 1;
  config.rtp_max_delay_ms = kDefaultTargetPlayoutDelay.InMilliseconds();
  config.rtp_payload_type = RtpPayloadType::AUDIO_OPUS;
  config.rtp_timebase = 48000;
  config.channels = 2;
  config.target_frame_rate = 100;  // 10ms of signal per frame
  config.codec = media::cast::Codec::kAudioOpus;
  return config;
}

FrameReceiverConfig GetDefaultVideoReceiverConfig() {
  FrameReceiverConfig config;
  config.receiver_ssrc = 12;
  config.sender_ssrc = 11;
  config.rtp_max_delay_ms = kDefaultTargetPlayoutDelay.InMilliseconds();
  config.rtp_payload_type = RtpPayloadType::VIDEO_VP8;
  config.rtp_timebase = kVideoFrequency;
  config.channels = 1;
  config.target_frame_rate = kDefaultMaxFrameRate;
  config.codec = media::cast::Codec::kVideoVp8;
  return config;
}

FrameSenderConfig GetDefaultAudioSenderConfig() {
  FrameReceiverConfig recv_config = GetDefaultAudioReceiverConfig();
  FrameSenderConfig config;
  config.sender_ssrc = recv_config.sender_ssrc;
  config.receiver_ssrc = recv_config.receiver_ssrc;
  config.rtp_payload_type = recv_config.rtp_payload_type;
  config.use_hardware_encoder = false;
  config.rtp_timebase = recv_config.rtp_timebase;
  config.channels = recv_config.channels;
  config.max_bitrate = config.min_bitrate = config.start_bitrate =
      kDefaultAudioEncoderBitrate;
  config.max_frame_rate = recv_config.target_frame_rate;
  config.codec = recv_config.codec;
  return config;
}

FrameSenderConfig GetDefaultVideoSenderConfig() {
  FrameReceiverConfig recv_config = GetDefaultVideoReceiverConfig();
  FrameSenderConfig config;
  config.sender_ssrc = recv_config.sender_ssrc;
  config.receiver_ssrc = recv_config.receiver_ssrc;
  config.rtp_payload_type = recv_config.rtp_payload_type;
  config.use_hardware_encoder = false;
  config.rtp_timebase = recv_config.rtp_timebase;
  config.max_bitrate = kDefaultMaxVideoBitrate;
  config.min_bitrate = kDefaultMinVideoBitrate;
  config.start_bitrate = config.max_bitrate;
  config.max_frame_rate = recv_config.target_frame_rate;
  config.codec = recv_config.codec;
  config.video_codec_params.max_qp = kDefaultMaxQp;
  config.video_codec_params.min_qp = kDefaultMinQp;
  config.video_codec_params.max_cpu_saver_qp = kDefaultMaxCpuSaverQp;
  config.video_codec_params.max_number_of_video_buffers_used =
      kDefaultNumberOfVideoBuffers;
  config.video_codec_params.number_of_encode_threads = 2;
  return config;
}

}  // namespace cast
}  // namespace media
