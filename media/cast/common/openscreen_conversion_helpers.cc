// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/common/openscreen_conversion_helpers.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"
#include "media/cast/cast_config.h"
#include "third_party/openscreen/src/platform/base/span.h"

namespace media::cast {

openscreen::Clock::time_point ToOpenscreenTimePoint(
    std::optional<base::TimeTicks> ticks) {
  if (!ticks) {
    return openscreen::Clock::time_point::min();
  }
  return ToOpenscreenTimePoint(*ticks);
}

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
      ToOpenscreenTimePoint(encoded_frame.capture_begin_time),
      ToOpenscreenTimePoint(encoded_frame.capture_end_time),
      openscreen::ByteView(
          reinterpret_cast<const uint8_t*>(encoded_frame.data.data()),
          encoded_frame.data.size()));
}

openscreen::cast::AudioCodec ToOpenscreenAudioCodec(media::AudioCodec codec) {
  switch (codec) {
    case media::AudioCodec::kUnknown:
      return openscreen::cast::AudioCodec::kNotSpecified;
    case media::AudioCodec::kOpus:
      return openscreen::cast::AudioCodec::kOpus;
    case media::AudioCodec::kAAC:
      return openscreen::cast::AudioCodec::kAac;
    default:
      NOTREACHED();
  }
}

openscreen::cast::VideoCodec ToOpenscreenVideoCodec(media::VideoCodec codec) {
  switch (codec) {
    case media::VideoCodec::kUnknown:
      return openscreen::cast::VideoCodec::kNotSpecified;
    case media::VideoCodec::kVP8:
      return openscreen::cast::VideoCodec::kVp8;
    case media::VideoCodec::kH264:
      return openscreen::cast::VideoCodec::kH264;
    case media::VideoCodec::kVP9:
      return openscreen::cast::VideoCodec::kVp9;
    case media::VideoCodec::kAV1:
      return openscreen::cast::VideoCodec::kAv1;
    default:
      NOTREACHED();
  }
}

AudioCodec ToAudioCodec(openscreen::cast::AudioCodec codec) {
  switch (codec) {
    case openscreen::cast::AudioCodec::kNotSpecified:
      // media::AudioCodec doesn't have a concept of "not specified."
      return AudioCodec::kUnknown;
    case openscreen::cast::AudioCodec::kOpus:
      return AudioCodec::kOpus;
    case openscreen::cast::AudioCodec::kAac:
      return AudioCodec::kAAC;
  }
  NOTREACHED();
}

VideoCodec ToVideoCodec(openscreen::cast::VideoCodec codec) {
  switch (codec) {
    // media::VideoCodec doesn't have a concept of "not specified."
    case openscreen::cast::VideoCodec::kNotSpecified:
      return VideoCodec::kUnknown;
    case openscreen::cast::VideoCodec::kVp8:
      return VideoCodec::kVP8;
    case openscreen::cast::VideoCodec::kH264:
      return VideoCodec::kH264;
    case openscreen::cast::VideoCodec::kVp9:
      return VideoCodec::kVP9;
    case openscreen::cast::VideoCodec::kAv1:
      return VideoCodec::kAV1;
    case openscreen::cast::VideoCodec::kHevc:
      return VideoCodec::kHEVC;
  }
  NOTREACHED();
}

openscreen::IPAddress ToOpenscreenIPAddress(const net::IPAddress& address) {
  const auto version = address.IsIPv6() ? openscreen::IPAddress::Version::kV6
                                        : openscreen::IPAddress::Version::kV4;
  return openscreen::IPAddress(version, address.bytes().data());
}

std::array<uint8_t, kAesKeyLength> AesKeyToArray(std::string aes_key) {
  std::vector<uint8_t> vec;
  if (!base::HexStringToBytes(aes_key, &vec)) {
    return {};
  }
  if (vec.size() != static_cast<unsigned long>(kAesKeyLength)) {
    return {};
  }
  std::array<uint8_t, kAesKeyLength> out;
  for (size_t i = 0; i < vec.size(); ++i) {
    out[i] = vec[i];
  }
  return out;
}

openscreen::cast::SessionConfig ToOpenscreenSessionConfig(
    const FrameSenderConfig& config,
    bool is_pli_enabled) {
  return openscreen::cast::SessionConfig(
      config.sender_ssrc, config.receiver_ssrc, config.rtp_timebase,
      config.channels,
      std::chrono::milliseconds(config.max_playout_delay.InMilliseconds()),

      AesKeyToArray(config.aes_key), AesKeyToArray(config.aes_iv_mask),
      is_pli_enabled);
}

openscreen::cast::AudioCaptureConfig ToOpenscreenAudioConfig(
    const FrameSenderConfig& config) {
  return openscreen::cast::AudioCaptureConfig{
      .codec = ToOpenscreenAudioCodec(config.audio_codec()),
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
      .codec = ToOpenscreenVideoCodec(config.video_codec()),
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
