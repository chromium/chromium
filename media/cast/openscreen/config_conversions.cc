// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/openscreen/config_conversions.h"

#include "base/check.h"
#include "base/notreached.h"
#include "media/base/media_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media::cast {
namespace {

media::VideoCodecProfile ToVideoDecoderConfigCodecProfile(
    openscreen::cast::VideoCodec codec) {
  switch (codec) {
    // TODO(b/186875732): Determine values for Hevc, Vp9, Av1 experimentally.
    case openscreen::cast::VideoCodec::kH264:
      return media::VideoCodecProfile::H264PROFILE_BASELINE;
    case openscreen::cast::VideoCodec::kHevc:
      return media::VideoCodecProfile::HEVCPROFILE_MAIN;
    case openscreen::cast::VideoCodec::kVp8:
      return media::VideoCodecProfile::VP8PROFILE_MIN;
    case openscreen::cast::VideoCodec::kVp9:
      return media::VideoCodecProfile::VP9PROFILE_PROFILE0;
    case openscreen::cast::VideoCodec::kAv1:
      return media::VideoCodecProfile::AV1PROFILE_PROFILE_MAIN;
    case openscreen::cast::VideoCodec::kNotSpecified:
      break;
  }

  NOTREACHED();
}

media::AudioCodec ToAudioDecoderConfigCodec(
    openscreen::cast::AudioCodec codec) {
  switch (codec) {
    case openscreen::cast::AudioCodec::kAac:
      return media::AudioCodec::kAAC;
    case openscreen::cast::AudioCodec::kOpus:
      return media::AudioCodec::kOpus;
    case openscreen::cast::AudioCodec::kNotSpecified:
      break;
  }

  NOTREACHED();
}

media::VideoCodec ToVideoDecoderConfigCodec(
    openscreen::cast::VideoCodec codec) {
  switch (codec) {
    case openscreen::cast::VideoCodec::kH264:
      return media::VideoCodec::kH264;
    case openscreen::cast::VideoCodec::kVp8:
      return media::VideoCodec::kVP8;
    case openscreen::cast::VideoCodec::kHevc:
      return media::VideoCodec::kHEVC;
    case openscreen::cast::VideoCodec::kVp9:
      return media::VideoCodec::kVP9;
    case openscreen::cast::VideoCodec::kAv1:
      return media::VideoCodec::kAV1;
    case openscreen::cast::VideoCodec::kNotSpecified:
      break;
  }

  NOTREACHED();
}

}  // namespace

openscreen::cast::AudioCodec ToAudioCaptureConfigCodec(
    media::AudioCodec codec) {
  switch (codec) {
    case media::AudioCodec::kAAC:
      return openscreen::cast::AudioCodec::kAac;
    case media::AudioCodec::kOpus:
      return openscreen::cast::AudioCodec::kOpus;
    default:
      break;
  }

  NOTREACHED();
}

openscreen::cast::VideoCodec ToVideoCaptureConfigCodec(
    media::VideoCodec codec) {
  switch (codec) {
    case media::VideoCodec::kH264:
      return openscreen::cast::VideoCodec::kH264;
    case media::VideoCodec::kVP8:
      return openscreen::cast::VideoCodec::kVp8;
    case media::VideoCodec::kHEVC:
      return openscreen::cast::VideoCodec::kHevc;
    case media::VideoCodec::kVP9:
      return openscreen::cast::VideoCodec::kVp9;
    case media::VideoCodec::kAV1:
      return openscreen::cast::VideoCodec::kAv1;
    default:
      break;
  }

  NOTREACHED();
}

openscreen::cast::AudioCaptureConfig ToAudioCaptureConfig(
    const media::AudioDecoderConfig& audio_config) {
  DCHECK(!audio_config.is_encrypted());

  openscreen::cast::AudioCaptureConfig audio_capture_config;
  audio_capture_config.codec = ToAudioCaptureConfigCodec(audio_config.codec());
  audio_capture_config.channels =
      media::ChannelLayoutToChannelCount(audio_config.channel_layout());
  audio_capture_config.sample_rate = audio_config.samples_per_second();
  audio_capture_config.bit_rate = 0;  // Selected by the sender.

  return audio_capture_config;
}

openscreen::cast::VideoCaptureConfig ToVideoCaptureConfig(
    const media::VideoDecoderConfig& video_config) {
  DCHECK(!video_config.is_encrypted());

  openscreen::cast::VideoCaptureConfig video_capture_config;
  video_capture_config.codec = ToVideoCaptureConfigCodec(video_config.codec());
  video_capture_config.resolutions.push_back(
      {video_config.visible_rect().width(),
       video_config.visible_rect().height()});
  video_capture_config.max_bit_rate = 0;  // Selected by the sender.
  return video_capture_config;
}

media::AudioDecoderConfig ToAudioDecoderConfig(
    const openscreen::cast::AudioCaptureConfig& audio_capture_config) {
  media::AudioCodec media_audio_codec =
      ToAudioDecoderConfigCodec(audio_capture_config.codec);

  return media::AudioDecoderConfig(
      media_audio_codec, media::SampleFormat::kSampleFormatF32,
      media::GuessChannelLayout(audio_capture_config.channels),
      audio_capture_config.sample_rate /* samples_per_second */,
      media::EmptyExtraData(), media::EncryptionScheme::kUnencrypted);
}

media::VideoDecoderConfig ToVideoDecoderConfig(
    const openscreen::cast::VideoCaptureConfig& video_capture_config) {
  // Gather data for the video decoder config.
  DCHECK(video_capture_config.resolutions.size());
  uint32_t video_width = video_capture_config.resolutions[0].width;
  uint32_t video_height = video_capture_config.resolutions[0].height;
  gfx::Size video_size(video_width, video_height);
  gfx::Rect video_rect(video_width, video_height);

  media::VideoCodec media_video_codec =
      ToVideoDecoderConfigCodec(video_capture_config.codec);
  media::VideoCodecProfile video_codec_profile =
      ToVideoDecoderConfigCodecProfile(video_capture_config.codec);

  return media::VideoDecoderConfig(
      media_video_codec, video_codec_profile,
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::VideoTransformation(), video_size, video_rect, video_size,
      media::EmptyExtraData(), media::EncryptionScheme::kUnencrypted);
}

}  // namespace media::cast
