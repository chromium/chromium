// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/cast_streaming/config_conversions.h"

#include "base/notreached.h"
#include "media/base/media_util.h"

namespace cast_streaming {

openscreen::cast::AudioCaptureConfig AudioDecoderConfigToAudioCaptureConfig(
    const media::AudioDecoderConfig& audio_config) {
  openscreen::cast::AudioCaptureConfig audio_capture_config;

  switch (audio_config.codec()) {
    case media::AudioCodec::kCodecAAC:
      audio_capture_config.codec = openscreen::cast::AudioCodec::kAac;
      break;
    case media::AudioCodec::kCodecOpus:
      audio_capture_config.codec = openscreen::cast::AudioCodec::kOpus;
      break;
    default:
      NOTREACHED();
  }

  audio_capture_config.channels =
      media::ChannelLayoutToChannelCount(audio_config.channel_layout());
  audio_capture_config.sample_rate = audio_config.samples_per_second();

  return audio_capture_config;
}

openscreen::cast::VideoCaptureConfig VideoDecoderConfigToVideoCaptureConfig(
    const media::VideoDecoderConfig& video_config) {
  openscreen::cast::VideoCaptureConfig video_capture_config;

  switch (video_config.codec()) {
    case media::VideoCodec::kCodecH264:
      video_capture_config.codec = openscreen::cast::VideoCodec::kH264;
      break;
    case media::VideoCodec::kCodecVP8:
      video_capture_config.codec = openscreen::cast::VideoCodec::kVp8;
      break;
    default:
      NOTREACHED();
  }

  video_capture_config.resolutions.push_back(
      {video_config.visible_rect().width(),
       video_config.visible_rect().height()});
  return video_capture_config;
}

media::AudioDecoderConfig AudioCaptureConfigToAudioDecoderConfig(
    const openscreen::cast::AudioCaptureConfig& audio_capture_config) {
  // Gather data for the audio decoder config.
  media::AudioCodec media_audio_codec = media::AudioCodec::kUnknownAudioCodec;
  switch (audio_capture_config.codec) {
    case openscreen::cast::AudioCodec::kAac:
      media_audio_codec = media::AudioCodec::kCodecAAC;
      break;
    case openscreen::cast::AudioCodec::kOpus:
      media_audio_codec = media::AudioCodec::kCodecOpus;
      break;
    default:
      NOTREACHED();
      break;
  }

  return media::AudioDecoderConfig(
      media_audio_codec, media::SampleFormat::kSampleFormatF32,
      media::GuessChannelLayout(audio_capture_config.channels),
      audio_capture_config.sample_rate /* samples_per_second */,
      media::EmptyExtraData(), media::EncryptionScheme::kUnencrypted);
}

media::VideoDecoderConfig VideoCaptureConfigToVideoDecoderConfig(
    const openscreen::cast::VideoCaptureConfig& video_capture_config) {
  // Gather data for the video decoder config.
  uint32_t video_width = video_capture_config.resolutions[0].width;
  uint32_t video_height = video_capture_config.resolutions[0].height;
  gfx::Size video_size(video_width, video_height);
  gfx::Rect video_rect(video_width, video_height);

  media::VideoCodec media_video_codec = media::VideoCodec::kUnknownVideoCodec;
  media::VideoCodecProfile video_codec_profile =
      media::VideoCodecProfile::VIDEO_CODEC_PROFILE_UNKNOWN;
  switch (video_capture_config.codec) {
    case openscreen::cast::VideoCodec::kH264:
      media_video_codec = media::VideoCodec::kCodecH264;
      video_codec_profile = media::VideoCodecProfile::H264PROFILE_BASELINE;
      break;
    case openscreen::cast::VideoCodec::kVp8:
      media_video_codec = media::VideoCodec::kCodecVP8;
      video_codec_profile = media::VideoCodecProfile::VP8PROFILE_MIN;
      break;
    case openscreen::cast::VideoCodec::kHevc:
    case openscreen::cast::VideoCodec::kVp9:
    default:
      NOTREACHED();
      break;
  }

  return media::VideoDecoderConfig(
      media_video_codec, video_codec_profile,
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::VideoTransformation(), video_size, video_rect, video_size,
      media::EmptyExtraData(), media::EncryptionScheme::kUnencrypted);
}

}  // namespace cast_streaming
