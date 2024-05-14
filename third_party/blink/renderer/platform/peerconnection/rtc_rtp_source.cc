// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_source.h"

#include <cmath>

#include "base/notreached.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/system_wrappers/include/ntp_time.h"

namespace blink {

RTCRtpSource::RTCRtpSource(const webrtc::RtpSource& source) : source_(source) {}

RTCRtpSource::~RTCRtpSource() {}

RTCRtpSource::Type RTCRtpSource::SourceType() const {
  switch (source_.source_type()) {
    case webrtc::RtpSourceType::SSRC:
      return RTCRtpSource::Type::kSSRC;
    case webrtc::RtpSourceType::CSRC:
      return RTCRtpSource::Type::kCSRC;
    default:
      NOTREACHED_IN_MIGRATION();
      return RTCRtpSource::Type::kSSRC;
  }
}

base::TimeTicks RTCRtpSource::Timestamp() const {
  return ConvertToBaseTimeTicks(source_.timestamp());
}

uint32_t RTCRtpSource::Source() const {
  return source_.source_id();
}

std::optional<double> RTCRtpSource::AudioLevel() const {
  if (!source_.audio_level())
    return std::nullopt;
  // Converted according to equation defined here:
  // https://w3c.github.io/webrtc-pc/#dom-rtcrtpcontributingsource-audiolevel
  uint8_t rfc_level = *source_.audio_level();
  if (rfc_level > 127u)
    rfc_level = 127u;
  if (rfc_level == 127u)
    return 0.0;
  return std::pow(10.0, -(double)rfc_level / 20.0);
}

uint32_t RTCRtpSource::RtpTimestamp() const {
  return source_.rtp_timestamp();
}

std::optional<int64_t> RTCRtpSource::CaptureTimestamp() const {
  if (!source_.absolute_capture_time().has_value()) {
    return std::nullopt;
  }
  return webrtc::UQ32x32ToInt64Ms(
      source_.absolute_capture_time()->absolute_capture_timestamp);
}

std::optional<int64_t> RTCRtpSource::SenderCaptureTimeOffset() const {
  if (!source_.absolute_capture_time().has_value() ||
      !source_.absolute_capture_time()
           ->estimated_capture_clock_offset.has_value()) {
    return std::nullopt;
  }
  return webrtc::Q32x32ToInt64Ms(
      source_.absolute_capture_time()->estimated_capture_clock_offset.value());
}

}  // namespace blink
