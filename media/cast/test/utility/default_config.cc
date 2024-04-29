// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/utility/default_config.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"
#include "media/cast/cast_config.h"
#include "media/cast/constants.h"

namespace media {
namespace cast {

FrameSenderConfig GetDefaultAudioSenderConfig() {
  FrameSenderConfig config;
  config.sender_ssrc = 1;
  config.receiver_ssrc = 2;
  config.rtp_payload_type = RtpPayloadType::AUDIO_OPUS;
  config.use_hardware_encoder = false;
  config.rtp_timebase = 48000;
  config.channels = 2;
  config.max_bitrate = config.min_bitrate = config.start_bitrate =
      kDefaultAudioEncoderBitrate;
  config.max_frame_rate = 100;  // 10ms of signal per frame
  config.audio_codec_params = AudioCodecParams({.codec = AudioCodec::kOpus});
  return config;
}

FrameSenderConfig GetDefaultVideoSenderConfig() {
  FrameSenderConfig config;
  config.sender_ssrc = 11;
  config.receiver_ssrc = 12;
  config.rtp_payload_type = RtpPayloadType::VIDEO_VP8;
  config.use_hardware_encoder = false;
  config.rtp_timebase = kVideoFrequency;
  config.start_bitrate = config.min_bitrate = config.max_bitrate =
      kDefaultMaxVideoBitrate;
  config.max_frame_rate = kDefaultMaxFrameRate;
  VideoCodecParams video_codec_params;
  video_codec_params.codec = VideoCodec::kVP8;
  video_codec_params.max_qp = kDefaultMaxQp;
  video_codec_params.min_qp = kDefaultMinQp;
  video_codec_params.max_cpu_saver_qp = kDefaultMaxCpuSaverQp;
  video_codec_params.max_number_of_video_buffers_used =
      kDefaultNumberOfVideoBuffers;
  video_codec_params.number_of_encode_threads = 2;
  config.video_codec_params = std::move(video_codec_params);
  return config;
}

}  // namespace cast
}  // namespace media
