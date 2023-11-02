// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/common/openscreen_conversion_helpers.h"

namespace media::cast {

openscreen::Clock::time_point ToOpenscreenTimePoint(base::TimeTicks ticks) {
  static_assert(sizeof(openscreen::Clock::time_point::rep) >=
                sizeof(base::TimeTicks::Max()));
  return openscreen::Clock::time_point(
      std::chrono::microseconds(ticks.since_origin().InMicroseconds()));
}

// Returns the tick count in the given timebase nearest to the base::TimeDelta.
int64_t TimeDeltaToTicks(base::TimeDelta delta, int timebase) {
  DCHECK_GT(timebase, 0);
  const double ticks = delta.InSecondsF() * timebase + 0.5 /* rounding */;
  return base::checked_cast<int64_t>(ticks);
}

openscreen::cast::RtpTimeTicks ToRtpTimeTicks(base::TimeDelta delta,
                                              int timebase) {
  return openscreen::cast::RtpTimeTicks(TimeDeltaToTicks(delta, timebase));
}

openscreen::cast::RtpTimeDelta ToRtpTimeDelta(base::TimeDelta delta,
                                              int timebase) {
  return openscreen::cast::RtpTimeDelta::FromTicks(
      TimeDeltaToTicks(delta, timebase));
}

base::TimeDelta ToTimeDelta(openscreen::cast::RtpTimeDelta rtp_delta,
                            int timebase) {
  DCHECK_GT(timebase, 0);
  return base::Microseconds(
      rtp_delta.ToDuration<std::chrono::microseconds>(timebase).count());
}

base::TimeDelta ToTimeDelta(openscreen::cast::RtpTimeTicks rtp_ticks,
                            int timebase) {
  DCHECK_GT(timebase, 0);
  return ToTimeDelta(rtp_ticks - openscreen::cast::RtpTimeTicks{}, timebase);
}

base::TimeDelta ToTimeDelta(openscreen::Clock::duration tp) {
  return base::Microseconds(
      std::chrono::duration_cast<std::chrono::microseconds>(tp).count());
}

const openscreen::cast::EncodedFrame ToOpenscreenEncodedFrame(
    const SenderEncodedFrame& encoded_frame) {
  return openscreen::cast::EncodedFrame(
      encoded_frame.dependency, encoded_frame.frame_id,
      encoded_frame.referenced_frame_id, encoded_frame.rtp_timestamp,
      ToOpenscreenTimePoint(encoded_frame.reference_time),
      std::chrono::milliseconds(encoded_frame.new_playout_delay_ms),
      // We return a const EncodedFrame, so this is safe even though weird.
      absl::Span<uint8_t>(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(
                              encoded_frame.data.data())),
                          encoded_frame.data.size()));
}

openscreen::cast::AudioCodec ToOpenscreenAudioCodec(media::cast::Codec codec) {
  switch (codec) {
    case CODEC_AUDIO_REMOTE:
      return openscreen::cast::AudioCodec::kNotSpecified;
    case CODEC_AUDIO_OPUS:
      return openscreen::cast::AudioCodec::kOpus;
    case CODEC_AUDIO_AAC:
      return openscreen::cast::AudioCodec::kAac;
    default:
      NOTREACHED();
      return openscreen::cast::AudioCodec::kNotSpecified;
  }
}

openscreen::cast::VideoCodec ToOpenscreenVideoCodec(media::cast::Codec codec) {
  switch (codec) {
    case CODEC_VIDEO_REMOTE:
      return openscreen::cast::VideoCodec::kNotSpecified;
    case CODEC_VIDEO_VP8:
      return openscreen::cast::VideoCodec::kVp8;
    case CODEC_VIDEO_H264:
      return openscreen::cast::VideoCodec::kH264;
    case CODEC_VIDEO_VP9:
      return openscreen::cast::VideoCodec::kVp9;
    case CODEC_VIDEO_AV1:
      return openscreen::cast::VideoCodec::kAv1;
    default:
      NOTREACHED();
      return openscreen::cast::VideoCodec::kNotSpecified;
  }
}
media::cast::Codec ToCodec(openscreen::cast::AudioCodec codec) {
  switch (codec) {
    case openscreen::cast::AudioCodec::kNotSpecified:
      return CODEC_AUDIO_REMOTE;
    case openscreen::cast::AudioCodec::kOpus:
      return CODEC_AUDIO_OPUS;
    case openscreen::cast::AudioCodec::kAac:
      return CODEC_AUDIO_AAC;
  }
  NOTREACHED();
  return CODEC_UNKNOWN;
}

media::cast::Codec ToCodec(openscreen::cast::VideoCodec codec) {
  switch (codec) {
    case openscreen::cast::VideoCodec::kNotSpecified:
      return CODEC_VIDEO_REMOTE;
    case openscreen::cast::VideoCodec::kVp8:
      return CODEC_VIDEO_VP8;
    case openscreen::cast::VideoCodec::kH264:
      return CODEC_VIDEO_H264;
    case openscreen::cast::VideoCodec::kVp9:
      return CODEC_VIDEO_VP9;
    case openscreen::cast::VideoCodec::kAv1:
      return CODEC_VIDEO_AV1;
    case openscreen::cast::VideoCodec::kHevc:
      return CODEC_UNKNOWN;
  }
  NOTREACHED();
  return CODEC_UNKNOWN;
}

openscreen::IPAddress ToOpenscreenIPAddress(const net::IPAddress& address) {
  const auto version = address.IsIPv6() ? openscreen::IPAddress::Version::kV6
                                        : openscreen::IPAddress::Version::kV4;
  return openscreen::IPAddress(version, address.bytes().data());
}

openscreen::cast::AudioCaptureConfig ToOpenscreenAudioConfig(
    const FrameSenderConfig& config) {
  return openscreen::cast::AudioCaptureConfig{
      .codec = media::cast::ToOpenscreenAudioCodec(config.codec),
      .channels = config.channels,
      .bit_rate = config.max_bitrate,
      .sample_rate = config.rtp_timebase,
      .target_playout_delay =
          std::chrono::milliseconds(config.max_playout_delay.InMilliseconds()),
      .codec_parameter = std::string()};
}

openscreen::cast::VideoCaptureConfig ToOpenscreenVideoConfig(
    const FrameSenderConfig& config) {
  // Currently we just hardcode 1080P as the resolution.
  static constexpr openscreen::cast::Resolution kResolutions[] = {{1920, 1080}};

  // NOTE: currently we only support a frame rate of 30FPS, so casting
  // directly to an integer is fine.
  return openscreen::cast::VideoCaptureConfig{
      .codec = media::cast::ToOpenscreenVideoCodec(config.codec),
      .max_frame_rate =
          openscreen::SimpleFraction{static_cast<int>(config.max_frame_rate),
                                     1},
      .max_bit_rate = config.max_bitrate,
      .resolutions =
          std::vector(std::begin(kResolutions), std::end(kResolutions)),
      .target_playout_delay =
          std::chrono::milliseconds(config.max_playout_delay.InMilliseconds()),
      .codec_parameter = std::string()};
}

}  // namespace media::cast
