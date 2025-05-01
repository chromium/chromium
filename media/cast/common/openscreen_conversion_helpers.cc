// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/common/openscreen_conversion_helpers.h"

#include <iterator>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"
#include "media/cast/cast_config.h"
#include "third_party/openscreen/src/platform/base/span.h"

namespace media::cast {

namespace {

using media::mojom::RemotingSinkAudioCapability;
using media::mojom::RemotingSinkVideoCapability;

}  // namespace

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
  // Open Screen does not permit nullptr data properties. We also cannot set it
  // here, since it needs to be owned outside of `encoded_frame.`
  CHECK(encoded_frame.data.data());
  return openscreen::cast::EncodedFrame(
      encoded_frame.is_key_frame
          ? openscreen::cast::EncodedFrame::Dependency::kKeyFrame
          : openscreen::cast::EncodedFrame::Dependency::kDependent

      ,
      encoded_frame.frame_id, encoded_frame.referenced_frame_id,
      encoded_frame.rtp_timestamp,
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
    case media::VideoCodec::kVP9:
      return openscreen::cast::VideoCodec::kVp9;
    case media::VideoCodec::kAV1:
      return openscreen::cast::VideoCodec::kAv1;
    case media::VideoCodec::kH264:
      return openscreen::cast::VideoCodec::kH264;
    case media::VideoCodec::kHEVC:
      return openscreen::cast::VideoCodec::kHevc;
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

RemotingSinkAudioCapability ToRemotingAudioCapability(
    openscreen::cast::AudioCapability capability) {
  switch (capability) {
    case openscreen::cast::AudioCapability::kBaselineSet:
      return RemotingSinkAudioCapability::CODEC_BASELINE_SET;

    case openscreen::cast::AudioCapability::kAac:
      return RemotingSinkAudioCapability::CODEC_AAC;

    case openscreen::cast::AudioCapability::kOpus:
      return RemotingSinkAudioCapability::CODEC_OPUS;
  }
}

RemotingSinkVideoCapability ToRemotingVideoCapability(
    openscreen::cast::VideoCapability capability) {
  switch (capability) {
    case openscreen::cast::VideoCapability::kSupports4k:
      return RemotingSinkVideoCapability::SUPPORT_4K;

    case openscreen::cast::VideoCapability::kH264:
      return RemotingSinkVideoCapability::CODEC_H264;

    case openscreen::cast::VideoCapability::kVp8:
      return RemotingSinkVideoCapability::CODEC_VP8;

    case openscreen::cast::VideoCapability::kVp9:
      return RemotingSinkVideoCapability::CODEC_VP9;

    case openscreen::cast::VideoCapability::kHevc:
      return RemotingSinkVideoCapability::CODEC_HEVC;

    case openscreen::cast::VideoCapability::kAv1:
      return RemotingSinkVideoCapability::CODEC_AV1;
  }
}

// Convert the sink capabilities to media::mojom::RemotingSinkMetadata.
media::mojom::RemotingSinkMetadata ToRemotingSinkMetadata(
    const openscreen::cast::RemotingCapabilities& capabilities,
    std::string_view friendly_name) {
  media::mojom::RemotingSinkMetadata sink_metadata;
  sink_metadata.friendly_name = friendly_name;

  std::transform(capabilities.audio.begin(), capabilities.audio.end(),
                 std::back_insert_iterator(sink_metadata.audio_capabilities),
                 ToRemotingAudioCapability);

  std::transform(capabilities.video.begin(), capabilities.video.end(),
                 std::back_insert_iterator(sink_metadata.video_capabilities),
                 ToRemotingVideoCapability);

  return sink_metadata;
}

}  // namespace media::cast
